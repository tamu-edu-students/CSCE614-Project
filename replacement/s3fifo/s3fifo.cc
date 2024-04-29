#include <algorithm>
#include <cassert>
#include <map>
#include <vector>
#include <queue>
//#include <iostream>

#include "cache.h"

#define NUM_WAY 16
#define MAX_HOT 255

struct CacheObject
{
    uint32_t id;
    uint8_t hotness; // Using 2 bytes
};

class S3_FIFO
{
private:
    const uint32_t cacheSize;
    const uint32_t smallQueueSize;
    std::queue<CacheObject> smallQueue;
    std::queue<CacheObject> mainQueue;
    std::queue<CacheObject> ghostQueue;
    std::unordered_map<uint32_t, uint32_t> cacheMap;

public:
    S3_FIFO(uint32_t size) : cacheSize(size), smallQueueSize(size / 8) {}
    // need to set up main queue size and ghostQueue size to be 90% of cache size

    bool isInCache(uint32_t id)
    {
        return cacheMap.find(id) != cacheMap.end();
    }

    void accessObject(uint32_t id)
    {
        if (isInCache(id))
        {
            updateHotness(id);
            // cout << "Cache hit for id: " << id << endl;
        }
        else
        {
            // Cache miss. Check Ghost Queue for id.
            if (isTrackedInGhostQueue(id))
            {
                std::queue<CacheObject> tempGhost;
                while (!ghostQueue.empty())
                {
                    CacheObject obj = ghostQueue.front();
                    ghostQueue.pop();
                    if (obj.id == id)
                    {
                        insertIntoMainQueue(obj.id);
                    }
                    else
                    {
                        tempGhost.push(obj);
                    }
                }
                ghostQueue = tempGhost;
            }
            else
            {
                // Object not tracked in ghost queue, insert into small queue
                insertIntoSmallQueue(id);
            }
            // cout << "Cache miss for id: " << id << endl;
        }
    }

    void insertIntoSmallQueue(uint32_t id)
    {
        if (smallQueue.size() >= smallQueueSize)
        {
            evictFromSmallQueue();
        }
        CacheObject newObj = {id, 0};
        smallQueue.push(newObj);
        cacheMap[id] = newObj.id;
    }

    void insertIntoMainQueue(uint32_t id)
    {
        if (mainQueue.size() >= (cacheSize - smallQueueSize))
        {
            evictFromMainQueue();
        }
        CacheObject newObj = {id, 0};
        mainQueue.push(newObj);
        cacheMap[id] = newObj.id;
    }

    // Update hotness of an object
    /*
     * created steps to check in ghost and small queue. Can probably be simplified more.
     */
    void updateHotness(uint32_t objectId)
    {
        // Search in main queue
        std::queue<CacheObject> tempMain;
        bool foundMain = false;

        while (!mainQueue.empty())
        {
            CacheObject obj = mainQueue.front();
            mainQueue.pop();
            if (obj.id == objectId)
            {
                if (obj.hotness != MAX_HOT)
                    obj.hotness++;
                foundMain = true;
            }
            tempMain.push(obj);
        }
        mainQueue = tempMain;
        if (foundMain)
        {
            return;
        }

        // search in small queue
        std::queue<CacheObject> tempSmall;
        while (!smallQueue.empty())
        {
            CacheObject obj = smallQueue.front();
            smallQueue.pop();
            if (obj.id == objectId)
            {
                if (obj.hotness != MAX_HOT)
                    obj.hotness++;
            }
            tempSmall.push(obj);
        }
        smallQueue = tempSmall;
        return;
    }

    /*
     * rewrote function so that it evicts value from small queue.
     * If heat = 0, insert into ghost queue. If heat > 0, insert into main queue.
     * -Jonathan
     */
    void evictFromSmallQueue()
    {
        CacheObject obj = smallQueue.front();
        smallQueue.pop();
        if (obj.hotness == 0)
        {
            ghostQueue.push(obj);
            //cacheMap.erase(obj.id); commenting out this to test caching format
            if (ghostQueue.size() >= (cacheSize - smallQueueSize))
            {
                ghostQueue.pop();
            }
        }
        else
        {
            obj.hotness = 0;
            mainQueue.push(obj);
        }
    }

    /*
     * Leaving alone for now. The example does not show how evictions from main queue works.
     * Based on the description, this seems correct.
     * -Jonathan
     */
    uint32_t evictFromMainQueue()
    {
        while (!mainQueue.empty())
        {
            CacheObject obj = mainQueue.front();
            mainQueue.pop();
            // if hotness is 0, remove from main queue and clear from cache
            if (obj.hotness == 0)
            {
                cacheMap.erase(obj.id);
                return obj.id;
            }
            else
            {
                // Reduce hotness of id and reinsert into main queue
                obj.hotness--;
                mainQueue.push(obj);
            }
        }
        // TODO: What to return if mainqueue empty??
        return -1;
    }

