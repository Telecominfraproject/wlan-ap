#!/bin/sh
# com-wr.sh tty time command parser
# example com-wr.sh /dev/ttyMSM1 1 "\x01\x1D\xFC\x01\x00" | hexdump.sh  --> send "\x01\x1D\xFC\x01\x00" to /dev/ttyMSM1 and then hexdump receive data until 100ms timeout
#command example "\x7E\x03\xD0\xAF und normaler Text" 
tty=$1
time=$2
command=$3
parser=$4
stty -F $tty time $time
exec 99< $tty
echo -en $command > $tty
cat $tty
exec 99<&-
