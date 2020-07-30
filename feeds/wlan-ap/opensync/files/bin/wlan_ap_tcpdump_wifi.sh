#!/bin/sh

/usr/opensync/tools/ovsh insert Command_Config command:="tcpdump-wifi" delay:=5 duration:=15 timestamp:=123 payload:="[\"map\",[[\"wifi\",\"radio0\"],[\"ul_url\",\"http://192.168.178.9/upload\"]]]"
