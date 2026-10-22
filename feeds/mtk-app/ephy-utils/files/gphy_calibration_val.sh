# ./gphy_calibration_val.sh -s -p=0 --r50_all=-2
# ./gphy_calibration_val.sh -s -p=0 --r50_all=2
source lib.sh

dump_tx_amp_from_CL45() {
	cmd_gen 45 "read" 0x1e 0x12
	MTK_PHY_TXVLD_DA_RG=$(eval "${CMD}${response}")
	MTK_PHY_DA_TX_I2MPB_A_GBE_MASK=$(( (${MTK_PHY_TXVLD_DA_RG} >> 10) & 0x3f )) #bit[15:10]
	MTK_PHY_DA_TX_I2MPB_A_TBT_MASK=$(( ${MTK_PHY_TXVLD_DA_RG} & 0x3f )) #bit[5:0]
	cmd_gen 45 "read" 0x1e 0x16
	MTK_PHY_TX_I2MPB_TEST_MODE_A2=$(eval "${CMD}${response}")
	MTK_PHY_DA_TX_I2MPB_A_HBT_MASK=$(( (${MTK_PHY_TX_I2MPB_TEST_MODE_A2} >> 10) & 0x3f )) #bit[15:10]
	MTK_PHY_DA_TX_I2MPB_A_TST_MASK=$(( ${MTK_PHY_TX_I2MPB_TEST_MODE_A2} & 0x3f )) #bit[5:0]

	cmd_gen 45 "read" 0x1e 0x17
	MTK_PHY_TX_I2MPB_TEST_MODE_B1=$(eval "${CMD}${response}")
	MTK_PHY_DA_TX_I2MPB_B_GBE_MASK=$(( (${MTK_PHY_TX_I2MPB_TEST_MODE_B1} >> 8) & 0x3f )) #bit[13:8]
	MTK_PHY_DA_TX_I2MPB_B_TBT_MASK=$(( ${MTK_PHY_TX_I2MPB_TEST_MODE_B1} & 0x3f )) #bit[5:0]
	cmd_gen 45 "read" 0x1e 0x18
	MTK_PHY_TX_I2MPB_TEST_MODE_B2=$(eval "${CMD}${response}")
	MTK_PHY_DA_TX_I2MPB_B_HBT_MASK=$(( (${MTK_PHY_TX_I2MPB_TEST_MODE_B2} >> 8) & 0x3f )) #bit[13:8]
	MTK_PHY_DA_TX_I2MPB_B_TST_MASK=$(( ${MTK_PHY_TX_I2MPB_TEST_MODE_B2} & 0x3f )) #bit[5:0]

	cmd_gen 45 "read" 0x1e 0x19
	MTK_PHY_TX_I2MPB_TEST_MODE_C1=$(eval "${CMD}${response}")
	MTK_PHY_DA_TX_I2MPB_C_GBE_MASK=$(( (${MTK_PHY_TX_I2MPB_TEST_MODE_C1} >> 8) & 0x3f )) #bit[13:8]
	MTK_PHY_DA_TX_I2MPB_C_TBT_MASK=$(( ${MTK_PHY_TX_I2MPB_TEST_MODE_C1} & 0x3f )) #bit[5:0]
	cmd_gen 45 "read" 0x1e 0x20
	MTK_PHY_TX_I2MPB_TEST_MODE_C2=$(eval "${CMD}${response}")
	MTK_PHY_DA_TX_I2MPB_C_HBT_MASK=$(( (${MTK_PHY_TX_I2MPB_TEST_MODE_C2} >> 8) & 0x3f )) #bit[13:8]
	MTK_PHY_DA_TX_I2MPB_C_TST_MASK=$(( ${MTK_PHY_TX_I2MPB_TEST_MODE_C2} & 0x3f )) #bit[5:0]

	cmd_gen 45 "read" 0x1e 0x21
	MTK_PHY_TX_I2MPB_TEST_MODE_D1=$(eval "${CMD}${response}")
	MTK_PHY_DA_TX_I2MPB_D_GBE_MASK=$(( (${MTK_PHY_TX_I2MPB_TEST_MODE_D1} >> 8) & 0x3f )) #bit[13:8]
	MTK_PHY_DA_TX_I2MPB_D_TBT_MASK=$(( ${MTK_PHY_TX_I2MPB_TEST_MODE_D1} & 0x3f )) #bit[5:0]
	cmd_gen 45 "read" 0x1e 0x22
	MTK_PHY_TX_I2MPB_TEST_MODE_D2=$(eval "${CMD}${response}")
	MTK_PHY_DA_TX_I2MPB_D_HBT_MASK=$(( (${MTK_PHY_TX_I2MPB_TEST_MODE_D2} >> 8) & 0x3f )) #bit[13:8]
	MTK_PHY_DA_TX_I2MPB_D_TST_MASK=$(( ${MTK_PHY_TX_I2MPB_TEST_MODE_D2} & 0x3f )) #bit[5:0]
}

