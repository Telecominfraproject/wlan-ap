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
    echo "Usage: $0 client_mac [bridge_name] [interface name] [ofport]"
}

# Delete all flows in br-home for testing purpose
del_flows_br_home_cmd() {
    cat << EOF
ovs-ofctl del-flows br-home
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

# Create ddpi table = 0 rules

gen_oflow_ddpi_cmd1() {
    cat << EOF
ovsh i Openflow_Config \
     token:=${of_out_token} \
     bridge:=${bridge} \
     table:=0 \
     priority:=100 \
     action:="resubmit(,8)"
EOF
}


gen_oflow_ddpi_cmd2() {
    cat << EOF
ovsh i Openflow_Config \
     token:=${of_out_token} \
     bridge:=${bridge} \
     table:=0 \
     priority:=101 \
     rule:="dl_src=\${#dpi_mac}" \
     action:="resubmit(,9)"
EOF
}

gen_oflow_ddpi_cmd3() {
    cat << EOF
ovsh i Openflow_Config \
     token:=${of_out_token} \
     bridge:=${bridge} \
     table:=0 \
     priority:=0 \
     action:="NORMAL"
EOF
}


#Move Table=0 to Table=8
gen_offlow_del_table0() {
    cat << EOF
ovsh d Openflow_Config \
     -w table==0
EOF
}

#main_t_dns_ipv6_src
gen_offlow_table8_cmd1() {
    cat << EOF
ovsh i Openflow_Config \
     token:=${of_out_token} \
     bridge:=${bridge} \
     table:=8 \
     priority:=20 \
     rule:="udp6,tp_src=53" \
     action:="resubmit(,6)"
EOF
}

#main_t_dns_ipv4_auto_dst
gen_offlow_table8_cmd2() {
    cat << EOF
ovsh i Openflow_Config \
     token:=${of_out_token} \
     bridge:=${bridge} \
     table:=8 \
     priority:=10 \
     rule:="udp,tp_dst=53"
     action:="resubmit(,6)"
EOF
}

#node_eth_dns_ipv4_req
gen_offlow_table8_cmd3() {
    cat << EOF
ovsh i Openflow_Config \
     token:=${of_out_token} \
     bridge:=${bridge} \
     table:=8 \
     priority:=40 \
     rule:="udp,tp_dst=53,dl_src=\${#node_eth}"
     action:="NORMAL"
EOF
}

#main_t_df
gen_offlow_table8_cmd4() {
    cat << EOF
ovsh i Openflow_Config \
     token:=${of_out_token} \
     bridge:=${bridge} \
     table:=8 \
     priority:=30 \
     rule:="dl_src=\${frozen}"
     action:="resubmit(,3)"
EOF
}

#main_t_dhcp_ipv6_src
gen_offlow_table8_cmd5() {
    cat << EOF
ovsh i Openflow_Config \
     token:=${of_out_token} \
     bridge:=${bridge} \
     table:=8 \
     priority:=20 \
     rule:="udp6,tp_src=547"
     action:="resubmit(,6)"
EOF
}

#main_t_dhcp_ipv6_dst
gen_offlow_table8_cmd6() {
    cat << EOF
ovsh i Openflow_Config \
     token:=${of_out_token} \
     bridge:=${bridge} \
     table:=8 \
     priority:=20 \
     rule:="udp6,tp_dst=547"
     action:="resubmit(,6)"
EOF
}

#main_t_dns_ipv4_src
gen_offlow_table8_cmd7() {
    cat << EOF
ovsh i Openflow_Config \
     token:=${of_out_token} \
     bridge:=${bridge} \
     table:=8 \
     priority:=20 \
     rule:="udp,tp_src=53"
     action:="resubmit(,6)"
EOF
}

#node_eth_dns_ipv6_req
gen_offlow_table8_cmd8() {
    cat << EOF
ovsh i Openflow_Config \
     token:=${of_out_token} \
     bridge:=${bridge} \
     table:=8 \
     priority:=40 \
     rule:="udp6,tp_dst=53,dl_src=\${#node_eth}"
     action:="NORMAL"
EOF
}

#main_t_homepass
gen_offlow_table8_cmd9() {
    cat << EOF
ovsh i Openflow_Config \
     token:=${of_out_token} \
     bridge:=${bridge} \
     table:=8 \
     priority:=0 \
     rule:="["set",[]]"
     action:="resubmit(,5)"
EOF
}

#main_t_dns_ipv4_dst
gen_offlow_table8_cmd10() {
    cat << EOF
ovsh i Openflow_Config \
     token:=${of_out_token} \
     bridge:=${bridge} \
     table:=8 \
     priority:=20 \
     rule:="udp,tp_dst=53"
     action:="resubmit(,6)"
EOF
}

#main_t_iapp_l2uf
gen_offlow_table8_cmd11() {
    cat << EOF
ovsh i Openflow_Config \
     token:=${of_out_token} \
     bridge:=${bridge} \
     table:=8 \
     priority:=45 \
     rule:="dl_dst=01:00:00:00:00:00/01:00:00:00:00:00,dl_type=0x05ff"
     action:="NORMAL"
EOF
}

#main_t_dns_ipv4_skip_dst
gen_offlow_table8_cmd12() {
    cat << EOF
ovsh i Openflow_Config \
     token:=${of_out_token} \
     bridge:=${bridge} \
     table:=8 \
     priority:=15 \
     rule:="udp,tp_dst=53,dl_src=\$[dns-exclude]"
     action:="resubmit(,5)"
EOF
}

#main_t_dhcp_ipv4_dst
gen_offlow_table8_cmd13() {
    cat << EOF
ovsh i Openflow_Config \
     token:=${of_out_token} \
     bridge:=${bridge} \
     table:=8 \
     priority:=20 \
     rule:="udp,tp_dst=67"
     action:="resubmit(,6)"
EOF
}

#node_eth_dns_ipv6_res
gen_offlow_table8_cmd14() {
    cat << EOF
ovsh i Openflow_Config \
     token:=${of_out_token} \
     bridge:=${bridge} \
     table:=8 \
     priority:=40 \
     rule:="udp6,tp_src=53,dl_dst=\${#node_eth}"
     action:="NORMAL"
EOF
}

#main_t_dns_ipv6_skip_dst
gen_offlow_table8_cmd15() {
    cat << EOF
ovsh i Openflow_Config \
     token:=${of_out_token} \
     bridge:=${bridge} \
     table:=8 \
     priority:=15 \
     rule:="udp6,tp_dst=53,dl_src=\$[dns-exclude]"
     action:="resubmit(,5)"
EOF
}


#main_t_dns_ipv6_dst
gen_offlow_table8_cmd16() {
    cat << EOF
ovsh i Openflow_Config \
     token:=${of_out_token} \
     bridge:=${bridge} \
     table:=8 \
     priority:=20 \
     rule:="udp6,tp_dst=53"
     action:="resubmit(,6)"
EOF
}

#main_t_dns_ipv6_auto_dst
gen_offlow_table8_cmd17() {
    cat << EOF
ovsh i Openflow_Config \
     token:=${of_out_token} \
     bridge:=${bridge} \
     table:=8 \
     priority:=10 \
     rule:="udp6,tp_dst=53"
     action:="resubmit(,6)"
EOF
}

#fsm_inject
gen_offlow_table8_cmd18() {
    cat << EOF
ovsh i Openflow_Config \
     token:=${of_out_token}
     bridge:=${bridge} \
     table:=8 \
     priority:=25 \
     rule:="in_port=204"
     action:="NORMAL"
EOF
}

#main_t_dhcp_ipv4_src
gen_offlow_table8_cmd19() {
    cat << EOF
ovsh i Openflow_Config \
     token:=${of_out_token} \
     bridge:=${bridge} \
     table:=8 \
     priority:=20 \
     rule:="udp,tp_src=67"
     action:="resubmit(,6)"
EOF
}

#node_eth_dns_ipv4_res
gen_offlow_table8_cmd20() {
    cat << EOF
ovsh i Openflow_Config \
     token:=${of_out_token} \
     bridge:=${bridge} \
     table:=8 \
     priority:=40 \
     rule:="udp,tp_src=53,dl_dst=\${#node_eth}"
     action:="NORMAL"
EOF
}


#Table=9 rules
#ip4
gen_oflow_dpi_ip4_cmd1() {
    cat << EOF
ovsh i Openflow_Config \
     token:=${of_out_token} \
     bridge:=${bridge} \
     table:=9 \
     priority:=0 \
     action:="NORMAL"
EOF
}
gen_oflow_dpi_ip4_cmd2() {
    cat << EOF
ovsh i Openflow_Config \
     token:=${of_out_token} \
     bridge:=${bridge} \
     table:=9 \
     priority:=${priority} \
     rule:=${of_out_ip4_rule_ct} \
     action:=${of_out_ip4_action_ct}
EOF
}
gen_oflow_dpi_ip4_cmd3() {
    cat << EOF
ovsh i Openflow_Config \
     token:=${of_out_token} \
     bridge:=${bridge} \
     table:=9 \
     priority:=${priority} \
     rule:=${of_out_ip4_rule_ct_inspect_new_conn} \
     action:=${of_out_ip4_action_ct_inspect_new_conn}
EOF
}
gen_oflow_dpi_ip4_cmd4() {
    cat << EOF
ovsh i Openflow_Config \
     token:=${of_out_token} \
     bridge:=${bridge} \
     table:=9 \
     priority:=${priority} \
     rule:=${of_out_ip4_rule_ct_inspect} \
     action:=${of_out_ip4_action_ct_inspect}
EOF
}
gen_oflow_dpi_ip4_cmd5() {
    cat << EOF
ovsh i Openflow_Config \
     token:=${of_out_token} \
     bridge:=${bridge} \
     table:=9 \
     priority:=${priority} \
     rule:=${of_out_ip4_rule_ct_passthru} \
     action:=${of_out_ip4_action_ct_passthru}
EOF
}
gen_oflow_dpi_ip4_cmd6() {
    cat << EOF
ovsh i Openflow_Config \
     token:=${of_out_token} \
     bridge:=${bridge} \
     table:=9 \
     priority:=${priority} \
     rule:=${of_out_ip4_rule_ct_drop} \
     action:=${of_out_ip4_action_ct_drop}
EOF
}
#Table=9 rules
#ip6
gen_oflow_dpi_ip6_cmd1() {
    cat << EOF
ovsh i Openflow_Config \
     token:=${of_out_token} \
     bridge:=${bridge} \
     table:=9 \
     priority:=0 \
     action:="NORMAL"
EOF
}
gen_oflow_dpi_ip6_cmd2() {
    cat << EOF
ovsh i Openflow_Config \
     token:=${of_out_token} \
     bridge:=${bridge} \
     table:=9 \
     priority:=${priority} \
     rule:=${of_out_ip6_rule_ct} \
     action:=${of_out_ip6_action_ct}
EOF
}
gen_oflow_dpi_ip6_cmd3() {
    cat << EOF
ovsh i Openflow_Config \
     token:=${of_out_token} \
     bridge:=${bridge} \
     table:=9 \
     priority:=${priority} \
     rule:=${of_out_ip6_rule_ct_inspect_new_conn} \
     action:=${of_out_ip6_action_ct_inspect_new_conn}
EOF
}
gen_oflow_dpi_ip6_cmd4() {
    cat << EOF
ovsh i Openflow_Config \
     token:=${of_out_token} \
     bridge:=${bridge} \
     table:=9 \
     priority:=${priority} \
     rule:=${of_out_ip6_rule_ct_inspect} \
     action:=${of_out_ip6_action_ct_inspect}
EOF
}
gen_oflow_dpi_ip6_cmd5() {
    cat << EOF
ovsh i Openflow_Config \
     token:=${of_out_token} \
     bridge:=${bridge} \
     table:=9 \
     priority:=${priority} \
     rule:=${of_out_ip6_rule_ct_passthru} \
     action:=${of_out_ip6_action_ct_passthru}
EOF
}
gen_oflow_dpi_ip6_cmd6() {
    cat << EOF
ovsh i Openflow_Config \
     token:=${of_out_token} \
     bridge:=${bridge} \
     table:=9 \
     priority:=${priority} \
     rule:=${of_out_ip6_rule_ct_drop} \
     action:=${of_out_ip6_action_ct_drop}
EOF
}


# Create a dynamic dpi tag entry
gen_ddpi_tag_cmd() {
    cat << EOF
ovsh i Openflow_Tag \
       name:=${ddpi_tag_name} \
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
               "other_config": ["map",[["mqtt_v","${mqtt_v}"],["dso_init","${dso_init}"]]]
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

# Let's start
client_mac=$1
bridge=${2:-br-home}
intf=${3:-br-home.dpidemo}
ofport=${4:-2001} # must be unique to the bridge

# of_out_token: openflow_config egress rule name. Must start with 'dev' so the
# controller leaves it alone
#Table=9 ip4 rules
of_out_token=dev_flow_demo_dpi_out

of_out_ip4_rule_ct="\"ct_state=-trk,ip\""
of_out_ip4_action_ct="\"ct(table=9,zone=1)\""
of_out_ip4_rule_ct_inspect_new_conn="\"ct_zone=1,ct_state=+trk,ct_mark=0,ip\""
of_out_ip4_action_ct_inspect_new_conn="\"ct(commit,zone=1,exec(load:0x1->NXM_NX_CT_MARK[])),resubmit(,8),output:${ofport}\""
of_out_ip4_rule_ct_inspect="\"ct_zone=1,ct_state=+trk,ct_mark=1,ip\""
of_out_ip4_action_ct_inspect="\"resubmit(,8),output:${ofport}\""
of_out_ip4_rule_ct_passthru="\"ct_zone=1,ct_state=+trk,ct_mark=2,ip\""
of_out_ip4_action_ct_passthru="\"resubmit(,8)\""
of_out_ip4_rule_ct_drop="\"ct_state=+trk,ct_mark=3,ip\""
of_out_ip4_action_ct_drop="\"DROP\""
#Table=9 ip6 rules
of_out_ip6_rule_ct="\"ct_state=-trk,ip6\""
of_out_ip6_action_ct="\"ct(table=9,zone=1)\""
of_out_ip6_rule_ct_inspect_new_conn="\"ct_zone=1,ct_state=+trk,ct_mark=0,ip6\""
of_out_ip6_action_ct_inspect_new_conn="\"ct(commit,zone=1,exec(load:0x1->NXM_NX_CT_MARK[])),resubmit(,8),output:${ofport}\""
of_out_ip6_rule_ct_inspect="\"ct_zone=1,ct_state=+trk,ct_mark=1,ip6\""
of_out_ip6_action_ct_inspect="\"resubmit(,8),output:${ofport}\""
of_out_ip6_rule_ct_passthru="\"ct_zone=1,ct_state=+trk,ct_mark=2,ip6\""
of_out_ip6_action_ct_passthru="\"resubmit(,8)\""
of_out_ip6_rule_ct_drop="\"ct_state=+trk,ct_mark=3,ip6\""
of_out_ip6_action_ct_drop="\"DROP\""


# tag_name: openflow_tag name. Nust start with 'dev' so the controller
# leaves it alone
tag_name=dev_tag_demo_dpi
ddpi_tag_name=dpi_mac

# Flow_Service_Manager_Config parameters
filter=ip
plugin=/usr/plume/lib/libfsm_demo_dpi.so
dso_init=fsm_demo_dpi_plugin_init
fsm_handler=dev_demo_dpi # must start with 'dev' so the controller leaves it alone

priority=200 # must be higher than controller pushed rules

# Check required commands
check_cmd 'ovsh'
check_cmd 'ovs-vsctl'
check_cmd 'ip'
check_cmd 'ovs-ofctl'

# Enforce filtering on mac address wifi device
#if [ -z ${client_mac} ]; then
#    usage
#    exit 1
#fi

location_id=$(get_location_id)
node_id=$(get_node_id)
mqtt_v="dev-test/${fsm_handler}/${node_id}/${location_id}"

#$(del_flows_br_home_cmd)
$(gen_tap_cmd)
$(tap_up_cmd)
$(gen_no_flood_cmd)
$(gen_ddpi_tag_cmd)
$(gen_offlow_table8_cmd1)
$(gen_offlow_table8_cmd2)
$(gen_offlow_table8_cmd3)
$(gen_offlow_table8_cmd4)
$(gen_offlow_table8_cmd5)
$(gen_offlow_table8_cmd6)
$(gen_offlow_table8_cmd7)
$(gen_offlow_table8_cmd8)
$(gen_offlow_table8_cmd9)
$(gen_offlow_table8_cmd10)
$(gen_offlow_table8_cmd11)
$(gen_offlow_table8_cmd12)
$(gen_offlow_table8_cmd13)
$(gen_offlow_table8_cmd14)
$(gen_offlow_table8_cmd15)
$(gen_offlow_table8_cmd16)
$(gen_offlow_table8_cmd17)
$(gen_offlow_table8_cmd18)
$(gen_offlow_table8_cmd19)
$(gen_offlow_table8_cmd20)
$(gen_oflow_dpi_ip4_cmd1)
$(gen_oflow_dpi_ip4_cmd2)
$(gen_oflow_dpi_ip4_cmd3)
$(gen_oflow_dpi_ip4_cmd4)
$(gen_oflow_dpi_ip4_cmd5)
$(gen_oflow_dpi_ip4_cmd6)
$(gen_oflow_dpi_ip6_cmd1)
$(gen_oflow_dpi_ip6_cmd2)
$(gen_oflow_dpi_ip6_cmd3)
$(gen_oflow_dpi_ip6_cmd4)
$(gen_oflow_dpi_ip6_cmd5)
$(gen_oflow_dpi_ip6_cmd6)
#$(gen_offlow_del_table0)
$(gen_oflow_ddpi_cmd1)
$(gen_oflow_ddpi_cmd2)
$(gen_oflow_ddpi_cmd3)
eval ovsdb-client transact \'$(gen_fsmc_cmd)\'
