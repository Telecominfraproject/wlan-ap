#CC := $(ATH_CROSS_COMPILE_TYPE)gcc
TARGET_TYPE ?= AR9888
TARGET_VERS ?= v2

#Sources to compile
CSRCS := ftm_main.c ftm_wlan.c ftm_write_to_flash.c

all:
	$(CC) $(CFLAGS) $(LDFLAGS) -g3 -Wall  \
		$(CSRCS) -o ftm
clean:
	rm -f ftm
