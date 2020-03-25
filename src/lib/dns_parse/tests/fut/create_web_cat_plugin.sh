#!/bin/sh

# Copyright (c) 2015, Plume Design Inc. All rights reserved.
# 
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#    1. Redistributions of source code must retain the above copyright
#       notice, this list of conditions and the following disclaimer.
#    2. Redistributions in binary form must reproduce the above copyright
#       notice, this list of conditions and the following disclaimer in the
#       documentation and/or other materials provided with the distribution.
#    3. Neither the name of the Plume Design Inc. nor the
#       names of its contributors may be used to endorse or promote products
#       derived from this software without specific prior written permission.
# 
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL Plume Design Inc. BE LIABLE FOR ANY
# DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
# (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
# LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
# ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
# SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.


# Series of generic routines updating ovsdb tables.
# TBD: It would make sense to commonize them all.

prog=$0

# Check if a specific command is in the path. Bail if not found.
check_cmd() {
    cmd=$1
    path_cmd=$(which ${cmd})
    if [ -z ${path_cmd} ]; then
        echo "Error: could not find ${cmd} command in path"
        exit 1
    fi
    echo "found ${cmd} as ${path_cmd}"
}

# usage
usage() {
  cat <<EOF
          Usage: ${prog} --mac=<the client device> <[options]>
          Options:
                -h this mesage
                --web_cat_provider=<dev_brightcloud | dev_webpulse>
                --server=<categorization service URL>
                --device=<device type>
                --oem=<OEM identifier>
                --uid=<device id>
EOF
}


# Create a FSM brightcloud config entry. Resorting to json format due some
# unexpected map programming errors.
gen_fsmc_brightcloud_cmd() {
    cat << EOF
["Open_vSwitch",
    {
        "op": "insert",
        "table": "Flow_Service_Manager_Config",
        "row": {
               "handler": "${provider_plugin}",
               "type": "web_cat_provider",
               "plugin": "/usr/plume/lib/libfsm_brightcloud.so",
               "other_config":
                        ["map",[
                        ["bc_dbserver","${provider_server}"],
                        ["bc_device","${device}"],
                        ["bc_oem","${oem}"],
                        ["bc_server","${provider_server}"],
                        ["bc_uid","${uid}"],
                        ["dso_init","brightcloud_plugin_init"]
                        ]]
         }
    }
]
EOF
}


# Create a FSM webpulse config entry. Resorting to json format due some
# unexpected map programming errors.
gen_fsmc_webpulse_cmd() {
    cat << EOF
["Open_vSwitch",
    {
        "op": "insert",
        "table": "Flow_Service_Manager_Config",
        "row": {
               "handler": "${provider_plugin}",
               "type": "web_cat_provider",
               "plugin": "/usr/plume/lib/libfsm_webpulse.so",
               "other_config":
                        ["map",[
                        ["dso_init","webpulse_plugin_init"]
                        ]]
        }
    }
]
EOF
}

# Check credential
check_credential() {
    type=$1
    cred=$2
    if [ -z ${cred} ]; then
        echo "No ${type} passed. Exiting"
        exit 1
    fi
}

# Check credentials
check_credentials() {
    check_credential "server" ${provider_server}
    check_credential "device" ${device}
    check_credential "oem" ${oem}
    check_credential "uid" ${uid}
}


# h for help, long options otherwise
optspec="h-:"
while getopts "$optspec" optchar; do
    case "${optchar}" in
        -) LONG_OPTARG="${OPTARG#*=}"
           case "${OPTARG}" in
                web_cat_provider=?* )
                    val=${LONG_OPTARG}
                    opt=${OPTARG%=$val}
                    PROVIDER_PLUGIN=$val
                    ;;
                server=?* )
                    val=${LONG_OPTARG}
                    opt=${OPTARG%=$val}
                    PROVIDER_SERVER=$val
                    ;;
                device=?* )
                    val=${LONG_OPTARG}
                    opt=${OPTARG%=$val}
                    DEVICE=$val
                    ;;
                oem=?* )
                    val=${LONG_OPTARG}
                    opt=${OPTARG%=$val}
                    OEM=$val
                    ;;
                uid=?* )
                    val=${LONG_OPTARG}
                    opt=${OPTARG%=$val}
                    UID=$val
                    ;;
                *)
                    if [ "$OPTERR" = 1 ] && [ "${optspec:0:1}" != ":" ]; then
                        echo "Unknown option --${OPTARG}" >&2
                    fi
                    ;;
            esac;;
        h)
            usage
            exit 2
            ;;
        *)
            if [ "$OPTERR" != 1 ] || [ "${optspec:0:1}" = ":" ]; then
                echo "Non-option argument: '-${OPTARG}'"
            fi
            ;;
    esac
done

provider_plugin=${PROVIDER_PLUGIN}
provider_server=${PROVIDER_SERVER}
device=${DEVICE}
oem=${OEM}
uid=${UID}

if [ -z ${provider_plugin} ]; then
    usage
    exit 1
fi

if [ ${provider_plugin} == "dev_brightcloud" ]; then
    check_credentials
    eval ovsdb-client transact \'$(gen_fsmc_brightcloud_cmd)\'
elif [ ${provider_plugin} == "dev_webpulse" ]; then
    eval ovsdb-client transact \'$(gen_fsmc_webpulse_cmd)\'
else
    echo "Unknown provider ${provider_plugin}. Exiting"
    exit 1
fi
