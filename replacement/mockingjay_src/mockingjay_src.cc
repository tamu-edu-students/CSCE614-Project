#include <unordered_map>
#include <stdlib.h>

#include <math.h>

#include "ooo_cpu.h"
#include "cache.h"

using namespace std;

const uint32_t NUM_SET = 2048;
const uint32_t NUM_WAY = 16;

constexpr uint32_t BUDGET = 2;

const int LOG2_LLC_SET = log2(NUM_SET);
constexpr int LOG2_LLC_SIZE = LOG2_LLC_SET + log2(NUM_WAY) + LOG2_BLOCK_SIZE; // 11 + 4 + 6 = 21
constexpr int LOG2_SAMPLED_SETS = (LOG2_LLC_SIZE - 16) * BUDGET; // 21 - 16

constexpr int HISTORY = 8 * BUDGET;
// "constant factor f , where f is set to 8 in our evaluation"
constexpr int F_FACTOR = 8;

// "Infinite reuse distance, which should be 127"
constexpr int INF_RD = 127;
// "MAX_RD = 104"
constexpr int MAX_RD = INF_RD - 23;
constexpr int INF_ETR = (NUM_WAY * HISTORY / F_FACTOR) - 1;

constexpr int SAMPLED_CACHE_WAYS = 5 * BUDGET; // "We implement the Sampled Cache as a 5-way set-associative cache with 512 sets."
constexpr int LOG2_SAMPLED_CACHE_SETS = 4;
constexpr int SAMPLED_CACHE_TAG_BITS = 31 - LOG2_LLC_SIZE;
constexpr int PC_SIGNATURE_BITS = 11 + BUDGET; // "11-bit PC signature"
constexpr int TIMESTAMP_BITS = 8 + BUDGET;

constexpr double TEMP_DIFFERENCE = 1.0/16.0;

int etr[NUM_SET][NUM_WAY];
int etr_clock[NUM_SET];

// "The RDP is a direct-mapped array that is indexed by the PC signature"
unordered_map<uint32_t, int> rdp;

int current_timestamp[NUM_SET];

struct SampledCacheLine {
    bool valid;
    uint64_t tag;
    uint64_t signature;
    int timestamp;
};
unordered_map<uint32_t, SampledCacheLine* > sampled_cache;


bool is_sampled_set(int set) {
    int mask_length = LOG2_LLC_SET-LOG2_SAMPLED_SETS;
    int mask = (1 << mask_length) - 1;
    return (set & mask) == ((set >> (LOG2_LLC_SET - mask_length)) & mask);
}

// uint64_t CRC_HASH( uint64_t _blockAddress )
// {
//     static const unsigned long long crcPolynomial = 3988292384ULL; // "10-bit block address hash"
//     unsigned long long _returnVal = _blockAddress;
//     for( unsigned int i = 0; i < 3; i++)
//         _returnVal = ( ( _returnVal & 1 ) == 1 ) ? ( ( _returnVal >> 1 ) ^ crcPolynomial ) : ( _returnVal >> 1 );
//     return _returnVal;
// }

uint64_t get_pc_signature(uint64_t pc, bool hit) {
    pc = pc << 1;
    if(hit) {
        pc = pc | 1;
    }
    pc = pc << 1;
    //pc = CRC_HASH(pc);
    pc = (pc << (64 - PC_SIGNATURE_BITS)) >> (64 - PC_SIGNATURE_BITS);
    return pc;
}

uint32_t get_sampled_cache_index(uint64_t full_addr) {
    full_addr = full_addr >> LOG2_BLOCK_SIZE;
    full_addr = (full_addr << (64 - (LOG2_SAMPLED_CACHE_SETS + LOG2_LLC_SET))) >> (64 - (LOG2_SAMPLED_CACHE_SETS + LOG2_LLC_SET));
    return full_addr;
}

