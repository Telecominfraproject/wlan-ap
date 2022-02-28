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
./scripts/gen_config.py udaya

...
Now we do some change in .config
make menuconfig
then go into Firmware 
    <M>       ath10k-firmware-qca4019-ct-full-htt                                                            
    <*>     ath10k-firmware-qca4019-ct-htt...... ath10k CT 10.4 htt-mgt for QCA4018/9
and un-selected 
   < >`ath10k-firmware-qca4019.......................... ath10k qca4019 firmware
then go into 
  uCentral and select
   <*> usb-console.................................................. usb-console
   
 Now save and exit.
Finally we can build the tree.
```
make -j X V=s
```
the bin file is find at location 
      /<folder path>/wlan-ap/openwrt/bin/targets/ipq40xx/generic
with name
      openwrt-ipq40xx-generic-qcom_ap-dk01.1-c1-squashfs-sysupgrade.bin
copy this file in ap at /tmp
through minicom or console 
    cd /tmp
    sysupgrade -v openwrt-ipq40xx-generic-qcom_ap-dk01.1-c1-squashfs-sysupgrade.bin
    
    firstboot
      then type y enter.
     reboot -f.
     
     
