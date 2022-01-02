#!/bin/sh
com-wr.sh /dev/ttyMSM1 3 "\x01\x1D\xFC\x01\x00" > /dev/null # this command dealy time must >= 3, if small then 3, the following commands  will be something wrong
com-wr.sh /dev/ttyMSM1 1 "\x01\x00\xFE\x08\x08\x00\x00\x00\x00\x00\x00\x00" > /dev/null
com-wr.sh /dev/ttyMSM1 1 "\x01\x61\xFE\x02\x01\x02" > /dev/null
com-wr.sh /dev/ttyMSM1 1 "\x01\x61\xFE\x02\x01\x03" > /dev/null 
com-wr.sh /dev/ttyMSM1 1 "\x01\x61\xFE\x02\x01\x04" > /dev/null
com-wr.sh /dev/ttyMSM1 1 "\x01\x61\xFE\x02\x01\x05" > /dev/null 
com-wr.sh /dev/ttyMSM1 3 "\x01\x51\xFE\x06\x00\x00\xF4\x01\x28\x00" | tee /tmp/blescan.data | ble-scan-rx-parser.sh 