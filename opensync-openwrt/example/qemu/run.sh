#!/bin/bash

IMAGE=zImage-initramfs

if [ $# -eq 1 ]; then
    IMAGE_URL=$1
    wget $IMAGE_URL -O $IMAGE || rm -f $IMAGE
fi

if [ ! -f "$IMAGE" ]; then
    echo "Please provide the link to the armvirt zImage"
    echo "Example: sudo ./run.sh https://downloads.openwrt.org/releases/18.06.4/targets/armvirt/32/openwrt-18.06.4-armvirt-32-zImage-initramfs"
    echo
    exit 1;
fi

LAN=ledetap0
# create a tap interface which will be connected to OpenWrt LAN NIC
ip tuntap add mode tap $LAN
ip link set dev $LAN up

# configure the interface with a static IP to avoid overlapping routes
ip addr add 192.168.1.101/24 dev $LAN
qemu-system-arm \
    -device virtio-net-pci,netdev=lan \
    -netdev tap,id=lan,ifname=$LAN,script=no,downscript=no \
    -device virtio-net-pci,netdev=wan \
    -netdev user,id=wan \
    -M virt -nographic -m 64 -kernel $IMAGE

# cleanup: delete the tap interface created earlier
ip addr flush dev $LAN
ip link set dev $LAN down
ip tuntap del mode tap dev $LAN
