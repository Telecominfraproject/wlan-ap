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

/usr/bin/ovs-vsctl add-port br-home br-home.demo  -- set interface br-home.demo  type=internal -- set interface br-home.demo  ofport_request=1001
/usr/sbin/ip link set br-home.demo up
/usr/plume/tools/ovsh i Openflow_Config token:="dev_flow_demo" bridge:="br-home" table:="0" priority:="200" rule:="dl_src=\${dev_tag_demo},udp,tp_dst=12345" action:="normal,output:1001"
/usr/plume/tools/ovsh i Openflow_Tag name:="dev_tag_demo" cloud_value:="[\"set\",[\"78:4f:43:9c:f7:fc\",\"5c:f9:38:94:78:4a\"]]"
/usr/plume/tools/ovsh i Flow_Service_Manager_Config handler:="dev_demo" if_name:="br-home.demo" pkt_capt_filter:="udp port 12345" plugin:="/tmp/libfsm_demo.so" other_config:="[\"map\",[[\"mqtt_v\",\"foo_mqtt_v\"],[\"dso_init\",\"fsm_plugin_demo_init\"]]]"
