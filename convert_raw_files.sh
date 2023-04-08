#!/bin/bash
# Convert all files with .raw extension located at the root of the current folder into .wav files.
# Usage: ./convert_raw_files.sh

total=$(find . -maxdepth 1 -name "*.raw" | wc -l)
if [ ! $total -eq 0 ]; then
    count=0
    for rawfile in *.raw
    do
        echo "Processing file $((count=count+1)) of $total: $rawfile"
        ./sandec/sandec "$rawfile"
    done
    echo "All conversions are complete."
else
    echo "No .raw files found"
fi