dump_tx_offset_from_CL45() {
	cmd_gen 45 "read" 0x1e 0x172
	MTK_PHY_RG_DEV1E_REG172=$(eval "${CMD}${response}")
	MTK_PHY_CR_TX_AMP_OFFSET_A_MASK=$(( (${MTK_PHY_RG_DEV1E_REG172} >> 8) & 0x3f )) #bit[13:8]
	MTK_PHY_DA_TX_I2MPB_A_TBT_MASK=$(( ${MTK_PHY_RG_DEV1E_REG172} & 0x3f )) #bit[6:0]
	cmd_gen 45 "read" 0x1e 0x16
	MTK_PHY_TX_I2MPB_TEST_MODE_A2=$(eval "${CMD}${response}")
	MTK_PHY_DA_TX_I2MPB_A_HBT_MASK=$(( (${MTK_PHY_TX_I2MPB_TEST_MODE_A2} >> 10) & 0x3f )) #bit[15:10]
	MTK_PHY_DA_TX_I2MPB_A_TST_MASK=$(( ${MTK_PHY_TX_I2MPB_TEST_MODE_A2} & 0x3f )) #bit[5:0]
}

dump_tx_r50_from_CL45() {
	cmd_gen 45 "read" 0x1e 0x53d
	MTK_PHY_RG_DEV1E_REG53d=$(eval "${CMD}${response}")
	MTK_PHY_DA_TX_R50_A_NORMAL_MASK=$(( ${MTK_PHY_RG_DEV1E_REG53d} & 0xff ))
	MTK_PHY_DA_TX_R50_A_TBT_MASK=$(( (${MTK_PHY_RG_DEV1E_REG53d} >> 8) & 0xff ))

	cmd_gen 45 "read" 0x1e 0x53d
	MTK_PHY_RG_DEV1E_REG53e=$(eval "${CMD}${response}")
	MTK_PHY_DA_TX_R50_B_NORMAL_MASK=$(( ${MTK_PHY_RG_DEV1E_REG53e} & 0xff ))
	MTK_PHY_DA_TX_R50_B_TBT_MASK=$(( (${MTK_PHY_RG_DEV1E_REG53e} >> 8) & 0xff ))

	cmd_gen 45 "read" 0x1e 0x53f
	MTK_PHY_RG_DEV1E_REG53f=$(eval "${CMD}${response}")
	MTK_PHY_DA_TX_R50_C_NORMAL_MASK=$(( ${MTK_PHY_RG_DEV1E_REG53f} & 0xff ))
	MTK_PHY_DA_TX_R50_C_TBT_MASK=$(( (${MTK_PHY_RG_DEV1E_REG53f} >> 8) & 0xff ))

	cmd_gen 45 "read" 0x1e 0x540
	MTK_PHY_RG_DEV1E_REG540=$(eval "${CMD}${response}")
	MTK_PHY_DA_TX_R50_D_NORMAL_MASK=$(( ${MTK_PHY_RG_DEV1E_REG540} & 0xff ))
	MTK_PHY_DA_TX_R50_D_TBT_MASK=$(( (${MTK_PHY_RG_DEV1E_REG540} >> 8) & 0xff ))
}

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
	#gphy0_efuse0=`regs d 0x11f50940 | head -1 | awk {'print $5'}`
	#gphy0_efuse1=`regs d 0x11f50944 | head -1 | awk {'print $5'}`
	#gphy0_efuse2=`regs d 0x11f50948 | head -1 | awk {'print $5'}`
	#gphy0_efuse3=`regs d 0x11f5094c | head -1 | awk {'print $5'}`
	#TEST_CMD="switch"
fi

echo "Gphy Port 0, tx amp:" #efs_DA_TX_I2MPB_A_P0~efs_DA_TX_I2MPB_D_P0
gphy_tx_amp_pairA=$(( ${gphy_efuse0} & 0x3f ))
gphy_tx_amp_pairB=$(( (${gphy_efuse0} >> 6) & 0x3f ))
gphy_tx_amp_pairC=$(( (${gphy_efuse0} >> 12) & 0x3f ))
gphy_tx_amp_pairD=$(( (${gphy_efuse0} >> 18) & 0x3f ))
dump_tx_amp_from_CL45