uint64_t get_sampled_cache_tag(uint64_t x) {
    x >>= LOG2_LLC_SET + LOG2_BLOCK_SIZE + LOG2_SAMPLED_CACHE_SETS;
    x = (x << (64 - SAMPLED_CACHE_TAG_BITS)) >> (64 - SAMPLED_CACHE_TAG_BITS);
    return x;
}

int search_sampled_cache(uint64_t blockAddress, uint32_t set) {
    SampledCacheLine* sampled_set = sampled_cache[set];
    for (uint32_t way = 0; way < SAMPLED_CACHE_WAYS; way++) {
        if (sampled_set[way].valid && (sampled_set[way].tag == blockAddress)) {
            return way;
        }
    }
    // Not found
    return -1;
}

void detrain(uint32_t set, uint32_t way) {
    SampledCacheLine temp = sampled_cache[set][way];
    if (!temp.valid) {
        return;
    }

    if (rdp.count(temp.signature)) {
        rdp[temp.signature] = min(rdp[temp.signature] + 1, INF_RD);
    } else {
        rdp[temp.signature] = INF_RD;
    }
    sampled_cache[set][way].valid = false;
}


// initialize cache replacement state
void CACHE::initialize_replacement()
{
    for(uint32_t i = 0; i < NUM_SET; i++) {
        etr_clock[i] = F_FACTOR;
        current_timestamp[i] = 0;
    }
    for(uint32_t set = 0; set < NUM_SET; set++) {
        if (is_sampled_set(set)) {
            int modifier = 1 << LOG2_LLC_SET;
            int limit = 1 << LOG2_SAMPLED_CACHE_SETS;
            for (int i = 0; i < limit; i++) {
                sampled_cache[set + modifier*i] = new SampledCacheLine[SAMPLED_CACHE_WAYS]();
            }
        }
    }
}


/* find a cache block to evict
 * return value should be 0 ~ 15 (corresponds to # of ways in cache) 
 * current_set: an array of BLOCK, of size 16 */
uint32_t CACHE::find_victim(uint32_t triggering_cpu, uint64_t instr_id, uint32_t set, const BLOCK *current_set, uint64_t pc, uint64_t full_addr, uint32_t type)
{
    /* don't modify this code or put anything above it;
     * if there's an invalid block, we don't need to evict any valid ones */
    for (uint32_t way = 0; way < NUM_WAY; way++) {
        if (current_set[way].valid == false) {
            return way;
        }
    }


    int max_etr = 0;
    uint32_t victim_way = 0;
    for (uint32_t way = 0; way < NUM_WAY; way++) {
        if (abs(etr[set][way]) > max_etr ||
                (abs(etr[set][way]) == max_etr &&
                        etr[set][way] < 0)) {
            max_etr = abs(etr[set][way]);
            victim_way = way;
        }
    }
    
    uint64_t pc_signature = get_pc_signature(pc, false);
    if (access_type{type} != access_type::WRITE && rdp.count(pc_signature) && (rdp[pc_signature] > MAX_RD || rdp[pc_signature] / F_FACTOR > max_etr)) {
        return NUM_WAY;
    }
    
    return victim_way;
}


int temporal_difference(int init, int sample) {
    if (sample > init) {
        int diff = sample - init;
        diff = diff * TEMP_DIFFERENCE;
        diff = min(1, diff);
        return min(init + diff, INF_RD);
    } else if (sample < init) {
        int diff = init - sample;
        diff = diff * TEMP_DIFFERENCE;
        diff = min(1, diff);
        // "Finally, on a cache miss, the line with the largest absolute ETR value is evicted.""
        return max(init - diff, 0);
    } else {
        return init;
    }
}

int increment_timestamp(int input) {
    input++;
    // "add 1 << TIMESTAMP BITS to the current timestamp to prevent overflow."
    input = input % (1 << TIMESTAMP_BITS);
    return input;
}

int time_elapsed(int global, int local) {
    if (global >= local) {
        return global - local;
    }
    // "In this case, we add 1 << TIMESTAMP BITS to the current timestamp before computing the difference."
    global = global + (1 << TIMESTAMP_BITS);
    return global - local;
}


