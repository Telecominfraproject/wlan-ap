#!/bin/bash

set -ex
ROOT_PATH=${PWD}
BUILD_DIR=${ROOT_PATH}/openwrt
TARGET=${1}

if [ -z "$1" ]; then
	echo "Error: please specify TARGET"
	exit 1
fi

if [ ! "$(ls -A $BUILD_DIR)" ]; then
	python3 setup.py --setup || exit 1
    
else
	python3 setup.py --rebase
	echo "### OpenWrt repo already setup"
fi

cd ${BUILD_DIR}
./scripts/gen_config.py ${TARGET} || exit 1
cd -

echo "### Building image ..."
cp ${ROOT_PATH}/feeds/python/python-async-timeout/Makefile ${BUILD_DIR}/feeds/packages/lang/python/python-async-timeout
cd $BUILD_DIR
make -j$(nproc) V=s
