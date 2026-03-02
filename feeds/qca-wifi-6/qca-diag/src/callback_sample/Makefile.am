
AM_CFLAGS = -Wall \
		-Wundef \
		-Wstrict-prototypes \
		-Wno-trigraphs

AM_CPPFLAGS = -D__packed__= \
		-DIMAGE_APPS_PROC \
		-DFEATURE_Q_SINGLE_LINK \
		-DFEATURE_Q_NO_SELF_QPTR \
		-DFEATURE_LINUX \
		-DFEATURE_NATIVELINUX \
		-DFEATURE_DSM_DUP_ITEMS \
		-DFEATURE_LE_DIAG \
		-I../src \
		-I../include

bin_PROGRAMS = diag_callback_sample

diag_callback_sample_SOURCES = diag_callback_sample.c
diag_callback_sample_LDADD = ../src/libdiag.la
if USE_GLIB
diag_callback_sample_CFLAGS =  -DUSE_GLIB @GLIB_CFLAGS@
diag_callback_sample_LDFLAGS = -lpthread @GLIB_LIBS@
else
diag_callback_sample_LDFLAGS = -lpthread -lcutils
endif