// called on every cache hit and cache fill
void CACHE::update_replacement_state(uint32_t triggering_cpu, uint32_t set, uint32_t way, uint64_t full_addr, uint64_t pc, uint64_t victim_addr, uint32_t type, uint8_t hit)
{
    if (access_type{type} == access_type::WRITE) {
        if(!hit) {
            etr[set][way] = -INF_ETR;
        }
        return;
    }
        

    pc = get_pc_signature(pc, hit);


    if (is_sampled_set(set)) {
        uint32_t sampled_cache_index = get_sampled_cache_index(full_addr);
        uint64_t sampled_cache_tag = get_sampled_cache_tag(full_addr);

        // "For every access to a sampled LLC set, the Sampled Cache is searched"
        int sampled_cache_way = search_sampled_cache(sampled_cache_tag, sampled_cache_index);
        // "On a Sampled Cache hit..."
        if (sampled_cache_way > -1) {
            uint64_t last_signature = sampled_cache[sampled_cache_index][sampled_cache_way].signature;
            uint64_t last_timestamp = sampled_cache[sampled_cache_index][sampled_cache_way].timestamp;
            int sample = time_elapsed(current_timestamp[set], last_timestamp);

            if (sample <= INF_RD) {
                // "last timestamp of the block is used to train the RDP with the observed reuse distance."
                if (rdp.count(last_signature)) {
                    int init = rdp[last_signature];
                    rdp[last_signature] = temporal_difference(init, sample);
                } else {
                    rdp[last_signature] = sample;
                }

                sampled_cache[sampled_cache_index][sampled_cache_way].valid = false;
            }
        }

        // "On a Sampled Cache miss, the least recently used line is evicted from the Sample Cache"        
        int lru_way = -1;
        int lru_rd = -1;
        for (uint32_t w = 0; w < SAMPLED_CACHE_WAYS; w++) {
            if (sampled_cache[sampled_cache_index][w].valid == false) {
                lru_way = w;
                lru_rd = INF_RD + 1;
                continue;
            }

            uint64_t last_timestamp = sampled_cache[sampled_cache_index][w].timestamp;
            int sample = time_elapsed(current_timestamp[set], last_timestamp);
            if (sample > INF_RD) {
                lru_way = w;
                lru_rd = INF_RD + 1;
                detrain(sampled_cache_index, w);
            } else if (sample > lru_rd) {
                lru_way = w;
                lru_rd = sample;
            }
        }
        detrain(sampled_cache_index, lru_way);


        for (int w = 0; w < SAMPLED_CACHE_WAYS; w++) {
            if (sampled_cache[sampled_cache_index][w].valid == false) {
                sampled_cache[sampled_cache_index][w].valid = true;
                sampled_cache[sampled_cache_index][w].signature = pc;
                sampled_cache[sampled_cache_index][w].tag = sampled_cache_tag;
                sampled_cache[sampled_cache_index][w].timestamp = current_timestamp[set];
                break;
            }
        }
        
        current_timestamp[set] = increment_timestamp(current_timestamp[set]);
    }

    if(etr_clock[set] == F_FACTOR) {
        for (uint32_t w = 0; w < NUM_WAY; w++) {
            if ((uint32_t) w != way && abs(etr[set][w]) < INF_ETR) {
                etr[set][w]--;
            }
        }
        etr_clock[set] = 0;
    }
    etr_clock[set]++;
    
    
    if (way < NUM_WAY) {
        if(!rdp.count(pc)) {
            etr[set][way] = 0;
        } else {
            if(rdp[pc] > MAX_RD) {
                etr[set][way] = INF_ETR;
            } else {
                etr[set][way] = rdp[pc] / F_FACTOR;
            }
        }
    }
}


// use this function to print out your own stats at the end of simulation
void CACHE::replacement_final_stats() {}