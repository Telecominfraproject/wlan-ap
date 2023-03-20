# sources and intermediate files are separated

AM_CFLAGS = -Wall \
				-Wundef \
				-Wstrict-prototypes \
				-Wno-trigraphs

AM_CPPFLAGS = -DIMAGE_APPS_PROC \
				  -DFEATURE_Q_SINGLE_LINK \
				  -DFEATURE_Q_NO_SELF_QPTR \
				  -DFEATURE_LINUX \
				  -DFEATURE_NATIVELINUX \
				  -DFEATURE_DSM_DUP_ITEMS \
				  -DFEATURE_LE_DIAG \
				  -Iinclude \
				  -I../include

h_sources  =../include/diag.h
h_sources +=../include/diag_lsm.h
h_sources +=../include/diag_lsm_dci.h
h_sources +=../include/diagpkt.h
h_sources +=../include/event_defs.h
h_sources +=../include/event.h
h_sources +=../include/log_codes.h
h_sources +=../include/log.h
h_sources +=../include/msg_arrays_i.h
h_sources +=../include/msgcfg.h
h_sources +=../include/msg.h
h_sources +=../include/msg_pkt_defs.h
h_sources +=../include/msg_qsr.h
h_sources +=../include/msgtgt.h
h_sources +=../include/diagcmd.h
h_sources +=../src/diagdiag.h
h_sources +=../include/diagi.h

c_sources  =diag_lsm.c \
				ts_linux.c \
				diag_lsm_dci.c \
				diag_lsm_event.c \
				diag_lsm_log.c \
				diag_lsm_msg.c \
				diag_lsm_pkt.c \
				diagsvc_malloc.c \
				msg_arrays_i.c \
				diag_logger.c \
				diag_qshrink4_db_parser.c

requiredlibs = $(TGENOFF_LIBS)

library_includedir = $(pkgincludedir)
library_include_HEADERS = $(h_sources)
lib_LTLIBRARIES = libdiag.la
libdiag_la_CC = @CC@
libdiag_la_SOURCES = $(c_sources)
libdiag_la_CPPFLAGS =$(AM_CPPFLAGS)
libdiag_la_LIBADD = $(requiredlibs)
if USE_GLIB
libdiag_la_CFLAGS = $(AM_CFLAGS) -fPIC -DUSE_GLIB @GLIB_CFLAGS@
libdiag_la_LDFLAGS = -lpthread @GLIB_LIBS@ -shared -version-info $(DIAG_LIBRARY_VERSION)
else
libdiag_la_CFLAGS = $(AM_CFLAGS) -fPIC
libdiag_la_LDFLAGS = -lcutils -shared -version-info $(DIAG_LIBRARY_VERSION)
endif
