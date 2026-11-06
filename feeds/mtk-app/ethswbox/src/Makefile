# SPDX-License-Identifier: GPL-2.0-or-later */
#

CFLAGS = -Wall -Wextra -DCONFIG_GSWIP_MAC -DCONFIG_GSWIP_EVLAN \
		-DCONFIG_GSWIP_MCAST -DCONFIG_GSWIP_STP

# Include directories
INCLUDES := -Iexample/ethswbox_sdk/src \
		-Iexample/ethswbox_sdk/src/cli \
		-Iexample/ethswbox_sdk/src/cli/cmds \
		-Iexample/ethswbox_sdk/src/fapi \
		-Iexample/ethswbox_sdk/src/os \
		-Iswitch_hostapi/include/gpy \
		-Iswitch_hostapi/include/gswip \
		-Iswitch_hostapi/src \
		-Iswitch_ioctl/src

# Source files
SRCS := $(wildcard example/ethswbox_sdk/src/cli/*.c) \
		$(wildcard example/ethswbox_sdk/src/cli/cmds/*.c) \
		$(wildcard example/ethswbox_sdk/src/fapi/*.c) \
		$(wildcard example/ethswbox_sdk/src/os/*.c) \
		$(wildcard switch_ioctl/src/*.c) \
		switch_hostapi/src/gsw_api.c \
		switch_hostapi/src/gsw_cli_common.c \
		switch_hostapi/src/gsw_ss.c \
		switch_hostapi/src/host_adapt_user.c \
		switch_hostapi/src/host_api_impl.c \
		switch_hostapi/src/host_smdio_ssb_user.c \
		switch_hostapi/src/mdio_relay.c \
		switch_hostapi/src/sys_misc.c

# Define the C object files
OBJS := $(SRCS:.c=.o)

# Define the name of the executable file
TARGET := ethswbox

.PHONY: clean

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)
