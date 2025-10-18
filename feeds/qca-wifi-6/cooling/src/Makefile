#****************************************************************************
#
#	Copyright (c) 2024 Sonicfi, Inc.
#	All Rights Reserved.
#	Sonicfi Confidential and Proprietary.
#
#****************************************************************************/

ifneq ($(strip $(TOOLPREFIX)),)
export  CROSS:=$(TOOLPREFIX)
endif

COOLING_INSTALL_ROOT := $(COOLINGDIR)/install

ifndef INSTALL_ROOT
INSTALL_ROOT=$(COOLING_INSTALL_ROOT)
endif

export CC = $(CROSS)gcc
export CFLAGS += -O2 -Wall -c
export STRIP = $(CROSS)strip
export SOURCES= \
		cooling.c
export OBJECTS=$(SOURCES:.c=.o)
export EXECUTABLE=cooling

LIBS += -lubus -lubox
CFLAGS += -L$(INSTALL_ROOT)/lib $(TARGET_CFLAGS) \
		-fstack-protector-all -fpie
LDFLAGS += $(TARGET_LDFLAGS) -pie

# What we build by default:
ALL = $(EXECUTABLE)

# RULES ---------------------------------------------------------------

# Making default targets:
all: local
	@echo All done in `pwd`

$(EXECUTABLE): $(OBJECTS)
	$(CC) $(LIB_PATH) $(LIBS) $(LDFLAGS) $(OBJECTS) -o $@
	@echo Build $@ successufully...

.c.o:
	$(CC) $(INCLUDE) $(CFLAGS) $(LDFLAGS) $< -o $@

clean:
	rm -rf *.o *.d $(EXECUTABLE)

local: $(SOURCES) $(EXECUTABLE)
	@echo Build $@ successufully...

# Doing installation (see comments at top of this file)
install: local
	mkdir -p $(INSTALL_ROOT)/usr/sbin/
	cp -a -f $(ALL) $(INSTALL_ROOT)/usr/sbin/
	mkdir -p $(INSTALL_ROOT)/etc/cooling
	cp -a -f sonicfi-rap630*-cooling.conf $(INSTALL_ROOT)/etc/cooling/
	@echo Installed outputs from `pwd`

# Remove all generated files
#clean: default_clean # from Makefile.rules
clean:
	rm -rf $(INSTALL_ROOT)/usr/sbin/cooling
	rm -rf ./$(ALL)

# END --------------------------------------------------------------------
