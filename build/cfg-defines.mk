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

ifeq ($(BUILD_BHAUL_WDS),y)
CFLAGS += -DUSE_BHAUL_WDS
endif

ifeq ($(BUILD_LOG_PREFIX_PLUME),y)
CFG_DEFINES += -DUSE_LOG_PREFIX_PLUME
endif # BUILD_LOG_PREFIX_PLUME

ifeq ($(BUILD_LOG_HOSTNAME),y)
CFG_DEFINES += -DUSE_LOG_HOSTNAME
endif # BUILD_LOG_HOSTNAME

ifeq ($(BUILD_QM),y)
CFG_DEFINES += -DUSE_QM
else
UNIT_DISABLE_src/qm                     := y
UNIT_DISABLE_src/qm/qm_cli              := y
UNIT_DISABLE_src/qm/qm_conn             := y
endif # BUILD_QM

ifeq ($(BUILD_CAPACITY_QUEUE_STATS),y)
CFG_DEFINES += -DUSE_CAPACITY_QUEUE_STATS
endif # BUILD_CAPACITY_QUEUE_STATS

ifeq ($(BUILD_CLIENT_NICKNAME),y)
CFG_DEFINES += -DUSE_CLIENT_NICKNAME
endif # BUILD_CLIENT_NICKNAME

ifeq ($(BUILD_CLIENT_FREEZE),y)
CFG_DEFINES += -DUSE_CLIENT_FREEZE
endif # BUILD_CLIENT_FREEZE

ifeq ($(BUILD_REMOTE_LOG),y)
CFG_DEFINES += -DBUILD_REMOTE_LOG
endif

ifdef CONFIG_USE_KCONFIG
UNIT_DISABLE_src/fsm := $(if $(CONFIG_TARGET_MANAGER_FSM),n,y)
UNIT_DISABLE_src/fcm := $(if $(CONFIG_TARGET_MANAGER_FCM),n,y)
endif
