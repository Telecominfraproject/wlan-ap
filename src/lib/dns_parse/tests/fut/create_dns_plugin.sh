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
                --bridge=<the ovs bridge>
                --provider_plugin=<the web cat provider>
                --provider=<the web cat provider>
                --intf=<the tap interface>
                --tx_intf=<the tx intf>
                --of_port=<openflow port>
EOF
}

# Create tap interface
gen_tap_cmd() {
    cat << EOF
ovs-vsctl add-port ${bridge} ${intf}  \
          -- set interface ${intf}  type=internal \
          -- set interface ${intf}  ofport_request=${ofport}
EOF
}

# Bring tap interface up
tap_up_cmd() {
    cat << EOF
ip link set ${intf} up
EOF
}

# Mark the interface no-flood, only the traffic matching the flow filter
# will hit the plugin
gen_no_flood_cmd() {
    cat << EOF
ovs-ofctl mod-port ${bridge} ${intf} no-flood
EOF
}

# Create the openflow rule for the egress traffic
gen_oflow_egress_cmd() {
    cat << EOF
ovsh i Openflow_Config \
     token:=${of_out_token} \
     bridge:=${bridge} \
     table:=0 \
     priority:=${priority} \
     rule:=${of_out_rule} \
     action:="normal,output:${ofport}"
EOF
}


# Create the openflow rule for the ingress traffic
gen_oflow_ingress_cmd() {
    cat <<EOF
ovsh i Openflow_Config \
     token:=${of_in_token} \
     bridge:=${bridge} \
     table:=0 \
     priority:=${priority} \
     rule:=${of_in_rule} \
     action:="output:${ofport}"
EOF
}

# Create the openflow rule for the egress traffic
gen_oflow_tx_cmd() {
    cat << EOF
ovsh i Openflow_Config \
     token:=${of_tx_token} \
     bridge:=${bridge} \
     table:=0 \
     priority:=${tx_priority} \
     rule:=${of_tx_rule} \
     action:="normal"
EOF
}


# Create a tag entry for client mac
gen_tag_cmd() {
    cat << EOF
ovsh i Openflow_Tag \
       name:=${tag_name} \
       cloud_value:='["set",["${client_mac}"]]'
EOF
}


# Create a FSM config entry. Resorting to json format due some
# unexpected map programming errors.
gen_fsmc_cmd() {
    cat << EOF
["Open_vSwitch",
    {
        "op": "insert",
        "table": "Flow_Service_Manager_Config",
        "row": {
               "handler": "${fsm_handler}",
               "if_name": "${intf}",
               "pkt_capt_filter": "${filter}",
               "plugin": "${plugin}",
               "type": "parser",
               "other_config":
                        ["map",[
                        ["mqtt_v","${mqtt_v}"],
                        ["dso_init","${dso_init}"],
                        ["provider_plugin","${provider_plugin}"],
                        ["policy_table","${policy_table}"],
                        ["wc_health_stats_topic","${mqtt_hs}"],
                        ["wc_health_stats_interval_secs","10"]
                        ]]
        }
    }
]
EOF
}

# Create a dev webpulse policy entry
gen_dev_webpulse_policy() {
    cat <<EOF
["Open_vSwitch",
    {
        "op": "insert",
        "table": "FSM_Policy",
        "row": {
               "policy": "dev_webpulse",
               "name": "dev_wp_adult",
               "idx": 0,
               "action": "drop",
               "log": "blocked",
               "fqdncat_op": "in",
               "fqdncats": ["set",
                           [1,20]
                           ],
               "redirect": "A-18.204.152.241"
       }
   }
]
EOF
}


# Create a dev webpulse policy entry
gen_dev_brightcloud_policy() {
    cat <<EOF
["Open_vSwitch",
    {
        "op": "insert",
        "table": "FSM_Policy",
        "row": {
               "policy": "dev_brightcloud",
               "name": "dev_br_adult",
               "idx": 0,
               "action": "drop",
               "log": "blocked",
               "fqdncat_op": "in",
               "fqdncats": ["set",
                           [11]
                           ],
               "redirect": "A-18.204.152.241"
       }
   }
]
EOF
}

# get pod's location ID
get_location_id() {
    ovsh s AWLAN_Node mqtt_headers | \
        awk -F'"' '{for (i=1;i<NF;i++) {if ($(i)=="locationId"){print $(i+2)}}}'
}

