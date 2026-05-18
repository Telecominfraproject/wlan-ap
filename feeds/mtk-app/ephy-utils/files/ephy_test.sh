#!/bin/sh
# Normal: ./ephy_test.sh -p=0 --dst_ip=10.77.1.254(Cheetah)
# ./ephy_test.sh -p=0 --dst_ip=10.77.1.254 --delay=25 --packet=10
# ./ephy_test.sh -p=0 --dst_ip=10.77.1.254 --delay=10 --packet=2 --an_type=phy_power_downup
# ./ephy_test.sh -p=0 --dst_ip=10.77.1.254 --delay=25 --packet=10 --an_type=phy_reset
#
# Cheetah:
# [Basic test]
# $ ./ephy_test.sh -p=0 --dst_ip=192.168.1.2
#
# [Flow=IoT_exam2]
# $ ./ephy_test.sh --switch --port=1 --packet=5 --flow=IoT_exam2 --mode=force-slave --log_duration=25
# $ ./ephy_test.sh --switch --dst_ip=192.168.1.3 --port=0 --packet=5 --mode=force-slave
#
# [Flow=Dump Coeff]
# $ ./ephy_test.sh --port=0 --dst_ip=192.168.1.3 --mode=force-slave \
#   --an_type=phy_power_downup --flow=dump_coeff --round=3
#
# [For SA]
# $ ./ephy_test.sh -p=0 --dst_ip=192.168.1.3 --an_type=ifconfig --mode=force-slave --packet=0 --delay=3 --flow=pause_after_linkup
#
# [For MSZ]
# $ ./ephy_test.sh -p=0 --dst_ip=10.77.1.254 --an_type=phy_power_downup --mode=force-master
# $ ./ephy_test.sh -p=0 --dst_ip=10.77.1.254 --an_type=phy_power_downup --mode=prefer-master
# $ ./ephy_test.sh -p=0 --dst_ip=192.168.1.3 --an_type=ifconfig --mode=force-slave


# [Jaguar RFB2 layout]
#                    |   Phy Addr  | interface
# Left 10G phy       |     0x8     |   eth2
# Right 10G phy      |     0x0     |   eth1
# Left ext 2.5Gphy   |     0x5     |   eth2
# Right ext 2.5Gphy  |     0xd     |   eth1
# Switch Gphy0~3     |
# Internal 2.5Gphy   |     0xf     |   eth1

# [For SQC]
# $ ./ephy_test.sh -s -p=0 --dst_ip=192.168.1.1 --an_type=phy_power_downup --delay=10

##############
# [For Development Unit test]
# Port match case:
#  ./ephy_test.sh --switch --port=0 --dst_ip=192.168.1.3 --an_type=phy_power_downup (Cheetah switch port=0~3)
#  ./ephy_test.sh --port=0 --dst_ip=192.168.1.3 --an_type=phy_power_downup (Cheetah internal GBE, eth1)
#  ./ephy_test.sh --switch --port=0 --dst_ip=192.168.1.3 --an_type=phy_power_downup (Jaguar switch/mt7531 port=0~3)
#  ./ephy_test.sh --port=8 --dst_ip=192.168.1.3 --an_type=phy_power_downup (Jaguar left 10Gphy)
#  ./ephy_test.sh --port=0 --dst_ip=192.168.1.3 --an_type=phy_power_downup (Jaguar right 10Gphy)
#  ./ephy_test.sh --port=5 --dst_ip=192.168.1.3 --an_type=phy_power_downup (Jaguar left e2.5Gphy, eth2)
#  ./ephy_test.sh --port=13 --dst_ip=192.168.1.3 --an_type=phy_power_downup (Jaguar right e2.5Gphy)
#  ./ephy_test.sh --port=15 --dst_ip=192.168.1.3 --an_type=phy_power_downup (Jaguar i2.5Gphy)
#
# Force-slave mode check
#
# Port not exist case:
#  ./ephy_test.sh --switch --port=4 --dst_ip=192.168.1.3 --an_type=phy_power_downup
#  ./ephy_test.sh --switch --port=0 --dst_ip=192.168.1.3 --an_type=ifconfig (under GSW)
#  ./ephy_test.sh --port=1 --dst_ip=192.168.1.3 --an_type=phy_power_downup
##############

# Set some default values here
delay=20
packet_num=10
dst_ip="192.168.1.2"
round=1000
flow="" # SLT, IoT_exam1, IoT_exam2, stop_when_fail, dump_coeff
an_type="phy_reAN" #phy_power_downup, phy_reset, ifconfig, phy_reAN, manual_plugin
mode="auto" #auto, force-master, force-slave, round-robin
pure_cl45=0
RR_lock=0
end_condition=""
target_interface=""
debug_mode=0
log_duration=0
link_10G_cnt=0
link_5G_cnt=0
link_2p5G_cnt=0
link_1G_cnt=0
link_100M_cnt=0
link_10M_cnt=0
link_master_cnt=0
link_slave_cnt=0
interface_down_time=0 # This value is only for an_type=ifconfig or manual_plugin
link_status=0
DSPState=0
version=4.1

##############################################################
# [Debug mode options]                                       #
# Adjust these settings to print debug messages or not       #
pr_packet_cnt=0
pr_DSPstate=1
##############################################################

TEST_CMD="mii"
kernel_ver=`uname -r`

# Must source after initializing global vars, or they will be messed up
source lib.sh

platform=`cat /proc/device-tree/compatible | grep -E -o 'mt[0-9]+' | head -1`
echo "Platform: ${platform}"
if [ "${platform}" = "mt7629" ]
then
	eth_prefix="/sys/devices/platform"
	eth_baseaddr="1b100000"
elif [ "${platform}" = "mt7981" ] || [ "${platform}" = "mt7988" ]
then
	eth_prefix="/sys/devices/platform"
	eth_baseaddr="15100000"
