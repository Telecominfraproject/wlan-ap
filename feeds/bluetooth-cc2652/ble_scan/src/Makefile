#all: ble_scan
#ble_scan: ble_scan.o
#	$(CC) $(LDFLAGS) ble_scan.o -o ble_scan
#blescan.o: ble_scan.c
#	$(CC) $(CFLAGS) -c ble_scan.c
#clean:
#	rm *.o ble_scan

#
#
# Author: Teunis van Beelen
#
# email: teuniz@protonmail.com
#
#

#CROSS-COMPILE:=../../../../../qsdk/staging_dir/toolchain-arm/bin/arm-openwrt-linux-
#CC:=$(CROSS-COMPILE)gcc

CC = gcc
CFLAGS = -Wall -Wextra -Wshadow -Wformat-nonliteral -Wformat-security -Wtype-limits -O2

objects = rs232.o

all: ble_scan

ble_scan : $(objects) ble_scan.o
	$(CC) $(CFLAGS) $(objects) ble_scan.o -o ble_scan

ble_scan.o : ble_scan.c rs232.h
	$(CC) $(CFLAGS) -c ble_scan.c -o ble_scan.o

rs232.o : rs232.h rs232.c
	$(CC) $(CFLAGS) -c rs232.c -o rs232.o

clean :
	$(RM) ble_scan $(objects) ble_scan.o

#
#
#
#