GBE_bias=$((${MTK_PHY_DA_TX_I2MPB_A_GBE_MASK}-${gphy_tx_amp_pairA}))

printf "[efuse] PairA: 0x%x\n" ${gphy_tx_amp_pairA}
printf "[CR] PairA GBE MASK(%+d): 0x%x, TBT_MASK(%+d): 0x%x, HBT_MASK(%+d): 0x%x, TST_MASK(%+d): 0x%x\n" \
	$((${MTK_PHY_DA_TX_I2MPB_A_GBE_MASK}-${gphy_tx_amp_pairA})) \
	${MTK_PHY_DA_TX_I2MPB_A_GBE_MASK} \
	$((${MTK_PHY_DA_TX_I2MPB_A_TBT_MASK}-${gphy_tx_amp_pairA})) \
	${MTK_PHY_DA_TX_I2MPB_A_TBT_MASK} \
	$((${MTK_PHY_DA_TX_I2MPB_A_HBT_MASK}-${gphy_tx_amp_pairA})) \
	${MTK_PHY_DA_TX_I2MPB_A_HBT_MASK} \
	$((${MTK_PHY_DA_TX_I2MPB_A_TST_MASK}-${gphy_tx_amp_pairA})) \
	${MTK_PHY_DA_TX_I2MPB_A_TST_MASK}
printf "[efuse] PairB: 0x%x\n" ${gphy_tx_amp_pairB}
printf "[CR] PairB GBE MASK(%+d): 0x%x, TBT_MASK(%+d): 0x%x, HBT_MASK(%+d): 0x%x, TST_MASK(%+d): 0x%x\n" \
	$((${MTK_PHY_DA_TX_I2MPB_B_GBE_MASK}-${gphy_tx_amp_pairB})) \
	${MTK_PHY_DA_TX_I2MPB_B_GBE_MASK} \
	$((${MTK_PHY_DA_TX_I2MPB_B_TBT_MASK}-${gphy_tx_amp_pairB})) \
	${MTK_PHY_DA_TX_I2MPB_B_TBT_MASK} \
	$((${MTK_PHY_DA_TX_I2MPB_B_HBT_MASK}-${gphy_tx_amp_pairB})) \
	${MTK_PHY_DA_TX_I2MPB_B_HBT_MASK} \
	$((${MTK_PHY_DA_TX_I2MPB_B_TST_MASK}-${gphy_tx_amp_pairB})) \
	${MTK_PHY_DA_TX_I2MPB_B_TST_MASK}
printf "[efuse] PairC: 0x%x\n" ${gphy_tx_amp_pairC}
printf "[CR] PairC GBE MASK(%+d): 0x%x, TBT_MASK(%+d): 0x%x, HBT_MASK(%+d): 0x%x, TST_MASK(%+d): 0x%x\n" \
	$((${MTK_PHY_DA_TX_I2MPB_C_GBE_MASK}-${gphy_tx_amp_pairC})) \
	${MTK_PHY_DA_TX_I2MPB_C_GBE_MASK} \
	$((${MTK_PHY_DA_TX_I2MPB_C_TBT_MASK}-${gphy_tx_amp_pairC})) \
	${MTK_PHY_DA_TX_I2MPB_C_TBT_MASK} \
	$((${MTK_PHY_DA_TX_I2MPB_C_HBT_MASK}-${gphy_tx_amp_pairC})) \
	${MTK_PHY_DA_TX_I2MPB_C_HBT_MASK} \
	$((${MTK_PHY_DA_TX_I2MPB_C_TST_MASK}-${gphy_tx_amp_pairC})) \
	${MTK_PHY_DA_TX_I2MPB_C_TST_MASK}
printf "[efuse] PairD: 0x%x\n" ${gphy_tx_amp_pairD}
printf "[CR] PairD GBE MASK(%+d): 0x%x, TBT_MASK(%+d): 0x%x, HBT_MASK(%+d): 0x%x, TST_MASK(%+d): 0x%x\n" \
	$((${MTK_PHY_DA_TX_I2MPB_D_GBE_MASK}-${gphy_tx_amp_pairD})) \
	${MTK_PHY_DA_TX_I2MPB_D_GBE_MASK} \
	$((${MTK_PHY_DA_TX_I2MPB_D_TBT_MASK}-${gphy_tx_amp_pairD})) \
	${MTK_PHY_DA_TX_I2MPB_D_TBT_MASK} \
	$((${MTK_PHY_DA_TX_I2MPB_D_HBT_MASK}-${gphy_tx_amp_pairD})) \
	${MTK_PHY_DA_TX_I2MPB_D_HBT_MASK} \
	$((${MTK_PHY_DA_TX_I2MPB_D_TST_MASK}-${gphy_tx_amp_pairD})) \
	${MTK_PHY_DA_TX_I2MPB_D_TST_MASK}

