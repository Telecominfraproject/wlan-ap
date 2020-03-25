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


default_bridge='br-home'
bridge=${1:-${default_bridge}}
tap_intf=${bridge}.tx

gen_json_create_intf() {
    cat << EOF
["Open_vSwitch",
   {
      "op": "insert",
      "table": "Interface",
      "row": {
         "name": "${tap_intf}",
         "type": "internal",
         "ofport_request": 401
      },
      "uuid-name": "iface"
   },
   {
      "op": "insert",
      "table": "Port",
      "row": {
         "name": "${tap_intf}",
         "interfaces": ["set", [["named-uuid","iface"]]]
      },
      "uuid-name": "port"
   },
   {
      "op": "mutate",
      "table": "Bridge",
      "where": [["name", "==", "${bridge}" ]],
      "mutations": [["ports", "insert", ["set", [["named-uuid", "port"]]]]]
   }
]
EOF
}

gen_json_config_intf() {
    cat << EOF
["Open_vSwitch",
   {
      "op": "insert",
      "table": "Wifi_Inet_Config",
      "row": {
         "if_name": "${tap_intf}",
         "if_type": "tap",
         "enabled": true,
         "network": false,
         "ip_assign_scheme": "none",
         "dhcp_sniff": false
      }
   }
]
EOF
}

gen_json_tap_rules() {
    cat << EOF
["Open_vSwitch",
   {
      "op": "insert",
      "table": "Openflow_Config",
      "row": {
         "token": "dns_reply_to_fsm",
         "bridge": "${bridge}",
         "table": 0,
         "priority": 20,
         "rule": "udp,tp_src=53",
         "action": "output:br-home.dns"
      }
   },
   {
      "op": "insert",
      "table": "Openflow_Config",
      "row": {
         "token": "dns_reply_from_fsm",
         "bridge": "${bridge}",
         "table": 0,
         "priority": 60,
         "rule": "in_port=${tap_intf}",
         "action": "normal"
      }
   }
]
EOF
}

gen_json_create_intf | xargs -0 ovsdb-client transact
gen_json_config_intf | xargs -0 ovsdb-client transact
gen_json_tap_rules   | xargs -0 ovsdb-client transact
