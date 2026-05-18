#!/bin/ash

interface=$1    # phy0/phy1/ra0
cmd_type=$2     # set/show/e2p/mac/dump
full_cmd=$3
interface_ori=${interface}
SOC_start_idx="0"
SOC_end_idx="0"
connac_ver="0"

work_mode="RUN" # RUN/PRINT/DEBUG
iwpriv_file="/tmp/iwpriv_wrapper"
interface_file="/tmp/interface"
phy_idx=$(echo ${interface} | tr -dc '0-9')
main_phy_idx=0

function do_cmd() {
    case ${work_mode} in
        "RUN")
            eval "$1"
            ;;
        "PRINT")
            echo "$1"
            ;;
        "DEBUG")
            echo "$1"
            eval "$1"
            ;;
    esac
}

function print_debug() {
    if [ "${work_mode}" = "DEBUG" ]; then
        echo "[DEBUG] $1"
    fi
}

function print_err() {
    echo "[ERROR] $1"
}

function write_dmesg() {
    echo "$1" > /dev/kmsg
}

function record_config() {
    local config=$1
    local tmp_file=$3

    # check it is SOC(mt7986)/mt7996/mt7992/mt7990/mt7999 or
    # PCIE card (mt7915/7916), and write its config
    if [ ${tmp_file} != ${interface_file} ]; then
        if [ $phy_idx -lt $SOC_start_idx ]; then
            config="${config}_PCIE"
        elif [ $phy_idx -ge $SOC_start_idx ]; then
            config="${config}_SOC"
        fi
    fi

    if [ -f ${tmp_file} ]; then
        if grep -q ${config} ${tmp_file}; then
            sed -i "/${config}/c\\${config}=$2" ${tmp_file}
        else
            echo "${config}=$2" >> ${tmp_file}
        fi
    else
        echo "${config}=$2" >> ${tmp_file}
    fi
}

function get_config() {
    local config=$1
    local tmp_file=$2

    if [ ! -f ${tmp_file} ]; then
        echo ""
        return
    fi

    # check it is SOC(mt7986)/mt7996/mt7992/mt7990/mt7999 or
    # PCIE card (mt7915/7916), and write its config
    if [ ${tmp_file} != ${interface_file} ]; then
        if [ $phy_idx -lt $SOC_start_idx ]; then
            config="${config}_PCIE"
        elif [ $phy_idx -ge $SOC_start_idx ]; then
            config="${config}_SOC"
        fi
    fi

    if grep -q ${config} ${tmp_file}; then
        echo "$(cat ${tmp_file} | grep ${config} | sed s/=/' '/g | cut -d " " -f 2)"
    else
        echo ""
    fi
}

function parse_sku {
    SOC_start_idx=$(get_config "STARTIDX" ${interface_file})
    SOC_end_idx=$(get_config "ENDIDX" ${interface_file})
    connac_ver=$(get_config "CONNAC_VER" ${interface_file})
    local eeprom_file=/sys/kernel/debug/ieee80211/phy0/mt76/eeprom
    if [ -z "${SOC_start_idx}" ] || [ -z "${SOC_end_idx}" ] || [ -z "${connac_ver}" ]; then
        if [ ! -z "$(head -c 2 ${eeprom_file} | hexdump | grep "7916")" ]; then
            SOC_start_idx="2"
            SOC_end_idx="3"
            connac_ver="2"
        elif [ ! -z "$(head -c 2 ${eeprom_file} | hexdump | grep "7915")" ]; then
            SOC_start_idx="1"
            SOC_end_idx="2"
            connac_ver="2"
        elif [ ! -z "$(head -c 2 ${eeprom_file} | hexdump | grep "7981")" ]; then
            SOC_start_idx="0"
            SOC_end_idx="1"
            connac_ver="2"
        elif [ ! -z "$(head -c 2 ${eeprom_file} | hexdump | grep "7986")" ]; then
            SOC_start_idx="0"
            SOC_end_idx="1"
            connac_ver="2"
        elif [ ! -z "$(head -c 2 ${eeprom_file} | hexdump | grep "7990")" ]; then
            SOC_start_idx="0"
            SOC_end_idx="2"
            connac_ver="3"
        elif [ ! -z "$(head -c 2 ${eeprom_file} | hexdump | grep "7992")" ]; then
            SOC_start_idx="0"
            SOC_end_idx="1"
            connac_ver="3"
        elif [ ! -z "$(head -c 2 ${eeprom_file} | hexdump | grep "7993")" ]; then
            SOC_start_idx="0"
            SOC_end_idx="1"
            connac_ver="3"
        elif [ ! -z "$(head -c 2 ${eeprom_file} | hexdump | grep "80f2")" ]; then
            SOC_start_idx="0"
            SOC_end_idx="2"
            connac_ver="5"
        else
            echo "Interface Conversion Failed!"
            echo "Please use iwpriv <phy0/phy1/..> set <...> or configure the sku of your board manually by the following commands"
            echo "For AX3000/AX6000:"
            echo "      echo STARTIDX=0 >> ${interface_file}"
            echo "      echo ENDIDX=1 >> ${interface_file}"
            echo "      echo CONNAC_VER=2 >> ${interface_file}"
            echo "For AX7800:"
            echo "      echo STARTIDX=2 >> ${interface_file}"
            echo "      echo ENDIDX=3 >> ${interface_file}"
            echo "      echo CONNAC_VER=2 >> ${interface_file}"
            echo "For AX8400:"
            echo "      echo STARTIDX=1 >> ${interface_file}"
            echo "      echo ENDIDX=2 >> ${interface_file}"
            echo "      echo CONNAC_VER=2 >> ${interface_file}"
            echo "For MT7996:"
            echo "      echo STARTIDX=0 >> ${interface_file}"
            echo "      echo ENDIDX=2 >> ${interface_file}"
            echo "      echo CONNAC_VER=3 >> ${interface_file}"
            echo "For MT7992:"
            echo "      echo STARTIDX=0 >> ${interface_file}"
            echo "      echo ENDIDX=1 >> ${interface_file}"
            echo "      echo CONNAC_VER=3 >> ${interface_file}"
            echo "For MT7990:"
            echo "      echo STARTIDX=0 >> ${interface_file}"
            echo "      echo ENDIDX=1 >> ${interface_file}"
            echo "      echo CONNAC_VER=3 >> ${interface_file}"
            echo "For MT7999:"
            echo "      echo STARTIDX=0 >> ${interface_file}"
            echo "      echo ENDIDX=2 >> ${interface_file}"
            echo "      echo CONNAC_VER=5 >> ${interface_file}"
            exit 0
        fi
        record_config "STARTIDX" ${SOC_start_idx} ${interface_file}
        record_config "ENDIDX" ${SOC_end_idx} ${interface_file}
        record_config "CONNAC_VER" ${connac_ver} ${interface_file}
    fi
}

function convert_interface {
    if [ ${connac_ver} == "2" ]; then
        if [[ $1 == "raix"* ]]; then
            phy_idx=1
        elif [[ $1 == "rai"* ]]; then
            phy_idx=0
        elif [[ $1 == "rax"* ]]; then
            phy_idx=$((SOC_start_idx+1))
        else
            phy_idx=$SOC_start_idx
        fi

        # convert phy index according to band idx
        local band_idx=$(get_config "ATECTRLBANDIDX" ${iwpriv_file})
        if [ "${band_idx}" = "0" ]; then
            if [[ $1 == "raix"* ]]; then
                phy_idx=0
            elif [[ $1 == "rax"* ]]; then
                phy_idx=$SOC_start_idx
            fi
        elif [ "${band_idx}" = "1" ]; then
            if [[ $1 == "rai"* ]]; then
                # AX8400: mt7915 remain phy0
                # AX7800: mt7916 becomes phy1
                phy_idx=$((SOC_start_idx-1))
            elif [[ $1 == "ra"* ]]; then
                phy_idx=$((SOC_start_idx+1))
            fi
        fi
    else
        # Connac 3/5 chips has different mapping method
        # phy0: ra0
        # phy1: rai0
        # phy2: rax0
        if [[ $1 == "rai"* ]]; then
            phy_idx=1
        elif [[ $1 == "rax"* ]]; then
            phy_idx=2
        else
            phy_idx=0
        fi
    fi

    interface="phy${phy_idx}"
}

