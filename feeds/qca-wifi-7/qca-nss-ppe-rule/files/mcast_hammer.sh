#!/bin/sh
#
# Copyright (c) 2024 Qualcomm Innovation Center, Inc. All rights reserved.
#
# Permission to use, copy, modify, and/or distribute this software for any
# purpose with or without fee is hereby granted, provided that the above
# copyright notice and this permission notice appear in all copies.
#
# THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
# WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
# MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
# ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
# WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
# ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
# OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
#

# Function to convert alphanumeric rate to bits per second
convert_rate_to_bps() {
	local rate=$1
	local value=${rate//[^0-9]/}
	local unit=${rate//[0-9]/}
	unit=$(echo "$unit" | tr '[:upper:]' '[:lower:]')

	case $unit in
		K)
			echo $((value * 1000))
			;;
		M)
			echo $((value * 1000000))
			;;
		G)
			echo $((value * 1000000000))
			;;
		*)
			echo $value
			;;
		esac
}

# Check if the correct number of arguments is provided
if [ "$#" -ne 1 ]; then
	echo "Usage: $0 rate (in bps)"
	exit 1
fi

# Assign command-line arguments to variables
ARG=$1
RATE=$(convert_rate_to_bps $ARG)
echo ${RATE}
COMMITTED_RATE=$((RATE/8))
PEAK_RATE=$((COMMITTED_RATE))

# MAC mask value
DIP_MASK_NALL="224.0.0.0"
DIP_MASK_ALL="239.255.255.0"
DIP_VAL="224.0.0.0"

# Read the board name from the file
BOARD_NAME=$(cat /tmp/sysinfo/board_name)

# Check if the board is Alder
if [[ "$BOARD_NAME" == *"9574"* ]]; then
	# Execute the following ppecfg commands for alder
	ppecfg family=policer cmd=rule_add rule_id=1 committed_rate=$COMMITTED_RATE committed_burst_size=8000 peak_rate=$PEAK_RATE peak_burst_size=9000
	ppecfg family=acl cmd=rule_add rule_id=1 src_dev=eth0 priority=6 dip dip_is_v6=0 dip_val=$DIP_VAL dip_mask=$DIP_MASK_ALL action fwd_cmd="FWD"
	ppecfg family=acl cmd=rule_add rule_id=2 src_dev=eth1 priority=6 dip dip_is_v6=0 dip_val=$DIP_VAL dip_mask=$DIP_MASK_ALL action fwd_cmd="FWD"
	ppecfg family=acl cmd=rule_add rule_id=3 src_dev=eth2 priority=6 dip dip_is_v6=0 dip_val=$DIP_VAL dip_mask=$DIP_MASK_ALL action fwd_cmd="FWD"
	ppecfg family=acl cmd=rule_add rule_id=4 src_dev=eth3 priority=6 dip dip_is_v6=0 dip_val=$DIP_VAL dip_mask=$DIP_MASK_ALL action fwd_cmd="FWD"
	ppecfg family=acl cmd=rule_add rule_id=5 src_dev=eth4 priority=6 dip dip_is_v6=0 dip_val=$DIP_VAL dip_mask=$DIP_MASK_ALL action fwd_cmd="FWD"
	ppecfg family=acl cmd=rule_add rule_id=6 src_dev=eth5 priority=6 dip dip_is_v6=0 dip_val=$DIP_VAL dip_mask=$DIP_MASK_ALL action fwd_cmd="FWD"
	ppecfg family=acl cmd=rule_add rule_id=7 src_dev=eth0 priority=5 dip dip_is_v6=0 dip_val=$DIP_VAL dip_mask=$DIP_MASK_NALL action policer_id=1
	ppecfg family=acl cmd=rule_add rule_id=8 src_dev=eth1 priority=5 dip dip_is_v6=0 dip_val=$DIP_VAL dip_mask=$DIP_MASK_NALL action policer_id=1
	ppecfg family=acl cmd=rule_add rule_id=9 src_dev=eth2 priority=5 dip dip_is_v6=0 dip_val=$DIP_VAL dip_mask=$DIP_MASK_NALL action policer_id=1
	ppecfg family=acl cmd=rule_add rule_id=10 src_dev=eth3 priority=5 dip dip_is_v6=0 dip_val=$DIP_VAL dip_mask=$DIP_MASK_NALL action policer_id=1
	ppecfg family=acl cmd=rule_add rule_id=11 src_dev=eth4 priority=5 dip dip_is_v6=0 dip_val=$DIP_VAL dip_mask=$DIP_MASK_NALL action policer_id=1
	ppecfg family=acl cmd=rule_add rule_id=12 src_dev=eth5 priority=5 dip dip_is_v6=0 dip_val=$DIP_VAL dip_mask=$DIP_MASK_NALL action policer_id=1
