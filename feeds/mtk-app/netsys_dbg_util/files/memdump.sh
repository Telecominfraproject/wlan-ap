# ./memdump.sh 0x15110000 0x200
#!/bin/sh
if [ $# -lt 2 ]; then
        echo "usage: $0 [base_hex] [range_hex]"
        exit
fi
base=$1
range=$2
sleep_int=4
cur_dex=$(printf %d ${base})
echo "base=${base}=${cur_dex}DEX"
len_dex=$(printf %d ${range})
echo "range=${range}=${len_dex}DEX"
last=$(expr ${cur_dex} + ${len_dex})
last_hex=$(printf %x ${last})
echo "regs d ${base} to 0x${last_hex}"
echo "-------------"
interval=0
while [ ${cur_dex} -lt ${last} ]; do
        cur_hex=$(printf %x ${cur_dex})
        echo "regs d ${cur_hex}"
        regs d ${cur_hex}
        cur_dex=$(expr ${cur_dex} + 256)
        if [ $interval -gt $sleep_int ]; then
                sleep 1
                interval=0
        else
                interval=$(expr ${interval} + 1)
        fi
done
