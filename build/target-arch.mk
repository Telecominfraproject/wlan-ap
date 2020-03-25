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

# default target
TARGET ?= $(DEFAULT_TARGET)

# append list of all supported targets
OS_TARGETS += native

ifeq ($(TARGET),native)
ARCH = native
ARCH_MK = build/$(ARCH).mk
CPU_TYPE = $(shell uname -m)
BUILD_LOG_PREFIX_PLUME                 := n
BUILD_LOG_HOSTNAME                     := n
# Disable unneeded units
UNIT_DISABLE_src/bm                    := y
UNIT_DISABLE_src/blem                  := y
UNIT_DISABLE_src/nm2                   := n
UNIT_DISABLE_src/sm                    := y
UNIT_DISABLE_src/wm2                   := n
UNIT_DISABLE_src/lib/bsal              := y
UNIT_DISABLE_src/lib/cev               := y
UNIT_DISABLE_src/lib/network_metadata  := n
UNIT_DISABLE_src/lib/network_telemetry := n
UNIT_DISABLE_src/lib/ustack            := n
UNIT_DISABLE_src/lib/fsm_demo_plugin   := n
UNIT_DISABLE_src/fcm                   := n
UNIT_DISABLE_src/lib/ip_map            := n
UNIT_DISABLE_src/lib/fcm_filter        := n
UNIT_DISABLE_src/lib/nf_utils          := n
UNIT_DISABLE_src/fsm                   := n
KCONFIG_TARGET                         := kconfig/targets/config_$(TARGET)
endif

