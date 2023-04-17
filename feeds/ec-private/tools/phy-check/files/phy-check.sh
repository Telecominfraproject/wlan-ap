#!/bin/sh

retry_interval=15

ubus call system watchdog '{"frequency":1,"timeout":15}'

while [ 1 ]
do
  echo 0 >/sys/ssdk/dev_id
  ssdk_sh debug phy get 0x7 0x02 | grep "0x004d" > /dev/null
  if [ $? -ne 0 ]; then
    break
  fi
  ssdk_sh debug phy get 0x7 0x03 | grep "0xd0c0" > /dev/null
  if [ $? -ne 0 ]; then
    break
  fi
  
  echo 1 >/sys/ssdk/dev_id
  ssdk_sh debug phy get 0x0 0x02 | grep "0x004d" > /dev/null
  if [ $? -ne 0 ]; then
    break
  fi
  ssdk_sh debug phy get 0x0 0x03 | grep "0xd036" > /dev/null
  if [ $? -ne 0 ]; then
    break
  fi
  ssdk_sh debug phy get 0x1 0x02 | grep "0x004d" > /dev/null
  if [ $? -ne 0 ]; then
    break
  fi
  ssdk_sh debug phy get 0x1 0x03 | grep "0xd036" > /dev/null
  if [ $? -ne 0 ]; then
    break
  fi
  ssdk_sh debug phy get 0x2 0x02 | grep "0x004d" > /dev/null
  if [ $? -ne 0 ]; then
    break
  fi
  ssdk_sh debug phy get 0x2 0x03 | grep "0xd036" > /dev/null
  if [ $? -ne 0 ]; then
    break
  fi
  ssdk_sh debug phy get 0x3 0x02 | grep "0x004d" > /dev/null
  if [ $? -ne 0 ]; then
    break
  fi
  ssdk_sh debug phy get 0x3 0x03 | grep "0xd036" > /dev/null
  if [ $? -ne 0 ]; then
    break
  fi

  sleep $retry_interval
done

logger "Checking phy error, rebooting..."
reboot
