# Setting up your build machine

Requires a recent linux installation. Older systems without python 3.7 will have trouble.  See this link for details: https://openwrt.org/docs/guide-developer/quickstart-build-images

Install build packages:  sudo apt install build-essential libncurses5-dev gawk git libssl-dev gettext zlib1g-dev swig unzip time rsync python3 python3-setuptools python3-yaml.

Plus specific for TIP: sudo apt-get install openvswitch-common

# Doing a native build on Linux
First we need to clone and setup our tree. This will result in an openwrt/.
```
python3 setup.py --setup
```
Next we need to select the profile and base package selection. This setup will install the feeds, packages and generate the .config file. The available profiles are ap2220, ea8300, ecw5211, ecw5410.
```
cd openwrt
./scripts/gen_config.py ap2220 wlan-ap wifi
```
Finally we can build the tree.
```
make -j X V=s
```
Builds for different profiles can co-exist in the same tree. Switching is done by simple calling gen_config.py again.

# Doing a docker build

Start by installing docker.io on your host system and ensuring that you can run an unprivileged container.
Once this is done edit the Dockerfile and choose the Ubuntu flavour. This might depend on your host installation.
Then simple call (available targets are AP2220, EA8300, ECW5211, ECW5410)
```
TARGET=AP2200 make -j 8
```
