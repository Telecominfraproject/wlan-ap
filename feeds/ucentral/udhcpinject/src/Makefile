include	$(TOPDIR)/rules.mk

TARGET_CFLAGS += -Wall

obj-y := udhcpinject.o

all: udhcpinject

udhcpinject: $(obj-y)
	$(CC) $(LDFLAGS) -lpcap -luci -o $@ $(obj-y)

%.o: %.c
	$(CC) $(CFLAGS) $(TARGET_CFLAGS) -c $< -o $@

clean:
	rm -f *.o udhcpinject
