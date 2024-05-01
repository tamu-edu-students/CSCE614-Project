#!/bin/bash

traces="/scratch/user/djimenez/614/hw1/ersatz-benchmarks/traces"
mem_traces=(
    "/scratch/user/djimenez/speccpu/602.gcc_s-1850B.champsimtrace.xz"
    "/scratch/user/djimenez/speccpu/602.gcc_s-2226B.champsimtrace.xz"
    "/scratch/user/djimenez/speccpu/603.bwaves_s-1740B.champsimtrace.xz"
    "/scratch/user/djimenez/speccpu/603.bwaves_s-2609B.champsimtrace.xz"
    "/scratch/user/djimenez/speccpu/603.bwaves_s-891B.champsimtrace.xz"
    "/scratch/user/djimenez/speccpu/605.mcf_s-1152B.champsimtrace.xz"
    "/scratch/user/djimenez/speccpu/605.mcf_s-1536B.champsimtrace.xz"
    "/scratch/user/djimenez/speccpu/605.mcf_s-1554B.champsimtrace.xz"
    "/scratch/user/djimenez/speccpu/605.mcf_s-1644B.champsimtrace.xz"
    "/scratch/user/djimenez/speccpu/605.mcf_s-472B.champsimtrace.xz"
    "/scratch/user/djimenez/speccpu/605.mcf_s-484B.champsimtrace.xz"
    "/scratch/user/djimenez/speccpu/605.mcf_s-665B.champsimtrace.xz"
    "/scratch/user/djimenez/speccpu/605.mcf_s-782B.champsimtrace.xz"
    "/scratch/user/djimenez/speccpu/605.mcf_s-994B.champsimtrace.xz"
    "/scratch/user/djimenez/speccpu/607.cactuBSSN_s-2421B.champsimtrace.xz"
    "/scratch/user/djimenez/speccpu/619.lbm_s-2676B.champsimtrace.xz"
    "/scratch/user/djimenez/speccpu/619.lbm_s-2677B.champsimtrace.xz"
    "/scratch/user/djimenez/speccpu/619.lbm_s-3766B.champsimtrace.xz"
    "/scratch/user/djimenez/speccpu/619.lbm_s-4268B.champsimtrace.xz"
    "/scratch/user/djimenez/speccpu/620.omnetpp_s-141B.champsimtrace.xz"
    "/scratch/user/djimenez/speccpu/620.omnetpp_s-874B.champsimtrace.xz"
    "/scratch/user/djimenez/speccpu/621.wrf_s-6673B.champsimtrace.xz"
    "/scratch/user/djimenez/speccpu/621.wrf_s-8065B.champsimtrace.xz"
    "/scratch/user/djimenez/speccpu/623.xalancbmk_s-10B.champsimtrace.xz"
    "/scratch/user/djimenez/speccpu/623.xalancbmk_s-202B.champsimtrace.xz"
    "/scratch/user/djimenez/speccpu/627.cam4_s-490B.champsimtrace.xz"
    "/scratch/user/djimenez/speccpu/649.fotonik3d_s-10881B.champsimtrace.xz"
    "/scratch/user/djimenez/speccpu/654.roms_s-1390B.champsimtrace.xz"
    "/scratch/user/djimenez/speccpu/654.roms_s-523B.champsimtrace.xz"
)

binaries="build/bin"

mkdir -p "out-300M"

for bin in "$binaries"/*; do
    binname=$(basename $bin)
    mkdir -p "out-300M/$binname"
    echo $binname
    #for trace in "$traces"/*; do
    for trace in "${mem_traces[@]}"; do
        if [ -f "$trace" ]; then
            tracename=$(basename $trace)
            sbatch \
            --output="out-300M/$binname/$tracename.%j" \
            --job-name="$binname-$tracename" \
            job.sh $bin $trace 100000000 300000000
            echo $tracename
        fi
    done
done
