#!/bin/sh
SOC=`cat /proc/device-tree/ethernet*/compatible | cut -c 10-15`
if [ -z "$SOC" ]; then
	SOC=`cat /proc/device-tree/soc/ethernet*/compatible | cut -c 10-15`
fi
if [ -z "$SOC" ]; then
	SOC=`cat /proc/device-tree/soc_netsys/ethernet*/compatible | cut -c 10-15`
fi
init()
{
	case $SOC in
	*7988)
		BASE_FE=15100000
		BASE_SGMII=10060000
		OFFSET_PSE="0 100 200 300 400 500 600 c00 d00 e00 f00
			    1400 1500 1600 2800"
		OFFSET_PPE="2200 2300 2600 2700 2e00 2f00"
		OFFSET_ADMA="6800 6900 6a00"
		OFFSET_QDMA="4400 4500 4600 4700"
		OFFSET_WDMA="4800 4900 4a00 4b00 4c00 4d00 4e00 4f00"
		OFFSET_GMAC="10000 10100 10200"
		OFFSET_XFI="12000 12100 12200 13000 13100 13200"
		OFFSET_LRO="6c00 6d00"
		OFFSET_RSS="7000"
		OFFSET_SGMII="0 100 10000 10100"
		;;
	*7987)
		BASE_FE=15100000
		BASE_SGMII=10060000
		OFFSET_PSE="0 100 200 300 400 500 600 c00 d00 e00 f00
			    1400 1500 1600"
		OFFSET_PPE="2200 2300 2600 2700"
		OFFSET_ADMA="6800 6900 6a00"
		OFFSET_QDMA="4400 4500 4600 4700"
		OFFSET_WDMA="4800 4900 4a00 4b00"
		OFFSET_GMAC="10000 10100 10200"
		OFFSET_XFI="12000 12100 12200"
		OFFSET_LRO="6c00 6d00"
		OFFSET_RSS="7000"
		OFFSET_SGMII="0 100 10000 10100"
		;;
	*798*)
		BASE_FE=15100000
		BASE_SGMII=10060000
		OFFSET_PSE="0 100 200 300 400 500 1400 1500 1600 2800"
		OFFSET_PPE="2200 2300"
		OFFSET_ADMA="6000 6100 6200"
		OFFSET_QDMA="4400 4500 4600 4700"
		OFFSET_WDMA="4800 4900 4a00 4b00 4c00 4d00 4e00 4f00"
		OFFSET_GMAC="10000 10100 10200"
		OFFSET_LRO="6400 6500"
		OFFSET_RSS="3000 6800"
		OFFSET_SGMII="0 100 10000 10100"
		;;
	*762*)
		BASE_FE=1b100000
		BASE_SGMII=1b128000
		OFFSET_PSE="0 100 200 300 400 500 1400 1500 1600"
		OFFSET_PPE="e00 f00 1000"
		OFFSET_ADMA="800 900 a00"
		OFFSET_QDMA="1800 1900 1a00 1b00"
		OFFSET_WDMA="2800 2900 2a00 2b00 2c00 2d00 2e00 2f00"
		OFFSET_GMAC="10000 10100 10200"
		OFFSET_LRO="b00"
		OFFSET_RSS="3000"
		OFFSET_SGMII="0"
		;;
	esac
	echo "============== SoC Info =============="
	echo "SoC: $SOC"
	echo "BASE_FE: 0x$BASE_FE"
	echo "BASE_SGMII: 0x$BASE_SGMII"
	echo
}
dump_basic()
{
	echo "============== Basic Information =============="
	cat /proc/version
	cat /proc/interrupts
	cat /proc/mtketh/dbg_regs
	cat /proc/mtketh/esw_cnt
	switch vlan dump
	switch reg r 1fc0
	switch reg r 3600
}
dump_netsys()
{
	local NAME=$1
	local BASE=$2
	local OFFSET=$3
	if [ $BASE == 0 ]; then
		return
	fi
	echo "============== $NAME =============="
	for i in $OFFSET
	do
		ADDR=`printf '%x' $((0x$BASE + 0x$i))`
		regs d $ADDR
		echo
	done
}
init
dump_basic
dump_netsys	"PSE"		$BASE_FE	"$OFFSET_PSE"
dump_netsys	"PPE"		$BASE_FE	"$OFFSET_PPE"
dump_netsys	"ADMA"		$BASE_FE	"$OFFSET_ADMA"
dump_netsys	"QDMA"		$BASE_FE	"$OFFSET_QDMA"
dump_netsys	"WDMA"		$BASE_FE	"$OFFSET_WDMA"
dump_netsys	"GMAC"		$BASE_FE	"$OFFSET_GMAC"
dump_netsys	"XFI"		$BASE_FE        "$OFFSET_XFI"
dump_netsys	"LRO"		$BASE_FE	"$OFFSET_LRO"
dump_netsys	"RSS"		$BASE_FE	"$OFFSET_RSS"
dump_netsys	"SGMII"		$BASE_SGMII	"$OFFSET_SGMII"
