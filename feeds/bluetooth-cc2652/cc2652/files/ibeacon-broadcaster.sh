#!/bin/sh
function iBeconScan() {
	if [ "$#" -eq 4 ]; then
		UUID=$1
		MAJOR=$2
		MINOR=$3
		POWER=$4
	else
		UUID="\xE2\x0A\x39\xF4\x73\xF5\x4B\xC4\xA1\x2F\x17\xD1\xAD\x07\xA9\x61"
		MAJOR="\x01\x23"
		MINOR="\x45\x67"
		POWER="\xC8"
	fi	

	cc2562-wr.sh /dev/ttyMSM1 3 "\x01\x1D\xFC\x01\x00" > /dev/null # this command dealy time must >= 3, if small then 3, the following commands  will be something wrong
	cc2562-wr.sh /dev/ttyMSM1 1 "\x01\x00\xFE\x08\x01\x00\x00\x00\x00\x00\x00\x00" > /dev/null 
	cc2562-wr.sh /dev/ttyMSM1 1 "\x01\x3E\xFE\x15\x12\x00\xA0\x00\x00\xA0\x00\x00\x07\x00\x00\x00\x00\x00\x00\x00\x00\x7F\x01\x01\x00" > /dev/null
	cc2562-wr.sh /dev/ttyMSM1 1 "\x09\x44\xFE\x23\x00\x00\x00\x1E\x00\x02\x01\x1A\x1A\xFF\x4C\x00\x02\x15${UUID}${MAJOR}${MINOR}${POWER}\x00" > /dev/null
	cc2562-wr.sh /dev/ttyMSM1 1 "\x01\x3F\xFE\x04\x00\x00\x00\x00"
}

cc2562-reset.sh

while true
do
        iBeconScan
        sleep 1
done
