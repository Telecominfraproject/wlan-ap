#!/bin/sh -e

cd "$(dirname $0)"

PATCH_PATH="../../../openwrt/"
if [ ! -d "$PATCH_PATH" ]; then
    echo "openwrt directory does not exist: $(dirname $0)/$PATCH_PATH"
    exit 1
fi
patch -d "$PATCH_PATH" -p1 < "openwrt_feed_update.patch"
echo
echo "openwrt patched"
echo

FEED_PATH="../../../"
if [ ! -d "$FEED_PATH" ]; then
    echo "path to add patches to does not exist: $(dirname $0)/$FEED_PATH"
    exit 1
fi
cp -rv patches/* "$FEED_PATH"
echo
echo "patches added"
echo
