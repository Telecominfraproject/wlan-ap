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
# Statistics Manager
#
###############################################################################
UNIT_NAME    := sm

# Template type:
UNIT_TYPE    := BIN

UNIT_SRC     := src/sm_main.c
UNIT_SRC     += src/sm_ovsdb.c
UNIT_SRC     += src/sm_neighbor_report.c
UNIT_SRC     += src/sm_client_report.c
UNIT_SRC     += src/sm_device_report.c
UNIT_SRC     += src/sm_survey_report.c
UNIT_SRC     += src/sm_radio_config.c
UNIT_SRC     += src/sm_scan_schedule.c
UNIT_SRC     += src/sm_rssi_report.c
UNIT_SRC     += src/sm_common.c

ifeq ($(BUILD_CAPACITY_QUEUE_STATS),y)
UNIT_SRC     += src/sm_capacity_report.c
endif

UNIT_LDFLAGS := -lpthread
UNIT_LDFLAGS += -ljansson
UNIT_LDFLAGS += -ldl
UNIT_LDFLAGS += -lev
UNIT_LDFLAGS += -lrt
UNIT_LDFLAGS += -lz

UNIT_DEPS    := src/lib/ovsdb
UNIT_DEPS    += src/lib/pjs
UNIT_DEPS    += src/lib/schema
UNIT_DEPS    += src/lib/datapipeline
UNIT_DEPS    += src/lib/version
UNIT_DEPS    += src/lib/target
UNIT_DEPS    += src/lib/evsched

ifeq ($(BUILD_QM),y)
UNIT_SRC     += src/sm_qm.c
UNIT_DEPS    += src/qm/qm_conn
else
UNIT_SRC     += src/sm_mqtt.c
UNIT_DEPS    += src/lib/mosqev
endif

UNIT_EXPORT_CFLAGS := $(UNIT_CFLAGS)
UNIT_EXPORT_LDFLAGS := $(UNIT_LDFLAGS)

