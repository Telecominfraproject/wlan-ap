UNIT_DISABLE := $(if $(CONFIG_MANAGER_NM),n,y)

UNIT_NAME := nm
UNIT_TYPE := BIN

UNIT_SRC    += src/ubus.c
UNIT_SRC    += src/wifi_inet_config.c
UNIT_SRC    += src/wifi_inet_state.c
UNIT_SRC    += src/dhcp.c
UNIT_SRC    += src/firewall.c
UNIT_SRC    += src/main.c
UNIT_SRC    += src/dhcp_lease.c
UNIT_SRC    += src/inet_iface.c
UNIT_SRC    += src/inet_conf.c
UNIT_SRC    += src/dhcp_fingerprint.c

UNIT_CFLAGS := -I$(UNIT_PATH)/inc
UNIT_CFLAGS += -Isrc/lib/common/inc/
UNIT_CFLAGS += -Isrc/lib/version/inc/

UNIT_LDFLAGS += -lev -lubus -lubox -luci -lblobmsg_json -lnl-tiny
UNIT_LDFLAGS += -lrt

UNIT_EXPORT_CFLAGS := $(UNIT_CFLAGS)
UNIT_EXPORT_LDFLAGS := $(UNIT_LDFLAGS)

UNIT_DEPS += src/lib/inet
UNIT_DEPS += src/lib/ovsdb
UNIT_DEPS += src/lib/pjs
UNIT_DEPS += src/lib/reflink
UNIT_DEPS += src/lib/schema
UNIT_DEPS += src/lib/synclist
UNIT_DEPS_CFLAGS += src/lib/version
