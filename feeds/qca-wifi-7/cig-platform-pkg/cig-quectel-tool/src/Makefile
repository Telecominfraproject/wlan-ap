ifneq ($(CROSS_COMPILE),)
CROSS-COMPILE:=$(CROSS_COMPILE)
endif
ifeq ($(CC),cc)
CC:=$(CROSS-COMPILE)gcc
endif
LD:=$(CROSS-COMPILE)ld

QL_CM_SRC=QmiWwanCM.c GobiNetCM.c main.c QCQMUX.c QMIThread.c util.c qmap_bridge_mode.c mbim-cm.c device.c
QL_CM_SRC+=atc.c atchannel.c at_tok.c

QL_CM_DHCP=udhcpc.c

CFLAGS += -Wall -Wextra -Werror -O1
LDFLAGS += -lpthread -ldl -lrt

release: clean qmi-proxy mbim-proxy atc-proxy
	$(CC) ${CFLAGS} ${QL_CM_SRC} ${QL_CM_DHCP} -o quectel-CM ${LDFLAGS}

debug: clean
	$(CC) ${CFLAGS} -g -DCM_DEBUG ${QL_CM_SRC} ${QL_CM_DHCP} -o quectel-CM -lpthread -ldl -lrt

qmi-proxy:
	$(CC) ${CFLAGS} quectel-qmi-proxy.c -o quectel-qmi-proxy ${LDFLAGS} 

mbim-proxy:
	$(CC) ${CFLAGS} quectel-mbim-proxy.c -o quectel-mbim-proxy ${LDFLAGS} 

qrtr-proxy:
	$(CC) ${CFLAGS} quectel-qrtr-proxy.c -o quectel-qrtr-proxy ${LDFLAGS} 

atc-proxy:
	$(CC) ${CFLAGS} quectel-atc-proxy.c atchannel.c at_tok.c util.c -o quectel-atc-proxy ${LDFLAGS} 

clean:
	rm -rf *.o libmnl/*.o quectel-CM quectel-qmi-proxy quectel-mbim-proxy quectel-atc-proxy