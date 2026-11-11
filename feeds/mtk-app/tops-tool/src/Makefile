# SPDX-License-Identifier: GPL-2.0-or-later */
#
# Copyright (c) 2023 MediaTek Inc. All Rights Reserved.
#
# Author: Alvin Kuo <Alvin.Kuo@mediatek.com>
#

PROJECT:= tops-tool

OBJECTS-$(CONFIG_MTK_TOPS_TOOL_SAVE_LOG)+= logger.o

OBJECTS:= tops-tool.o common.o dump.o $(OBJECTS-y)

all: $(PROJECT)

$(PROJECT): $(OBJECTS) Makefile
	$(CC) $(CFLAGS) $(LDFLAGS) $(OBJECTS) -o $@

%.o: %.c inc/%.h Makefile
	$(CC) $(CFLAGS) -c $< -o $@

.PHONY : clean
clean:
	rm -f $(PROJECT) *.o
