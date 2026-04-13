#!/bin/sh
: '
 Copyright (c) 2020, The Linux Foundation. All rights reserved.
 Copyright (c) 2024, Qualcomm Innovation Center, Inc. All rights reserved.

 Permission to use, copy, modify, and/or distribute this software for any
 purpose with or without fee is hereby granted, provided that the above
 copyright notice and this permission notice appear in all copies.

 THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
'
SERVER=$(fw_printenv serverip | cut -c10-24)
TIMESTAMP=$(date +%y%m%d%H%M%S)$(printf "%04d" $((RANDOM % 10000)))

if [ -z "$SERVER" ]; then
    echo "Wrong configuration: SERVER is empty" > /dev/console
    exit 1
fi

COREDUMP_PATH="/sys/class/devcoredump"
LATEST_FILE=$(basename "$DEVPATH")

if [ -n "$LATEST_FILE" ] && [ -e "$COREDUMP_PATH/$LATEST_FILE/data" ]; then
    TARGET_PATH="$COREDUMP_PATH/$LATEST_FILE/failing_device"

    if [ -e "$TARGET_PATH/subsystem_device" ]; then
	# Extract the hexadecimal value (without '0x') from the file 'subsystem_device'
        target=$(sed -n 's/.*0x\([0-9a-fA-F]*\).*/\1/p' "$TARGET_PATH/subsystem_device")
        TARGET_PATH_N=$(readlink -n "$TARGET_PATH")
        pci_path=$(basename "$TARGET_PATH_N")
        pci_slot=$(echo "$pci_path" | awk '{print substr($0, 4, 1)}')

        FILENAME="q6dump-${target}-pci${pci_slot}-${TIMESTAMP}.bin"
    else
        BOARD_NAME_PATH="/tmp/sysinfo/board_name"
        PD_PATH="$TARGET_PATH"

        if [ -e "$PD_PATH/name" ]; then
            target=$(awk -F'[,-]' '{print $2}' "$BOARD_NAME_PATH")
            FILENAME="q6dump-${target}-rootpd-${TIMESTAMP}.bin"
        else
            target=$(awk -F'[,-]' '{print $2}' "$BOARD_NAME_PATH")
            pd_name=$(awk -F'_' '{print $NF}' "$PD_PATH/of_node/qcom,userpd-subsys-name")
            FILENAME="q6dump-${target}-${pd_name}-${TIMESTAMP}.bin"
        fi
    fi

    DUMPPATH="$COREDUMP_PATH/$LATEST_FILE/data"
fi

if [ -n "$FILENAME" ]; then
    echo "Collecting $FILENAME to $SERVER" > /dev/console
    (
        if tftp -l "$DUMPPATH" -r "$FILENAME" -p "$SERVER" 2>&1; then
            echo "$FILENAME collected to $SERVER" > /dev/console
        else
            echo "$FILENAME collection failed to $SERVER" > /dev/console
        fi
        echo 1 > $DUMPPATH
    ) &
fi

