#/* SPDX-License-Identifier: BSD-3-Clause */
OBJS =	sta_info.o rclient.o 

BIN_OBJS =

ifndef CC
CC=gcc
endif

ifndef STRIP
STRIP=strip
endif

ifndef CFLAGS
#CFLAGS = -MMD -O2 -Wall
CFLAGS = -MMD -g -Wall
endif

CFLAGS += -DRADIUS_CLIENT_DEBUG


# Uncomment following line and set the path to your kernel tree include
# directory if your C library does not include all header files.
CFLAGS += -DUSE_KERNEL_HEADERS

#ifndef extra_cflags-y
# If path to ieee80211 directory was not set, try to guess the correct one
# to make development easier
#extra_cflags-y=-I../../ieee80211/$(shell basename `pwd`)
#endif

#CFLAGS += -Wall -Werror $(EXTRA_CFLAGS) $(extra_cflags-y)
CFLAGS += -Wall $(EXTRA_CFLAGS) $(extra_cflags-y)

CFLAGS += -funsigned-char

-include .config


WIP_PKGS_SRC_DIR = $(shell dirname `pwd`)
LIBS =	-L ../../ -lradiusclient 
#LIBS =	-L$(WIP_PKGS_SRC_DIR)/libi802/ -li802 \
	-L$(WIP_PKGS_SRC_DIR)/liblsqlquery/ -llsqlquery \
	-L$(WIP_PKGS_SRC_DIR)/zlib/ \
	-L$(WIP_PKGS_SRC_DIR)/libxml2/.libs/ $(shell xml2-config --libs)

#CFLAGS += -I.
CFLAGS += -I. \
	-I../../include/radius 
	#-I../radlib/include/ 

#CFLAGS += -I. \
	-I$(WIP_PKGS_SRC_DIR)/libi802/include/ \
	-I$(WIP_PKGS_SRC_DIR)/libxml2/include/ \
	-I$(WIP_PKGS_SRC_DIR)/liblsqlquery/include/ \

LDFLAGS := $(subst EB,mbig-endian,$(LDFLAGS))

all: radiusclientd 

radiusclientd: $(OBJS)
	$(CC) $(CFLAGS) -o radiusclientd $(STARTFILES) $(OBJS) $(BIN_OBJS) $(LDFLAGS) $(ENDFILES)$(LIBS) 

install:
	install -d $(DESTDIR)/usr/sbin/
	install hostapd $(DESTDIR)/usr/sbin/
	$(STRIP) --remove-section=.pdr $(DESTDIR)/usr/sbin/hostapd

clean:
	rm -f core *~ $(OBJS) radiusclientd *.d

-include $(OBJS:%.o=%.d)
