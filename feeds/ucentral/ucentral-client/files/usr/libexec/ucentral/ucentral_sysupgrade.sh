#!/bin/sh

tar czf /tmp/sysupgrade.tgz /etc/config/ucentral /etc/ucentral/*.pem /etc/ucentral/*.crt /etc/ucentral/ucentral.active $(readlink /etc/ucentral/ucentral.active)
sysupgrade -f /tmp/sysupgrade.tgz $1
