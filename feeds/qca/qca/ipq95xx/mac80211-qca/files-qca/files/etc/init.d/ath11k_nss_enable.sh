#!/bin/sh /etc/rc.common
#
# Copyright (c) 2020 The Linux Foundation. All rights reserved.
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

START=01
STOP=95

boot()
{
	platform=$(grep -o "IPQ.*" /proc/device-tree/model | awk -F/ '{print $1}')
	board=$(grep -o "IPQ.*" /proc/device-tree/model | awk -F/ '{print $2}')
	radio_bmap=$(grep -o '\bath11k.skip_radio_bmap\=\w*\b' /proc/cmdline  | cut -d '=' -f2)
	if [ -z $radio_bmap ]; then
		radio_bmap=0;
	fi
        if [ "$platform" == "IPQ5018" ]; then
		ath11k="/etc/modules.d/ath11k"
		if [ -e $ath11k ];
		then
			sed -i '1d' $ath11k
			sed -i '1s/^/ath11k nss_offload=1 skip_radio_bmap='$radio_bmap'\n/' $ath11k
		fi
        fi
}
