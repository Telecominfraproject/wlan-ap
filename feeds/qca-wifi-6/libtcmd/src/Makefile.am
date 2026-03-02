AM_CFLAGS = -Wall \
            -g -O0 \
            -DLIBNL_2 \
            -DWLAN_API_NL80211 \
            $(LIBNL_CFLAGS)

if USE_GLIB
AM_CFLAGS += -DUSE_GLIB $(GLIB_CFLAGS)
endif

if MDM_SET
AM_CFLAGS += -DMDM
endif

c_sources = os.c \
            nl80211.c \
            libtcmd.c

AM_CFLAGS += -DWLAN_API_NL80211
lib_LIBRARIES  = libtcmd.a
libtcmd_a_SOURCES = $(c_sources)
libtcmd_a_LDFLAGS = -static
pkginclude_HEADERS = libtcmd.h
