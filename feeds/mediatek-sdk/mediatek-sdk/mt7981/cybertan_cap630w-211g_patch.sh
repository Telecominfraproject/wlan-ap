#!/bin/bash
mkdir package/kernel/mt76/files
mkdir package/kernel/mt76/files/lib
mkdir package/kernel/mt76/files/lib/firmware
mkdir package/kernel/mt76/files/lib/firmware/mediatek
mkdir tools/crc32sum
mkdir tools/crc32sum/src
patch -p1 < ../feeds/mediatek-sdk/mediatek-sdk/mt7981/patchs/0001-add-cybertan_cap630w-211g.patch
rm package/utils/ucode/patches -rf
cp ../feeds/mediatek-sdk/mediatek-sdk/mt7981/base-files/lib/firmware/mediatek/mt7981_eeprom_mt7976_dbdc.bin package/kernel/mt76/files/lib/firmware/mediatek/