function change_band_idx {
    local new_idx=$1
    local new_phy_idx=$phy_idx
    local old_idx=$(get_config "ATECTRLBANDIDX" ${iwpriv_file})

    if [[ ${interface_ori} == "ra"* ]]; then
        if [ -z "${old_idx}" ] || [ "${old_idx}" != "${new_idx}" ]; then
            if [ "${new_idx}" = "0" ]; then
                # raix0 & rai0 becomes rai0
                if [[ $interface_ori == "rai"* ]]; then
                    new_phy_idx=0
                # rax0 & ra0 becomes ra0
                elif [[ $interface_ori == "ra"* ]]; then
                    new_phy_idx=$SOC_start_idx
                fi
            elif [ "${new_idx}" = "1" ]; then
                # raix0 & rai0 becomes raix0
                if [[ $interface_ori == "rai"* ]]; then
                    # For AX8400 => don't change phy idx
                    if [ ${SOC_start_idx} != "1" ]; then
                        new_phy_idx=1
                    fi
                # rax0 & ra0 becomes rax0
                elif [[ $interface_ori == "ra"* ]]; then
                    new_phy_idx=$((SOC_start_idx+1))
                fi
            fi
        fi

        if [ ${new_phy_idx} != ${phy_idx} ]; then
            do_ate_work "ATESTOP"
            phy_idx=$new_phy_idx
            interface="phy${phy_idx}"
            do_ate_work "ATESTART"
        fi
    fi
    record_config "ATECTRLBANDIDX" ${new_idx} ${iwpriv_file}
}

function simple_convert() {
    if [ "$1" = "ATETXCNT" ]; then
        echo "tx_count"
    elif [ "$1" = "ATETXLEN" ]; then
        echo "tx_length"
    elif [ "$1" = "ATETXMCS" ]; then
        echo "tx_rate_idx"
    elif [ "$1" = "ATEVHTNSS" ] || [ "$1" = "ATETXNSS" ]; then
        echo "tx_rate_nss"
    elif [ "$1" = "ATETXLDPC" ]; then
        echo "tx_rate_ldpc"
    elif [ "$1" = "ATETXSTBC" ]; then
        echo "tx_rate_stbc"
    elif [ "$1" = "ATEPKTTXTIME" ]; then
        echo "tx_time"
    elif [ "$1" = "ATEIPG" ]; then
        echo "tx_ipg"
    elif [ "$1" = "ATEDUTYCYCLE" ]; then
        echo "tx_duty_cycle"
    elif [ "$1" = "ATETXFREQOFFSET" ]; then
        echo "freq_offset"
    else
        echo "undefined"
    fi
}

function convert_tx_mode() {
    # Remove leading zeros
    local tx_mode=$(echo $1 | sed -r 's/0+([0-9]+)/\1/g')

    if [ "$tx_mode" = "0" ]; then
        echo "cck"
    elif [ "$tx_mode" = "1" ]; then
        echo "ofdm"
    elif [ "$tx_mode" = "2" ]; then
        echo "ht"
    elif [ "$tx_mode" = "4" ]; then
        echo "vht"
    elif [ "$tx_mode" = "8" ]; then
        echo "he_su"
    elif [ "$tx_mode" = "9" ]; then
        echo "he_er"
    elif [ "$tx_mode" = "10" ]; then
        echo "he_tb"
    elif [ "$tx_mode" = "11" ]; then
        echo "he_mu"
    elif [ "$tx_mode" = "13" ]; then
        echo "eht_su"
    elif [ "$tx_mode" = "14" ]; then
        echo "eht_tb"
    elif [ "$tx_mode" = "15" ]; then
        echo "eht_mu"
    else
        echo "undefined"
    fi
}

function convert_gi {
    local tx_mode=$1
    local val=$2
    local sgi="0"
    local he_ltf="0"

    case ${tx_mode} in
        "ht"|"vht")
            sgi=${val}
            ;;
        "he_su"|"he_er")
            case ${val} in
                "0")
                    ;;
                "1")
                    he_ltf="1"
                    ;;
                "2")
                    sgi="1"
                    he_ltf="1"
                    ;;
                "3")
                    sgi="2"
                    he_ltf="2"
                    ;;
                "4")
                    he_ltf="2"
                    ;;
                *)
                    print_err "unknown gi"
            esac
            ;;
        "he_mu")
            case ${val} in
                "0")
                    he_ltf="2"
                    ;;
                "1")
                    he_ltf="1"
                    ;;
                "2")
                    sgi="1"
                    he_ltf="1"
                    ;;
                "3")
                    sgi="2"
                    he_ltf="2"
                    ;;
                *)
                    print_err "unknown gi"
            esac
            ;;
        "he_tb")
            case ${val} in
                "0")
                    sgi="1"
                    ;;
                "1")
                    sgi="1"
                    he_ltf="1"
                    ;;
                "2")
                    sgi="2"
                    he_ltf="2"
                    ;;
                *)
                    print_err "unknown gi"
            esac
            ;;
        *)
            print_err "legacy mode no need gi"
    esac

    do_cmd "mt76-test ${interface} set tx_rate_sgi=${sgi} tx_ltf=${he_ltf}"
}

function convert_tm_cbw_to_nl {
    local cbw=$1
    local bw="NOHT"

    case ${cbw} in
        # TM_CBW_20MHZ
        "0")
            bw="20"
            ;;
        # TM_CBW_40MHZ
        "1")
            bw="40"
            ;;
        # TM_CBW_80MHZ
        "2")
            bw="80"
            ;;
        # TM_CBW_10MHZ
        "3")
            bw="10"
            ;;
        # TM_CBW_5MHZ
        "4")
            bw="5"
            ;;
        # TM_CBW_160MHZ
        "5")
            bw="160"
            ;;
        # TM_CBW_8080MHZ
        "6")
            bw="80p80"
            ;;
        # TM_CBW_320MHZ
        "12")
            bw="320"
            ;;
    esac

    echo ${bw}
}

function convert_bw {
    local system_bw=$(echo $1 | sed s/:/' '/g | cut -d " " -f 1)
    local data_bw=$(echo $1 | sed s/:/' '/g | cut -d " " -f 2)
    # Convert TM_CBW to NL80211_CHAN_WIDTH
    local tx_pkt_bw=$(convert_tm_cbw_to_nl ${data_bw})

    record_config "ATETXBW" ${system_bw} ${iwpriv_file}

    # apply per-packet bw
    if [[ $1 == *":"* ]]; then
        do_cmd "mt76-test phy${phy_idx} set tx_pkt_bw=${tx_pkt_bw}"
    fi
}

