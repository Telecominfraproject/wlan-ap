# Makefile for ath11k-fwtest

ALL=ath11k-fwtest

OBJS = fwtest.o

# Making default targets:
all: local install
	@echo All done in `pwd`

local : $(ALL)
	@echo Made outputs in `pwd`

install: local
	@cp -a -f ath11k-fwtest ./install/sbin
	@echo Installed outputs from `pwd`

fwtest.o : fwtest.c
	$(CC) -c $(CFLAGS) fwtest.c

ath11k-fwtest: $(OBJS)
	$(CC) $(OBJS) $(LDFLAGS) -o $@

# Remove all generated files
clean:
	@rm -f *.o

.PHONY: all clean install

