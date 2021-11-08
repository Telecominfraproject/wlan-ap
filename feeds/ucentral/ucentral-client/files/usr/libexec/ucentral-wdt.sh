#!/bin/sh

sleep 60

[ -f /etc/ucentral/redirector.json ] || return 0

active=$(ubus call ucentral status | jsonfilter -e '@.active')

[ -n "$active" -a ! "$active" -eq 1 ] && {
	logger ucentral-wdt: all good
	exit 0
}

logger ucentral-wdt: restarting client

/etc/init.d/ucentral restart
