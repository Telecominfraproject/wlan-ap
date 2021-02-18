#!/bin/bash

set -ex
ROOT_PATH=${PWD}
BUILD_DIR=${ROOT_PATH}/openwrt
TARGET=${1}

if [ -z "$1" ]; then
	echo "Error: please specify TARGET"
	echo "One of: WF194C, ZYXEL_GS1900-10HP"
	exit 1
fi

if [ ! "$(ls -A $BUILD_DIR)" ]; then
	python3 setup.py --setup || exit 1
    
else
	python3 setup.py --rebase
	echo "### OpenWrt repo already setup"
fi

case "${TARGET}" in
WF194C)
	TARGET=wf194c
	;;
ZYXEL_GS1900-10HP)
	TARGET=zyxel_gs1900-10hp
	;;
*)
	echo "${TARGET} is unknown"
	exit 1
	;;
esac
cd ${BUILD_DIR}
./scripts/gen_config.py ${TARGET} ucentral-ap || exit 1
cd -

echo "### Building image ..."
cd $BUILD_DIR
make -j$(nproc) V=s 2>&1 | tee build.log
echo "Done"
