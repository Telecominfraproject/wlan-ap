# Example environment for OpenSync on OpenWrt

This project provides a single entry point for OpenSync build system embedded into OpenWrt SDK. The only parameters
that need to be provided are OpenSync TARGET and the SDK_URL (a link to a particular version of the OpenWrt SDK).
That OpenWrt SDK will be downloaded and extracted. The OpenSync feed will then be added, and the OpenSync OpenWrt
package will be built using a docker container. The output is a single OpenSync `ipk` file.


Requirements
------------

* A Linux distribution with [Docker](https://docs.docker.com/install/)
* [OpenWrt SDK](https://openwrt.org/downloads) (external dependency)
* Optionally, the [QEMU](https://www.qemu.org) emulator for the example deployment


Building the example ARMVIRT target
-----------------------------------

Let's suppose the package must be built for 18.06.4 OpenWrt on 32-bit `armvirt` target. The first step is to find
an OpenWrt SDK file for that version and architecture. Open https://downloads.openwrt.org/releases/ and navigate to
**18.06.4 -> targets -> armvirt -> 32**.  
Under "Supplementary Files", locate the `openwrt-sdk-*.tar.xz` file and copy the link to that file.

Run (for example):
```
make TARGET=ARMVIRT SDK_URL=https://downloads.openwrt.org/releases/18.06.4/targets/armvirt/32/openwrt-sdk-18.06.4-armvirt-32_gcc-7.3.0_musl_eabi.Linux-x86_64.tar.xz
```

After a while, the 'menuconfig' window should appear. At this point any custom packages can be selected, but
in most cases it is enough to just save without any changes (keep the suggested `.config` file name) and exit.

And that's it. When the build finishes, the `ipk` file should be available in the `out/` directory.


Deploying the example ARMVIRT target to QEMU
--------------------------------------------

First, install `qemu-system-arm`. For example, on Ubuntu run:

```
sudo apt-get install qemu-system-arm
```

Assuming the 18.06.4 32-bit `armvirt` target is used, navigate to https://downloads.openwrt.org/releases/18.06.4/targets/armvirt/32/.  
Under "Image Files", locate the `zImage-initramfs` image, and copy the link to that file.

Next, run the following command (using the above link):
```
make start_qemu IMAGE_URL=https://downloads.openwrt.org/releases/18.06.4/targets/armvirt/32/openwrt-18.06.4-armvirt-32-zImage-initramfs
```

Subsequent uses will not require the `IMAGE_URL` parameter.

Now, the QEMU console should be started with internet connectivity setup.
From another terminal push the OpenSync `ipk` package to the `/tmp` directory, for example:

```
scp -o StrictHostKeyChecking=no out/opensync_1.0-1_arm_cortex-a15_neon-vfpv4.ipk root@192.168.1.1:/tmp
```

Get back to the QEMU console, and run the following commands:

```
opkg update
cd /tmp
opkg install opensync_1.0-1_arm_cortex-a15_neon-vfpv4.ipk
```

This should install and start OpenSync. The logs can be dumped (using `logread`),
and OVSDB tables can be inspected (using `ovsh`), for example:

```
$ /usr/plume/tools/ovsh s AWLAN_Node
--------------------------------------------------------------------------------------------------
_uuid            | e68b~8c78                                                                     |
_version         | 8c9b~ce51                                                                     |
device_mode      | ["set",[]]                                                                    |
factory_reset    | ["set",[]]                                                                    |
firmware_pass    |                                                                               |
firmware_url     |                                                                               |
firmware_version | 0.1.0                                                                         |
id               | 0123456789ab                                                                  |
led_config       | ["map",[]]                                                                    |
manager_addr     |                                                                               |
max_backoff      | 60                                                                            |
min_backoff      | 30                                                                            |
model            | WRTVM01                                                                       |
mqtt_headers     | ["map",[]]                                                                    |
mqtt_settings    | ["map",[]]                                                                    |
mqtt_topics      | ["map",[]]                                                                    |
platform_version | OPENWRT_VM                                                                    |
redirector_addr  | ssl:wildfire.plume.tech:443                                                   |
revision         | 1                                                                             |
serial_number    | 0123456789ab                                                                  |
sku_number       | ["set",[]]                                                                    |
upgrade_dl_timer | 0                                                                             |
upgrade_status   | 0                                                                             |
upgrade_timer    | 0                                                                             |
version_matrix   | ["map",[["DATE","Tue Dec 12 14:01:00 UTC 2019"],["FIRMWARE",                  |
                 : "0.1.0-0-g8a886c1-mods-development"],["FW_BUILD","0"],["FW_COMMIT",           :
                 : "g8a886c1-mods"],["FW_PROFILE","development"],["FW_VERSION","0.1.0"],["HOST", :
                 : "michalkowalczyk@a45d97a3e03b"],["PML","1.4.0.1"],["core",                    :
                 : "1.4.0.1/+0/gd46964b"],["device","0.0/=16/g8a886c1+mods"],["vendor/armvirt",  :
                 : "0.1.0/=1/gc551daa+mods"]]]                                                   :
--------------------------------------------------------------------------------------------------
```

If the certificates in `opensync/vendor/armvirt/rootfs/common/usr/plume/certs/` are valid,
the target should connect successfully to the cloud controller,
and the "ACTIVE" state should be visible in the `Manager` table:

```
$ /usr/plume/tools/ovsh s Manager
----------------------------------------------------------------------------
_uuid            | cc93~1e03                                               |
_version         | ad39~3e71                                               |
connection_mode  | ["set",[]]                                              |
external_ids     | ["map",[]]                                              |
inactivity_probe | 30000                                                   |
is_connected     | true                                                    |
max_backoff      | ["set",[]]                                              |
other_config     | ["map",[]]                                              |
status           | ["map",[["sec_since_connect","42"],["state","ACTIVE"]]] |
target           | ssl:34.211.87.241:443                                   |
----------------------------------------------------------------------------
```

To quit QEMU, press `Ctrl-A`, then `X`.
