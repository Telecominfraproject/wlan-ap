#!/bin/sh

if [ $# -ne 1 ] ; then
	echo "Usage: $0 <redirector address>" >&2
	exit 1
fi

redirector_addr=$1

uci set system.tip.redirector="${redirector_addr}"
/etc/init.d/opensync restart
