#!/bin/sh

/usr/opensync/tools/ovsh insert Command_Config command:="tcpdump" delay:=30 duration:=60 timestamp:=123 payload:="[\"map\",[[\"network\",\"wan\"],[\"ul_url\",\"http://192.168.178.9/\"]]]"
