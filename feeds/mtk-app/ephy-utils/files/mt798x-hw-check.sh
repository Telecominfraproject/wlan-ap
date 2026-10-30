source lib.sh

if [ -z ${port} ]
then
	echo "Port not specified."
else
	echo "Port: ${port}"
fi

detect_platform

if [ "${platform}" = "mt7981" ]
then
	gphy_efuse0=`regs d 0x11f208dc | head -1 | awk {'print $5'}`
	gphy_efuse1=`regs d 0x11f208e0 | head -1 | awk {'print $5'}`
	gphy_efuse2=`regs d 0x11f208e4 | head -1 | awk {'print $5'}`
	gphy_efuse3=`regs d 0x11f208e8 | head -1 | awk {'print $5'}`
	TEST_CMD="mii"
elif [ "${platform}" = "mt7988" ]
then
	TEST_CMD="switch"
	if [ ${port} -eq 0 ]
	then
		gphy_efuse0=`regs d 0x11f50940 | head -1 | awk {'print $5'}`
		gphy_efuse1=`regs d 0x11f50944 | head -1 | awk {'print $5'}`
		gphy_efuse2=`regs d 0x11f50948 | head -1 | awk {'print $5'}`
		gphy_efuse3=`regs d 0x11f5094c | head -1 | awk {'print $5'}`
	elif [ ${port} -eq 1 ]
	then
		gphy_efuse0=`regs d 0x11f50954 | head -1 | awk {'print $5'}`
		gphy_efuse1=`regs d 0x11f50958 | head -1 | awk {'print $5'}`
		gphy_efuse2=`regs d 0x11f5095c | head -1 | awk {'print $5'}`
		gphy_efuse3=`regs d 0x11f50960 | head -1 | awk {'print $5'}`
	elif [ ${port} -eq 2 ]
	then
		gphy_efuse0=`regs d 0x11f50968 | head -1 | awk {'print $5'}`
		gphy_efuse1=`regs d 0x11f5096c | head -1 | awk {'print $5'}`
		gphy_efuse2=`regs d 0x11f50970 | head -1 | awk {'print $5'}`
		gphy_efuse3=`regs d 0x11f50974 | head -1 | awk {'print $5'}`
	elif [ ${port} -eq 3 ]
	then
		gphy_efuse0=`regs d 0x11f5097c | head -1 | awk {'print $5'}`
		gphy_efuse1=`regs d 0x11f50980 | head -1 | awk {'print $5'}`
		gphy_efuse2=`regs d 0x11f50984 | head -1 | awk {'print $5'}`
		gphy_efuse3=`regs d 0x11f50988 | head -1 | awk {'print $5'}`
	fi
fi

# EFS_DA_TX_I2MPB_A = $(( ${gphy_efuse0} & 0x3f )) , EFS_DA_TX_I2MPB_B, EFS_DA_TX_I2MPB_C, EFS_DA_TX_I2MPB_D
# tx_amp_cal_efuse
gphy_tx_amp_pairA=$(( ${gphy_efuse0} & 0x3f ))
gphy_tx_amp_pairB=$(( (${gphy_efuse0} >> 6) & 0x3f ))
gphy_tx_amp_pairC=$(( (${gphy_efuse0} >> 12) & 0x3f ))
gphy_tx_amp_pairD=$(( (${gphy_efuse0} >> 18) & 0x3f ))


# tx_r50_cal_efuse
gphy_r50_pairA=$(( (${gphy_efuse1} >> 18) & 0x3f ))
gphy_r50_pairB=$(( (${gphy_efuse1} >> 24) & 0x3f ))
gphy_r50_pairC=$(( (${gphy_efuse2}) & 0x3f ))
gphy_r50_pairD=$(( (${gphy_efuse2} >> 6) & 0x3f ))

check_value() {
	val=`printf "0x%04x" $1`
	expect=`printf "0x%04x" $2`
	if [ ! "${val}" = "${expect}" ]
	then
		echo "Value check fails!!"
		echo "${CMD}"
		echo "Read value(${val}) != Expect value(${expect})"
	else
		echo "Value check passes!!"
	fi
}

check_tr_value() {
	expect_high=`printf "0x%04x" $1`
	expect_low=`printf "0x%04x" $2`
	tr_high_val=`printf "0x%04x" ${tr_high_val}`
	tr_low_val=`printf "0x%04x" ${tr_low_val}`
	if [ ! "${tr_high_val}" = "${expect_high}" ]; then
		echo "High value check fails!!"
		echo "Read high value(${tr_high_val}) != Expect high value(${expect_high})"
	else
		echo "High value check passes!!"
	fi
	if [ ! "${tr_low_val}" = "${expect_low}" ]; then
		echo "Low value check fails!!"
		echo "Read low value(${tr_low_val}) != Expect low value(${expect_low})"
	else
		echo "Low value check passes!!"
	fi
	echo ""
}

