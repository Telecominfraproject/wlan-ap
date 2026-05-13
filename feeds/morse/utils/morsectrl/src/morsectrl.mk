#
# Copyright 2022-2023 Morse Micro
#

MORSECTRL_SRCS := $(SRCS)

MORSECTRL_SRCS += fem.c
MORSECTRL_SRCS += mcs.c
MORSECTRL_SRCS += bw.c
MORSECTRL_SRCS += rpg.c
MORSECTRL_SRCS += ifs.c
MORSECTRL_SRCS += qos.c
MORSECTRL_SRCS += responseindication.c
MORSECTRL_SRCS += transmissionrate.c
MORSECTRL_SRCS += statype.c
MORSECTRL_SRCS += encmode.c
MORSECTRL_SRCS += txop.c
MORSECTRL_SRCS += controlresponse.c
MORSECTRL_SRCS += ndpprobes.c
MORSECTRL_SRCS += turbo.c
MORSECTRL_SRCS += force_assert.c
MORSECTRL_SRCS += lnabypass.c
MORSECTRL_SRCS += txscaler.c
MORSECTRL_SRCS += transmit_cw.c
MORSECTRL_SRCS += periodic_cal.c
MORSECTRL_SRCS += dtim_channel.c
MORSECTRL_SRCS += bcn_rssi_threshold.c
MORSECTRL_SRCS += sig_field_error_evt.c
MORSECTRL_SRCS += antenna.c
MORSECTRL_SRCS += tdc_pg_disable.c
MORSECTRL_SRCS += capabilities.c
MORSECTRL_SRCS += transraw.c
MORSECTRL_SRCS += hwkeydump.c
MORSECTRL_SRCS += twt.c
MORSECTRL_SRCS += tsf.c
MORSECTRL_SRCS += mpsw.c
MORSECTRL_SRCS += phy_deaf.c
MORSECTRL_SRCS += override_pa_on_delay.c
MORSECTRL_SRCS += fsg.c
MORSECTRL_SRCS += chan_query.c
MORSECTRL_SRCS += edconfig.c
MORSECTRL_SRCS += ocs.c
MORSECTRL_SRCS += tx_pwr_adj.c
MORSECTRL_SRCS += agc_gaincode.c
MORSECTRL_SRCS += gpio.c
MORSECTRL_SRCS += physm_watchdog.c
MORSECTRL_SRCS += mesh_config.c
MORSECTRL_SRCS += mbca.c
MORSECTRL_SRCS += dynamic_peering.c

MORSECTRL_WIN_SRCS := $(WIN_SRCS)

MORSECTRL_LINUX_SRCS := $(LINUX_SRCS)
MORSECTRL_LINUX_SRCS += io.c
MORSECTRL_LINUX_SRCS += serial.c
MORSECTRL_LINUX_SRCS += jtag.c

# As noted in transport.c, the default transport is the first transport linked. Therefore we
# put LINUX_SRCS before SRCS so that nl80211 has higher priority.
MORSECTRL_OBJS = $(patsubst %.c, %_ctrl.o, $(MORSECTRL_LINUX_SRCS) $(MORSECTRL_SRCS))
MORSECTRL_OBJS_WIN = $(patsubst %.c, %_ctrl_win.o, $(MORSECTRL_SRCS) $(MORSECTRL_WIN_SRCS))

all: morsectrl morse_cli

%_ctrl.o: %.c $(DEPS)
	@echo Compiling $<
	$(Q) $(CC) $(MORSECTRL_CFLAGS) $(LINUX_CFLAGS) -c -o $@  $<

morsectrl: $(MORSECTRL_OBJS)
	@echo Linking $@
	$(Q) $(CC) $(MORSECTRL_CFLAGS) $(LINUX_CFLAGS) -o $@ $^ \
		$(MORSECTRL_LDFLAGS) $(LINUX_LDFLAGS)

%_ctrl_win.o: %.c $(DEPS)
	@echo Compiling $<
	$(Q) $(WIN_CC) $(MORSECTRL_CFLAGS) $(WIN_CFLAGS) -c -o $@  $<

morsectrl_win: $(MORSECTRL_OBJS_WIN)
	@echo Linking $@
	$(Q) $(WIN_CC) $(MORSECTRL_CFLAGS) $(WIN_CFLAGS) -o morsectrl $^ \
		$(MORSECTRL_LDFLAGS) $(WIN_LDFLAGS)

install:
	@echo Installing morsectrl to /usr/bin
	$(Q) cp morsectrl /usr/bin
