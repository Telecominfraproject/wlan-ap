# Setting up your build machine

Requires a recent linux installation. Older systems without python 3.7 will have trouble.  See this link for details: https://openwrt.org/docs/guide-developer/quickstart-build-images

Install build packages:  sudo apt install build-essential libncurses5-dev gawk git libssl-dev gettext zlib1g-dev swig unzip time rsync python3 python3-setuptools python3-yaml.

# Doing a native build on Linux
First we need to clone and setup our tree. This will result in an openwrt/.
```
./setup.py --setup
```
Next we need to select the profile and base package selection. This setup will install the feeds, packages and generate the .config file.
```
cd openwrt
./scripts/gen_config.py linksys_ea8300
```
Finally we can build the tree.
```
make -j X V=s
```