elif [ "${platform}" = "mt7987" ]
then
	eth_prefix="/sys/devices/platform/soc_netsys"
	eth_baseaddr="15100000"
fi

if [ -z ${port} ]
then
	echo "Port not specified."
else
	echo "Port: ${port}"
fi

if [ ${delay} -eq 20 ]
then
	echo "Delay time: ${delay}s (default)"
else
	echo "Delay time: ${delay}s"
fi

if [ "${dst_ip}" = "192.168.1.2" ]
then
	echo "Destination: ${dst_ip} (default)"
else
	echo "Destination: ${dst_ip}"
fi

if [ ${packet_num} -eq 10 ]
then
	echo "Packet number: ${packet_num} (default)"
else
	echo "Packet number: ${packet_num}"
fi

if [ ${round} -eq 1000 ]
then
	echo "Test round: ${round} times(default)"
else
	echo "Test round: ${round} times"
fi

if [ "${mode}" = "auto" ]
then
	echo "Mode: AUTO (default)"
elif [ "${mode}" = "force-master" ] || [ "${mode}" = "force-slave" ] ||
	[ "${mode}" = "prefer-master" ] || [ "${mode}" = "round-robin" ]
then
	echo "Mode: ${mode}"
	if [ "${mode}" = "round-robin" ]; then
		RR_lock=1
	fi
else
	echo "Please enter correct mode (auto/force-master/force-slave/prefer-master)"
fi

if [ "${an_type}" = "ifconfig" ] || [ "${an_type}" = "phy_power_downup" ]
then
	interface_down_time=3
elif [ "${an_type}" = "manual_plugin" ]
then
	interface_down_time=10
fi

##################### Constant for different test flows #####################
# Add 1 to this counter if certain round is without packet loss
logfile_path="."
buffer_final="${logfile_path}/buffer_final.txt"
if [ "${flow}" = "IoT_exam1" ] || [ "${flow}" = "IoT_exam2" ]  || [ "${flow}" = "dump_coeff" ]
then
	final_log="${logfile_path}/final.log"
	buffer_A="${logfile_path}/bufferA.txt"
	buffer_B="${logfile_path}/bufferB.txt"
	touch ${final_log}
fi
if [ "${flow}" = "IoT_exam1" ]
then
	packet_good_round_cnt=0
	packet_loss_round_cnt=0
	#Log FFE, DFE, EC, TREC, MSE, VGA
	log_script_1="${logfile_path}/table_1.sh"
	log_duration=60 #Logging for 60s
	after_linkup_log="${logfile_path}/after_linkup.log"
	packet_loss_log="${logfile_path}/packet_loss.log"
	packet_good_log="${logfile_path}/packet_good.log"

	# Check if log_script exists
	if [[ ! -f ${log_script_1} ]]
	then
		echo "${log_script_1} doesn't exist"
		exit 1
	else
		#rm -rf ${after_linkup_log} ${packet_loss_log} ${packet_good_log} ${final_log}
		#touch ${final_log}
		end_condition="packet_good_and_loss"
	fi
elif [ "${flow}" = "IoT_exam2" ]
then
	linkup_lt5s_round_cnt=0 #less than 5s
	linkup_gt10s_round_cnt=0 #greater than 10s
	log_script_2="${logfile_path}/table_2.sh"
	table2_log="${logfile_path}/table_2.log"
	log_duration=25 #Logging for 25s
	if [[ ! -f ${log_script_2} ]]
	then
		echo "${log_script_2} doesn't exist"
		exit 1
	else
		#rm -rf ${table2_log} ${final_log} ${buffer_A} ${buffer_B} ${buffer_final}
		end_condition="linkup_5s_and_10s"
	fi
elif [ "${flow}" = "dump_coeff" ]
then
	coeff_script="${logfile_path}/coeff.sh"
fi

if [ "${flow}" = "IoT_exam1" ] || [ "${flow}" = "IoT_exam2" ]; then
	if [ ${log_duration} -le 0 ]
	then
		echo "Please specify correct logging duration time"
		exit 1
	else
		echo "Logging duration: ${log_duration}s"
	fi
fi

########################################################################

disable_tx_power_saving() {
	#switch phy cl45 w ${port} 0x1e 0xc6 0x53aa
	cmd_gen 45 "write" 0x1e 0xc6 0x53aa && ${CMD}
}

mt7981_finetune() {
	# LoThresh to 0x55a0, now we skip this
	#mii_mgr -s -p 0 -r 0x1f -v 0x52b5
	#mii_mgr -s -p 0 -r 0x12 -v 0x0
	#mii_mgr -s -p 0 -r 0x11 -v 0x55a0
	#mii_mgr -s -p 0 -r 0x10 -v 0x83aa
	#mii_mgr -s -p 0 -r 0x1f -v 0x0
	#----------------------------------------------
	#cmd_gen 22 "write" 0x1f 0x52b5 && ${CMD}
	#cmd_gen 22 "write" 0x12 0x0 && ${CMD}
	#cmd_gen 22 "write" 0x11 0x55a0 && ${CMD}
	#cmd_gen 22 "write" 0x10 0x83aa && ${CMD}
	#cmd_gen 22 "write" 0x1f 0x0 && ${CMD}
	#----------------------------------------------

	# Force Kp=3 via token ring
	#mii_mgr -s -p 0 -r 0x1f -v 0x52b5 #switch to page 0x52b5
	#mii_mgr -s -p 0 -r 0x12 -v 0x0 #Write high value
	#mii_mgr -s -p 0 -r 0x11 -v 0x700 #Write low value
	#mii_mgr -s -p 0 -r 0x10 -v 0x9d86 #write via token ring
	cmd_gen 22 "write" 0x1f 0x52b5 && ${CMD}
	cmd_gen 22 "write" 0x12 0x0 && ${CMD}
	cmd_gen 22 "write" 0x11 0x700 && ${CMD}
	cmd_gen 22 "write" 0x10 0x9586 && ${CMD}
	# SlvTRstabletime*2 via token ring
	#mii_mgr -s -p 0 -r 0x12 -v 0x0 #Write high value
	#mii_mgr -s -p 0 -r 0x11 -v 0x64 #Write low value
	#mii_mgr -s -p 0 -r 0x10 -v 0x8f82 #write via token ring
	#mii_mgr -s -p 0 -r 0x1f -v 0x0 #switch to page 0x52b5
	cmd_gen 22 "write" 0x12 0x0 && ${CMD}
	cmd_gen 22 "write" 0x11 0x64 && ${CMD}
	cmd_gen 22 "write" 0x10 0x8f82 && ${CMD}
	cmd_gen 22 "write" 0x1f 0x0 && ${CMD}
}

