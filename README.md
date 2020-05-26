# wlan-ap prototype build environment.

The build environment will create a TIP based OpenWrt image that includes the OpenSync services and the required pre-requisites.  The required command is:
EA8300: 
 make TARGET=IPQ40XX
ECW5410:
 make TARGET=ECW5410

NOTE: The SDK_URL is no longer used in the build.  The parameter is retained to ensure the automated build is not disrupted.

The resulting image is found at 'wlan-ap/openwrt/bin/targets/ipq40xx/generic'.

The standalone OpenSync image is no longer required by the build process.  However, it can be can be built by 'cd example' and executing:
make opensync TARGET=IPQ40XX SDK_URL=https://downloads.openwrt.org/releases/19.07.2/targets/ipq40xx/generic/openwrt-sdk-19.07.2-ipq40xx-generic_gcc-7.5.0_musl_eabi.Linux-x86_64.tar.xz

The resulting image is found at 'wlan-ap/example/out'.
 
Caution: The work is in-progress and the following known issues apply:
  a)  The OpenWrt target library for OpenSync is incomplete and minimally functional.
  b)  The top level Makefile cleans the OpenWrt image and build directories BUT does not clean the OpenSync image and directories when 'make purge' is executed.
  c)  To clean the OpenSync image and build directories, change directory to 'example' and 'make purge'.

