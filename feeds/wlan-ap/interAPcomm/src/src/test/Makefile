#/* SPDX-License-Identifier: BSD-3-Clause */
OBJS = interapcommtest.o

CFLAGS += -Wall $(EXTRA_CFLAGS) $(extra_cflags-y)

LIBS =	-L ../../ -linterapcomm -lev

CFLAGS += -I. \
	-I../../include/

LDFLAGS := $(subst EB,mbig-endian,$(LDFLAGS))

all: test_interap

.c.o:
	$(CC) -fPIC $(CFLAGS) -c $< -o $@

test_interap: $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS) $(LIBS)

clean:
	rm -f $(OBJS) test_interap
