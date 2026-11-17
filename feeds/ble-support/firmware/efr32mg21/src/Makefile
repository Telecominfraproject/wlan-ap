# SPDX-License-Identifier: BSD-3-Clause

CFLAGS ?= -Wall -Wextra -std=gnu99 -g

SOURCES := main.c app.c system.c uart_posix.c \
	sl_bt_ncp_host.c sl_bt_ncp_host_api.c
TARGET := bt_host_uart_dfu

all: $(TARGET)

$(TARGET): $(SOURCES)
	$(CC) $(CFLAGS) $(SOURCES) -o $@

clean:
	$(RM) $(TARGET)
