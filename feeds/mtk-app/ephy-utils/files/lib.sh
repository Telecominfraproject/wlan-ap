
for i in "$@"; do
	case $i in
	-a=* | --an_type=* )
		an_type="${i#*=}"
		shift # past argument=value
		;;
	-s | --switch )
		TEST_CMD="switch"
		shift # past argument=value
		;;
	-p=* | --port=* )
		port="${i#*=}"
		port=`printf "0x%x" ${port}`
		port_decimal=`printf "%d" ${port}`
		shift # past argument=value
		;;
	-d=* | --debug )
		debug_mode=1
		shift # past argument=value
		;;
	--packet=* )
		packet_num="${i#*=}"
		shift # past argument=value
		;;
	-t=* | --delay=* )
		delay="${i#*=}"
		shift # past argument=value
		;;
	-i=* | --dst_ip=* )
		dst_ip="${i#*=}"
		shift # past argument=value
		;;
	-r=* | --round=* )
		round="${i#*=}"
		shift # past argument=value
		;;
	-l=* | --log_duration=* )
		log_duration="${i#*=}"
		shift # past argument=value
		;;
	-f=* | --flow=* )
		flow="${i#*=}"
		debug_mode=1
		shift # past argument=value
		;;
	-m=* | --mode=* )
		mode="${i#*=}"
		shift # past argument=value
		;;
	-e=* | --end_condition=* )
		end_condition="${i#*=}"
		shift # past argument=value
		;;
	--r50_all=* )
		r50_all_bias="${i#*=}"
		shift # past argument=value
		;;
	--tclkoffset=* )
		tclkoffset="${i#*=}"
		shift # past argument=value
		;;
	-cl45 | --pure_cl45 )
		pure_cl45=1
		shift # past argument=value
		;;
	-v | --version )
		echo "Version: ${version}"
		exit 0
		;;

	#-s=*|--searchpath=*)
	#        SEARCHPATH="${i#*=}"
	#        shift # past argument=value
	#        ;;
	--default)
		DEFAULT=YES
		shift # past argument with no value
		;;
	-*|--*)
		echo "Unknown option $i"
		exit 1
		;;
	*)
		;;
	esac
done

