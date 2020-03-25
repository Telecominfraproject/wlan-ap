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

###############################################################################
#
# Network manager
#
###############################################################################
UNIT_NAME := nm

# Template type:
UNIT_TYPE := BIN

UNIT_SRC    += src/nm2_dhcp_lease.c
UNIT_SRC    += src/nm2_dhcp_option.c
UNIT_SRC    += src/nm2_dhcp_rip.c
UNIT_SRC    += src/nm2_dhcpv6_client.c
UNIT_SRC    += src/nm2_dhcpv6_lease.c
UNIT_SRC    += src/nm2_dhcpv6_server.c
UNIT_SRC    += src/nm2_iface.c
UNIT_SRC    += src/nm2_inet_config.c
UNIT_SRC    += src/nm2_inet_state.c
UNIT_SRC    += src/nm2_ip_interface.c
UNIT_SRC    += src/nm2_ipv6_address.c
UNIT_SRC    += src/nm2_ipv6_neighbors.c
UNIT_SRC    += src/nm2_ipv6_prefix.c
UNIT_SRC    += src/nm2_ipv6_routeadv.c
UNIT_SRC    += src/nm2_mac_learning.c
UNIT_SRC    += src/nm2_mac_tags.c
UNIT_SRC    += src/nm2_main.c
UNIT_SRC    += src/nm2_portfw.c
UNIT_SRC    += src/nm2_route.c
UNIT_SRC    += src/nm2_util.c

UNIT_CFLAGS := -I$(UNIT_PATH)/inc
UNIT_CFLAGS += -Isrc/lib/common/inc/

UNIT_LDFLAGS += -ljansson
UNIT_LDFLAGS += -lev
UNIT_LDFLAGS += -lrt
ifneq ($(BUILD_SHARED_LIB),y)
UNIT_LDFLAGS += -lpcap
endif

UNIT_EXPORT_CFLAGS := $(UNIT_CFLAGS)
UNIT_EXPORT_LDFLAGS := $(UNIT_LDFLAGS)

UNIT_DEPS += src/lib/evsched
UNIT_DEPS += src/lib/inet
UNIT_DEPS += src/lib/ovsdb
UNIT_DEPS += src/lib/pjs
UNIT_DEPS += src/lib/reflink
UNIT_DEPS += src/lib/schema
UNIT_DEPS += src/lib/synclist
UNIT_DEPS += src/lib/version
