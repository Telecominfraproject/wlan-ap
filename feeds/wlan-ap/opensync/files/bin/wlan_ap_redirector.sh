#!/bin/sh

redirector_addr=$1
[ -z "$redirector_addr" ] && redirector_addr=ssl:opensync.zone1.art2wave.com:6643

echo $redirector_addr > /usr/opensync/etc/redirector
/etc/init.d/opensync restart
