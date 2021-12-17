
CFLAGS := -s -Wall -c -Os $(CFLAGS)
LDFLAGS = -s -Wall


CFLAGS  += -ffunction-sections -fdata-sections
LDFLAGS += --static -s -Wl,--gc-sections


all:		clean rtk_txcalr flash rtk_pcap wpsled rtk_bootinfo


rtk_bootinfo:		rtk_bootinfo.o
			$(CC) $(LDFLAGS) -o rtk_bootinfo rtk_bootinfo.o

rtk_txcalr:		rtk_tx_calibration.o
			$(CC) $(LDFLAGS) -o rtk_txcalr rtk_tx_calibration.o

#named as flash for compliant to MP tool
flash:		mibhw.o
			$(CC) $(LDFLAGS) -o flash mibhw.o 

wpsled:		rtk_wpsled.o
			$(CC) $(LDFLAGS) -o wpsled rtk_wpsled.o

clean:
			rm -f *.o
			rm -f rtk_txcalr
			rm -f flash
			rm -f rtk_pcap
			rm -f wpsled
			rm -f rtk_bootinfo

rtk_bootinfo.o:		rtk_bootinfo.c 
			$(CC) $(CFLAGS) -o rtk_bootinfo.o rtk_bootinfo.c

rtk_tx_calibration.o:		rtk_tx_calibration.c 
			$(CC) $(CFLAGS) -o rtk_tx_calibration.o rtk_tx_calibration.c
			
mibhw.o:		mibhw.c 
			$(CC) $(CFLAGS) -o mibhw.o mibhw.c

rtk_wpsled.o:		rtk_wpsled.c 
			$(CC) $(CFLAGS) -o rtk_wpsled.o rtk_wpsled.c

rtk_pcap:		packet_capture.c
			$(CC) --static -o rtk_pcap packet_capture.c -I../../libpcap-1.5.3 -L../../libpcap-1.5.3 -lpcap 


			
