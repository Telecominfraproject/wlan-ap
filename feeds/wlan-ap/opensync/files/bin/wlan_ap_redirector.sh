#!/bin/sh

AP_PRIVATE_KEY_FILE="/usr/opensync/certs/client_dec.key"
AP_CERTIFICATE_FILE="/usr/opensync/certs/client.pem"
AP_DEVICE_ID_FILE="/usr/opensync/certs/client_deviceid.txt"
DIGICERT_API_URI="clientauth.one.digicert.com"

if [ "$1" = "-h" ]; then
  echo "Usage: $0 [redirector address]" >&2
  exit 1
fi

# Query DigiCert's API if redirector wasn't specified
if [ -z "$1" ]; then
  if [ ! -f "$AP_DEVICE_ID_FILE" ]; then
      echo "Device ID file $AP_DEVICE_ID_FILE does not exist. Make sure to create it or specify the redirector address manually."
      exit 1
  fi

  digicert_device_id=`cat ${AP_DEVICE_ID_FILE}`
  device_data=`curl -s \
    --retry 5 \
    --show-error \
    --key "${AP_PRIVATE_KEY_FILE}" \
    --cert "${AP_CERTIFICATE_FILE}" \
    "https://${DIGICERT_API_URI}/iot/api/v2/device/${digicert_device_id}"`

  controller_url=`echo ${device_data} | jsonfilter -e '@.fields[@.name="Redirector"].value'`
  if [ -z "$controller_url" ]; then
    echo "No redirector found for this device"
    exit 1
  fi
  controller_port=`echo ${controller_url} | cut -s -d ":" -f2)`
  if [ -z "$controller_port" ]; then
    redirector_addr="ssl:${controller_url}:6643"
  else
    redirector_addr="ssl:${controller_url}"
  fi
else
  redirector_addr=$1
fi

# Enable lan ssh accsess
lan-ssh-firewall disable

echo "${redirector_addr}" > /usr/opensync/certs/redirector.txt
/etc/init.d/uhttpd enable
/etc/init.d/uhttpd start
uci set system.tip.redirector="${redirector_addr}"
uci set system.tip.deployed=0
uci commit system
/etc/init.d/opensync restart