# echo "Gphy Port 0, tx offset:" #efs_DA_TX_AMP_OFFSET_A_P0~efs_DA_TX_AMP_OFFSET_D_P0
# gphy0_tx_offset_pairA=$(( (${gphy0_efuse0} >> 24) & 0x3f ))
# gphy0_tx_offset_pairB=$(( ${gphy0_efuse1} & 0x3f ))
# gphy0_tx_offset_pairC=$(( (${gphy0_efuse1} >> 6) & 0x3f ))
# gphy0_tx_offset_pairD=$(( (${gphy0_efuse1} >> 12) & 0x3f ))
# dump_tx_offset_from_CL45 0

# printf "PairA: 0x%x\n" ${gphy0_tx_offset_pairA}
# printf "PairB: 0x%x\n" ${gphy0_tx_offset_pairB}
# printf "PairC: 0x%x\n" ${gphy0_tx_offset_pairC}
# printf "PairD: 0x%x\n" ${gphy0_tx_offset_pairD}
echo "#####################################################"

gphy_tx_r50_pairA=$(( (${gphy_efuse1} >> 18) & 0x3f ))
gphy_tx_r50_pairB=$(( (${gphy_efuse1} >> 24) & 0x3f ))
gphy_tx_r50_pairC=$(( (${gphy_efuse2}) & 0x3f ))
gphy_tx_r50_pairD=$(( (${gphy_efuse2} >> 6) & 0x3f ))
dump_tx_r50_from_CL45

# echo "Gphy Port 0, r50:" #efs_DA_TX_R50_A_P0~efs_DA_TX_R50_D_P0
# gphy0_r50_pairA=$(( (${gphy0_efuse1} >> 18) & 0x3f ))
# gphy0_r50_pairB=$(( (${gphy0_efuse1} >> 24) & 0x3f ))
# gphy0_r50_pairC=$(( (${gphy0_efuse2}) & 0x3f ))
# gphy0_r50_pairD=$(( (${gphy0_efuse2} >> 6) & 0x3f ))
#printf "PairA: 0x%x\n" ${gphy_r50_pairA}
#printf "PairB: 0x%x\n" ${gphy_r50_pairB}
#printf "PairC: 0x%x\n" ${gphy_r50_pairC}
#printf "PairD: 0x%x\n" ${gphy_r50_pairD}

echo "Gphy Port 0, TX R50:"
printf "[efuse] PairA: 0x%x\n" ${gphy_tx_r50_pairA}
printf "[CR] PairA NORMAL_MASK(%+d): 0x%x, TBT_MASK(%+d): 0x%x\n" \
	$((${MTK_PHY_DA_TX_R50_A_NORMAL_MASK}-${gphy_tx_r50_pairA})) \
	${MTK_PHY_DA_TX_R50_A_NORMAL_MASK} \
	$((${MTK_PHY_DA_TX_R50_A_TBT_MASK}-${gphy_tx_r50_pairA})) \
	${MTK_PHY_DA_TX_R50_A_TBT_MASK}

printf "[efuse] PairB: 0x%x\n" ${gphy_tx_r50_pairB}
printf "[CR] PairB NORMAL_MASK(%+d): 0x%x, TBT_MASK(%+d): 0x%x\n" \
	$((${MTK_PHY_DA_TX_R50_B_NORMAL_MASK}-${gphy_tx_r50_pairB})) \
	${MTK_PHY_DA_TX_R50_B_NORMAL_MASK} \
	$((${MTK_PHY_DA_TX_R50_B_TBT_MASK}-${gphy_tx_r50_pairB})) \
	${MTK_PHY_DA_TX_R50_B_TBT_MASK}

printf "[efuse] PairC: 0x%x\n" ${gphy_tx_r50_pairC}
printf "[CR] PairC NORMAL_MASK(%+d): 0x%x, TBT_MASK(%+d): 0x%x\n" \
	$((${MTK_PHY_DA_TX_R50_C_NORMAL_MASK}-${gphy_tx_r50_pairC})) \
	${MTK_PHY_DA_TX_R50_C_NORMAL_MASK} \
	$((${MTK_PHY_DA_TX_R50_C_TBT_MASK}-${gphy_tx_r50_pairC})) \
	${MTK_PHY_DA_TX_R50_C_TBT_MASK}

