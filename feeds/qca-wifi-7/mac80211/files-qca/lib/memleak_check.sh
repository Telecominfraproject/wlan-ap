#!/bin/sh
# Copyright (c) 2024, Qualcomm Innovation Center, Inc. All rights reserved.
# SPDX-License-Identifier: ISC

temp_file=$(mktemp)

# Use find to populate the temporary file with file paths
find / -type f -name "soc_dp_stats" -print0 > "$temp_file"

# Define the threshold in KB (10MB = 10,000KB)
threshold_kb=10000

# Define a function to check and print processes with VmSize greater than the threshold
check_vmsize() {
    # Loop through each directory in /proc
    for pid_dir in /proc/*; do
        # Get the directory name (process ID) using 'basename'
        pid=$(basename "$pid_dir")

        # Check if the directory name is a valid numeric PID
        if [ "$pid" -eq "$pid" ] 2>/dev/null; then
            # Check if the 'status' file exists for this process
            if [ -f "/proc/$pid/status" ]; then
                # Extract the VmSize value from the 'status' file
                vmsize_kb=$(awk '/VmSize/ {print $2}' "/proc/$pid/status")

                # Check if VmSize is not empty and is greater than the threshold
                if [ -n "$vmsize_kb" ] && [ "$vmsize_kb" -gt "$threshold_kb" ]; then
                    # Get the process name
                    process_name=$(awk '/Name/ {print $2}' "/proc/$pid/status")
set -x
                    # Print process information
                    echo "Process ID: $pid"
                    echo "Process Name: $process_name"
                    echo "VmSize: ${vmsize_kb}KB"
		    cat /proc/$pid/status
	            cat /proc/$pid/statm
set +x
                    echo "========================="
                fi
            fi
        fi
    done
}

check_unreclaimable_memory() {
        # Get the unreclaimable memory in kilobytes
        unreclaimable_mem=$(grep -i SUnreclaim /proc/meminfo | awk '{print $2}');
        # Mention the threshold limit, where you want to end the cycle
        threshold=550600;
       echo 3 > /proc/sys/vm/drop_caches
       echo 2 > /proc/sys/vm/drop_caches
       echo 1 > /proc/sys/vm/drop_caches
set -x
        cat /proc/meminfo
set +x
        while IFS= read -r -d '' file; do
set -x
        cat "$file"
set +x
        done < "$temp_file"
set -x
        mpstat -P ALL
        cat /proc/interrupts
	cat /sys/kernel/debug/ath_memdebug/meminfo
set +x
        # Check if unreclaimable memory is greater than threshold
        if [[ $unreclaimable_mem -gt $threshold ]]
        then
                #echo "Unreclaimable Memory: ${unreclaimable_mem} MB"
                return 0
        else
                #echo "Unreclaimable Memory: ${unreclaimable_mem} MB"
                return 1

        fi
}

# This is sleep time for checking the memory. Currently it is 15 seconds.
sleep_time=15
while true; do
        check_unreclaimable_memory
        check_vmsize
        sleep $sleep_time
done
rm "$temp_file"

