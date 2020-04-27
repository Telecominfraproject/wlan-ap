# wlan-ap prototype build environment.

This  environment will build an OpenWrt image for linksys_ea8300 that includes the OpenSync services and the required pre-requistites.
Step 1:  Build the OpenSync services into a package using the OpenWrt SDK and the OpenWrt target library supplied by TIP wlan-ap (this) repo.
Step 2:  Assemble an OpenWrt image for the target device using the OpenWrt Image Builder with the OpenSync package and selected package dependencies.

Caution: The work is in-progress and the following known issues apply:
  a)  The OpenWrt target library for OpenSync is incomplete and minimally functional.
  b)  The top level Makefile cleans the OpenWrt image and build directories BUT does not clean the OpenSync image and directories when 'make purge' is executed.
  c)  To clean the OpenSync image and build directories, change directory to 'example' and 'make purge'.  The kludge from item d) also must be undone.
  d)  Directory opensync/core is a git submodule pulled from OpenSync.  Several files are patched in the source tree by the build process.
      This will cause errors if the SDK is reinstalled after 'make purge'.  
      Therefore, after 'make purge', change directory to 'opensync/core' and execute 'git status'.  Use 'git checkout -- <filename>' for the modified files.

1) To build the example OpenWrt image, execute the following command:
  make TARGET=IPQ40XX SDK_URL=https://downloads.openwrt.org/releases/19.07.2/targets/ipq40xx/generic/openwrt-sdk-19.07.2-ipq40xx-generic_gcc-7.5.0_musl_eabi.Linux-x86_64.tar.xz PROFILE=linksys_ea8300 IMAGE_URL=https://downloads.openwrt.org/releases/19.07.2/targets/ipq40xx/generic/openwrt-imagebuilder-19.07.2-ipq40xx-generic.Linux-x86_64.tar.xz

This will build the opensync package if it does not already exist, install the selected image builder tools if they are not already present, add the opensync package to the set of available packages, and  build the OpenWrt image.

The resulting image is found at 'build/workdir/bin/targets/ipq40xx/generic/'
 
2) To build only the opensync package:
  cd example
  make opensync TARGET=IPQ40XX SDK_URL=https://downloads.openwrt.org/releases/19.07.2/targets/ipq40xx/generic/openwrt-sdk-19.07.2-ipq40xx-generic_gcc-7.5.0_musl_eabi.Linux-x86_64.tar.xz

This will install the OpenWrt SDK if it does not exist and build the opensync package.
The resulting image is found at 'example/out'.


