#!/bin/bash

set -e
ROOT_PATH=${PWD}
BUILD_DIR=${ROOT_PATH}/openwrt
DIFFCONFIG=ea8300_config

if [ -z "$1" ]; then
    echo "Error: please specify TARGET"
    echo "For example: IPQ40XX"
    exit 1
fi
 
if [ ! "$(ls -A $BUILD_DIR)" ]; then
    echo "Cloning OpenWrt..."
    git clone https://git.openwrt.org/openwrt/openwrt.git $BUILD_DIR
    cd $BUILD_DIR
    git checkout v19.07.2

    echo "Configuring OpenSync build..."
    echo "src-link kconfiglib $ROOT_PATH/feeds/lang" >> $BUILD_DIR/feeds.conf.default
    echo "src-link opensync $ROOT_PATH/feeds/network" >> $BUILD_DIR/feeds.conf.default
    cd $ROOT_PATH
    git apply patch/opensync/core/01-add-lib-uci-to-wm2.patch --directory=opensync/core || true
    git apply patch/opensync/core/02-wm2-write-temporary-change.patch --directory=opensync/core || true
    git apply patch/opensync/core/03-sm-add-libiwinfo.patch --directory=opensync/core || true

    cd $BUILD_DIR
    ./scripts/feeds update -a
    ./scripts/feeds install -a
 
    cat $ROOT_PATH/config/$DIFFCONFIG >> .config
    make defconfig
 
    echo "Done"
else
    echo "OpenWrt repo already installed"
fi

echo "Building image ..."
cd $BUILD_DIR
make V=sc OPENSYNC_SRC=${ROOT_PATH} TARGET=$1 2>&1 | tee build.log
echo "Done"
