#/* SPDX-License-Identifier: BSD-3-Clause */
srcdir ?= .
VPATH ?= $(srcdir)/src
LIBNAME=libinterapcomm.so
subdirs=src/test

CFLAGS += -O -Wall -Werror -Wshadow
CFLAGS += -I./include/
LIBS = -lev

all: $(LIBNAME) $(subdirs)

.c.o:
	$(CC) -fPIC $(CFLAGS) $(LIBS) -c $< -o $@

OBJS=interAPcomm.o
$(LIBNAME): $(OBJS)
	$(CC) $(CFLAGS) -Wl,-Bsymbolic-functions -shared -o $@ $^

$(subdirs):
	$(MAKE) --directory=$@

$(subdirs): $(LIBNAME)

clean:
	$(RM) -f libinterapcomm.so $(OBJS)
