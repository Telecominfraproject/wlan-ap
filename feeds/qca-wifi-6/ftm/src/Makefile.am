AM_CFLAGS = -Wall \
            -g -O0 \
            $(DIAG_CFLAGS)

AM_CPPFLAGS = -Wall \
            -g -O0 \
            $(DIAG_CFLAGS)

AM_CFLAGS += -I${WORKSPACE}/system/bt/hci/include
AM_CFLAGS += -I${WORKSPACE}/vendor/qcom/proprietary/bt/hci_qcomm_init/

AM_CPPFLAGS += -I${WORKSPACE}/system/bt/hci/include
AM_CPPFLAGS += -I${WORKSPACE}/vendor/qcom/proprietary/bt/hci_qcomm_init/

if DEBUG
AM_CFLAGS += -DDEBUG
AM_CPPFLAGS += -DDEBUG
endif

requiredlibs = -lrt $(DIAG_LIBS)

if USE_GLIB
AM_CFLAGS += -DUSE_GLIB $(GLIB_CFLAGS)
AM_CPPFLAGS += -DUSE_GLIB $(GLIB_CFLAGS)
requiredlibs += $(GLIB_LIBS)
endif

#By default build for MDM_LE
AM_CFLAGS += -DMDM_LE
AM_CPPFLAGS += -DMDM_LE

if MDM_ROME
AM_CFLAGS += -DBT_SOC_TYPE_ROME
AM_CPPFLAGS += -DBT_SOC_TYPE_ROME
else
if MDM_PRONTO
AM_CFLAGS += -DHCI_USE_MCT
AM_CPPFLAGS += -DHCI_USE_MCT
endif
endif

c_sources = ftm_main.c

if CONFIG_FTM_BT
AM_CFLAGS += -DCONFIG_FTM_BT -DBT_NV_SUPPORT
AM_CPPFLAGS += -DCONFIG_FTM_BT -DBT_NV_SUPPORT
c_sources += ftm_bt.c
c_sources += ftm_bt_power_pfal_linux.c
c_sources += ftm_bt_hci_pfal_linux.c
c_sources += ftm_bt_persist.cpp
endif

if CONFIG_FTM_FM
AM_CFLAGS += -DCONFIG_FTM_FM
c_sources += ftm_fm.c
c_sources += ftm_fm_pfal_linux.c
endif

if CONFIG_FTM_ANT
AM_CFLAGS += -DCONFIG_FTM_ANT
c_sources += ftm_ant.c
endif

if CONFIG_FTM_NFC
AM_CFLAGS += -DCONFIG_FTM_NFC
c_sources += ftm_nfc.c
c_sources += ftm_nfcnq.c
c_sources += ftm_nfcqti.c
c_sources += ftm_nfcnq_fwdl.c
c_sources += ftm_nfcnq_test.c
endif

if CONFIG_FTM_WLAN

AM_CFLAGS += -DCONFIG_FTM_WLAN -DCONFIG_FTM_WLAN_AUTOLOAD
AM_CFLAGS += $(LIBNL_CFLAGS) $(ATH6KL_UTILS_CFLAGS)
AM_CPPFLAGS += $(LIBNL_CFLAGS) $(ATH6KL_UTILS_CFLAGS)

requiredlibs += $(ATH6KL_UTILS_LIBS) $(LIBNL_LIBS)

c_sources += ftm_wlan.c
endif

ftmdaemon_SOURCES = $(c_sources)
ftmdaemon_LDADD = -ldl $(requiredlibs) -lbtnv
bin_PROGRAMS = ftmdaemon
