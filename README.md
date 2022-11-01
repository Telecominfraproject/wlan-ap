# OpenWiFi AP NOS

OpenWrt-based access point network operating system (AP NOS) for TIP OpenWiFi.
Read more at [openwifi.tip.build](https://openwifi.tip.build/).

## Building

### Setting up your build machine

Building requires a recent Linux installation. Older systems without Python 3.7
will have trouble. See this guide for details:
https://openwrt.org/docs/guide-developer/toolchain/beginners-build-guide

Install build packages on Debian/Ubuntu (or see above guide for other systems):
```
sudo apt install build-essential libncurses5-dev gawk git libssl-dev gettext zlib1g-dev swig unzip time rsync python3 python3-setuptools python3-yaml
```

### Doing a native build on Linux

Use `./build.sh <target>`, or follow the manual steps below:

1. Clone and set up the tree. This will create an `openwrt/` directory.
```shell
./setup.py --setup    # for subsequent builds, use --rebase instead
```

2. Select the profile and base package selection. This setup will install the
   feeds and packages and generate the `.config` file.
```shell
cd openwrt
./scripts/gen_config.py linksys_ea8300
```

3. Build the tree (replace `-j 8` with the number of cores to use).
```shell
make -j 8 V=s
```

### Build output

The build results are located in the `openwrt/bin/` directory:

| Type             | Path                                                 |
| ---------------- | ---------------------------------------------------- |
| Firmware images  | `openwrt/bin/targets/<target>/<subtarget>/`          |
| Kernel modules   | `openwrt/bin/targets/<target>/<subtarget>/packages/` |
| Package binaries | `openwrt/bin/packages/<platform>/<feed>/`            |

## Developer Notes

### Branching model

- `main` - Stable dev branch
- `next` - Integration branch
- `staging-*` - Feature/bug branches
- `release/v#.#.#` - Release branches (*major.minor.patch*)

### Repository structure

Build files:
- `Makefile` - Calls Docker environment per target
- `dock-run.sh` - Dockerized build environment
- `docker/Dockerfile` - Dockerfile for build image
- `build.sh` - Build script
- `setup.py` - Clone and set up the tree
- `config.yml` - Specifies OpenWrt version and patches to apply

Directories:
- `feeds/` - OpenWiFi feeds
- `patches/` - OpenWiFi patches applied during builds
- `profiles/` - Per-target kernel configs, packages, and feeds
    - [wifi-ax](profiles/wifi-ax.yml): Wi-Fi AX packages
    - [ucentral-ap](profiles/ucentral-ap.yml): uCentral packages
    - [x64_vm](profiles/x64_vm.yml): x86-64 VM image

### uCentral packages

AP-NOS packages implementing the uCentral protocol include the following
repositories (refer to the [ucentral](feeds/ucentral/) feed for a full list):
- ucentral-client: https://github.com/Telecominfraproject/wlan-ucentral-client
- ucentral-schema: https://github.com/Telecominfraproject/wlan-ucentral-schema
- ucentral-wifi: https://github.com/blogic/ucentral-wifi
