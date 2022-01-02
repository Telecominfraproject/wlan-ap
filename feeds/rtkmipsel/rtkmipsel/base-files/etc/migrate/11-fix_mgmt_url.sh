#!/bin/sh

url=$(uci -q get acn.register.url)

if [ "$url" == "https://staging.ignitenet.com/register" ]; then
    uci set acn.register.url="https://regsvc.ignitenet.com/register"
	uci commit acn.register
fi
