##############################################################################
#
# Unity test suite
#
##############################################################################
UNIT_NAME := unity
# Template type:
UNIT_TYPE := STATIC_LIB

UNIT_SRC := src/unity.c

UNIT_CFLAGS := -I$(UNIT_PATH)/inc
UNIT_CFLAGS += -fasynchronous-unwind-tables

UNIT_EXPORT_CFLAGS := $(UNIT_CFLAGS)
UNIT_EXPORT_LDFLAGS := -lm
