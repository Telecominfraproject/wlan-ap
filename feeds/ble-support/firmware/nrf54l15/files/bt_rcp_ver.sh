#!/bin/sh

bt_ver_hex="$(hcitool cmd 0x3f 0x310 | sed /HCI/d | awk '{printf "0x%s%s 0x%s%s 0x%s%s 0x%s%s", $6, $5, $8, $7, $10, $9, $12, $11}')"
bt_ver_txt="$(echo $bt_ver_hex | awk '{printf "v%d.%d.%d-b%d", $1, $2, $3, $4}')"

echo $bt_ver_txt