function convert_channel {
    local ch=$(echo $1 | sed s/:/' '/g | cut -d " " -f 1)
    local control_ch=$(echo $1 | sed s/:/' '/g | cut -d " " -f 1)
    local band=$(echo $1 | sed s/:/' '/g | cut -d " " -f 2)
    local pri_sel=$(echo $1 | sed s/:/' '/g | cut -d " " -f 3)
    local center_ch2=$(echo $1 | sed s/:/' '/g | cut -d " " -f 4)
    local fast_cal=$(echo $1 | sed s/:/' '/g | cut -d " " -f 5)
    local fast_cal_type="none"
    local ctrl_band_idx=$(get_config "ATECTRLBANDIDX" ${iwpriv_file})
    local bw=$(get_config "ATETXBW" ${iwpriv_file} | cut -d ":" -f 1)
    local bw_str="20"
    local base_chan=1
    local control_freq=0
    local base_freq=0
    local temp=$((phy_idx+1))

    # Handle ATECTRLBANDIDX
    if [ ! -z ${ctrl_band_idx} ]; then
        if [ "${ctrl_band_idx}" == "1" ] && [ ${band} == "0" ]; then
            local temp=$(cat "/etc/config/wireless"| grep "option band" | sed -n ${temp}p | cut -c 15)
            if [ "${temp}" == "2" ]; then
                local band=0
            elif [ "${temp}" == "5" ]; then
                local band=1
            elif [ "${temp}" == "6" ]; then
                local band=2
            else
                print_err "wrapper band translate error!"
            fi
        else
            # mt7915 in AX8400 case: band should be determined by only the input band
            if [ "${SOC_start_idx}" == "1" ] && [ ${phy_idx} == "0" ]; then
                local band=$((band))
            else
                local band=$((ctrl_band_idx * band))
            fi
        fi
    fi

    if [[ $1 == *":"* ]] && [ -n "${pri_sel}" ]; then
        do_cmd "mt76-test phy${phy_idx} set tx_pri_sel=${pri_sel}"
    fi

    if [[ $1 == *":"* ]] && [ -n "${fast_cal}" ]; then
        case ${fast_cal} in
            "200000")
                fast_cal_type="rx_verify"
                ;;
            "400000")
                fast_cal_type="power_cal"
                ;;
        esac
        do_cmd "mt76-test phy${phy_idx} set fast_cal=${fast_cal_type}"
    fi

    if [[ $1 != *":"* ]] || [ "${band}" = "0" ]; then
        case ${bw} in
            "1")
                if [ "${ch}" -lt "3" ] || [ "${ch}" -gt "12" ]; then
                    bw_str="20"
                else
                    bw_str="40"
                    control_ch=$(expr ${ch} - "2")
                fi
                ;;
        esac
        base_freq=2412
    else
        case ${bw} in
            "12")
                bw_str="320"
                control_ch=$(expr ${ch} - "30")
                ;;
            "5")
                bw_str="160"
                control_ch=$(expr ${ch} - "14")
                ;;
            "2")
                bw_str="80"
                control_ch=$(expr ${ch} - "6")
                ;;
            "1")
                bw_str="40"
                control_ch=$(expr ${ch} - "2")
                ;;
            "0")
                bw_str="20"
                control_ch=${ch}
                ;;
        esac
        if [ "${band}" = "1" ]; then
            base_freq=5180
            base_chan=36
        else
            base_freq=5955
        fi
    fi

    local center_freq=$(((ch - base_chan) * 5 + base_freq))
    local control_freq=$(((control_ch - base_chan) * 5 + base_freq))
    if [ "${center_freq}" == "${control_freq}" ]; then
        do_cmd "iw dev mon${phy_idx} set freq ${control_freq} ${bw_str}"
    else
        do_cmd "iw dev mon${phy_idx} set freq ${control_freq} ${bw_str} ${center_freq}"
    fi
}

function convert_rxstat {
    local res=$(do_cmd "mt76-test ${interface} dump stats")
    local mdrdy=$(echo "${res}" | grep "rx_packets" | cut -d "=" -f 2)
    local fcs_error=$(echo "${res}" | grep "rx_fcs_error" | cut -d "=" -f 2)
    local rx_ok=$(echo "${res}" | grep "rx_success" | cut -d "=" -f 2)
    local rssi=$(echo "${res}" | grep "last_rssi" | cut -d "=" -f 2 | sed 's/,/ /g')
    local rcpi=$(echo "${res}" | grep "last_rcpi" | cut -d "=" -f 2 | sed 's/,/ /g')
    local ib_rssi=$(echo "${res}" | grep "last_ib_rssi" | cut -d "=" -f 2 | sed 's/,/ /g')
    local wb_rssi=$(echo "${res}" | grep "last_wb_rssi" | cut -d "=" -f 2 | sed 's/,/ /g')

    write_dmesg "rssi: ${rssi}"
    write_dmesg "rcpi: ${rcpi}"
    write_dmesg "fagc rssi ib: ${ib_rssi}"
    write_dmesg "fagc rssi wb: ${wb_rssi}"
    write_dmesg "all_mac_rx_mdrdy_cnt: ${mdrdy}"
    write_dmesg "all_mac_rx_fcs_err_cnt: ${fcs_error}"
    write_dmesg "all_mac_rx_ok_cnt : ${rx_ok}"
}

function set_mac_addr {
    record_config ${cmd} ${param} ${iwpriv_file}

    local addr1=$(get_config "ATEDA" ${iwpriv_file})
    local addr2=$(get_config "ATESA" ${iwpriv_file})
    local addr3=$(get_config "ATEBSSID" ${iwpriv_file})

    if [ -z "${addr1}" ]; then
        addr1="00:11:22:33:44:55"
    fi
    if [ -z "${addr2}" ]; then
        addr2="00:11:22:33:44:55"
    fi
    if [ -z "${addr3}" ]; then
        addr3="00:11:22:33:44:55"
    fi

    do_cmd "mt76-test phy${phy_idx} set mac_addrs=${addr1},${addr2},${addr3}"
}

