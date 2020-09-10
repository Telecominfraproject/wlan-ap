#/* SPDX-License-Identifier: BSD-3-Clause */
srcdir ?= .
VPATH ?= $(srcdir)/src
LIBNAME=libradiusclient.so

CFLAGS += -O -Wall -Werror -Wshadow 
CFLAGS += -I./include/radius

all: $(LIBNAME)

.c.o:
	$(CC) -fPIC $(CFLAGS) -c $< -o $@

OBJS=md5.o eloop.o radius.o radius_debug.o radius_client.o accounting.o

$(LIBNAME): $(OBJS)
	$(CC) $(CFLAGS) -Wl,-Bsymbolic-functions -shared -o $@ $^

clean:
	$(RM) -f libradiusclient.so $(OBJS)