# We've tried all these finetunes.
# Effective finetunes are pushed to SDK driver
mt7988_finetune() {
	# TX filter: (in SDK already)
	#  $ switch phy cl45 w ${port} 0x1e 0xfe 0x5
	cmd_gen 45 "write" 0x1e 0xfe 0x5 && ${CMD}

	# LoThresh to 0x55a0 (Not Work)
	#  $ switch phy cl22 w ${port} 0x1f 0x52b5
	#  $ switch trreg w ${port} 0x0 0x7 0x15 0x0 0x55a0
	#  $ switch phy cl22 w ${port} 0x1f 0x0
	cmd_gen 22 "write" 0x1f 0x52b5 && ${CMD}
	switch trreg w ${port} 0x0 0x7 0x15 0x0 0x55a0
	switch trreg w ${port} 0x2 0xd 0x3 0x0 0x900
	cmd_gen 22 "write" 0x1f 0x0 && ${CMD}
}

relay_board_init() {
	regs w 0x1001f328 0x700000
	regs w 0x1001f378 0x70
	regs w 0x1001f004 0x200000
	regs w 0x1001f014 0x2000000
}

relay_board_on() {
	regs w 0x1001f108 0x200000
	regs w 0x1001f118 0x2000000
	sleep 1
}

relay_board_off() {
	regs w 0x1001f104 0x200000
	regs w 0x1001f114 0x2000000
	sleep 1
}

dsa_match_port() {
	interfaces=`ls -1 ${1}`
	for ifs in ${interfaces}
	do
		port_dump=`ethtool ${ifs} | grep -E -o "PHYAD: [0-9a-fA-F]+" | sed 's/PHYAD: //g'`
		if [[ "$kernel_ver" = "6.6"* ]]
		then
			port_dump=`printf "%d" 0x$port_dump`
		fi

		if [ ${port_dump} -eq ${port_decimal} ]
		then
			target_interface=${ifs}
			is_portMatch=1
			#echo "Port matches!!"
			break
		fi
	done
}

