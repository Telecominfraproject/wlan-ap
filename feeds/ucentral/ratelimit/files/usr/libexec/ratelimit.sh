#!/bin/sh

case $2 in
AP-STA-CONNECTED)
	ratelimit addclient $1 $3 $4 $5
	;;
AP-STA-DISCONNECTED)
	ratelimit delclient $1 $3
	;;
esac
