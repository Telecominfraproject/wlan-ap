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


prog=$0

# usage
usage() {
  cat <<EOF
          Usage: ${prog} --mac=<the client device> <[options]>
          Options:
                -h this mesage
                --bridge=<the ovs bridge>
                --intf=<the tap interface>
EOF
}

optspec="h-:"


while getopts "$optspec" optchar; do
    case "${optchar}" in
        -) LONG_OPTARG="${OPTARG#*=}"
           case "${OPTARG}" in
                mac=?* )
                    val=${LONG_OPTARG}
                    opt=${OPTARG%=$val}
                    # echo "Parsing option: '--${opt}', value: '${val}'"
                    MAC=$val
                    ;;
                bridge=?* )
                    val=${LONG_OPTARG}
                    opt=${OPTARG%=$val}
                    # echo "Parsing option: '--${opt}', value: '${val}'"
                    BRIDGE=$val
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

intf=${INTF:-br-home.tdns}
bridge=${BRIDGE:-br-home}
fsm_handler=dev_dns
of_out_token=dev_flow_dns_out
of_in_token=dev_flow_dns_in
of_tx_token=dev_dns_fwd_flow
tag_name=dev_tag_dns

check_cmd() {
    cmd=$1
    path_cmd=$(which ${cmd})
    if [ -z ${path_cmd} ]; then
        echo "Error: could not find ${cmd} command in path"
        exit 1
    fi
    echo "found ${cmd} as ${path_cmd}"
}

# Check required commands
check_cmd 'ovsh'
check_cmd 'ovs-vsctl'
check_cmd 'ip'
check_cmd 'ovs-ofctl'

ovsh d Flow_Service_Manager_Config -w handler==${fsm_handler}
ovsh d Openflow_Tag -w name==${tag_name}
ovsh d Openflow_Config -w token==${of_in_token}
ovsh d Openflow_Config -w token==${of_out_token}
ovsh d Openflow_Config -w token==${of_tx_token}
ovs-vsctl del-port ${bridge} ${intf}
ovsh d FSM_Policy -w policy==dev_webpulse
ovsh d FSM_Policy -w policy==dev_brightcloud
ovsh d Flow_Service_Manager_Config -w handler==dev_webpulse
ovsh d Flow_Service_Manager_Config -w handler==dev_brightcloud