elif [[ "$BOARD_NAME" == *"5332"* ]]; then
	# Execute the following ppecfg commands for miami
	ppecfg family=policer cmd=rule_add rule_id=1 committed_rate=$COMMITTED_RATE committed_burst_size=8000 peak_rate=$PEAK_RATE peak_burst_size=9000
	ppecfg family=acl cmd=rule_add rule_id=1 src_dev=eth0 priority=6 dip dip_is_v6=0 dip_val=$DIP_VAL dip_mask=$DIP_MASK_ALL action fwd_cmd="FWD"
	ppecfg family=acl cmd=rule_add rule_id=2 src_dev=eth1 priority=6 dip dip_is_v6=0 dip_val=$DIP_VAL dip_mask=$DIP_MASK_ALL action fwd_cmd="FWD"
	ppecfg family=acl cmd=rule_add rule_id=3 src_dev=eth0 priority=5 dip dip_is_v6=0 dip_val=$DIP_VAL dip_mask=$DIP_MASK_NALL action policer_id=1
	ppecfg family=acl cmd=rule_add rule_id=4 src_dev=eth1 priority=5 dip dip_is_v6=0 dip_val=$DIP_VAL dip_mask=$DIP_MASK_NALL action policer_id=1
elif [[ "$BOARD_NAME" == *"5424"* ]]; then
	# Execute the following ppecfg commands for marina
	ppecfg family=policer cmd=rule_add rule_id=1 committed_rate=$COMMITTED_RATE committed_burst_size=8000 peak_rate=$PEAK_RATE peak_burst_size=9000
	ppecfg family=acl cmd=rule_add rule_id=1 src_dev=eth0 priority=6 dip dip_is_v6=0 dip_val=$DIP_VAL dip_mask=$DIP_MASK_ALL action fwd_cmd="FWD"
	ppecfg family=acl cmd=rule_add rule_id=2 src_dev=eth1 priority=6 dip dip_is_v6=0 dip_val=$DIP_VAL dip_mask=$DIP_MASK_ALL action fwd_cmd="FWD"
	ppecfg family=acl cmd=rule_add rule_id=3 src_dev=eth2 priority=6 dip dip_is_v6=0 dip_val=$DIP_VAL dip_mask=$DIP_MASK_ALL action fwd_cmd="FWD"
	ppecfg family=acl cmd=rule_add rule_id=4 src_dev=eth0 priority=5 dip dip_is_v6=0 dip_val=$DIP_VAL dip_mask=$DIP_MASK_NALL action policer_id=1
	ppecfg family=acl cmd=rule_add rule_id=5 src_dev=eth1 priority=5 dip dip_is_v6=0 dip_val=$DIP_VAL dip_mask=$DIP_MASK_NALL action policer_id=1
	ppecfg family=acl cmd=rule_add rule_id=6 src_dev=eth2 priority=5 dip dip_is_v6=0 dip_val=$DIP_VAL dip_mask=$DIP_MASK_NALL action policer_id=1
fi
