# sources and intermediate files are separated
vpath %.c $(SRCDIR)

CPPFLAGS += $(QCT_CPPFLAGS)
CPPFLAGS += -I$(SRCDIR)
CPPFLAGS += -I$(SRCDIR)/../include
CPPFLAGS += -I$(KERNEL_DIR)/include
CPPFLAGS += -I$(KERNEL_OBJDIR)/include
CPPFLAGS += -I$(KERNEL_OBJDIR)/include2
CPPFLAGS += -DFEATURE_LE_DIAG

CFLAGS   += $(QCT_CFLAGS)

all: libdiag.so.$(LIBVER)

libdiag.so.$(LIBVER): diag_lsm.c diag_lsm_dci.c ts_linux.c diag_lsm_event.c diag_lsm_log.c diag_lsm_msg.c diag_lsm_pkt.c diagsvc_malloc.c msg_arrays_i.c diag_logger.c diag_qshrink4_db_parser.c
	$(CC) $(CPPFLAGS) $(CFLAGS) $(QCT_CFLAGS_SO) $(QCT_LDFLAGS_SO) -Wl,-soname,libdiag.so.$(LIBMAJOR) -o $@ $^


###############################################################################
# Test Target
###############################################################################
ifdef BUILD_TEST

test_diag: libdiag.so.$(LIBVER)
diag_klog: libdiag.so.$(LIBVER)
PktRspTest:   libdiag.so.$(LIBVER)
diag_socket_log:   libdiag.so.$(LIBVER)

all: test_diag diag_klog PktRspTest diag_socket_log

# build invoked elsewhere to keep sources and intermediate files separate
vpath %.c $(SRCDIR)/../test

TEST_DIAG_LDLIBS += $(OBJDIR)/libdiag.so.$(LIBVER) -lstringl

test_diag: test_diag.c
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(TEST_DIAG_LDLIBS) -lpthread

vpath %.c $(SRCDIR)/../klog
diag_klog: diag_klog.c
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(TEST_DIAG_LDLIBS) -lpthread

vpath %.c $(SRCDIR)/../PktRspTest
PktRspTest: PktRspTest.c
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(TEST_DIAG_LDLIBS) -lpthread

vpath %.c $(SRCDIR)/../socket_log
diag_socket_log: diag_socket_log.c
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(TEST_DIAG_LDLIBS) -lpthread

endif	# BUILD_TEST

