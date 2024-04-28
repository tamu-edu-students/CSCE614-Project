#!/bin/bash
#SBATCH --time=8:00:00
#SBATCH --mem=8GB
#SBATCH --ntasks=1

bin=$1
trace=$2
warmup_instructions=$3
simulation_instructions=$4

module load GCCcore/7.3.0

# Run champsim
time {
    $bin \
        --warmup-instructions $warmup_instructions \
        --simulation-instructions $simulation_instructions \
        $trace
}