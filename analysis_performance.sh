#!/bin/bash

start=$(date +%s.%N)

count=10
total_duration=0
for ((i=1; i<=$count; i++))
do
    build/X86/gem5.opt --debug-flags EccMemobj configs/mark_ecc/ecc_memobj.py
done
end=$(date +%s.%N)

duration=$(echo "$end - $start" | bc)
echo "Execution time: $duration seconds"