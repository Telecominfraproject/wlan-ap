# SPDX-License-Identifier: BSD-3-Clause

UNIT_NAME := rrm 

UNIT_DISABLE := $(if $(CONFIG_MANAGER_RRM),n,y)

# Template type:
UNIT_TYPE := BIN

UNIT_SRC := src/rrm_main.c
UNIT_SRC += src/rrm_ovsdb.c
UNIT_SRC += src/rrm_channel.c
UNIT_SRC += src/rrm_ubus.c

UNIT_CFLAGS := -I$(UNIT_PATH)/inc
UNIT_CFLAGS += -Isrc/lib/common/inc/
UNIT_CFLAGS += -Isrc/lib/version/inc/

UNIT_LDFLAGS := -lev -lubus -lubox -luci -lblobmsg_json -lnl-tiny
UNIT_LDFLAGS += -lrt

UNIT_EXPORT_CFLAGS := $(UNIT_CFLAGS)
UNIT_EXPORT_LDFLAGS := $(UNIT_LDFLAGS)

UNIT_DEPS += src/lib/common
UNIT_DEPS += src/lib/ovsdb
UNIT_DEPS += src/lib/pjs
UNIT_DEPS += src/lib/schema
UNIT_DEPS += src/lib/datapipeline
UNIT_DEPS += src/lib/json_util
UNIT_DEPS += src/lib/schema
UNIT_DEPS_CFLAGS += src/lib/version