cmd_gen() {
	_cl=$1
	_direction=$2

	## Initialize values
	_dev_num=""
	_reg_num=""

	#### Error handling here.........####
	if [ "${_cl}" -ne 22 ] && [ "${_cl}" -ne 45 ]; then
			echo "command is neither cl22 nor cl45"
			return 1
	fi
	#####################################

	# For $1 & $2 here, backslash is necessary so we can correctly use pipeline vars.
	if [ "${TEST_CMD}" = "switch" ] && [ "${_cl}" -eq 22 ]
	then
		CMD="switch phy cl22 "
		response=" | awk -F'value=' '{print \$2}' | awk '{print \$1}'"
	elif [ "${TEST_CMD}" = "switch" ] && [ "${_cl}" -eq 45 ]
	then
		CMD="switch phy cl45 "
		response=" | awk -F'value=' '{print \$2}' | awk '{print \$1}'"
	elif [ "${TEST_CMD}" = "mii" ] && [ "${_cl}" -eq 22 ]
	then
		CMD="mii_mgr "
		response=" | awk {'print \$4'} | awk '{print \"0x\"\$0}'" ## Append 0x in front
	elif [ "${TEST_CMD}" = "mii" ] && [ "${_cl}" -eq 45 ]
	then
		CMD="mii_mgr_cl45 "
		response=" | awk {'print \$5'}"
	fi

	## Determine read or write
	if [ "${TEST_CMD}" = "switch" ] && [ "${_direction}" = "read" ]
	then
			dir="r "${port}" "
	elif [ "${TEST_CMD}" = "switch" ] && [ "${_direction}" = "write" ]
	then
			response=""
			dir="w "${port}" "
	elif [ "${TEST_CMD}" = "mii" ] && [ "${_direction}" = "read" ]
	then
			dir="-g -p "${port}" "
	elif [ "${TEST_CMD}" = "mii" ] && [ "${_direction}" = "write" ]
	then
			response=""
			dir="-s -p "${port}" "
	fi
	CMD=${CMD}${dir}

	if [ "${TEST_CMD}" = "mii" ]; then
			_dev_num=" -d "
			_reg_num=" -r "
	fi
	if [ "${_cl}" -eq 22 ]
	then
			_dev_num=""
			_reg_num=${_reg_num}$3
	elif [ "${_cl}" -eq 45 ]
	then
			_dev_num=${_dev_num}$3" "
			_reg_num=${_reg_num}$4
	fi
	CMD=${CMD}${_dev_num}${_reg_num}

	if [ "${_direction}" = "write" ]; then
			if [ "${_cl}" -eq 22 ]
			then
					_val=$4
			elif [ "${_cl}" -eq 45 ]
			then
					_val=$5
			fi
			if [ "${TEST_CMD}" = "switch" ]
			then
					CMD=${CMD}" "${_val}
			elif [ "${TEST_CMD}" = "mii" ]
			then
					CMD=${CMD}" -v "${_val}
			fi
	fi
	#echo "[Debug]"${CMD}${response}
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

	tr_access_val=$(( (${PKT_XMT_STA} << 15) |
				((${WR_RD_CTRL} & 0x1 ) << 13) |
				((${CH_ADDR} & 0x3) << 11) |
				((${NODE_ADDR} & 0xf) << 7) |
				((${DATA_ADDR} & 0x3f) << 1) ))
	tr_access_val=`printf "0x%x" ${tr_access_val}`

	#echo ${tr_access}
	if [ "$1" = "read" ]
	then
		mii_mgr -s -p ${port} -r 0x10 -v ${tr_access_val} > /dev/null
		tr_high_val=0x`mii_mgr -g -p ${port} -r 0x12 | grep -E -o " = [0-9A-Fa-f]+" | grep -E -o "[0-9A-Fa-f]+"` > /dev/null
		tr_low_val=0x`mii_mgr -g -p ${port} -r 0x11 | grep -E -o " = [0-9A-Fa-f]+" | grep -E -o "[0-9A-Fa-f]+"`  > /dev/null
	elif [ "$1" = "write" ]
	then
		mii_mgr -s -p ${port} -r 0x12 -v ${high_value}
		mii_mgr -s -p ${port} -r 0x11 -v ${low_value}
		mii_mgr -s -p ${port} -r 0x10 -v ${tr_access_val}
		tr_high_val=${high_value}
		tr_low_val=${low_value}
	fi

	tr_output=`printf "port=%x, ch_addr=%x, node_addr=%x, data_addr=%x\nswitch trreg read tr_reg_control=%x, value_H=%x, value_L=%x"\
		${port} ${CH_ADDR} ${NODE_ADDR} ${DATA_ADDR} ${tr_access_val} ${tr_high_val} ${tr_low_val}`

	#echo "port=${port}, ch_addr=${CH_ADDR}, node_addr=${NODE_ADDR}, data_addr=${DATA_ADDR}"
	#echo "tr_reg_control=${tr_access_val}, value_H=${tr_high_val}, value_L=${tr_low_val}"

	mii_mgr -s -p ${port} -r 0x1f -v 0x0 > /dev/null
}

tr_access() {
	rw=$1
	CH_ADDR=$2 #[12:11]
	NODE_ADDR=$3 #[10:7]
	DATA_ADDR=$4 #[6:1]
	# bit0 is reserved
	high_value=$5
	low_value=$6

	if [ "${TEST_CMD}" = "mii" ] && [ "${rw}" = "read" ]
	then
		mii_mgr_tr_access "read" ${CH_ADDR} ${NODE_ADDR} ${DATA_ADDR}
	elif [ "${TEST_CMD}" = "mii" ] && [ "${rw}" = "write" ]
	then
		mii_mgr_tr_access "write" ${CH_ADDR} ${NODE_ADDR} ${DATA_ADDR} ${high_value} ${low_value}
	elif [ "${TEST_CMD}" = "switch" ] && [ "${rw}" = "read" ]
	then
		switch phy cl22 w ${port} 0x1f 0x52b5 > /dev/null
		tr_output=`switch trreg r ${port} ${CH_ADDR} ${NODE_ADDR} ${DATA_ADDR}`
		tr_high_val=0x`echo ${tr_output} | grep -E -o "value_H=[0-9A-Fa-f]+" | sed 's/value_H=//g' `
		tr_low_val=0x`echo ${tr_output} | grep -E -o "value_L=[0-9A-Fa-f]+" | sed 's/value_L=//g' `
		switch phy cl22 w ${port} 0x1f 0x0 > /dev/null
	elif [ "${TEST_CMD}" = "switch" ] && [ "${rw}" = "write" ]
	then
		switch phy cl22 w ${port} 0x1f 0x52b5 > /dev/null
		tr_output=`switch trreg w ${port} ${CH_ADDR} ${NODE_ADDR} ${DATA_ADDR} ${high_value} ${low_value}`
		switch phy cl22 w ${port} 0x1f 0x0 > /dev/null
	fi
}

detect_platform() {
	platform=`cat /proc/device-tree/compatible | grep -E -o 'mt[0-9]+' | head -1`
	#echo "Platform: ${platform}"
}