function convert_ibf {
    local cmd=$1
    local param=$2
    local new_cmd=""
    local new_param=$(echo ${param} | sed s/":"/","/g)

    case ${cmd} in
        "ATETxBfInit")
            new_cmd="init"
            new_param="1"
            do_cmd "mt76-test phy${phy_idx} set state=idle"
            ;;
        "ATETxBfGdInit")
            new_cmd="golden_init"
            new_param="1"
            do_cmd "mt76-test phy${phy_idx} set state=idle"
            ;;
        "ATEIBFPhaseComp")
            new_cmd="phase_comp"
            new_param="${new_param}"
            ;;
        "ATEEBfProfileConfig")
            new_cmd="ebf_prof_update"
            ;;
        "ATEIBfProfileConfig")
            new_cmd="ibf_prof_update"
            ;;
        "ATEIBfInstCal")
            new_cmd="phase_cal"
            ;;
        "ATEIBfGdCal")
            local group=${new_param:0:2}
            local group_l_m_h=${new_param:3:2}
            local band_idx=${new_param:6:2}
            local cal_type=${new_param:9:2}
            local version=${new_param:12}
            local lna_level="00"

            # only ibf 2.0 will set version, so add null check for backward compatibility
            if [ -z $version ]; then
                version="00"
            fi

            new_cmd="phase_cal"
            new_param="${group},${group_l_m_h},${band_idx},${cal_type},${lna_level},${version}"
            ;;
        "TxBfTxApply")
            new_cmd="apply_tx"
            ;;
        "ATETxPacketWithBf")
            local bf_on=${new_param:0:2}
            local aid="01"
            local wlan_idx=${new_param:3:2}
            local update="00"
            local tx_count=${new_param:6}

            new_cmd="tx_prep"
            new_param="${bf_on},${aid},${wlan_idx},${update}"
            if [ "${tx_count}" = "00" ]; then
                new_param="${new_param} aid=1 tx_count=10000000 tx_length=1024"
            else
                new_param="${new_param} aid=1 tx_count=${tx_count} tx_length=1024"
            fi
            do_cmd "mt76-test phy${phy_idx} set state=idle"
            ;;
        "TxBfProfileData20MAllWrite")
            new_cmd="prof_update_all"
            ;;
        "ATEIBFPhaseE2pUpdate")
            new_cmd="e2p_update"
            ;;
        "ATEIBFPhaseVerify")
            local group=${new_param:0:2}
            local group_l_m_h=${new_param:3:2}
            local band_idx=${new_param:6:2}
            local phase_cal_type=${new_param:9:2}
            local LNA_gain_level=${new_param:12:2}
            local read_from_e2p=${new_param:15:2}

            do_cmd "mt76-test phy${phy_idx} set txbf_act=phase_comp txbf_param=1,${band_idx},${group},${read_from_e2p},0"
            new_cmd="phase_cal"
            new_param="${group},${group_l_m_h},${band_idx},${phase_cal_type},${LNA_gain_level}"
            ;;
        "TxBfProfileTagRead")
            new_cmd="pfmu_tag_read"
            ;;
        "TxBfProfileTagWrite")
            new_cmd="pfmu_tag_write"
            ;;
        "TxBfProfileTagInValid")
            new_cmd="set_invalid_prof"
            ;;
        "StaRecBfRead")
            new_cmd="sta_rec_read"
            ;;
        "TriggerSounding")
            new_cmd="trigger_sounding"
            ;;
        "StopSounding")
            new_cmd="stop_sounding"
            new_param="0"
            ;;
        "TxBfTxCmd")
            new_cmd="txcmd"
            ;;
        "ATEConTxETxBfGdProc")
            local tx_rate_mode=$(convert_tx_mode ${new_param:0:2})
            local tx_rate_idx=${new_param:3:2}
            local bw=$(echo ${new_param:6:2} | sed 's/^0//')
            local channel=${new_param:9:3}
            local channel2=${new_param:13:3}
            local band=${new_param:17}

            new_cmd="ebf_golden_init"
            do_ate_work "ATESTART"
            do_cmd "mt76-test phy${phy_idx} set state=idle"
            record_config "ATETXBW" ${bw} ${iwpriv_file}
            convert_channel "${channel}:${band}"
            if [ "${bw}" = "5" ]; then
                new_param="1,1"
            else
                new_param="1,0"
            fi
            do_cmd "mt76-test phy${phy_idx} set tx_rate_mode=${tx_rate_mode} tx_rate_idx=${tx_rate_idx} tx_rate_sgi=0"
            ;;
        "ATEConTxETxBfInitProc")
            local tx_rate_mode=$(convert_tx_mode ${new_param:0:2})
            local tx_rate_idx=${new_param:3:2}
            local bw=$(echo ${new_param:6:2} | sed 's/^0//')
            local tx_rate_nss=${new_param:9:2}
            local tx_stream=${new_param:12:2}
            local tx_antenna=$((2 ** tx_stream - 1))
            local tx_power=${new_param:15:2}
            local channel=$(echo ${new_param:18:3} | sed 's/^0//')
            local channel2=${new_param:22:3}
            local band=${new_param:26:1}
            local tx_length=$(echo ${new_param:28:5} | sed 's/^0//')

            new_cmd="ebf_init"
            do_ate_work "ATESTART"
            do_cmd "mt76-test phy${phy_idx} set state=idle"
            record_config "ATETXBW" ${bw} ${iwpriv_file}
            convert_channel "${channel}:${band}"
            new_param="1"
            do_cmd "mt76-test phy${phy_idx} set tx_rate_mode=${tx_rate_mode} tx_rate_idx=${tx_rate_idx} tx_rate_nss=${tx_rate_nss} tx_rate_sgi=0 tx_rate_ldpc=1 tx_power=${tx_power},0,0,0 tx_count=10000000 tx_length=${tx_length} tx_ipg=4 tx_antenna=${tx_antenna}"
            ;;
        *)
    esac

    do_cmd "mt76-test phy${phy_idx} set txbf_act=${new_cmd} txbf_param=${new_param}"

    if [ "${cmd}" = "ATETxPacketWithBf" ]; then
        do_cmd "mt76-test phy${phy_idx} set state=tx_frames"
    elif [ "${cmd}" = "ATEConTxETxBfInitProc" ]; then
        local wlan_idx="1"
        if [ ${connac_ver} != "2" ]; then
            local wlan_idx=$((phy_idx+1))
        fi
        do_cmd "mt76-test phy${phy_idx} set aid=1"
        do_cmd "mt76-test phy${phy_idx} set txbf_act=stop_sounding txbf_param=1"
        do_cmd "mt76-test phy${phy_idx} set txbf_act=update_ch txbf_param=1"
        do_cmd "mt76-test phy${phy_idx} set txbf_act=ebf_prof_update txbf_param=0,0,0"
        do_cmd "mt76-test phy${phy_idx} set txbf_act=apply_tx txbf_param=${wlan_idx},1,0,0,0"
        if [ ${connac_ver} != "2" ]; then
            do_cmd "mt76-test phy${phy_idx} set txbf_act=txcmd txbf_param=1,1,1"
        fi
        do_cmd "mt76-test phy${phy_idx} set txbf_act=pfmu_tag_read txbf_param=0,1"
        do_cmd "mt76-test phy${phy_idx} set txbf_act=sta_rec_read txbf_param=${wlan_idx}"
        do_cmd "mt76-test phy${phy_idx} set txbf_act=trigger_sounding txbf_param=0,1,0,${wlan_idx},0,0,0"
        do_cmd "mt76-test phy${phy_idx} set txbf_act=trigger_sounding txbf_param=2,1,ff,${wlan_idx},0,0,0"
        do_cmd "mt76-test phy${phy_idx} set state=rx_frames"
    elif [ "${cmd}" = "ATEConTxETxBfGdProc" ]; then
        do_cmd "mt76-test phy${phy_idx} set aid=1"
        do_cmd "mt76-test phy${phy_idx} set state=rx_frames"
    elif [ "${cmd}" = "ATETxBfInit" ]; then
        do_cmd "mt76-test phy${phy_idx} set aid=1"
    elif [ "${cmd}" = "ATETxBfGdInit" ]; then
        do_cmd "mt76-test phy${phy_idx} set aid=1"
    elif [ "${cmd}" = "ATEIBFPhaseE2pUpdate" ]; then
        do_cmd "atenl -i phy${phy_idx} -c \"eeprom ibf sync\""
    fi
}

