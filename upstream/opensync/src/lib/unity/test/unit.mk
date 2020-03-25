##############################################################################
#
# Unity test suite
#
##############################################################################
UNIT_DISABLE := n
UNIT_NAME := test_unity

# Template type:
UNIT_TYPE := TEST_BIN

# List of source files
UNIT_SRC := testunity.c
UNIT_SRC += testunityrunner.c
# this is required so that putcharSpy special implementation for this test
# is used in this case.
UNIT_SRC += ../src/unity.c

# This flag is needed because unity library isn't actually used, in this
# case we are compiling unity source as part of the test binary project
UNIT_CFLAGS += -I$(UNIT_PATH)/../inc
UNIT_CFLAGS += -Werror
UNIT_CFLAGS += -Wno-switch-enum

# ifeq ($(filter FAST5250 ,$(TARGET)),)
# UNIT_CFLAGS += -Wno-double-promotion
# endif

UNIT_CFLAGS += -Wbad-function-cast
UNIT_CFLAGS += -Wcast-qual
UNIT_CFLAGS += -Wold-style-definition
UNIT_CFLAGS += -Wstrict-overflow
UNIT_CFLAGS += -Wstrict-prototypes
UNIT_CFLAGS += -Wswitch-default
UNIT_CFLAGS += -Wundef
UNIT_CFLAGS += -D UNITY_OUTPUT_CHAR=putcharSpy
UNIT_CFLAGS += -D UNITY_OUTPUT_CHAR_HEADER_DECLARATION=putcharSpy\(int\)
UNIT_CFLAGS += -D UNITY_NO_WEAK

UNIT_LDFLAGS += -lm
