#!/bin/bash

# THIS SCRIPT IS INTENDED TO BE RUN FROM DOCKER

set -e
ROOT_PATH=${PWD}
SDK_PATH=${ROOT_PATH}/example
SDK_FILENAME=openwrt-sdk.tar.xz
BUILD_DIR=${SDK_PATH}/build
OUT_DIR=${SDK_PATH}/out

if [ -z "$1" ]; then
    echo "Error: please specify TARGET"
    echo "For example: IPQ40XX"
    exit 1
fi

if [ ! -d $BUILD_DIR ]; then
    if [ -z "$2" ]; then
        echo "Error: please specify OpenWrt SDK URL"
        echo "For example: https://downloads.openwrt.org/releases/19.07.2/targets/ipq40xx/generic/openwrt-sdk-19.07.2-ipq40xx-generic_gcc-7.5.0_musl_eabi.Linux-x86_64.tar.xz"
        exit 1
    fi
    SDK_URL=$2
    mkdir $BUILD_DIR
    echo "Downloading SDK..."
    wget $SDK_URL -O $BUILD_DIR/$SDK_FILENAME
    echo "Done"

    echo "Unpacking SDK..."
    mkdir $BUILD_DIR/workdir
    tar x -C $BUILD_DIR/workdir -f $BUILD_DIR/$SDK_FILENAME --strip-components 1
    rm $BUILD_DIR/$SDK_FILENAME
    cp $BUILD_DIR/workdir/feeds.conf.default $BUILD_DIR/workdir/feeds.conf
    echo "src-link kconfiglib $ROOT_PATH/feeds/lang" >> $BUILD_DIR/workdir/feeds.conf
    echo "src-link opensync $ROOT_PATH/feeds/network" >> $BUILD_DIR/workdir/feeds.conf
    git apply patch/opensync/core/01-add-lib-uci-to-wm2.patch --directory=opensync/core
    git apply patch/opensync/core/02-wm2-write-temporary-change.patch --directory=opensync/core
    git apply patch/opensync/core/03-sm-add-libiwinfo.patch --directory=opensync/core
    $BUILD_DIR/workdir/scripts/feeds update -a
    $BUILD_DIR/workdir/scripts/feeds install -a
    if [ "$1" = "IPQ40XX" ]; then
        echo "Copying config file"
        cp $ROOT_PATH/config/ipq40xx/ipq40xx.config $BUILD_DIR/workdir/.config
    fi
    echo "Done"
fi

echo "Compiling using source dir ${ROOT_PATH}"
make -C $BUILD_DIR/workdir -j $(nproc) package/protobuf/compile
make -C $BUILD_DIR/workdir -j $(nproc) V=sc package/opensync/clean
make -C $BUILD_DIR/workdir -j $(nproc) V=sc package/opensync/compile OPENSYNC_SRC=${ROOT_PATH} TARGET=$1
echo "Copying output to $OUT_DIR/"
mkdir -pv $OUT_DIR
cp -rv $BUILD_DIR/workdir/bin/packages/*/opensync/* $OUT_DIR/
echo "Done"