function convert_ruinfo {
    local new_param=$1

    do_cmd "mt76-test phy${phy_idx} set state=idle"
    while [ -n "$new_param" ]
    do
        [ ${new_param:1:1} = ':' ] && {
            new_param=${new_param:2}
        }
        local oIFS="$IFS"; IFS=":"; set -- $new_param; IFS="$oIFS"

        parsing_ruinfo $new_param
        new_param=${new_param:${#1}+1}
    done
}

function parsing_ruinfo {
    local new_param=$1
    local oIFS="$IFS"; IFS="-:"; set -- $new_param; IFS="$oIFS"

    # $7 is Start spatial stream and it should be 0, $9 is alpha, not used
    do_cmd "mt76-test phy${phy_idx} set tx_rate_mode=he_mu tx_rate_sgi=0 tx_ltf=0 ru_alloc=$1 aid=$2 ru_idx=$3\
        tx_rate_idx=$4 tx_rate_ldpc=$5 tx_rate_nss=$6 tx_length=$8"
}

function convert_dfs {
    local cmd=$1
    local param=$2

    case ${cmd} in
        "DfsRxCtrl")
            local offchan_ch="$(echo $param | cut -d ':' -f1)"
            local offchan_bw="$(echo $param | cut -d ':' -f2)"

            if [ "$offchan_bw" = "0" ]; then
                offchan_bw="20"
            elif [ "$offchan_bw" = "1" ]; then
                offchan_bw="40"
            elif [ "$offchan_bw" = "2" ]; then
                offchan_bw="80"
            elif [ "$offchan_bw" = "3" ]; then
                offchan_bw="160"
            fi

            do_cmd "mt76-test phy${phy_idx} set state=idle"
            do_cmd "mt76-test phy${phy_idx} set offchan_ch=${offchan_ch} offchan_bw=${offchan_bw}"
            ;;
        "DfsRxHist")
            local ipi_th="$(echo $param | cut -d ':' -f 1)"
            local ipi_period="$(echo $param | cut -d ':' -f 2)"
	        local ipi_antenna="$(echo $param | cut -d ':' -f 3)"

            if [ -z $ipi_antenna ]; then
                do_cmd "mt76-test phy${phy_idx} set ipi_threshold=${ipi_th} ipi_period=${ipi_period}"
            else
                do_cmd "mt76-test phy${phy_idx} set ipi_threshold=${ipi_th} ipi_period=${ipi_period} ipi_antenna_idx=${ipi_antenna}"
            fi
            ;;
        *)
    esac
}

function do_ate_work() {
    local ate_cmd=$1

    case ${ate_cmd} in
        "ATESTART")
            local if_str=$(ifconfig | grep mon${phy_idx})

            if [ ! -z "${if_str}" -a "${if_str}" != " " ]; then
                print_debug "ATE already starts."
            elif [ ${connac_ver} != "2" ]; then
                do_cmd "mt76-test phy${phy_idx} add mon${phy_idx}"
            else
                do_cmd "iw phy ${interface} interface add mon${phy_idx} type monitor"

                if [ $phy_idx -ge $SOC_start_idx ]; then
                    local end_idx=$SOC_end_idx
                    local start_idx=$SOC_start_idx
                else
                    local end_idx=$((SOC_start_idx-1))
                    local start_idx="0"
                fi

                for phy_index in $( seq $start_idx $end_idx )
                do
                    # "phy#-1" would not be in iw dev, therefore it will grep the line from the phy#${phy_index} to the end
                    local prev_phy_index=$((phy_index-1))
                    local if_num=$(iw dev | awk "/phy#${phy_index}/,/phy#${prev_phy_index}/" | grep -c Interface)
                    local j="1"
                    # avoid del_if_count reset to 0 when start ate on another band in dbdc case
                    local del_if_count=$(get_config "DEL_IF${phy_index}_NUM" ${interface_file})
                    if [ -z "${del_if_count}" ]; then
                        local del_if_count="0"
                    fi

                    for if_count in $( seq 1 $if_num )
                    do
                        local del_if=$(iw dev | awk "/phy#${phy_index}/,/phy#${prev_phy_index}/" | grep Interface | sed -n ${j}p | cut -d " " -f 2)
                        if [  ! -z "${del_if}" ] && [[ "$del_if" != *"mon"* ]]; then
                            do_cmd "iw dev ${del_if} del"
                            del_if_count=$((del_if_count+1))
                            # handle the case of multiple interface in a phy
                            record_config "DEL_IF${phy_index}-${del_if_count}" ${del_if} ${interface_file}
                        else
                            # j add 1 to skip mon interface
                            j=$((j+1))
                        fi
                    done
                    record_config "DEL_IF${phy_index}_NUM" ${del_if_count} ${interface_file}
                done

                do_cmd "ifconfig mon${phy_idx} up"
                do_cmd "iw reg set VV"
            fi
            ;;
        "ATESTOP")
            local if_str=$(ifconfig | grep mon${phy_idx})

            if [ -z "${if_str}" -a "${if_str}" != " " ]; then
                print_debug "ATE does not start."
            elif [ ${connac_ver} != "2" ]; then
                do_cmd "mt76-test phy${phy_idx} del mon${phy_idx}"
            else
                do_cmd "mt76-test ${interface} set state=off"
                do_cmd "iw dev mon${phy_idx} del"

                if [ $phy_idx -ge $SOC_start_idx ]; then
                    local end_idx=$SOC_end_idx
                    local start_idx=$SOC_start_idx
                else
                    local end_idx=$((SOC_start_idx-1))
                    local start_idx="0"
                fi

                # first check its phy and dbdc band phy has monitor interface or not
                # if has at lease one mon interface, then skip adding back normal interface
                local has_mon="0"
                for phy_index in $( seq $start_idx $end_idx )
                do
                     # "phy#-1" would not be in iw dev, therefore it will grep the line from the phy#${phy_index} to the end
                    local prev_phy_index=$((phy_index-1))
                    local has_mon_phy=$(iw dev | awk "/phy#${phy_index}/,/phy#${prev_phy_index}/" | grep "Interface mon")
                    # if this phy interface has mon interface
                    if [ ! -z "${has_mon_phy}" ]; then
                        local has_mon="1"
                    fi
                done

                for phy_index in $( seq $start_idx $end_idx )
                do
                     # "phy#-1" would not be in iw dev, therefore it will grep the line from the phy#${phy_index} to the end
                    local prev_phy_index=$((phy_index-1))
                    local j="1"
                    local add_if_num=$(get_config "DEL_IF${phy_index}_NUM" ${interface_file})
                    if [ -z "${add_if_num}" ]; then
                        local add_if_num="0"
                    fi
                    # if this phy interface (including its dbdc phy) has no mon interface and can find deleted interface in file, then add it back
                    if [ "${has_mon}" == "0" ] && [ $add_if_num -ge "1" ]; then
                        local if_index=$add_if_num
                        # add interface backwards
                        while [ $if_index -gt "0" ]
                        do
                            local add_if=$(get_config "DEL_IF${phy_index}-${if_index}" ${interface_file})
                            do_cmd "iw phy phy${phy_index} interface add ${add_if} type managed"
                            # remove the deleted interface in interface_file since it is added back
                            sed -i "/DEL_IF${phy_index}-${if_index}=/d" ${interface_file}
                            if_index=$((if_index-1))
                        done
                        # remove the number of deleted interface in interface_file since it is all added back
                        sed -i "/DEL_IF${phy_index}_NUM=/d" ${interface_file}
                    fi
                done

                do_cmd "mt76-test ${interface} set aid=0"
            fi

            if [ ${phy_idx} -lt ${SOC_start_idx} ]; then
                sed -i '/_PCIE=/d' ${iwpriv_file}
            elif [ ${phy_idx} -ge ${SOC_start_idx} ]; then
                sed -i '/_SOC=/d' ${iwpriv_file}
            fi
            ;;
        "TXCOMMIT")
            do_cmd "mt76-test ${interface} set aid=1"
            ;;
        "TXFRAME")
            do_cmd "mt76-test ${interface} set state=tx_frames"
            ;;
        "TXSTOP"|"RXSTOP"|"TXCONTSTOP")
            do_cmd "mt76-test ${interface} set state=idle"
            ;;
        "TXREVERT")
            do_cmd "mt76-test ${interface} set aid=0"
            ;;
        "RXFRAME")
            do_cmd "mt76-test ${interface} set state=rx_frames"
            ;;
        "TXCONT")
            do_cmd "mt76-test ${interface} set state=tx_cont"
            ;;
        "GROUPREK")
            do_cmd "mt76-test ${interface} set state=group_prek"
            do_cmd "atenl -i ${interface} -c \"eeprom precal sync group\""
            ;;
        "GROUPREKDump")
            do_cmd "mt76-test ${interface} set state=group_prek_dump"
            ;;
        "GROUPREKClean")
            do_cmd "mt76-test ${interface} set state=group_prek_clean"
            do_cmd "atenl -i ${interface} -c \"eeprom precal group clean\""
            ;;
        "DPD2G")
            do_cmd "mt76-test ${interface} set state=dpd_2g"
            do_cmd "atenl -i ${interface} -c \"eeprom precal sync dpd 2g\""
            ;;
        "DPD5G")
            do_cmd "mt76-test ${interface} set state=dpd_5g"
            do_cmd "atenl -i ${interface} -c \"eeprom precal sync dpd 5g\""
            ;;
        "DPD6G")
            do_cmd "mt76-test ${interface} set state=dpd_6g"
            do_cmd "atenl -i ${interface} -c \"eeprom precal sync dpd 6g\""
            ;;
        "DPDDump")
            do_cmd "mt76-test ${interface} set state=dpd_dump"
            ;;
        "DPDClean")
            do_cmd "mt76-test ${interface} set state=dpd_clean"
            do_cmd "atenl -i ${interface} -c \"eeprom precal dpd clean\""
            ;;
        "RXGAINCAL")
            do_cmd "mt76-test ${interface} set state=rx_gain_cal"
            do_cmd "atenl -i ${interface} -c \"eeprom rx gain sync\""
            ;;
        *)
            print_err "Unknown cmd: ${ate_cmd}"
            ;;
    esac
}