printf "[efuse] PairD: 0x%x\n" ${gphy_tx_r50_pairD}
printf "[CR] PairB NORMAL_MASK(%+d): 0x%x, TBT_MASK(%+d): 0x%x\n" \
	$((${MTK_PHY_DA_TX_R50_D_NORMAL_MASK}-${gphy_tx_r50_pairD})) \
	${MTK_PHY_DA_TX_R50_D_NORMAL_MASK} \
	$((${MTK_PHY_DA_TX_R50_D_TBT_MASK}-${gphy_tx_r50_pairD})) \
	${MTK_PHY_DA_TX_R50_D_TBT_MASK}

if [ -n "${r50_all_bias}" ]; then
	r50_all_bias=`printf "%d" ${r50_all_bias}`
	tx_r50_pairA=`printf "0x%x" $(( ${gphy_tx_r50_pairA} + ${r50_all_bias}))`
	tx_r50_pairB=`printf "0x%x" $(( ${gphy_tx_r50_pairB} + ${r50_all_bias}))`
	tx_r50_pairC=`printf "0x%x" $(( ${gphy_tx_r50_pairC} + ${r50_all_bias}))`
	tx_r50_pairD=`printf "0x%x" $(( ${gphy_tx_r50_pairD} + ${r50_all_bias}))`

	tx_r50_pairA=`printf "0x%x" $(( (${tx_r50_pairA} << 8) + ${tx_r50_pairA}))`
	tx_r50_pairB=`printf "0x%x" $(( (${tx_r50_pairB} << 8) + ${tx_r50_pairB}))`
	tx_r50_pairC=`printf "0x%x" $(( (${tx_r50_pairC} << 8) + ${tx_r50_pairC}))`
	tx_r50_pairD=`printf "0x%x" $(( (${tx_r50_pairD} << 8) + ${tx_r50_pairD}))`

	cmd_gen 45 "write" 0x1e 0x53d ${tx_r50_pairA} && ${CMD}
	cmd_gen 45 "write" 0x1e 0x53e ${tx_r50_pairB} && ${CMD}
	cmd_gen 45 "write" 0x1e 0x53f ${tx_r50_pairC} && ${CMD}
	cmd_gen 45 "write" 0x1e 0x540 ${tx_r50_pairD} && ${CMD}

	printf "Set TX R50 = %d\n" ${r50_all_bias}
fi

echo "#####################################################"
if [ -n "${tclkoffset}" ] && [[ "${tclkoffset}" =~ ^[0-9]+$ ]];
then
	tclkoffset=`printf "%d" ${tclkoffset}`
	if [ ${tclkoffset} -le 31 ] || [ ${tclkoffset} -ge 0 ]
	then
		cmd_gen 22 "write" 0x1f 0x2a30 && ${CMD} > /dev/null
		cmd_gen 22 "read" 0x10
		val=$(eval "${CMD}${response}")
		val=$(( ${val} & ~0x1f00))
		val=`printf "0x%x" $(( ${val} | (${tclkoffset} << 8) ))`
		echo ${val}
		cmd_gen 22 "write" 0x10 ${val} && ${CMD} > /dev/null
		cmd_gen 22 "write" 0x1f 0x0 && ${CMD} > /dev/null
	else
		echo "Enter tclkoffset with 0x0~0x1f"
	fi
fi
# echo "Gphy Port 0, r50:" #efs_DA_TX_R50_A_P0~efs_DA_TX_R50_D_P0
# gphy0_r50_pairA=$(( (${gphy0_efuse1} >> 18) & 0x3f ))
# gphy0_r50_pairB=$(( (${gphy0_efuse1} >> 24) & 0x3f ))
# gphy0_r50_pairC=$(( (${gphy0_efuse2}) & 0x3f ))
# gphy0_r50_pairD=$(( (${gphy0_efuse2} >> 6) & 0x3f ))
# printf "PairA: 0x%x\n" ${gphy0_r50_pairA}
# printf "PairB: 0x%x\n" ${gphy0_r50_pairB}
# printf "PairC: 0x%x\n" ${gphy0_r50_pairC}
# printf "PairD: 0x%x\n" ${gphy0_r50_pairD}

#gphy0_efuse1=`regs d 0x11f50944 | head -1 | awk {'print $5'}`
#gphy0_r50_pairA=$(( ${gphy0_efuse1} & 0x3f ))
#echo "gphy0 r50 pair A efuse value: ${gphy0_r50_pairA}"

#r50_pairA=`switch phy cl45 r 0 0x1e 0x53d | awk {'print $4'} | awk '{split($0,a,"="); print a[2]}'`
#r50_pairA=$(( ${r50_pairA} & 0xff ))
#echo "gphy0 r50 pair A real value: ${r50_pairA}"
