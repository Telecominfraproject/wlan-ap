#!/bin/sh

tar czf /sysupgrade.tgz /usr/opensync/certs/
jffs2reset -r -y -k
