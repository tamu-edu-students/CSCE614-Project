#!/bin/bash

# Specify the directory path
directory="run-grace/build/configs"

cd '../..'

module load GCCcore/7.3.0

# Loop over each file in the directory
for file in "$directory"/*
do
    if [ -f "$file" ]; then
        echo "Processing file: $file"
        ./config.sh $file
        make
    fi
done

cd -