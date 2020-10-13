# Copyright (c) 2019, Plume Design Inc. All rights reserved.
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

$(info xxx $(OVERRIDE_DIR))
UNIT_CFLAGS  += -I$(OVERRIDE_DIR)/inc
UNIT_CFLAGS  += -I$(TOP_DIR)/src/sm/src

UNIT_EXPORT_CFLAGS := $(UNIT_CFLAGS)

UNIT_SRC_TOP := $(OVERRIDE_DIR)/src/radio.c
UNIT_SRC_TOP += $(OVERRIDE_DIR)/src/managers.c
UNIT_SRC_TOP += $(OVERRIDE_DIR)/src/stats.c
UNIT_SRC_TOP += $(OVERRIDE_DIR)/src/target.c
UNIT_SRC_TOP += $(OVERRIDE_DIR)/src/vif.c
UNIT_SRC_TOP += $(OVERRIDE_DIR)/src/radio_nl80211.c
UNIT_SRC_TOP += $(OVERRIDE_DIR)/src/radio_ubus.c
UNIT_SRC_TOP += $(OVERRIDE_DIR)/src/stats_nl80211.c
UNIT_SRC_TOP += $(OVERRIDE_DIR)/src/uci.c
UNIT_SRC_TOP += $(OVERRIDE_DIR)/src/ubus.c
UNIT_SRC_TOP += $(OVERRIDE_DIR)/src/utils.c
UNIT_SRC_TOP += $(OVERRIDE_DIR)/src/iface.c
UNIT_SRC_TOP += $(OVERRIDE_DIR)/src/vlan.c
UNIT_SRC_TOP += $(OVERRIDE_DIR)/src/captive.c
UNIT_SRC_TOP += $(OVERRIDE_DIR)/src/sysupgrade.c
UNIT_SRC_TOP += $(OVERRIDE_DIR)/src/dhcpdiscovery.c
UNIT_SRC_TOP += $(OVERRIDE_DIR)/src/radius_probe.c

CONFIG_USE_KCONFIG=y
CONFIG_INET_ETH_LINUX=y
CONFIG_INET_VIF_LINUX=y
CONFIG_INET_GRE_LINUX=y
CONFIG_INET_FW_NULL=y
CONFIG_INET_DHCPC_NULL=y
CONFIG_INET_DHCPS_NULL=y
CONFIG_INET_UPNP_NULL=y
CONFIG_INET_DNS_NULL=y

UNIT_SRC := $(filter-out src/target_inet.c,$(UNIT_SRC))
UNIT_SRC := $(filter-out src/target_dhcp.c,$(UNIT_SRC))
UNIT_DEPS := $(filter-out src/lib/inet,$(UNIT_DEPS))
UNIT_DEPS += src/lib/evsched
UNIT_LDFLAGS += -luci
UNIT_LDFLAGS += -libiwinfo
UNIT_LDFLAGS += -libnl-tiny
UNIT_LDFLAGS += -lcurl
UNIT_LDFLAGS += -libradiusclient
UNIT_DEPS_CFLAGS += src/lib/inet
