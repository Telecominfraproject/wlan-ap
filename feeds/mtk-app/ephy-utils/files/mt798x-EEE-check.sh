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
	#gphy_efuse0=`regs d 0x11f208dc | head -1 | awk {'print $5'}`
	#gphy_efuse1=`regs d 0x11f208e0 | head -1 | awk {'print $5'}`
	#gphy_efuse2=`regs d 0x11f509e4 | head -1 | awk {'print $5'}`
	#gphy_efuse3=`regs d 0x11f509e8 | head -1 | awk {'print $5'}`
	TEST_CMD="mii"
elif [ "${platform}" = "mt7988" ]
then
	TEST_CMD="switch"
fi

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


printf "\n# MTK_PHY_LPI_SIG_EN_LO_THRESH1000_MASK = 0, MTK_PHY_LPI_SIG_EN_HI_THRESH1000_MASK = 20 (0x14)\n"
cmd_gen 45 "read" 0x1e 0x120
val=$(eval "${CMD}${response}")
check_value "${val}" "0x8014"

printf "\n# MTK_PHY_LPI_NORM_MSE_HI_THRESH1000_MASK = 255(0xff)\n"
cmd_gen 45 "read" 0x1e 0x122
val=$(eval "${CMD}${response}")
check_value "${val}" "0xffff"

printf "\n# MTK_PHY_RG_TXEN_DIG_MASK = 0\n"
cmd_gen 45 "read" 0x1e 0x144
val=$(eval "${CMD}${response}")
check_value "${val}" "0x0200"

printf "\n# MTK_PHY_BYPASS_DSP_LPI_READY = 1\n"
cmd_gen 45 "read" 0x1e 0x19b
val=$(eval "${CMD}${response}")
check_value "${val}" "0x0111"

printf "\n# MTK_PHY_TR_OPEN_LOOP_EEE_EN = 1\n"
cmd_gen 45 "read" 0x1e 0x234
val=$(eval "${CMD}${response}")
if [ "${platform}" = "mt7981" ]
then
	check_value "${val}" "0x0191"
elif [ "${platform}" = "mt7988" ]
then
	check_value "${val}" "0x01a1"
fi

printf "\n# MTK_PHY_LPI_SLV_SEND_TX_TIMER_MASK = 0x120, MTK_PHY_LPI_SLV_SEND_TX_EN = 0\n"
cmd_gen 45 "read" 0x1e 0x238
val=$(eval "${CMD}${response}")
check_value "${val}" "0x0120"

printf "\n# Keep MTK_PHY_LPI_SEND_LOC_TIMER as 375\n"
cmd_gen 45 "read" 0x1e 0x239
val=$(eval "${CMD}${response}")
check_value "${val}" "0x0177"

printf "\n# MTK_PHY_MAX_GAIN_MASK = 0x8, MTK_PHY_MIN_GAIN_MASK = 0x13\n"
cmd_gen 45 "read" 0x1e 0x2c7
val=$(eval "${CMD}${response}")
check_value "${val}" "0x1308"

printf "\n# MTK_PHY_VCO_SLICER_THRESH_BITS_HIGH_EEE_MASK = 0x33, "
printf "MTK_PHY_LPI_SKIP_SD_SLV_TR = 1, MTK_PHY_LPI_TR_READY = 1, MTK_PHY_LPI_VCO_EEE_STG0_EN = 1\n"
cmd_gen 45 "read" 0x1e 0x2d1
val=$(eval "${CMD}${response}")
check_value "${val}" "0x0733"

printf "\n# MTK_PHY_EEE_WAKE_MAS_INT_DC = 1, MTK_PHY_EEE_WAKE_SLV_INT_DC = 1\n"
cmd_gen 45 "read" 0x1e 0x323
val=$(eval "${CMD}${response}")
check_value "${val}" "0x0011"

printf "\n# MTK_PHY_SMI_DETCNT_MAX_MASK = 0x3f, MTK_PHY_SMI_DET_MAX_EN = 1\n"
cmd_gen 45 "read" 0x1e 0x324
val=$(eval "${CMD}${response}")
check_value "${val}" "0x013f"

printf "\n# MTK_PHY_LPI_MODE_SD_ON = 1, MTK_PHY_RESET_RANDUPD_CNT = 1, "
printf "MTK_PHY_TREC_UPDATE_ENAB_CLR = 1, MTK_PHY_LPI_QUIT_WAIT_DFE_SIG_DET_OFF = 1, MTK_PHY_TR_READY_SKIP_AFE_WAKEUP = 1\n"
cmd_gen 45 "read" 0x1e 0x326
val=$(eval "${CMD}${response}")
check_value "${val}" "0x0037"

printf "\n##########################################################################\n"
printf "# Regsigdet_sel_1000 = 0\n"
tr_access "read" 0x2 0xd 0x8
check_tr_value "0x0000" "0x000b"

printf "# REG_EEE_st2TrKf1000 = 2\n"
tr_access "read" 0x2 0xd 0xd
check_tr_value "0x0002" "0x114f"

printf "# RegEEE_slv_wake_tr_timer_tar = 6, RegEEE_slv_remtx_timer_tar = 20\n"
tr_access "read" 0x2 0xd 0xf
check_tr_value "0x0000" "0x3028"

printf "# RegEEE_slv_int_checktr_timer_tar = 10(default), RegEEE_slv_wake_int_timer_tar = 8\n"
tr_access "read" 0x2 0xd 0x10
check_tr_value "0x0000" "0x5010"

printf "# RegEEE_trfreeze_timer2[9:0] = 586\n"
tr_access "read" 0x2 0xd 0x14
check_tr_value "0x0000" "0x024a"

printf "# RegEEE100Stg1_tar[8:0] = 16\n"
tr_access "read" 0x2 0xd 0x1c
check_tr_value "0x0000" "0x3210"

printf "# REGEEE_wake_slv_tr_wait_dfesigdet_en[11] = 0 \n"
tr_access "read" 0x2 0xd 0x25
check_tr_value "0x0000" "0x1463"

printf "# DfeTailEnableVgaThresh1000[5:1] = 27\n"
tr_access "read" 0x1 0xf 0x0
check_tr_value "0x0000" "0x0036"

printf "\n##########################################################################\n"

cmd_gen 22 "write" 0x1f 0x3 && ${CMD} > /dev/null
printf "\n# lpi_wake_timer_1000[8:0] = 412(0x19c)\n"
cmd_gen 22 "read" 0x14
val=$(eval "${CMD}${response}")
check_value "${val}" "0x419c"

printf "\n# SMI detection on threshold = 12(0xc)\n"
cmd_gen 22 "read" 0x1c
val=$(eval "${CMD}${response}")
check_value "${val}" "0x0c92"

printf "\n# LPI quit wait AFE signal detection = 1\n"
cmd_gen 22 "read" 0x1d
val=$(eval "${CMD}${response}")
check_value "${val}" "0x0001"

cmd_gen 22 "write" 0x1f 0x0 && ${CMD} > /dev/null
