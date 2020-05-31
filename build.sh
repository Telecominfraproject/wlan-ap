#!/bin/bash

set -e
ROOT_PATH=${PWD}
BUILD_DIR=${ROOT_PATH}/openwrt

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

    # Apply patches to OpenSync core
    git apply patch/opensync/core/01-add-lib-uci-to-wm2.patch --directory=opensync/core || true
    git apply patch/opensync/core/02-wm2-write-temporary-change.patch --directory=opensync/core || true
    git apply patch/opensync/core/03-sm-add-libiwinfo.patch --directory=opensync/core || true

    
    # Apply patches to Openwrt
     cd $BUILD_DIR
    if [ "$1" = "IPQ40XX" ]; then
	DIFFCONFIG=ea8300_config
    fi

    if [ "$1" = "ECW5410" ]; then
        DIFFCONFIG=ecw5410_config
        patch -p1 -i ../patch/openwrt/ecw5410/0001-ipq806x-4.14-enable-AT803X-driver.patch
        patch -p1 -i ../patch/openwrt/ecw5410/0002-ipq806x-add-GSBI1-node-to-DTSI.patch
        patch -p1 -i ../patch/openwrt/ecw5410/0003-firmware-ipq-wifi-enable-use-on-IPQ806x.patch
        patch -p1 -i ../patch/openwrt/ecw5410/0004-ipq806x-import-bootargs-append-from-IPQ40xx.patch
        patch -p1 -i ../patch/openwrt/ecw5410/0005-ipq806x-add-Edgecore-ECW5410-support.patch
        cp ../patch/openwrt/ecw5410/board-edgecore_ecw5410.qca9984 $BUILD_DIR/package/firmware/ipq-wifi
    fi

    if [ "$1" = "AP2220" ]; then
        DIFFCONFIG=ap2220_config
        git apply ../patch/openwrt/ap2220/0001-tp-link-ap2220-support.patch
        cp ../patch/openwrt/ap2220/board-tp-link_ap2220.bin $BUILD_DIR/package/firmware/ipq-wifi/
    fi

    if [ "$1" = "ECW5211" ]; then
        DIFFCONFIG=ecw5211_config
        git apply ../patch/openwrt/ecw5211/0001-ipq40xx-add-Edgecore-ECW5211-support.patch
        cp ../patch/openwrt/ecw5211/board-edgecore_ecw5211.qca4019 $BUILD_DIR/package/firmware/ipq-wifi/
    fi

    cd $BUILD_DIR
    ./scripts/feeds update -a
    ./scripts/feeds install -a
 
    cat $ROOT_PATH/config/$DIFFCONFIG >> $BUILD_DIR/.config
    make defconfig
 
    echo "Done"
else
    echo "OpenWrt repo already installed"
fi

echo "Building image ..."
cd $BUILD_DIR
make -j$(nproc)  V=s OPENSYNC_SRC=${ROOT_PATH} TARGET=$1 2>&1 | tee build.log
echo "Done"
