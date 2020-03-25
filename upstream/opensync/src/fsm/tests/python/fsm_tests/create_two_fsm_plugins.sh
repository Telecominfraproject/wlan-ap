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

/usr/bin/ovs-vsctl add-port br-home br-home.foo  -- set interface br-home.foo  type=internal -- set interface br-home.foo  ofport_request=1001
/usr/sbin/ip link set br-home.foo up
/usr/bin/ovs-vsctl add-port br-home br-home.bar  -- set interface br-home.bar  type=internal -- set interface br-home.bar  ofport_request=1002
/usr/sbin/ip link set br-home.bar up
/usr/plume/tools/ovsh i Openflow_Config token:="dev_flow_foo" bridge:="br-home" table:="0" priority:="200" rule:="dl_src=\${dev_tag_foo},udp,tp_dst=12345" action:="normal,output:1001"
/usr/plume/tools/ovsh i Openflow_Config token:="dev_flow_bar" bridge:="br-home" table:="0" priority:="250" rule:="dl_src=\${dev_tag_bar},tcp,tp_dst=54321" action:="normal,output:1002"
/usr/plume/tools/ovsh i Openflow_Tag name:="dev_tag_bar" device_value:="[\"set\",[\"de:ad:be:ef:00:11\",\"66:55:44:33:22:11\"]]"
/usr/plume/tools/ovsh i Openflow_Tag name:="dev_tag_foo" cloud_value:="[\"set\",[\"aa:bb:cc:dd:ee:ff\",\"11:22:33:44:55:66\"]]"
/usr/plume/tools/ovsh i Flow_Service_Manager_Config handler:="dev_foo" if_name:="br-home.foo" pkt_capt_filter:="udp port 12345" plugin:="/tmp/libfsm_foo.so" other_config:="[\"map\",[[\"dso_init\",\"fsm_foo_init\"],[\"mqtt_v\",\"foo_mqtt_v\"]]]"
/usr/plume/tools/ovsh i Flow_Service_Manager_Config handler:="dev_bar" if_name:="br-home.bar" pkt_capt_filter:="tcp port 54321" plugin:="/tmp/libfsm_bar.so" other_config:="[\"map\",[[\"dso_init\",\"fsm_bar_init\"],[\"mqtt_v\",\"bar_mqtt_v\"]]]"
