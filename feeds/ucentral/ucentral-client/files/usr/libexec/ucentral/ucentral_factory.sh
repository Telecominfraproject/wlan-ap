#!/bin/sh

tar czf /sysupgrade.tgz /etc/config/ucentral /etc/ucentral/*.pem /etc/ucentral/*.crt /etc/ucentral/ucentral.active $(readlink /etc/ucentral/ucentral.active)
jffs2reset -r -y -k
