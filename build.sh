#!/bin/bash

set -e
ROOT_PATH=${PWD}
SDK_PATH=${ROOT_PATH}/example
SDK_OUT_DIR=${SDK_PATH}/out
BUILD_FILENAME=openwrt-imagebuilder.tar.xz
BUILD_DIR=${ROOT_PATH}/build

if [ -z "$1" ]; then
    echo "Error: please specify TARGET"
    echo "For example: IPQ40XX"
    exit 1
fi

if [ -z "$2" ]; then
    echo "Error: please specify OpenWrt SDK URL"
    echo "For example: https://downloads.openwrt.org/releases/19.07.2/targets/ipq40xx/generic/openwrt-sdk-19.07.2-ipq40xx-generic_gcc-7.5.0_musl_eabi.Linux-x86_64.tar.xz"
    exit 1
fi

if [ -z "$3" ]; then
    echo "Error: please specify PROFILE"
    echo "For example: linksys_ea8300"
    exit 1
fi

if [ -z 'ls $SDK_OUT_DIR/*' ]; then
    echo "Building OpenSync first"
    cd $SDK_PATH && make TARGET=$1 SDK_URL=$2
else
    echo "OpenSync exists"
fi 
    
if [ ! -d $BUILD_DIR ]; then
    if [ -z "$4" ]; then
        echo "Error: please specify OpenWrt ImageBuilder URL"
        echo "For example: https://downloads.openwrt.org/releases/19.07.2/targets/ipq40xx/generic/openwrt-imagebuilder-19.07.2-ipq40xx-generic.Linux-x86_64.tar.xz"
        exit 1
    fi
    BUILD_URL=$4
    mkdir $BUILD_DIR
    echo "Downloading Image Builder..."
    wget $BUILD_URL -O $BUILD_DIR/$BUILD_FILENAME
    echo "Done"

    echo "Unpacking Image Builder..."
    mkdir $BUILD_DIR/workdir
    tar x -C $BUILD_DIR/workdir -f $BUILD_DIR/$BUILD_FILENAME --strip-components 1
    rm $BUILD_DIR/$BUILD_FILENAME
    cp -f $SDK_OUT_DIR/*.ipk $BUILD_DIR/workdir/packages
    make -C $BUILD_DIR/workdir clean 
    echo "Done"
else
    echo "Image builder already installed"
fi

echo "Building image ..."
make -C $BUILD_DIR/workdir image PROFILE=$3 PACKAGES="luci jansson protobuf libev libmosquitto-ssl libpcap libprotobuf-c libopenssl luci-ssl luci-base luci-mod-admin-full luci-mod-network luci-mod-status luci-mod-system luci-app-firewall luci-app-opkg openvswitch opensync"
echo "Done"
