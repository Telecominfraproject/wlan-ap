#!/bin/sh /etc/rc.common
#
#Copyright (c) 2022, Qualcomm Innovation Center, Inc. All rights reserved.
#
#Permission to use, copy, modify, and/or distribute this software for any
#purpose with or without fee is hereby granted, provided that the above
#copyright notice and this permission notice appear in all copies.
#
#THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
#WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
#MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
#ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
#WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
#ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
#OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
#

START=02

boot()
{
	ath11k="/etc/modules.d/ath11k"
	if [ -e $ath11k ]; then
		ath11k_mod_params_env=`/usr/sbin/fw_printenv -n ath11k_mod`
		if [ $? -eq 0 ] && [ "$ath11k_mod_params_env" != "" ]; then
			sed -i '1d' $ath11k
			ath11k_mod_params="ath11k $ath11k_mod_params_env"
			sed -i "1s/^/${ath11k_mod_params}\n/" $ath11k
		fi
	fi
}