function convert_listmode {
    local tag=$(echo $1 | sed s/,/' '/g | cut -d " " -f 1)

    case ${tag} in
        "16")
            local params=$(echo $1 | sed s/,/' '/g)
            local da=$(echo $params | cut -d " " -f 6)
            local sa=$(echo $params | cut -d " " -f 7)
            local bssid=$(echo $params | cut -d " " -f 8)
            local cmd="list_act=tx_seg"

            cmd="${cmd} tx_length=$(echo $params | cut -d " " -f 5)"
            cmd="${cmd} mac_addrs=${da},${sa},${bssid}"
            cmd="${cmd} tx_rate_stbc=$(echo $params | cut -d " " -f 9)"
            cmd="${cmd} lm_seg_idx=$(echo $params | cut -d " " -f 12)"
            cmd="${cmd} tx_antenna=$(echo $params | cut -d " " -f 15)"
            cmd="${cmd} lm_center_ch=$(echo $params | cut -d " " -f 17)"
            cmd="${cmd} lm_cbw=$(convert_tm_cbw_to_nl $(echo $params | cut -d " " -f 19))"
            cmd="${cmd} tx_pkt_bw=$(convert_tm_cbw_to_nl $(echo $params | cut -d " " -f 20))"
            cmd="${cmd} tx_pri_sel=$(echo $params | cut -d " " -f 21)"
            cmd="${cmd} tx_count=$(echo $params | cut -d " " -f 24)"
            cmd="${cmd} tx_power=$(echo $params | cut -d " " -f 25)"
            cmd="${cmd} tx_rate_mode=$(convert_tx_mode $(echo $params | cut -d " " -f 26))"
            cmd="${cmd} tx_rate_idx=$(echo $params | cut -d " " -f 27)"
            cmd="${cmd} tx_rate_ldpc=$(echo $params | cut -d " " -f 28)"
            cmd="${cmd} tx_ipg=$(echo $params | cut -d " " -f 29)"
            cmd="${cmd} tx_rate_sgi=$(echo $params | cut -d " " -f 30)"
            cmd="${cmd} tx_rate_nss=$(echo $params | cut -d " " -f 31)"
            cmd="${cmd} lm_seg_timeout=$(echo $params | cut -d " " -f 34)"
            do_cmd "mt76-test ${interface} set ${cmd}"
            ;;
        "17")
            do_cmd "mt76-test ${interface} set list_act=tx_start"
            ;;
        "19")
            do_cmd "mt76-test ${interface} set list_act=tx_stop"
            do_cmd "mt76-test ${interface} set list_act=clear_seg"
            ;;
        "20")
            local params=$(echo $1 | sed s/,/' '/g)
            local cmd="list_act=rx_seg"

            cmd="${cmd} mac_addrs=$(echo $params | cut -d " " -f 2)"
            cmd="${cmd} lm_seg_idx=$(echo $params | cut -d " " -f 5)"
            cmd="${cmd} tx_antenna=$(echo $params | cut -d " " -f 8)"
            cmd="${cmd} lm_center_ch=$(echo $params | cut -d " " -f 10)"
            cmd="${cmd} lm_cbw=$(convert_tm_cbw_to_nl $(echo $params | cut -d " " -f 12))"
            cmd="${cmd} tx_pkt_bw=$(convert_tm_cbw_to_nl $(echo $params | cut -d " " -f 13))"
            cmd="${cmd} tx_pri_sel=$(echo $params | cut -d " " -f 14)"
            cmd="${cmd} lm_sta_idx=$(echo $params | cut -d " " -f 15)"
            cmd="${cmd} lm_seg_timeout=$(echo $params | cut -d " " -f 18)"
            do_cmd "mt76-test ${interface} set ${cmd}"
            ;;
        "21")
            do_cmd "mt76-test ${interface} set list_act=rx_start"
            ;;
        "22")
            do_cmd "mt76-test ${interface} set list_act=rx_stat"
            ;;
        "23")
            do_cmd "mt76-test ${interface} set list_act=rx_stop"
            do_cmd "mt76-test ${interface} set list_act=clear_seg"
            ;;
        "24")
            do_cmd "mt76-test ${interface} set list_act=dut_stat"
            ;;
        "25")
            do_cmd "mt76-test ${interface} set list_act=switch_seg"
            ;;
        *)
            print_err "Unknown tag: ${tag}"
            ;;
    esac
}

function dump_usage {
    echo "Usage:"
    echo "  mwctl <interface> set csi ctrl=<opt1>,<opt2>,<opt3>,<opt4> (macaddr=<macaddr>)"
    echo "  mwctl <interface> set csi interval=<interval (us)>"
    echo "  mwctl <interface> dump csi <packet num> <filename>"
    echo "  mwctl <interface> set amnt <index>(0x0~0xf) <mac addr>(xx:xx:xx:xx:xx:xx)"
    echo "  mwctl <interface> dump amnt <index> (0x0~0xf or 0xff)"
    echo "  mwctl <interface> set ap_rfeatures he_gi=<val>"
    echo "  mwctl <interface> set ap_rfeatures he_ltf=<val>"
    echo "  mwctl <interface> set ap_rfeatures trig_type=<enable>,<val> (val: 0-7)"
    echo "  mwctl <interface> set ap_rfeatures ack_policy=<val> (val: 0-4)"
    echo "  mwctl <interface> set ap_wireless fixed_mcs=<val>"
    echo "  mwctl <interface> set ap_wireless ofdma=<val> (0: disable, 1: DL, 2: UL)"
    echo "  mwctl <interface> set ap_wireless nusers_ofdma=<val>"
    echo "  mwctl <interface> set ap_wireless ppdu_type=<val> (0: SU, 1: MU, 4: LEGACY)"
    echo "  mwctl <interface> set ap_wireless add_ba_req_bufsize=<val>"
    echo "  mwctl <interface> set ap_wireless mimo=<val> (0: DL, 1: UL)"
    echo "  mwctl <interface> set ap_wireless ampdu=<enable>"
    echo "  mwctl <interface> set ap_wireless amsdu=<enable>"
    echo "  mwctl <interface> set ap_wireless cert=<enable>"
    echo "  mwctl <interface> set mu onoff=<val> (bitmap- UL MU-MIMO(bit3), DL MU-MIMO(bit2), UL OFDMA(bit1), DL OFDMA(bit0))"
    echo "  mwctl <interface> dump phy_capa"
}

