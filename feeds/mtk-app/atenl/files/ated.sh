#!/bin/ash
# This script is used for wrapping atenl daemon to ated
# 0 is normal mode, 1 is used for doing specific commands such as "sync eeprom all"

work_mode="RUN" # RUN/PRINT/DEBUG
mode="0"
add_quote="0"
cmd="atenl"
interface=""
phy_idx=0
ated_file="/tmp/interface"
iwpriv_file="/tmp/iwpriv_wrapper"
SOC_start_idx="0"
SOC_end_idx="0"
connac_ver="0"

function do_cmd() {
    case ${work_mode} in
        "RUN")
            eval "$1"
            ;;
        "PRINT")
            echo "$1"
            ;;
        "DEBUG")
            eval "$1"
            echo "$1"
            ;;
    esac
}

function record_config() {
    local config=$1
    local tmp_file=$3

    # check it is SOC(mt7986)/mt7996/mt7992/mt7990/mt7999 or
    # PCIE card (mt7915/7916), and write its config
    if [ ${tmp_file} != ${ated_file} ]; then
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
    if [ ${tmp_file} != ${ated_file} ]; then
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
    SOC_start_idx=$(get_config "STARTIDX" ${ated_file})
    SOC_end_idx=$(get_config "ENDIDX" ${ated_file})
    connac_ver=$(get_config "CONNAC_VER" ${ated_file})
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
            echo "      echo STARTIDX=0 >> ${ated_file}"
            echo "      echo ENDIDX=1 >> ${ated_file}"
            echo "      echo CONNAC_VER=2 >> ${ated_file}"
            echo "For AX7800:"
            echo "      echo STARTIDX=2 >> ${ated_file}"
            echo "      echo ENDIDX=3 >> ${ated_file}"
            echo "      echo CONNAC_VER=2 >> ${ated_file}"
            echo "For AX8400:"
            echo "      echo STARTIDX=1 >> ${ated_file}"
            echo "      echo ENDIDX=2 >> ${ated_file}"
            echo "      echo CONNAC_VER=2 >> ${ated_file}"
            echo "For MT7996:"
            echo "      echo STARTIDX=0 >> ${ated_file}"
            echo "      echo ENDIDX=2 >> ${ated_file}"
            echo "      echo CONNAC_VER=3 >> ${ated_file}"
            echo "For MT7992:"
            echo "      echo STARTIDX=0 >> ${ated_file}"
            echo "      echo ENDIDX=1 >> ${ated_file}"
            echo "      echo CONNAC_VER=3 >> ${ated_file}"
            echo "For MT7990:"
            echo "      echo STARTIDX=0 >> ${ated_file}"
            echo "      echo ENDIDX=1 >> ${ated_file}"
            echo "      echo CONNAC_VER=3 >> ${ated_file}"
            echo "For MT7999:"
            echo "      echo STARTIDX=0 >> ${ated_file}"
            echo "      echo ENDIDX=2 >> ${ated_file}"
            echo "      echo CONNAC_VER=5 >> ${ated_file}"
            exit 0
        fi
        record_config "STARTIDX" ${SOC_start_idx} ${ated_file}
        record_config "ENDIDX" ${SOC_end_idx} ${ated_file}
        record_config "CONNAC_VER" ${connac_ver} ${ated_file}
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

for i in "$@"
do
    if [ "$i" = "-c" ]; then
        cmd="${cmd} -c"
        mode="1"
        add_quote="1"
    elif [ "${add_quote}" = "1" ]; then
        cmd="${cmd} \"${i}\""
        add_quote="0"
    else
        if [[ ${i} == "ra"* ]]; then
            parse_sku
            convert_interface $i
            cmd="${cmd} ${interface}"
        else
            cmd="${cmd} ${i}"
        fi
    fi
done

if [ "$mode" = "0" ]; then
    killall atenl > /dev/null 2>&1
fi

do_cmd "${cmd}"
