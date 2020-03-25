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
# Connection manager
#
###############################################################################
UNIT_NAME := cm

# Template type:
UNIT_TYPE := BIN

UNIT_SRC    := src/cm2_main.c
UNIT_SRC    += src/cm2_ovsdb.c
UNIT_SRC    += src/cm2_event.c
ifeq ($(BUILD_HAVE_LIBCARES),y)
UNIT_SRC    += src/cm2_resolve_ares.c
else
UNIT_SRC    += src/cm2_resolve_sync.c
endif
UNIT_SRC    += src/cm2_resolve.c
UNIT_SRC    += src/cm2_stability.c
UNIT_SRC    += src/cm2_net.c

UNIT_CFLAGS += -I$(TOP_DIR)/src/lib/common/inc/

UNIT_LDFLAGS := -lpthread
UNIT_LDFLAGS += -ljansson
UNIT_LDFLAGS += -ldl
UNIT_LDFLAGS += -lev
UNIT_LDFLAGS += -lrt

ifneq ($(CONFIG_USE_KCONFIG),y)
# Disable dryrun on GRE by default for all platforms without KConfig
UNIT_CFLAGS += -DCONFIG_PLUME_CM2_DISABLE_DRYRUN_ON_GRE
# Set default mtu on gre interface
UNIT_CFLAGS += -DCONFIG_OPENSYNC_CM2_MTU_ON_GRE=1500
endif

UNIT_EXPORT_CFLAGS := $(UNIT_CFLAGS)
UNIT_EXPORT_LDFLAGS := $(UNIT_LDFLAGS)

UNIT_DEPS := src/lib/ovsdb
UNIT_DEPS += src/lib/pjs
UNIT_DEPS += src/lib/schema
UNIT_DEPS += src/lib/version
UNIT_DEPS += src/lib/evsched
ifeq ($(BUILD_HAVE_LIBCARES),y)
UNIT_DEPS += src/lib/evx
endif