function register_handler {

	local offset=$1
	local val=$2
	local cmd=$3
	local w_cmd="write"

	regidx=/sys/kernel/debug/ieee80211/phy${main_phy_idx}/mt76/regidx
	regval=/sys/kernel/debug/ieee80211/phy${main_phy_idx}/mt76/regval

	echo ${offset} > ${regidx}
	if [[ "${cmd}" == "${w_cmd}" ]]; then
		echo ${val} > ${regval}
	fi

	res=$(cat ${regval} | cut -d 'x' -f 2)
	printf "%s       mac:[%s]:%s\n" ${interface_ori} ${offset} ${res}
}

# main start here
parse_sku
if [ -z ${interface} ]; then
    dump_usage
    exit
elif [[ ${interface} == "ra"* ]]; then
    convert_interface $interface
elif [[ ${interface} == "phy" ]]; then
    # handle mwctl phy phy0 e2p ... case
    interface=$2
    cmd_type=$3
    full_cmd=$4
fi

# get main phy idx
if [ "${phy_idx}" -ge "${SOC_start_idx}" ]; then
    main_phy_idx=${SOC_start_idx}
fi

tmp_work_mode=$(get_config "WORKMODE" ${iwpriv_file})

if [ ! -z ${tmp_work_mode} ]; then
    work_mode=${tmp_work_mode}
fi

cmd=$(echo ${full_cmd} | sed s/=/' '/g | cut -d " " -f 1)
param=$(echo ${full_cmd} | sed s/=/' '/g | cut -d " " -f 2)

if [ "${cmd_type}" = "set" ]; then
    skip=0
    case ${cmd} in
        ## In wifi 7 chipset, testmode & vendor command both use mwctl
        ## Therefore this wrapper would translate it to either mt76-test or mt76-vendor based on the attribute of the command
        ## Translate to mt76-vendor command
        "csi"|"amnt"|"ap_rfeatures"|"ap_wireless"|"mu"|"set_muru_manual_config")
            cert_cmd="$*"

            # FIXME: this line takes approximately 0.08 to 0.1 sec in kerenl 6.6
            mld=$(iw dev | grep 'link ID')
            if [ ! -z "$mld" ]; then
                mld_interface=$(iw dev | grep Interface | awk '{print $2}' | tail -n1)
                if [ $cmd == "ap_rfeatures" ] || [ "$cmd" == "ap_wireless" ]; then

                    band_number=$(echo "$cert_cmd" | grep -o 'band[0-9]*')

                    links_info_base="/sys/kernel/debug/ieee80211/phy0/netdev"
                    links_info_intf="${mld_interface}/mt76_links_info"
                    links_info_cmd="${links_info_base}:${links_info_intf}"

                    band_link_id_info=$(
                        cat "$links_info_cmd" | grep "${band_number}_link_id"
                    )
                    band_link_id=$(
                        echo "$band_link_id_info" | awk -F'=' '{print $2}' | tr -d ' '
                    )

                    cmd_w_link_id=$(echo $* | sed "s/band[0-9]/& -l ${band_link_id}/g")
                    cert_cmd=${cmd_w_link_id}
                fi
                ## convert bandX to mld interface name
                cert_cmd="$(echo ${cert_cmd} | sed 's/band[0-9]/${mld_interface}/')"
            fi

            if [ ${connac_ver} != "2" ]; then
                hostapd_cmd="$(echo $cert_cmd | sed 's/set/raw/')"
                do_cmd "hostapd_cli -i $hostapd_cmd"
            else
                do_cmd "mt76-vendor $cert_cmd"
            fi
            skip=1
            ;;
    	"txbftxsndinfo"|"TxBfTxSndInfo")
            cert_cmd="$*"
            cmd_setting=$(echo $cert_cmd | awk -F'=' '{print $2}')
            snd_cmd_base="/sys/kernel/debug/ieee80211/phy0/mt76/band0/bf_txsnd_info"
            mt76_snd_cmd="echo $cmd_setting > $snd_cmd_base"
            do_cmd "$mt76_snd_cmd"
            skip=1
            ;;
    	"muruDbgInfo")
            cert_cmd="$*"
            cmd_setting=$(echo $cert_cmd | awk -F'=' '{print $2}')
            muru_cmd_base="/sys/kernel/debug/ieee80211/phy0/mt76/muru_dbg"
            mt76_muru_cmd="echo $cmd_setting > $muru_cmd_base"
            do_cmd "$mt76_muru_cmd"
            skip=1
            ;;
        "ATE")
            do_ate_work ${param}

            skip=1
            ;;
        "ATETXCNT"|"ATETXLEN"|"ATETXMCS"|"ATEVHTNSS"|"ATETXNSS"|"ATETXLDPC"|"ATETXSTBC"| \
        "ATEPKTTXTIME"|"ATEIPG"|"ATEDUTYCYCLE"|"ATETXFREQOFFSET")
            cmd_new=$(simple_convert ${cmd})
            if [ "${param_new}" = "undefined" ]; then
                print_err "unknown cmd: ${cmd}"
                exit
            fi
            param_new=${param}
            if [ "${cmd}" = "ATETXCNT" ] && [ "${param}" = "0" ]; then
                param_new="0xFFFFFFFF"
            fi
            ;;
        "ATETXANT"|"ATERXANT")
            cmd_new="tx_antenna"
            param_new=${param}
            ;;
        "ATETXGI")
            if [ ${connac_ver} == "2" ]; then
                tx_mode=$(convert_tx_mode $(get_config "ATETXMODE" ${iwpriv_file}))
                convert_gi ${tx_mode} ${param}
                skip=1
            else
                cmd_new="tx_rate_sgi"
                param_new=${param}
            fi
            ;;
        "ATETXMODE")
            cmd_new="tx_rate_mode"
            param_new=$(convert_tx_mode ${param})
            if [ "${param_new}" = "undefined" ]; then
                print_err "unknown tx mode"
                print_debug "0:cck, 1:ofdm, 2:ht, 4:vht, 8:he_su, 9:he_er, 10:he_tb, 11:he_mu"
                exit
            else
                record_config ${cmd} ${param} ${iwpriv_file}
            fi
            ;;
        "ATETXPOW0"|"ATETXPOW1"|"ATETXPOW2"|"ATETXPOW3"|"ATETXPOW")
            cmd_new="tx_power"
            if [ "${param}" == "127" ]; then
                # for iTest verification
                exit
            fi
            param_new="${param},0,0,0"
            ;;
        "ATEMUAID")
            cmd_new="mu_aid"
            param_new=${param}
            ;;
        "ATETXBW")
            convert_bw ${param}
            skip=1
            ;;
        "ATECHANNEL")
            convert_channel ${param}
            skip=1
            ;;
        "ATERXSTAT")
            convert_rxstat
            skip=1
            ;;
        "ATECTRLBANDIDX")
            change_band_idx ${param}
            skip=1
            ;;
        "ATEDA"|"ATESA"|"ATEBSSID")
            set_mac_addr ${cmd} ${param}
            skip=1
            ;;
        "DfsRxCtrl"|"DfsRxHist")
            convert_dfs ${cmd} ${param}
            skip=1
            ;;
        "ATETxBfInit"|"ATETxBfGdInit"|"ATEIBFPhaseComp"|"ATEEBfProfileConfig"|"ATEIBfProfileConfig"| \
        "TxBfTxApply"|"ATETxPacketWithBf"|"TxBfProfileData20MAllWrite"|"ATEIBfInstCal"| \
        "ATEIBfGdCal"|"ATEIBFPhaseE2pUpdate"|"TriggerSounding"|"StopSounding"|"TxBfTxCmd"| \
        "StaRecBfRead"|"TxBfProfileTagInValid"|"TxBfProfileTagWrite"|"TxBfProfileTagRead"| \
        "ATEIBFPhaseVerify"|"ATEConTxETxBfGdProc"|"ATEConTxETxBfInitProc")
            convert_ibf ${cmd} ${param}
            skip=1
            ;;
        "bufferMode"|"buffermode")
            if [ "${param}" = "2" ]; then
                do_cmd "atenl -i ${interface} -c \"eeprom update buffermode\""
            fi
            skip=1
            ;;
        "ResetCounter"|"ATERXSTATRESET")
            skip=1
            ;;
        "WORKMODE")
            record_config "WORKMODE" ${param} ${iwpriv_file}
            echo "Entering ${param} mode in iwpriv"
            skip=1
            ;;
        "ATERUINFO")
            convert_ruinfo ${param}
            skip=1
            ;;
        "ATELISTMODE")
            convert_listmode ${param}
            skip=1
            ;;
        *)
            print_err "Unknown command to set: ${cmd}"
            skip=1
    esac

    if [ "${skip}" != "1" ]; then
        do_cmd "mt76-test ${interface} set ${cmd_new}=${param_new}"
    fi

