# SPDX-License-Identifier BSD-3-Clause

###############################################################################
#
# UCC Manager
#
###############################################################################
UNIT_DISABLE := n

UNIT_NAME := uccm

# Template type:
UNIT_TYPE := BIN

UNIT_SRC    += src/main.c

UNIT_CFLAGS := -I$(UNIT_PATH)/inc
UNIT_CFLAGS += -Isrc/lib/common/inc/
UNIT_CFLAGS += -Isrc/lib/version/inc/

UNIT_LDFLAGS += -lev -lubox -luci -lubus
UNIT_LDFLAGS += -lrt
UNIT_LDFLAGS += -lnl-tiny

UNIT_EXPORT_CFLAGS := $(UNIT_CFLAGS)
UNIT_EXPORT_LDFLAGS := $(UNIT_LDFLAGS)

UNIT_DEPS += src/lib/inet
UNIT_DEPS += src/lib/ovsdb
UNIT_DEPS += src/lib/pjs
UNIT_DEPS += src/lib/reflink
UNIT_DEPS += src/lib/schema
UNIT_DEPS += src/lib/synclist
UNIT_DEPS += src/lib/datapipeline
UNIT_DEPS_CFLAGS += src/lib/version