# get pod's node ID
get_node_id() {
    ovsh s AWLAN_Node mqtt_headers | \
        awk -F'"' '{for (i=1;i<NF;i++) {if ($(i)=="nodeId"){print $(i+2)}}}'
}

# create br-home.tx if it does not exist
set_br_home_tx() {
    ip link show dev br-home.tx
    ret=$?
    if [ ${ret} -eq 0 ]; then # br-home.tx exists
        return
    fi
    intf=${tx_intf}
    ofport=3002
    $(gen_tap_cmd)
    $(tap_up_cmd)
}


# h for help, long options otherwise
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
                provider=?* )
                    val=${LONG_OPTARG}
                    opt=${OPTARG%=$val}
                    # echo "Parsing option: '--${opt}', value: '${val}'"
                    PROVIDER=$val
                    ;;
                provider_plugin=?* )
                    val=${LONG_OPTARG}
                    opt=${OPTARG%=$val}
                    # echo "Parsing option: '--${opt}', value: '${val}'"
                    PROVIDER_PLUGIN=$val
                    ;;
                policy_table=?* )
                    val=${LONG_OPTARG}
                    opt=${OPTARG%=$val}
                    # echo "Parsing option: '--${opt}', value: '${val}'"
                    POLICY_TABLE=$val
                    ;;
                intf=?* )
                    val=${LONG_OPTARG}
                    opt=${OPTARG%=$val}
                    # echo "Parsing option: '--${opt}', value: '${val}'"
                    INTF=$val
                    ;;
                tx_intf=?* )
                    val=${LONG_OPTARG}
                    opt=${OPTARG%=$val}
                    # echo "Parsing option: '--${opt}', value: '${val}'"
                    TX_INTF=$val
                    ;;
                ofport=?* )
                    val=${LONG_OPTARG}
                    opt=${OPTARG%=$val}
                    # echo "Parsing option: '--${opt}', value: '${val}'"
                    OF_PORT=$val
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

client_mac=${MAC}
bridge=${BRIDGE:-br-home}
provider=${PROVIDER:-brightcloud}
provider_plugin=${PROVIDER_PLUGIN:-brightcloud}
policy_table=${POLICY_TABLE:-brightcloud}
intf=${INTF:-br-home.tdns}
tx_intf=${TX_INTF:-br-home.tx}
ofport=${OF_PORT:-3001}


# of_out_token: openflow_config egress rule name. Must start with 'dev' so the
# controller leaves it alone
of_out_token=dev_flow_dns_out
of_out_rule="dl_src=${client_mac},udp,tp_dst=53"

# of_in_token: openflow_config ingress rule name. Must start with 'dev' so the
# controller leaves it alone
of_in_token=dev_flow_dns_in
of_in_rule="dl_dst=${client_mac},udp,tp_src=53"

# tag_name: openflow_tag name. Nust start with 'dev' so the controller
# leaves it alone
tag_name=dev_tag_dns

# Flow_Service_Manager_Config parameters
filter="udp port 53"
plugin=/usr/plume/lib/libfsm_dns.so
dso_init=dns_plugin_init
fsm_handler=dev_dns # must start with 'dev' so the controller leaves it alone

priority=200 # must be higher than controller pushed rules
tx_priority=210 # must be higher than controller pushed rules
of_tx_token=dev_dns_fwd_flow
in_port=$(ovs-vsctl get Interface ${tx_intf} ofport)
of_tx_rule="in_port=${in_port}"

# Check required commands
check_cmd 'ovsh'
check_cmd 'ovs-vsctl'
check_cmd 'ip'
check_cmd 'ovs-ofctl'

# Enforce filtering on mac address wifi device
if [ -z ${client_mac} ]; then
    usage
    exit 1
fi

location_id=$(get_location_id)
node_id=$(get_node_id)
mqtt_v="dev-test/${fsm_handler}/${node_id}/${location_id}"
mqtt_hs="dev-test/${fsm_handler}/health_stats/${node_id}/${location_id}"

$(gen_tap_cmd)
$(tap_up_cmd)
$(gen_no_flood_cmd)
$(gen_oflow_egress_cmd)
$(gen_oflow_ingress_cmd)
$(gen_tag_cmd)
eval ovsdb-client transact \'$(gen_fsmc_cmd)\'
eval ovsdb-client transact \'$(gen_dev_brightcloud_policy)\'
eval ovsdb-client transact \'$(gen_dev_webpulse_policy)\'
set_br_home_tx
$(gen_oflow_tx_cmd)

