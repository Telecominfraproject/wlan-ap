# wlan-ap prototype build environment.

The build environment will create a TIP based OpenWrt image that includes the OpenSync services and the required pre-requisites.  The required command is:

- Linksys EA8300:
```
 make TARGET=IPQ40XX
```
- Edge-core ECW5410:
```
 make TARGET=ECW5410
```

- Edge-core ECW5211:
```
make TARGET=ECW5211
```

- TP-Link AP2220:
```
make TARGET=AP2220
```

NOTE: The SDK_URL is no longer used in the build.  The parameter is retained to ensure the automated build is not disrupted.

The resulting image is found at 'wlan-ap/openwrt/bin/targets/ipq40xx/generic'.

## Building OpenSync and generating ipk

- First build complete TIP based OpenWrt image following the instructions in the section above.
- Re-build OpenSync and generate ipk:
```
make opensync TARGET=<target>
where <target> can be either IPQ40XX, ECW5410, ECW5211, or AP2220
```
Example:
```
make opensync TARGET=ECW5410
```

The resulting image is found at 'wlan-ap/openwrt/bin/packages/<varient>/opensync/'.
 
Caution: The work is in-progress and the following known issues apply:
  a)  The top level Makefile cleans the OpenWrt image and build directories BUT does not clean the OpenSync image and directories when 'make purge' is executed.
  b)  To clean the OpenSync image and build directories, change directory to 'example' and 'make purge'.

