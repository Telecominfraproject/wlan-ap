#/* SPDX-License-Identifier: BSD-3-Clause */
srcdir ?= .
VPATH ?= $(srcdir)/src


LIBS = -lpthread -lrt -linterapcomm -lev
$(call output,usr/sbin/wc-apc)

CFLAGS += -I./include/ \
	-I../include/

CFLAGS  += -Wall -g
CFLAGS += -MMD -Wall -g -Wpointer-arith -Wcast-qual -Wshadow \
				-Waggregate-return -Wnested-externs -Wstrict-prototypes \
				-fno-omit-frame-pointer -g -rdynamic -fexceptions -funwind-tables -funsigned-char

OBJS := apc_main.o \
               hello.o iface.o neighbor.o apc.o

all: apc

#.c.o:
%.o: %.c
	$(CC) -fPIC $(CFLAGS) -c $< -o $@


apc: $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS) $(LIBS)

clean:
	$(RM) -f apc $(OBJS)
