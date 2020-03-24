# OpenSync on OpenWrt

This project gathers the building blocks needed to create an OpenSync package for OpenWrt based platforms.  
It also contains an example of a docker-based build environment, and instructions on how to run the 'ARMVIRT' target
in the QEMU emulator.


For more information on OpenSync, visit https://opensync.io

For more information on OpenWrt, visit https://openwrt.org


Quick Start
-----------

After cloning this repository, fetch the submodules:

```
git submodule update --init
```

The `example/` directory provides a docker-based build environment that downloads a specified
version of the OpenWrt SDK and creates the compiled OpenSync package, all in a single step.

See [`example/README.md`](example/README.md) for instructions.


Overview
--------

The `opensync-openwrt` project consists of the following key components:

* [opensync/core](https://github.com/plume-design/opensync)
    - OpenSync core repository, included as a submodule (see https://opensync.io/documentation for more details)
* [opensync/platform/openwrt](https://github.com/plume-design/opensync-platform-openwrt)
    - an example OpenSync target layer for OpenWrt based platforms (minimum implementation, stubs only)
* [opensync/vendor/armvirt](https://github.com/plume-design/opensync-vendor-armvirt)
    - an example OpenSync vendor layer for QEMU `armvirt` target (minimum implementation, stubs only)
* [feeds/network/services/opensync/Makefile](feeds/network/services/opensync/Makefile)
    - OpenWrt makefile needed to build OpenSync
* [feeds/lang/python/python3-kconfiglib/Makefile](feeds/lang/python/python3-kconfiglib/Makefile)
    - OpenWrt makefile needed to build the host-side kconfiglib dependency

External dependency:

* [OpenWrt SDK](https://openwrt.org/docs/guide-developer/using_the_sdk)
    - For more information, consult [OpenWrt Documentation](https://openwrt.org/docs/start)


Customizing the Build Procedure for the OpenSync Package
--------------------------------------------------------

The basic steps to build the OpenSync package for OpenWrt are listed below.

The `example/build.sh` script makes the necessary changes after downloading the OpenWrt SDK.  
To integrate the OpenSync package into an existing build system, modify the build system accordingly.

1. Add `python3-kconfiglib` to feeds.conf (build-time dependency):

    ```
    echo "src-link kconfiglib <path-to-this-repo>/feeds/lang" >> feeds.conf
    ```

2. Add the `opensync` feed to feeds.conf:

    ```
    echo "src-link opensync <path-to-this-repo>/feeds/network" >> feeds.conf
    ```

3. Update and install the feeds:

    ```
    ./scripts/feeds update -a
    ./scripts/feeds install -a
    ```

4. To build the package, specify the following parameters:

    ```
    package/opensync/compile OPENSYNC_SRC=<path-to-this-repo> TARGET=<target>
    ```

Note that currently only building from a local source is supported.


Creating a Custom Target (Vendor Layer)
---------------------------------------

To create a new target, it is best to use the `vendor/armvirt` tree as a template.
Follow these steps for the basic bring-up of a new target:

1. Copy `vendor/armvirt` to `vendor/<new_target>`
2. Modify `build/vendor-arch.mk`, replacing "ARMVIRT" and "armvirt" with the new name
3. Rename and adjust `vendor/<new_target>/src/lib/target/inc/target_<new_target>.h`
4. Modify `vendor/<new_target>/src/lib/target/entity.c` so that the functions return the correct model name, id, etc.
5. Modify `vendor/<new_target>/ovsdb/static_configuration.json` according to actual Wi-Fi radios configuration

To connect to the Plume Cloud, copy the certificates provided to you by Plume to `vendor/<new_target>/rootfs/common/usr/plume/certs/`.


Implementing Functionality in the Platform Layer
------------------------------------------------

The `opensync-platform-openwrt` repository contains a minimum (stubs only) implementation.

An actual implementation for specific hardware depends on the hardware itself, as well as the tools and drivers used.

Platform and vendor layers are separate so that multiple vendor targets can use the same platform layer implementation
(a vendor target is linked to a specific platform implementation in `build/vendor-arch.mk`).

The procedure to create custom platform implementations is similar to the procedure for creating a custom vendor target.
