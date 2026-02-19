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

# Assigning command-line arguments to variables
ARG=$1
COMMITTED_RATE=$(convert_rate_to_bps $ARG)
echo ${COMMITTED_RATE}
RATE=$((COMMITTED_RATE/8))

if [ ! -f "/etc/ppecfg/acl_default_cfg.json" ]; then
	echo "Mcast ACL configuration file does not exist."
	exit 1
fi

if [ ! -f "/etc/ppecfg/policer_default_cfg.json" ]; then
	echo "Mcast Policer configuration file does not exist."
	exit 1
fi

JSON_FILE="/etc/ppecfg/policer_default_cfg.json"

sed -i.bak -e "s/\"committed_rate\": \"[0-9]*\"/\"committed_rate\": \"$RATE\"/" \
	   -e "s/\"peak_rate\": \"[0-9]*\"/\"peak_rate\": \"$RATE\"/" "$JSON_FILE"

ppecfg config_type=json path="../../etc/ppecfg/acl_default_cfg.json"