    uint32_t evictFromQueue(uint32_t set)
    {

        //****************************************************************************************************************************
        //checking for ID in ghostqueue since it is no longer removed from cache
        std::queue<CacheObject> tempGhost;
        bool foundGhost = false;
        uint32_t ghostVictim;
        uint32_t comb = set * NUM_WAY;

        while (!ghostQueue.empty())
        {
            CacheObject obj = ghostQueue.front();
            ghostQueue.pop();
            // changed to obj.id >= set * NUM_WAY because it is inclusive of begin but not end. Also added !findGhost to only find the first id in range for the ghost queue
            if (obj.id >= comb && obj.id  < (comb + NUM_WAY) && !foundGhost)
            {
                foundGhost = true;
                ghostVictim = obj.id - comb;
                cacheMap.erase(obj.id);
            } else {
                tempGhost.push(obj);
            }
        }
        ghostQueue = tempGhost;
        if (foundGhost)
        {
            //fifocntGhost++;
            return ghostVictim;
        }



        //****************************************************************************************************************************

        // std::cout << "IN EFMQ" << std::endl;
        uint32_t size = mainQueue.size();
        uint32_t cnt = 0;
        bool noExit = false;

        while (cnt < size || noExit) //will exit loop if all main queue values are checked an none are in the id range
        {
            // std::cout << "evictfrommain" << std::endl;
            CacheObject obj = mainQueue.front();
            mainQueue.pop();
            if (obj.id >= comb && obj.id - comb < NUM_WAY) //check to see if the object is within the range, otherwise leave it alone
            {
                if (obj.hotness == 0)
                {
                    // std::cout << "Using fifo" << std::endl;
                    //fifocntMain++;
                    //std::cout << "fifo: " << fifocnt << std::endl;
                    cacheMap.erase(obj.id);
                    uint32_t victim = obj.id - comb;
                    return victim;
                }else{
                    obj.hotness--; //removing the hotness from the values in the id range to prevent removing hotness unnecessarily on other values in mainqueue
                    noExit = true;
                }
            }
            mainQueue.push(obj);
            cnt++;

        }
        //****************************************************************************************************************************
        //checking small queue since no id is in range for ghost and main
        size = smallQueue.size();
        cnt = 0;
        noExit = false;

        while (cnt < size || noExit) //will exit loop if all main queue values are checked an none are in the id range
        {
            // std::cout << "evictfrommain" << std::endl;
            CacheObject obj = smallQueue.front();
            smallQueue.pop();
            if (obj.id >= comb && obj.id - comb < NUM_WAY) //check to see if the object is within the range, otherwise leave it alone
            {
                if (obj.hotness == 0)
                {
                    // std::cout << "Using fifo" << std::endl;
                    //fifocntSmall++;

                    cacheMap.erase(obj.id);
                    uint32_t victim = obj.id - comb;
                    // std::cout << victim << std::endl;

                    while(cnt < size-1){ //passing through the rest of small queue so that the order is the same as we started (excluding the victim)
                        obj = smallQueue.front();
                        smallQueue.pop();
                        smallQueue.push(obj);
                        cnt++;
                    }

                    return victim;
                }else{
                    obj.hotness--; //removing the hotness from the values in the id range to prevent removing hotness unnecessarily on other values in mainqueue
                    noExit = true;
                }
            }
            smallQueue.push(obj);
            cnt++;

        }
        return rand() % NUM_WAY;
    }

    bool isTrackedInGhostQueue(uint32_t id)
    {
        std::queue<CacheObject> temp = ghostQueue;
        while (!temp.empty())
        {
            if (temp.front().id == id)
                return true;
            temp.pop();
        }
        return false;
    }
};

namespace
{
    std::map<CACHE *, std::vector<uint64_t>> last_used_cycles;
    S3_FIFO cache(2048 * 16); // FIXME: More efficient way to
}

void CACHE::initialize_replacement() { ::last_used_cycles[this] = std::vector<uint64_t>(NUM_SET * NUM_WAY); }

uint32_t CACHE::find_victim(uint32_t triggering_cpu, uint64_t instr_id, uint32_t set, const BLOCK *current_set, uint64_t ip, uint64_t full_addr, uint32_t type)
{
    auto begin = std::next(std::begin(::last_used_cycles[this]), set * NUM_WAY);
    auto end = std::next(begin, NUM_WAY);
    auto victim = std::next(begin, ::cache.evictFromQueue(set));
    assert(begin <= victim);
    assert(victim < end);
    return static_cast<uint32_t>(std::distance(begin, victim));
}

void CACHE::update_replacement_state(uint32_t triggering_cpu, uint32_t set, uint32_t way, uint64_t full_addr, uint64_t ip, uint64_t victim_addr, uint32_t type,
                                     uint8_t hit)
{
    // Mark the way as being used on the current cycle
    if (!hit || access_type{type} != access_type::WRITE)
    { // Skip this for writeback hits
        ::last_used_cycles[this].at(set * NUM_WAY + way) = current_cycle;
        ::cache.accessObject(set * NUM_WAY + way);
    }
}

void CACHE::replacement_final_stats() {}