elif [ "${cmd_type}" = "show" ]; then
    if [ "${cmd}" = "wtbl" ]; then
        wlan_idx=/sys/kernel/debug/ieee80211/phy${main_phy_idx}/mt76/wlan_idx
        wtbl_info=/sys/kernel/debug/ieee80211/phy${main_phy_idx}/mt76/wtbl_info

        do_cmd "echo ${param} > ${wlan_idx}"
        do_cmd "cat ${wtbl_info}"
    elif [ "${cmd}" = "ATERXSTAT" ]; then
        convert_rxstat
    else
        do_cmd "mt76-test ${interface} dump"
        do_cmd "mt76-test ${interface} dump stats"
    fi

elif [ "${cmd_type}" = "e2p" ]; then
    # support multiple read write
    # eeprom offset write
    if [[ ${full_cmd} == *"="* ]]; then
        IFS=,
        for tuple in $full_cmd
        do
            cmd=$(echo ${tuple} | sed s/=/' '/g | cut -d " " -f 1)
            param=$(echo ${tuple} | sed s/=/' '/g | cut -d " " -f 2)
            offset=$(printf "0x%s" ${cmd})
            val=$(printf "0x%s" ${param})
            tmp=$((${val} & 0xff))
            tmp=$(printf "0x%x" ${tmp})
            do_cmd "atenl -i ${interface} -c \"eeprom set ${offset}=${tmp}\""

            offset=$((${offset}))
            offset=$(expr ${offset} + "1")
            offset=$(printf "0x%x" ${offset})
            tmp=$(((${val} >> 8) & 0xff))
            tmp=$(printf "0x%x" ${tmp})
            do_cmd "atenl -i ${interface} -c \"eeprom set ${offset}=${tmp}\""
        done
    else
        IFS=,
        for tuple in $full_cmd
        do
            cmd=$(echo ${tuple} | sed s/=/' '/g | cut -d " " -f 1)
            param=$(echo ${tuple} | sed s/=/' '/g | cut -d " " -f 2)
            offset=$(printf "0x%s" ${cmd})
            val=$(printf "0x%s" ${param})
            v1=$(do_cmd "atenl -i ${interface} -c \"eeprom read ${param}\"")
            v1=$(echo "${v1}" | grep "val =" | cut -d '(' -f 2 | grep -o -E '[0-9]+')

            tmp=$(printf "0x%s" ${param})
            tmp=$((${tmp}))
            param2=$(expr ${tmp} + "1")
            param2=$(printf "%x" ${param2})
            v2=$(do_cmd "atenl -i ${interface} -c \"eeprom read ${param2}\"")
            v2=$(echo "${v2}" | grep "val =" | cut -d '(' -f 2 | grep -o -E '[0-9]+')

            param=$(printf "0x%s" ${param})
            param=$(printf "%04x" ${param})
            param=$(echo $param | tr 'a-z' 'A-Z')
            printf "%s       e2p:\n" ${interface_ori}
            printf "[0x%s]:0x%02x%02x\n" ${param} ${v2} ${v1}
        done
    fi

elif [ "${cmd_type}" = "mac" ]; then
    offset=$(printf "0x%s" ${cmd})
    val=$(printf "0x%s" ${param})

    # reg write
    if [[ ${full_cmd} == *"="* ]]; then
        register_handler ${offset} ${val} "write"
    else
	start_addr=$(echo ${full_cmd} | sed s/-/' '/g | cut -d " " -f 1)
	end_addr=$(echo ${full_cmd} | sed s/-/' '/g | cut -d " " -f 2)
	loop=$((0x${end_addr}-0x${start_addr}))

	if [[ ${loop} == "0" ]]; then
		register_handler ${offset} ${val}
        else
		i=0
		while [ $i -le $loop ]; do
			addr=$((0x${start_addr}+$i))
			offset=$(printf "0x%x" ${addr})
			register_handler ${offset} ${val}
			i=$(($i + 4))
		done
       fi
    fi

## dump command is only for vendor commands
elif [ "${cmd_type}" = "dump" ]; then
    do_cmd "mt76-vendor $*"
elif [ "${cmd_type}" = "switch" ]; then
    eeprom_mode_file=/sys/kernel/debug/ieee80211/phy0/mt76/eeprom_mode
    eeprom_mode=$(cat ${eeprom_mode_file} | grep "mode" | sed -n 2p | xargs)
    eeprom_testmode_offset="1af"
    testmode_enable="0"

    if [ ${connac_ver} == "3" ]; then
        chip_module="mt7996e"
    elif [ ${connac_ver} == "5" ]; then
        chip_module="mt7999e"
    else
        return
    fi

    if [ "${cmd}" = "testmode" ]; then
        testmode_enable="1"
        do_cmd "wifi down"
    fi

    do_cmd "uci set wireless.radio0.disabled=${testmode_enable}"
    do_cmd "uci set wireless.radio1.disabled=${testmode_enable}"
    do_cmd "uci set wireless.radio2.disabled=${testmode_enable}"
    do_cmd "uci commit"

    if [ "${eeprom_mode}" = "flash mode" ]; then
        ## flash mode should set eeprom testmode offset bit
        ## efuse/bin file/default bin mode rely on module param only
        do_cmd "atenl -i ${interface} -c \"eeprom set 0x${eeprom_testmode_offset}=0x${testmode_enable}\""
        ## If has no precal, it would not affect
        do_cmd "atenl -i ${interface} -c \"eeprom precal sync\""
        do_cmd "atenl -i ${interface} -c \"sync eeprom all\""
    fi

    do_cmd "rmmod ${chip_module}"
    do_cmd "rmmod mt76-connac-lib"
    do_cmd "rmmod mt76"
    do_cmd "rmmod mac80211"
    do_cmd "rmmod cfg80211"
    do_cmd "rmmod compat"
    do_cmd "insmod compat"
    do_cmd "insmod cfg80211"
    do_cmd "insmod mac80211"
    do_cmd "insmod mt76"
    do_cmd "insmod mt76-connac-lib"
    do_cmd "insmod ${chip_module} testmode_enable=${testmode_enable}"
    do_cmd "sleep 5"
    do_cmd "killall hostapd"
    do_cmd "killall netifd"
else
    print_err "Unknown command type"
fi