mii_match_port() {
	interfaces=`ls -1 ${1}`
	for ifs in ${interfaces}
	do
		#### Check if this interface is on the top of switch ####
		eth_found=0
		for path in \
		${eth_prefix}/${eth_baseaddr}.ethernet/net/${ifs}/of_node \
		${eth_prefix}/*/${eth_baseaddr}.ethernet/net/${ifs}/of_node; do
			if [ -e "$path" ]; then
				mac_port=`readlink $path`
				eth_found=1
				break
			fi
		done
		if [ $eth_found -eq 0 ]; then
			echo "eth interface of_node not found"
			exit 1
		fi

		if [ -n "${mac_port}" ]; then
			# mac@0 is for switch on mt7981/mt7988
			#if [[ ${mac_port} = *"mac@0"* ]]; then
			if echo "${mac_port}" | grep -q "mac@0"; then
				continue
			fi
		fi
		#supported_ports=`ethtool ${ifs} | grep "Supported ports"`
		#if [[ "${supported_ports}" = *"MII"* ]]; then
		#	continue
		#fi
		#########################################################

		# For kernel-6.6, ethtool will show port number as hex.
		# For kernel-5.4/kernel-6.12, ethtool will show port number as decimal.
		# However, it seems that this depends on your dts settings. So
		# we transform it here if it's hex.
		port_dump=`ethtool ${ifs} | grep -E -o "PHYAD: [0-9a-fA-F]+" | sed 's/PHYAD: //g'`
		if [[ "$kernel_ver" = "6.6"* ]]
		then
			port_dump=`printf "%d" 0x$port_dump`
		fi

		if [ ${port_dump} -eq ${port_decimal} ]
		then
			target_interface=${ifs}
			is_portMatch=1
			#echo "Port matches!!"
			break
		fi
	done
}

is_portMatch=0
if [ "${TEST_CMD}" = "mii" ]
then
	eth_found=0
	for path in \
	${eth_prefix}/${eth_baseaddr}.ethernet/net/ \
	${eth_prefix}/*/${eth_baseaddr}.ethernet/net/; do
		if [ -e "$path" ]; then
			eth_folder_temp=$path
			eth_found=1
			break
		fi
	done
	if [ $eth_found -eq 0 ]; then
		echo "eth folder not found"
		exit 1
	fi

	eth_folder=$(echo $eth_folder_temp)
	if [ ! -d ${eth_folder} ]
	then
		echo "${eth_folder} doesn't exist"
		exit 1
	fi
	mii_match_port ${eth_folder}
elif [ "${TEST_CMD}" = "switch" ]
then
	if [[ "$kernel_ver" = "5.4"* ]]
	then
		eth_folder="${eth_prefix}/${eth_baseaddr}.ethernet/mdio_bus/mdio-bus/mdio-bus:1f/net/"
	else
		eth_folder_temp="${eth_prefix}/*/15020000.switch/net/"
		eth_folder=$(echo $eth_folder_temp)
	fi

	if [ ! -d "${eth_folder}" ]
	then
		switch_framework="gsw"
		if [ "${an_type}" = "ifconfig" ]
		then
			echo "You can't test gsw with an_type=ifconfig"
			exit 1
		fi
		target_interface="br-lan"
		for lan_number in $(seq 0 3)
		do
			if [ ${port} -eq ${lan_number} ]
			then
				is_portMatch=1
				#echo "Port matches!!"
				break
			fi
		done
	else
		switch_framework="dsa"
		dsa_match_port ${eth_folder}
		target_interface="lan${port_decimal}"
	fi
fi

if [ ${is_portMatch} -eq 0 ]
then
	echo "Port specified doesn't match any network interfaces!"
	exit 1
else
	echo "Target interface: ${target_interface}"
fi
if [ -n "${switch_framework}" ]; then
	echo "Switch framework: ${switch_framework}"
fi


if [ "${flow}" = "SLT" ]; then
	relay_board_init
	relay_board_off
	relay_board_on
	echo "Sleep 10s after relay board is on"
	sleep 10
	dmesg -c > /dev/null
fi

format_log() {
	log_file=$1
	sed -i 's/$/",/' ${log_file} # Add ", to the tail of each line
	sed -i 's/^/"/' ${log_file} # Add ", to the start of each line
	sed -i ':a;N;$!ba;s/\n//g' ${log_file} # Remove new line of each line
}

log_params() {
	log_script=$1
	log_target_file=$2
	create_target_file=$3
	if [ "${create_target_file}" = true ]
	then
		date +"%Y%m%d %H:%M:%S" > ${log_target_file}
	else
		date +"%Y%m%d %H:%M:%S" >> ${log_target_file}
	fi
	source ${log_script} --port=${port} >> ${log_target_file}
	format_log ${log_target_file}
}

mii_mgr_tr_access() {
	mii_mgr -s -p ${port} -r 0x1f -v 0x52b5 > /dev/null

	PKT_XMT_STA=1 #[15]
	# bit14 is reserved
	if [ "$1" = "read" ]
	then
		WR_RD_CTRL=1
	elif [ "$1" = "write" ]
	then
		WR_RD_CTRL=0
	fi
	CH_ADDR=$2 #[12:11]
	NODE_ADDR=$3 #[10:7]
	DATA_ADDR=$4 #[6:1]
	# bit0 is reserved
	high_value=$5
	low_value=$6

	tr_access=$(( (${PKT_XMT_STA} << 15) |
				 ((${WR_RD_CTRL} & 0x1 ) << 13) |
				 ((${CH_ADDR} & 0x3) << 11) |
				 ((${NODE_ADDR} & 0xf) << 7) |
				 ((${DATA_ADDR} & 0x3f) << 1) ))
	tr_access=`printf "0x%x" ${tr_access}`

	#echo ${tr_access}
	if [ "$1" = "read" ]
	then
		mii_mgr -s -p ${port} -r 0x10 -v ${tr_access} > /dev/null
		tr_high_val=0x`mii_mgr -g -p ${port} -r 0x12 | grep -E -o " = [0-9A-Fa-f]+" | grep -E -o "[0-9A-Fa-f]+"` > /dev/null
		tr_low_val=0x`mii_mgr -g -p ${port} -r 0x11 | grep -E -o " = [0-9A-Fa-f]+" | grep -E -o "[0-9A-Fa-f]+"`  > /dev/null
	elif [ "$1" = "write" ]
	then
		mii_mgr -s -p ${port} -r 0x12 -v ${high_value}
		mii_mgr -s -p ${port} -r 0x11 -v ${low_value}
		mii_mgr -s -p ${port} -r 0x10 -v ${tr_access}
	fi

	mii_mgr -s -p ${port} -r 0x1f -v 0x0 > /dev/null
}

poll_DSPState() {
	if [ "${TEST_CMD}" = "switch" ]
	then
		switch phy cl22 w ${port} 0x1f 0x52b5 > /dev/null
		DSPState=`switch trreg r ${port} 0x1 0xf 0x39 | grep "value_H"`
		switch phy cl22 w ${port} 0x1f 0x0 > /dev/null
		DSPState_H=`echo ${DSPState} | grep -E -o "value_H=[0-9a-fA-F]+" | sed 's/value_H=//g'`
		DSPState_L=`echo ${DSPState} | grep -E -o "value_L=[0-9a-fA-F]+" | sed 's/value_L=//g'`
		#DSPState=$(( ((0x${DSPState_H} & 0xf0) << 16)+0x1 ))
		DSPState=$(( ((0x${DSPState_H} & 0xf0) << 16) + 0x${DSPState_L} ))
		#printf "DSPState: 0x%x\n" ${DSPState}
	elif [ "${TEST_CMD}" = "mii" ]
	then
		mii_mgr_tr_access "read" 0x1 0xf 0x39
		DSPState=$(( ((${tr_high_val} & 0xf0) << 16) + ${tr_low_val} ))
		#mii_mgr_tr_access "read" 0x0 0xf 0x2
		#AN_02h=$(( ((${tr_high_val} & 0xf0) << 16) + ${tr_low_val} ))
	fi

	if [ ${pr_DSPstate} -eq 1 ]; then
		printf "DSP state: 0x%x\n" ${DSPState}
		#printf "DSP state: 0x%x\n" ${AN_02h}
	fi
}

wait_DSP_ready() {
	while
		DSPState=0x1
		poll_DSPState
		[ ! ${DSPState} -eq 1 ]
	do :; done
	printf "DSPState: 0x%x\n" ${DSPState}
}

store_link_updown_msg() {
	local_buffer=$1
	grep_msg=$2
	if [ `cat ${local_buffer} | grep "${grep_msg}" | wc -l` -gt 0 ]
	then
		cat ${local_buffer} | grep "${grep_msg}" >> ${buffer_final}
		cat ${local_buffer} | grep "${grep_msg}" > ${buffer_B}
		format_log ${buffer_B}
		cat ${buffer_B} >> ${final_log}
	fi
}

start_log_table2() {
	start=$(date "+%s")
	second_cnt=${start}
	echo "Start dumpping SignalDetectT, DigPhase...."
	echo "Right after DSPState[19:0] is 0x1" > ${final_log}

	# Before we try to store link up/down messages during logging table2
	# Save the dmesg to buffer final first
	dmesg -c >> ${buffer_final}
	while
		#####################
		dmesg -c > ${buffer_A}
		store_link_updown_msg ${buffer_A} "Link is Up"
		store_link_updown_msg ${buffer_A} "Link is Down"

		log_params ${log_script_2} ${table2_log} "true"
		cat ${table2_log} >> ${final_log}
		end=$(date "+%s")
		counter=$((${end}-${second_cnt}))
		if [ ${counter} -ge 5 ]; then
			echo "Dump log, timestamp: ${end}s"
			second_cnt=${end}
		fi
		duration=$(( ${end} - ${start} ))
		[ ${duration} -le ${log_duration} ]
	do :; done
}

check_link_mode() {
	if [ ${pure_cl45} -eq 1 ]
	then
		cmd_gen 45 "read" 0x7 0x21
	else
		cmd_gen 22 "read" 0xa
	fi
	master1_or_slave0=$(eval "${CMD}${response}")
	master1_or_slave0=$(( (${master1_or_slave0} >> 14) & 0x1 ))
	if [ "${master1_or_slave0}" -eq 1 ]
	then
		current_mode="master"
		let "link_master_cnt++"
	else
		current_mode="slave"
		let "link_slave_cnt++"
	fi
}

check_target_link_mode() {
	cmd_gen 45 "read" 0x1e 0xa2
	dev1e_rega2=$(eval "${CMD}${response}")
	final_speed_1000=$(( (${dev1e_rega2} >> 3) & 0x1 ))

	if [ "${final_speed_1000}" -eq 0 ]
	then
		has_printed=0
		target_mode="None"
		return 0
	fi

	if [ "${has_printed}" -eq 0 ] && [ "${final_speed_1000}" -eq 1 ]
	then
		MSConfig1000=$(( (${dev1e_rega2} >> 4) & 0x1 ))
		if [ ${MSConfig1000} -eq 1 ]
		then
			echo "Try to connect @ master..."
			target_mode="master"
		else
			echo "Try to connect @ slave..."
			target_mode="slave"
		fi
		has_printed=1
	fi
}

# For summarize test result
total_firstdown_to_lastup=0
round_linkup_cnt=0 #linkup times for current test round
total_linkup_cnt=0
total_linkup_time=0
max_linkup_time=0
min_linkup_time=999999
test_result=""
test_index=0
current_mode=""
has_printed=0
linkup_ok_cnt=0
linkup_ok_ping_ok_cnt=0
linkup_ok_ping_fail_cnt=0
total_linkup_fail_cnt=0

test_end() {
	if [ "${flow}" = "IoT_exam1" ]
	then
		rm -rf ${after_linkup_log} ${packet_loss_log} ${packet_good_log} ${final_log} ${buffer_final}
	elif [ "${flow}" = "IoT_exam2" ]
	then
		rm -rf ${table2_log} ${final_log} ${buffer_A} ${buffer_B} ${buffer_final}
	elif [ "${flow}" = "dump_coeff" ]
	then
		rm -rf ${final_log} ${buffer_A}
	else
		#Normal test enters this case
		rm -rf ${buffer_final}
	fi
}

check_end_condition() {
	if [ "${flow}" = "IoT_exam1" ]
	then
		if [ ${packet_good_round_cnt} -ge 1 ] &&
			[ ${packet_loss_round_cnt} -ge 1 ]; then
			test_end
			exit 0
		fi
	elif [ "${flow}" = "IoT_exam2" ]
	then
		if [ ${linkup_lt5s_round_cnt} -ge 2 ] &&
			[ ${linkup_gt10s_round_cnt} -ge 2 ]; then
			test_end
			exit 0
		fi
	fi
	if [ "${flow}" = "stop_when_fail" ]; then
		if [ "${test_result}" = "linkup_fail" ] ||
			[ "${test_result}" = "linkup_ok_ping_fail" ]; then
			exit 1
		fi
	fi
}

print_summary() {
	echo "##################### Summary #####################"
	# stage1, print information for current test round
	echo "[Test round ${test_index}]"
	if [ ${round_linkup_cnt} -gt 1 ]; then
		relink_times=$((${round_linkup_cnt}-1))
		echo "Relink takes place ${relink_times} time!!!"
	fi
	if [ "${test_result}" = "linkup_ok_ping_ok" ]
	then
		echo "Result: Linkup ok & ping ok"
		echo "Link mode: ${current_mode}"
		echo "1st down~last up time: ${firstdown_to_lastup}s"
	elif [ "${test_result}" = "linkup_fail" ]
	then
		echo "Result: Linkup failed"
		echo "Target link mode: ${target_mode}"
		echo "(Target link won't be counted to link mode counter below)"
	elif [ "${test_result}" = "linkup_ok_ping_fail" ]
	then
		echo "Result: Ping failed"
		echo "Link mode: ${current_mode}"
		echo "1st down~last up time: ${firstdown_to_lastup}s"
	fi

	echo "[Total]"
	echo "Total 1st down~last up time: ${total_firstdown_to_lastup}s"
	echo "Average 1st down~last up time: ${avg_firstdown_to_lastup}s"
	# If we test N times, relink happens N+2 times, it means that 2 retries take place.
	echo "Test ${index} times, total relink ${total_linkup_cnt} times"
	echo "Average unit linkup time: ${avg_linkup_time}s"
	echo "Max unit link time: ${max_linkup_time}s, Min unit link time: ${min_linkup_time}s"
	echo ""
	echo "Link speed counter:"
	echo "  10G: ${link_10G_cnt} times"
	echo "  5G: ${link_5G_cnt} times"
	echo "  2.5G: ${link_2p5G_cnt} times"
	echo "  1G: ${link_1G_cnt} times"
	echo "  100M: ${link_100M_cnt} times"
	echo "  10M: ${link_10M_cnt} times"
	echo "Link mode counter(only for 1Gbps speed):"
	echo "  Master mode: ${link_master_cnt} times"
	echo "  Slave mode: ${link_slave_cnt} times"
	echo ""
	echo "Link up ok(without ping mode): ${linkup_ok_cnt} times"
	echo "Link up ok & ping ok: ${linkup_ok_ping_ok_cnt} times"
	echo "Link up ok & ping fail: ${linkup_ok_ping_fail_cnt} times"
	echo "Link up fail: ${total_linkup_fail_cnt} times"
	echo "###################################################"
}

dump_coeff() {
	idx=$1
	num=$2
	filename="Round${idx}_coeff_dump.log"
	for i in $(seq 1 ${num})
	do
		echo "Dump coeff ${i} times"
		log_params ${coeff_script} ${buffer_A} true
		cat ${buffer_A} >> ${final_log}
		#source ./coeff.sh --port=${port} >> ${filename}
	done
	mv ${final_log} ${filename}
}

check_link_status() {
	if [ ${pure_cl45} -eq 1 ]
	then
		cmd_gen 45 "read" 0x7 0x1 && ${CMD} > /dev/null
	else
		cmd_gen 22 "read" 0x1 && ${CMD} > /dev/null
	fi
	link_status=$(( ($(eval "${CMD}${response}") >> 2 ) & 0x1 ))
}

debug_point() {
	type=$1
	if [ ${debug_mode} -eq 0 ]; then
		return 0
	fi

	if [ ${pr_packet_cnt} -eq 1 ] && [ "${type}" = "packet_cnt" ]
	then
		cmd_gen 22 "write" 0x1f 0x1 && ${CMD} > /dev/null
		cmd_gen 22 "read" 0x12 && ${CMD}
		cmd_gen 22 "read" 0x17 && ${CMD}
		cmd_gen 22 "write" 0x1f 0x0 && ${CMD} > /dev/null
	fi

	if [ ${pr_DSPstate} -eq 1 ] && [ "${type}" = "poll DSPState" ]
	then
		poll_DSPState &
	fi
}

# Prepare
dmesg -c > /dev/null

for index in $(seq 1 ${round})
do
	echo "" > ${buffer_final} #Clear buffer_final at first
	test_index=${index}
	echo "*************Test Round $index starts***************"
	sleep 1
	if [ "${flow}" = "SLT" ]; then
		relay_board_on
		dmesg -c > /dev/null
	fi

	# Set bit12: enable master/slave manual configuration
	# Set bit11: 0->slave, 1->master
	# Dedicated master/slave mode is only available for phy_power_downup/phy_reAN an_types
	if [ "${mode}" = "auto" ]
	then
		cmd_gen 22 "write" 0x9 0x200 && ${CMD} > /dev/null
	elif [ "${mode}" = "force-master" ]\
		|| ([ "${mode}" = "round-robin" ] && [ ${RR_lock} -eq 1 ])
	then
		echo "Force master mode"
		if [ ${pure_cl45} -eq 1 ]
		then
			#Set bit15 to enable master/slave config, set bit14 to master mode
			cmd_gen 45 "write" 0x7 0x20 0xd1e1 && ${CMD} > /dev/null
		else
			cmd_gen 22 "write" 0x9 0x1a00 && ${CMD} > /dev/null #switch phy cl22 w ${port} 0x9 0x1a00
		fi
		RR_lock=2
	elif [ "${mode}" = "prefer-master" ]
	then
		echo "Prefer master mode"
		cmd_gen 22 "write" 0x9 0x1400 && ${CMD} > /dev/null #switch phy cl22 w ${port} 0x9 0x1a00
	elif ([ "${mode}" = "force-slave" ] || ([ "${mode}" = "round-robin" ] && [ ${RR_lock} -eq 2 ]) )\
		&& [ ! "${an_type}" = "phy_reset" ]
	then
		echo "Force slave mode"
		if [ ${pure_cl45} -eq 1 ]
		then
			#Set bit15 to enable master/slave config, clear bit14 to slave mode
			cmd_gen 45 "write" 0x7 0x20 0x91e1 && ${CMD} > /dev/null
		else
			cmd_gen 22 "write" 0x9 0x1200 && ${CMD} > /dev/null #switch phy cl22 w ${port} 0x9 0x1200
		fi
		RR_lock=1
	fi

	check_link_status
	#mii_mgr -s -p 0 -r 0x9 -v 0x1200, mii_mgr -g -p 0 -r 0x9
	#mii_mgr -s -p 0 -r 0x0 -v 0x1240
	if [ "${an_type}" = "phy_power_downup" ]
	then
		if [ ${pure_cl45} -eq 1 ]
		then
			#Set dev7, reg0[11] to enter low-power mode
			echo "Pure CL45 only supports Low Power mode, which won't shutdown phy."
			exit 1
		else
			cmd_gen 22 "write" 0x0 0x1840 && ${CMD}
		fi
		sleep ${interface_down_time}
		dmesg -c > /dev/null
		echo "${target_interface}: Link is Down" > /dev/kmsg
		cmd_gen 22 "write" 0x0 0x1040 && ${CMD}
	elif [ "${an_type}" = "phy_reset" ]
	then
		if [ "${link_status}" -eq 0 ]; then
			echo "${target_interface}: Link is Down" > /dev/kmsg
		fi
		if [ ${pure_cl45} -eq 1 ]
		then
			#Set dev7, reg0[15] to reset
			cmd_gen 45 "write" 0x7 0x0 0xb000 && ${CMD} > /dev/null
		else
			cmd_gen 22 "write" 0x0 0x9040 && ${CMD}
		fi
		if [ "${mode}" = "force-slave" ]; then
			echo "Force slave mode"
			if [ ${pure_cl45} -eq 1 ]
			then
				#Set bit15 to enable master/slave config, clear bit14 to slave mode
				cmd_gen 45 "write" 0x7 0x20 0x91e1 && ${CMD} > /dev/null
			else
				cmd_gen 22 "write" 0x9 0x1200 && ${CMD} > /dev/null #switch phy cl22 w ${port} 0x9 0x1200
			fi
		fi
	elif [ "${an_type}" = "ifconfig" ]
	then
		ifconfig ${target_interface} down
		sleep ${interface_down_time}
		dmesg -c > /dev/null
		echo "${target_interface}: Link is Down" > /dev/kmsg
		ifconfig ${target_interface} up
	elif [ "${an_type}" = "manual_plugin" ]
	then
		echo "Sleep for ${interface_down_time}s, plug in your cable..."
		sleep ${interface_down_time}
		if [ "${link_status}" -eq 0 ]; then
			echo "${target_interface}: Link is Down" > /dev/kmsg
		fi
	else
		## default case: phy_reAN
		if [ "${link_status}" -eq 0 ]; then
			echo "${target_interface}: Link is Down" > /dev/kmsg
		fi
		echo "Start re-AN"
		if [ ${pure_cl45} -eq 1 ]
		then
			#Set dev7, reg0[9] to start re-AN
			cmd_gen 45 "write" 0x7 0x0 0x3200 && ${CMD} > /dev/null
		else
			cmd_gen 22 "write" 0x0 0x1240 && ${CMD} > /dev/null #switch phy cl22 w ${port} 0x0 0x1240
		fi
	fi

	if [ "${flow}" = "IoT_exam2" ]
	then
		dmesg -c >> ${buffer_final}
		wait_DSP_ready
		start_log_table2
	elif [ "${flow}" = "dump_coeff" ]
	then
		until [ `dmesg -c | tee -a ${buffer_final} | grep -m 1 "Link is Up" | wc -l` -gt 0 ]
		do
			:
		done
		sleep 1
		dump_coeff ${index} 10
	elif [ "${flow}" = "pause_after_linkup" ]
	then
		until [ `dmesg -c | tee -a ${buffer_final} | grep -m 1 "Link is Up" | wc -l` -gt 0 ]
		do
			:
		done
		elapse=0
		while [ ${elapse} -lt ${delay} ]
		do
			check_target_link_mode
			sleep 1
			let "elapse++"
		done
		printf "\n"
		dmesg -c >> ${buffer_final}
	else
		elapse=0
		while [ ${elapse} -lt ${delay} ]
		do
			check_target_link_mode
			debug_point "poll DSPState"
			sleep 1
			let "elapse++"
		done
		printf "\n"
		dmesg -c >> ${buffer_final}
	fi

	linkup_cnt=`cat ${buffer_final} | grep "Link is Up" | wc -l`
	linkdown_cnt=`cat ${buffer_final} | grep "Link is Down" | wc -l`
	if [ `cat ${buffer_final} | grep "Link is Up" | wc -l` -gt 0 ] &&
		[ ${linkup_cnt} -ge ${linkdown_cnt} ]
	then
		link_speed=`cat ${buffer_final} | grep "Link is Up" | grep -E -o "([0-9]+.[0-9]+[GM]bps)|([0-9]+[GM]bps)" | tail -1`
		if [ "${link_speed}" = "10Gbps" ]
		then
			link_10G_cnt=$(( ${link_10G_cnt}+1 ))
		elif [ "${link_speed}" = "5Gbps" ]
		then
			link_5G_cnt=$(( ${link_5G_cnt}+1 ))
		elif [ "${link_speed}" = "2.5Gbps" ]
		then
			link_2p5G_cnt=$(( ${link_2p5G_cnt}+1 ))
		elif [ "${link_speed}" = "1Gbps" ]
		then
			link_1G_cnt=$(( ${link_1G_cnt}+1 ))
		elif [ "${link_speed}" = "100Mbps" ]
		then
			link_100M_cnt=$(( ${link_100M_cnt}+1 ))
		elif [ "${link_speed}" = "10Mbps" ]
		then
			link_10M_cnt=$(( ${link_10M_cnt}+1 ))
		fi

		if [ "${flow}" = "IoT_exam1" ]; then
			echo "After linkup:" >> ${final_log}
			log_params ${log_script_1} ${after_linkup_log} "true"
			cat ${after_linkup_log} >> ${final_log}
		fi

		if [ ${packet_num} -gt 0 ]
		then
			debug_point "packet_cnt"
			ping ${dst_ip} -c ${packet_num} | tee -a ${buffer_final}
			printf "\n"
			debug_point "packet_cnt"
		fi

		if [ ${packet_num} -eq 0 ]
		then
			test_result="linkup_ok"
			let "linkup_ok_cnt++"
		elif [ `cat ${buffer_final} | grep "${packet_num} packets received" | wc -l` -gt 0 ]
		then
			test_result="linkup_ok_ping_ok"
			let "linkup_ok_ping_ok_cnt++"

			########################################################################
			# Log params if ping is successful
			if [ "${flow}" = "IoT_exam1" ]; then
				packet_good_round_cnt=$(( ${packet_good_round_cnt}+1 ))
				if [ ${packet_good_round_cnt} -eq 0 ]; then
					echo "Ping is good:" >> ${final_log}
					start=$(date "+%s")
					second_cnt=${start}
					echo "Start dumpping FFE, DFE, EC, TREC, MSE, VGA params for 60s."
					while
						log_params ${log_script_1} ${packet_good_log} "true"
						cat ${packet_good_log} >> ${final_log}
						end=$(date "+%s")
						counter=$((${end}-${second_cnt}))
						if [ ${counter} -ge 5 ]; then
							echo "Dump log, timestamp: ${end}s"
							second_cnt=${end}
						fi
						duration=$(( ${end} - ${start} ))
						[ ${duration} -le ${log_duration} ]
					do :; done
				fi
			fi
			########################################################################
		else
			test_result="linkup_ok_ping_fail"
			let "linkup_ok_ping_fail_cnt++"

			########################################################################
			if [ "${flow}" = "IoT_exam1" ]; then
				packet_loss_round_cnt=$(( ${packet_loss_round_cnt}+1 ))
				total_packet=`cat ${buffer_final} |
					grep -E -o "[0-9]+ packets transmitted" | grep -E -o "[0-9]+"`
				receive_packet=`cat ${buffer_final} |
					grep -E -o "[0-9]+ packets received" | grep -E -o "[0-9]+"`
				packet_loss_num=$(( ${total_packet} - ${receive_packet} ))
				if [ ${packet_loss_num} -ge 5 ]; then
					echo "We lost more than 5 packets:" >> ${final_log}
					start=$(date "+%s")
					second_cnt=${start}
					echo "Start dumpping FFE, DFE, EC, TREC, MSE, VGA params for 60s."
					while
						log_params ${log_script_1} ${packet_loss_log} "true"
						cat ${packet_loss_log} >> ${final_log}
						end=$(date "+%s")
						counter=$((${end}-${second_cnt}))
						if [ ${counter} -ge 5 ]; then
							echo "Dump log, timestamp: ${end}s"
							second_cnt=${end}
						fi
						duration=$(( ${end} - ${start} ))
						[ ${duration} -le ${log_duration} ]
					do :; done
				fi
			fi
			########################################################################
			#echo "Index[$index]: Test Fail" >> /tmp/result.txt
			#exit 1
		fi

		first_down=`cat ${buffer_final} | grep "Link is Down" |
					head -1 | grep -E -o '[0-9]+.[0-9]+]' | sed 's/\]//'`
		last_up=`cat ${buffer_final} | grep "Link is Up" |
					tail -1 | grep -E -o '[0-9]+.[0-9]+]' | sed 's/\]//'`
		firstdown_to_lastup=`echo "${last_up} ${first_down}" | awk {'print $1-$2'}`
		total_firstdown_to_lastup=`echo "${total_firstdown_to_lastup} ${firstdown_to_lastup}" |
									awk {'print $1+$2'}`
		avg_firstdown_to_lastup=`echo "${total_firstdown_to_lastup} ${index}" |
									awk {'print $1/$2'}`

		T1=`printf "%.4f" ${firstdown_to_lastup} | tr -d "."`
		T_5s=`printf "%.4f" 5 | tr -d "."`
		T_10s=`printf "%.4f" 10 | tr -d "."`
		if [ "${flow}" = "IoT_exam2" ]
		then
			if [ $T1 -lt $T_5s ]
			then
				mv ${final_log} "round${index}_linkup_${firstdown_to_lastup}s.log"
				linkup_lt5s_round_cnt=$(( ${linkup_lt5s_round_cnt}+1 ))
			elif [ $T1 -gt $T_10s ]
			then
				mv ${final_log} "round${index}_linkup_${firstdown_to_lastup}s.log"
				linkup_gt10s_round_cnt=$(( ${linkup_gt10s_round_cnt}+1 ))
			else
				rm ${final_log}
			fi
		fi

		if [ "${link_speed}" = "1Gbps" ] || [ "${link_speed}" = "2.5Gbps" ] || [ "${link_speed}" = "10Gbps" ] ; then
			check_link_mode
		fi

		occur=`cat ${buffer_final} | grep "Link is Down" | wc -l`
		#on reijie board:
		#occur=`cat ${buffer_final} | grep "Link is Down" | grep "15100000.ethernet" | wc -l`
		if [ ${linkup_cnt} -eq ${linkdown_cnt} ] #-lt case won't enter here
		then
			round_linkup_cnt=0
			for i in $(seq 1 ${occur})
			do
				down=`cat ${buffer_final} | grep "Link is Down" |
						head -${i} | tail -1 | grep -E -o '[0-9]+.[0-9]+]' | sed 's/\]//'`
				up=`cat ${buffer_final} | grep "Link is Up" |
						head -${i} | tail -1 | grep -E -o '[0-9]+.[0-9]+]' | sed 's/\]//'`
				down_to_up=`echo "${up} ${down}" | awk {'print $1-$2'}`
				echo "${i} linkup time: ${down_to_up}s"
				total_linkup_time=`echo "${total_linkup_time} ${down_to_up}" | awk {'print $1+$2'}`
				let "round_linkup_cnt++"
				total_linkup_cnt=$(( ${total_linkup_cnt}+1 ))
				avg_linkup_time=`echo "${total_linkup_time} ${total_linkup_cnt}" | awk {'print $1/$2'}`

				T1=`printf "%.4f" ${down_to_up} | tr -d "."`
				T2=`printf "%.4f" ${max_linkup_time} | tr -d "."`
				T3=`printf "%.4f" ${min_linkup_time} | tr -d "."`
				if [ $T1 -gt $T2 ] ; then max_linkup_time=${down_to_up}; fi
				if [ $T1 -lt $T3 ] ; then min_linkup_time=${down_to_up}; fi
			done
		fi
	else
		test_result="linkup_fail"
		let "total_linkup_fail_cnt++"
		res_failed=`cat ${buffer_final} | grep "Master/Slave resolution failed" | wc -l`
		if [ ${res_failed} -gt 0 ]; then
			echo "Abort test due to resolution failure."
			exit 1
		fi
		#echo "##################### Summary #####################"
		#echo "Link-up Fail"
		#echo "Index[$index]: Link-up Fail" >> /tmp/result.txt
		#exit 1
	fi
	print_summary
	check_end_condition ${end_condition}
done
