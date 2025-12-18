#!/bin/sh
COUNTRY="$1"

[ -z "$COUNTRY" ] && { echo "Usage: $0 <country-code>"; exit 1; }

. /lib/functions.sh

board=$(board_name)
ucentral_default_1=/certificates/ucentral.defaults
ucentral_default_2=/etc/ucentral/ucentral.defaults

case "$board" in
sonicfi,rap630c-311g|\
sonicfi,rap630w-311g)
              fw_setenv country "$COUNTRY"
              echo "options cfg80211 ieee80211_regdom=$COUNTRY" > /etc/modules.conf
              echo -n "$COUNTRY" > /etc/ucentral/country

              [ -f "$ucentral_default_1" ] && \
              sed -i 's/"country"[[:space:]]*:[[:space:]]*"[^"]*"/"country":"'"$COUNTRY"'"/' "$ucentral_default_1"


              [ -f "$ucentral_default_2" ] && \
              sed -i 's/"country"[[:space:]]*:[[:space:]]*"[^"]*"/"country":"'"$COUNTRY"'"/' "$ucentral_default_2"

              if [ "$COUNTRY" == "US" ]; then
                     ln -sf "/lib/firmware/ath11k/IPQ5018/hw1.0/board.bin.US" \
                            /lib/firmware/ath11k/IPQ5018/hw1.0/board.bin
                     ln -sf "/lib/firmware/ath11k/qcn6122/hw1.0/board.bin.US" \
                            /lib/firmware/ath11k/qcn6122/hw1.0/board.bin
              elif [ "$COUNTRY" == "AU" ]; then
                     ln -sf "/lib/firmware/ath11k/IPQ5018/hw1.0/board.bin.AU" \
                            /lib/firmware/ath11k/IPQ5018/hw1.0/board.bin
                     ln -sf "/lib/firmware/ath11k/qcn6122/hw1.0/board.bin.AU" \
                            /lib/firmware/ath11k/qcn6122/hw1.0/board.bin
              elif [ "$COUNTRY" == "CA" ]; then
                     ln -sf "/lib/firmware/ath11k/IPQ5018/hw1.0/board.bin.CA" \
                            /lib/firmware/ath11k/IPQ5018/hw1.0/board.bin
                     ln -sf "/lib/firmware/ath11k/qcn6122/hw1.0/board.bin.CA" \
                            /lib/firmware/ath11k/qcn6122/hw1.0/board.bin
              elif [ "$COUNTRY" == "SG" ]; then
                     ln -sf "/lib/firmware/ath11k/IPQ5018/hw1.0/board.bin.SG" \
                            /lib/firmware/ath11k/IPQ5018/hw1.0/board.bin
                     ln -sf "/lib/firmware/ath11k/qcn6122/hw1.0/board.bin.SG" \
                            /lib/firmware/ath11k/qcn6122/hw1.0/board.bin
              else
                     ln -sf "/lib/firmware/ath11k/IPQ5018/hw1.0/board.bin.US" \
                            /lib/firmware/ath11k/IPQ5018/hw1.0/board.bin
                     ln -sf "/lib/firmware/ath11k/qcn6122/hw1.0/board.bin.US" \
                            /lib/firmware/ath11k/qcn6122/hw1.0/board.bin
              fi

              rmmod ath11k_ahb
              sleep 1
              rmmod ath11k_pci
              sleep 1
              rmmod ath11k
              sleep 1

              modprobe ath11k_ahb
              sleep 1

              wifi down
              sleep 1
              wifi up
              sleep 5
              ;;
esac



