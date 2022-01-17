INCLUDE_DIR +=  include -I src
LIB	:= -lpthread -shared
LDFLAGS +=  -Llib/ -ldiag -lpthread -pie
FLAGS = -fPIC -g -DUSE_MUSL
EXTRA_CFLAGS = $(TARGET_CFLAGS) $(TARGET_LDFLAGS) $(TARGET_CPPFLAGS) -fstack-protector-all -znow -zrelro -Werror
sample_cliobj	:= dci_client/diag_dci_sample.c
klogobj		:= klog/diag_klog.c
mdobj		:= mdlog/diag_mdlog.c
socketobj	:= socket_log/diag_socket_log.c
uartobj		:= uart_log/diag_uart_log.c
testdiag	:= test/test_diag.c

libdiag := src/diag_lsm.c src/diag_lsm_dci.c src/ts_linux.c src/diag_lsm_event.c \
	src/diag_lsm_log.c src/diag_lsm_msg.c src/diag_lsm_pkt.c \
	src/diagsvc_malloc.c src/msg_arrays_i.c src/diag_qshrink4_db_parser.c

all:
	$(CC) -o libdiag.so $(libdiag) $(TARGET_LDFLAGS) $(FLAGS) $(EXTRA_CFLAGS) -I $(INCLUDE_DIR) $(LIB)
	mkdir -p lib
	mv libdiag.so lib/

	$(CC) -o diag_socket_app  $(socketobj) $(LDFLAGS) $(EXTRA_CFLAGS) -fpie -I $(INCLUDE_DIR)
	mkdir -p bin
	mv diag_socket_app bin/

	$(CC) -o registerReboot registerReboot.c $(LDFLAGS) $(EXTRA_CFLAGS) -fpie -I $(INCLUDE_DIR)
	mkdir -p bin
	mv registerReboot bin/

	$(CC) -o qld_server qld_server.c $(LDFLAGS) $(EXTRA_CFLAGS) -fpie -I $(INCLUDE_DIR)
	mkdir -p bin
	mv qld_server bin/

	$(CC) -o diag_stress_app  $(testdiag) $(LDFLAGS) $(EXTRA_CFLAGS) -fpie -I $(INCLUDE_DIR)
	mv diag_stress_app bin/

	$(CC) -o diag_mdlog $(mdobj) $(LDFLAGS) $(EXTRA_CFLAGS) -fpie -I $(INCLUDE_DIR)
	mkdir -p bin
	mv diag_mdlog bin/

clean:
	rm -rf lib/
	rm -rf bin/
	rm -rf *.o
