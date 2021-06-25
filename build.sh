#!/bin/bash

set -e
ROOT_PATH=${PWD}
BUILD_DIR=${ROOT_PATH}/openwrt
TARGET=${1}

if [ -z "$1" ]; then
	echo "Error: please specify TARGET"
	echo "For example: IPQ40XX, ECW5410, AP2220, ECW5211 EC420 WF610D"
	exit 1
fi

if [ ! "$(ls -A $BUILD_DIR)" ]; then
	python3 setup.py --setup --docker || exit 1
    
else
	python3 setup.py --rebase --docker
	echo "### OpenWrt repo already setup"
fi

WIFI=wifi
case "${TARGET}" in
EA8300|\
IPQ40XX)
	TARGET=ea8300
	;;
ECW5211)
	TARGET=ecw5211
	;;
ECW5410)
	TARGET=ecw5410
	;;
AP2220)
	TARGET=ap2220
	;;
EC420)
	TARGET=ec420
	;;
EAP101)
	TARGET=eap101
	WIFI=wifi-ax
	;;
EAP102)
	TARGET=eap102
	WIFI=wifi-ax
	;;
EAP106)
	TARGET=eap106
	WIFI=wifi-ax
	;;
EX227)
	TARGET=ex227
	WIFI=wifi-ax
	;;
EX447)
	TARGET=ex447
	WIFI=wifi-ax
	;;
WF188N)
	TARGET=wf188n
	WIFI=wifi-ax
	;;
WF194C)
	TARGET=wf194c
	WIFI=wifi-ax
	;;
HAWKEYE)
	TARGET=hawkeye
	WIFI=wifi-ax
	;;
WALLABY)
	TARGET=wallaby
	WIFI=wifi-ax
	;;
WF610D)
	TARGET=wf610d
	;;
*)
	echo "${TARGET} is unknown"
	exit 1
	;;
esac
cd ${BUILD_DIR}
./scripts/gen_config.py ${TARGET} wlan-ap ${WIFI} || exit 1
cd -

echo "### Building image ..."
cd $BUILD_DIR
make -j$(nproc) V=s
echo "Done"