check_mt798x_phy_common_finetune() {
	printf "# SlvDSPreadyTime = 24, MasDSPreadyTime = 24\n"
	tr_access "read" 0x1 0xf 0x17
	check_tr_value "0x000c" "0x0c71"

	printf "# EnabRandUpdTrig = 1\n"
	tr_access "read" 0x1 0xf 0x18
	check_tr_value "0x000e" "0x2f00"

	printf "\n# NormMseLoThresh = 8'd85\n"
	tr_access "read" 0x0 0x7 0x15
	check_tr_value "0x0000" "0x55a0"

	printf "# FfeUpdGainForce = 1(Enable), FfeUpdGainForceVal = 4\n"
	tr_access "read" 0x2 0xd 0x00
	check_tr_value "0x0000" "0x0240"

	printf "\n# TrFreezeForce 0\n"
	tr_access "read" 0x2 0xd 0x3
	check_tr_value "0x0000" "0x0000"

	printf "\n# DSP, SSTr\n"
	tr_access "read" 0x2 0xd 0x6
	check_tr_value "0x002e" "0xbaef"
}

check_mt798x_phy_finetune() {
	if [ "${platform}" = "mt7981" ]
	then
		printf "\n# 100M eye finetune\n"
		cmd_gen 45 "read" 0x1e 0x1
		val=$(eval "${CMD}${response}")
		check_value "${val}" "0x01ce"
		cmd_gen 45 "read" 0x1e 0x2
		val=$(eval "${CMD}${response}")
		check_value "${val}" "0x01c1"
		cmd_gen 45 "read" 0x1e 0x4
		val=$(eval "${CMD}${response}")
		check_value "${val}" "0x020f"
		cmd_gen 45 "read" 0x1e 0x5
		val=$(eval "${CMD}${response}")
		check_value "${val}" "0x0202"
		cmd_gen 45 "read" 0x1e 0x7
		val=$(eval "${CMD}${response}")
		check_value "${val}" "0x03d0"
		cmd_gen 45 "read" 0x1e 0x8
		val=$(eval "${CMD}${response}")
		check_value "${val}" "0x03c0"
		cmd_gen 45 "read" 0x1e 0xa
		val=$(eval "${CMD}${response}")
		check_value "${val}" "0x0013"
		cmd_gen 45 "read" 0x1e 0xb
		val=$(eval "${CMD}${response}")
		check_value "${val}" "0x0005"

		printf "\n# ResetSyncOffset = 6\n"
		tr_access "read" 0x1 0xf 0x20
		check_tr_value "0x0000" "0x0600"

		printf "\n# VgaDecRate = 1\n"
		tr_access "read" 0x1 0xf 0x12
		check_tr_value "0x003e" "0x4c2a"

		printf "\n# PMA, MrvlTrFix100Kp=3, MrvlTrFix100Kf=2, MrvlTrFix1000Kp=3, MrvlTrFix1000Kf = 2\n"
		tr_access "read" 0x1 0xf 0x1
		check_tr_value "0x34" "0xd10a"

		printf "\n# VcoSlicerThreshBitsHigh = 1\n"
		tr_access "read" 0x1 0xd 0x20
		check_tr_value "0x0055" "0x5555"

		printf "\n# TR_OPEN_LOOP_EN=1, lpf_x_average = 9\n"
		cmd_gen 45 "read" 0x1e 0x234
		val=$(eval "${CMD}${response}")
		check_value "${val}" "0x0191"

		printf "\n# rg_tr_lpf_cnt_val = 512\n"
		cmd_gen 45 "read" 0x1e 0x235
		val=$(eval "${CMD}${response}")
		check_value "${val}" "0x200"

		printf "\n# IIR2 related\n"
		cmd_gen 45 "read" 0x1e 0x22a
		val=$(eval "${CMD}${response}")
		check_value "${val}" "0x0082"
		cmd_gen 45 "read" 0x1e 0x22b
		val=$(eval "${CMD}${response}")
		check_value "${val}" "0x0000"
		cmd_gen 45 "read" 0x1e 0x22c
		val=$(eval "${CMD}${response}")
		check_value "${val}" "0x0103"
		cmd_gen 45 "read" 0x1e 0x22d
		val=$(eval "${CMD}${response}")
		check_value "${val}" "0x0000"
		cmd_gen 45 "read" 0x1e 0x22e
		val=$(eval "${CMD}${response}")
		check_value "${val}" "0x0082"
		cmd_gen 45 "read" 0x1e 0x22f
		val=$(eval "${CMD}${response}")
		check_value "${val}" "0x0000"
		cmd_gen 45 "read" 0x1e 0x230
		val=$(eval "${CMD}${response}")
		check_value "${val}" "0xd177"
		cmd_gen 45 "read" 0x1e 0x231
		val=$(eval "${CMD}${response}")
		check_value "${val}" "0x0003"
		cmd_gen 45 "read" 0x1e 0x232
		val=$(eval "${CMD}${response}")
		check_value "${val}" "0x2c82"
		cmd_gen 45 "read" 0x1e 0x233
		val=$(eval "${CMD}${response}")
		check_value "${val}" "0x000e"

		printf "\n# FFE peaking\n"
		cmd_gen 45 "read" 0x1e 0x27c
		val=$(eval "${CMD}${response}")
		check_value "${val}" "0x1b18"
		cmd_gen 45 "read" 0x1e 0x27d
		val=$(eval "${CMD}${response}")
		check_value "${val}" "0x011e"

		printf "\n# Disable LDO pump\n"
		cmd_gen 45 "read" 0x1e 0x502
		val=$(eval "${CMD}${response}")
		check_value "${val}" "0x0000"
		cmd_gen 45 "read" 0x1e 0x503
		val=$(eval "${CMD}${response}")
		check_value "${val}" "0x0000"

		printf "\n# Adjust LDO output level\n"
		cmd_gen 45 "read" 0x1e 0xd7
		val=$(eval "${CMD}${response}")
		check_value "${val}" "0x2222"

		r50_pairA_bias=0
		printf "\n# R50 = efuse_R50_pariA_value %+d\n" ${r50_pairA_bias}
		cmd_gen 45 "read" 0x1e 0x53d
		val=$(eval "${CMD}${response}")
		target_val=$(( ((${gphy_r50_pairA} + ${r50_pairA_bias}) << 8) |
						(${gphy_r50_pairA} + ${r50_pairA_bias}) ))
		check_value "${val}" "${target_val}"

		r50_pairB_bias=0
		printf "\n# R50 = efuse_R50_pariB_value %+d\n" ${r50_pairB_bias}
		cmd_gen 45 "read" 0x1e 0x53e
		val=$(eval "${CMD}${response}")
		target_val=$(( ((${gphy_r50_pairB} + ${r50_pairB_bias}) << 8) |
						(${gphy_r50_pairB} + ${r50_pairB_bias}) ))
		check_value "${val}" "${target_val}"

		r50_pairC_bias=0
		printf "\n# R50 = efuse_R50_pariC_value %+d\n" ${r50_pairC_bias}
		cmd_gen 45 "read" 0x1e 0x53f
		val=$(eval "${CMD}${response}")
		target_val=$(( ((${gphy_r50_pairC} + ${r50_pairC_bias}) << 8) |
						(${gphy_r50_pairC} + ${r50_pairC_bias}) ))
		check_value "${val}" "${target_val}"

		r50_pairD_bias=0
		printf "\n# R50 = efuse_R50_pariC_value %+d\n" ${r50_pairD_bias}
		cmd_gen 45 "read" 0x1e 0x540
		val=$(eval "${CMD}${response}")
		target_val=$(( ((${gphy_r50_pairD} + ${r50_pairD_bias}) << 8) |
						(${gphy_r50_pairD} + ${r50_pairD_bias}) ))
		check_value "${val}" "${target_val}"

	elif [ "${platform}" = "mt7988" ]
	then
		printf "\n# Set default MLT3 shaper\n"
		cmd_gen 45 "read" 0x1e 0x0
		val=$(eval "${CMD}${response}")
		check_value "${val}" "0x0187"
		cmd_gen 45 "read" 0x1e 0x1
		val=$(eval "${CMD}${response}")
		check_value "${val}" "0x01cd"
		cmd_gen 45 "read" 0x1e 0x2
		val=$(eval "${CMD}${response}")
		check_value "${val}" "0x01c8"
		cmd_gen 45 "read" 0x1e 0x3
		val=$(eval "${CMD}${response}")
		check_value "${val}" "0x0182"
		cmd_gen 45 "read" 0x1e 0x4
		val=$(eval "${CMD}${response}")
		check_value "${val}" "0x020d"
		cmd_gen 45 "read" 0x1e 0x5
		val=$(eval "${CMD}${response}")
		check_value "${val}" "0x0206"
		cmd_gen 45 "read" 0x1e 0x6
		val=$(eval "${CMD}${response}")
		check_value "${val}" "0x0384"
		cmd_gen 45 "read" 0x1e 0x7
		val=$(eval "${CMD}${response}")
		check_value "${val}" "0x03d0"
		cmd_gen 45 "read" 0x1e 0x8
		val=$(eval "${CMD}${response}")
		check_value "${val}" "0x03c6"
		cmd_gen 45 "read" 0x1e 0x9
		val=$(eval "${CMD}${response}")
		check_value "${val}" "0x030a"
		cmd_gen 45 "read" 0x1e 0xa
		val=$(eval "${CMD}${response}")
		check_value "${val}" "0x0011"
		cmd_gen 45 "read" 0x1e 0xb
		val=$(eval "${CMD}${response}")
		check_value "${val}" "0x0005"

		printf "\n# TCT finetune\n"
		cmd_gen 45 "read" 0x1e 0xfe
		val=$(eval "${CMD}${response}")
		check_value "${val}" "0x5"

		printf "\n# Disable TX power saving\n"
		cmd_gen 45 "read" 0x1e 0xc6
		val=$(eval "${CMD}${response}")
		check_value "${val}" "0x53aa"

		printf "\n# ResetSyncOffset = 5\n"
		tr_access "read" 0x1 0xf 0x20
		check_tr_value "0x0000" "0x0500"

		# These are default values. Do not need to be written in PHY driver.
		printf "\n# VgaDecRate = 1(default value)\n"
		tr_access "read" 0x1 0xf 0x12
		check_tr_value "0x003e" "0x4d2a"

		printf "\n# PMA, MrvlTrFix100Kp=6, MrvlTrFix100Kf=7, MrvlTrFix1000Kp=6, MrvlTrFix1000Kf = 7\n"
		tr_access "read" 0x1 0xf 0x1
		check_tr_value "0x006f" "0xb90a"

		printf "\n# RemAckCntLimitCtrl = 1\n"
		tr_access "read" 0x0 0xf 0x3c
		check_tr_value "0x00c3" "0xfbba"

		printf "\n# TR_OPEN_LOOP_EN=1, lpf_x_average = 10\n"
		cmd_gen 45 "read" 0x1e 0x234
		val=$(eval "${CMD}${response}")
		check_value "${val}" "0x01a1"

		printf "\n# rg_tr_lpf_cnt_val = 1023\n"
		cmd_gen 45 "read" 0x1e 0x235
		val=$(eval "${CMD}${response}")
		check_value "${val}" "0x3ff"

		r50_pairA_bias=-1
		printf "\n# R50 = efuse_R50_pariA_value %+d\n" ${r50_pairA_bias}
		cmd_gen 45 "read" 0x1e 0x53d
		val=$(eval "${CMD}${response}")
		target_val=$(( ((${gphy_r50_pairA} + ${r50_pairA_bias}) << 8) |
						(${gphy_r50_pairA} + ${r50_pairA_bias}) ))
		check_value "${val}" "${target_val}"

		r50_pairB_bias=-1
		printf "\n# R50 = efuse_R50_pariB_value %+d\n" ${r50_pairB_bias}
		cmd_gen 45 "read" 0x1e 0x53e
		val=$(eval "${CMD}${response}")
		target_val=$(( ((${gphy_r50_pairB} + ${r50_pairB_bias}) << 8) |
						(${gphy_r50_pairB} + ${r50_pairB_bias}) ))
		check_value "${val}" "${target_val}"

		r50_pairC_bias=-1
		printf "\n# R50 = efuse_R50_pariC_value %+d\n" ${r50_pairC_bias}
		cmd_gen 45 "read" 0x1e 0x53f
		val=$(eval "${CMD}${response}")
		target_val=$(( ((${gphy_r50_pairC} + ${r50_pairC_bias}) << 8) |
						(${gphy_r50_pairC} + ${r50_pairC_bias}) ))
		check_value "${val}" "${target_val}"

		r50_pairD_bias=-1
		printf "\n# R50 = efuse_R50_pariC_value %+d\n" ${r50_pairD_bias}
		cmd_gen 45 "read" 0x1e 0x540
		val=$(eval "${CMD}${response}")
		target_val=$(( ((${gphy_r50_pairD} + ${r50_pairD_bias}) << 8) |
						(${gphy_r50_pairD} + ${r50_pairD_bias}) ))
		check_value "${val}" "${target_val}"
	fi
}

check_mt798x_phy_common_finetune
check_mt798x_phy_finetune
