/*
 * Copyright (c) 2019 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
*/

/*
* 2011-2012, 2016 Qualcomm Atheros Inc. All Rights Reserved.
* Qualcomm Atheros Proprietary and Confidential.
*/

#include "string.h"
#include "libtcmd.h"
#include "os.h"

#ifdef USE_GLIB
#include <glib.h>
#define strlcat g_strlcat
#define strlcpy g_strlcpy
#endif

int tcmd_tx(void *buf, int len, bool resp)
{
	int err = 0;

	/* XXX: just call nl80211 directly for now */
#ifdef WLAN_API_NL80211
	err = nl80211_tcmd_tx(&tcmd_cfg, buf, len);
	if (err)
		goto err_out;
#endif
	if (resp)
#ifdef WLAN_API_NL80211
		err = nl80211_tcmd_rx(&tcmd_cfg);
#endif

	return err;
err_out:
	A_DBG("tcmd_tx failed: %s\n", strerror(-err));
	return err;
}

static void tcmd_expire(union sigval sig)
{
	/* tcmd expired, do something */
	A_DBG("timer expired %d\n",sig.sival_int);
	tcmd_cfg.timeout = true;
}

#ifdef CONFIG_AR6002_REV6
int tcmd_tx_init(char *iface, void (*rx_cb)(void *buf, int len))
{
	int err;

	strlcpy(tcmd_cfg.iface, iface, sizeof(tcmd_cfg.iface));
	tcmd_cfg.rx_cb = rx_cb;

	tcmd_cfg.sev.sigev_notify = SIGEV_THREAD;
	tcmd_cfg.sev.sigev_notify_function = tcmd_expire;
	timer_create(CLOCK_REALTIME, &tcmd_cfg.sev, &tcmd_cfg.timer);

#ifdef WLAN_API_NL80211
	err = nl80211_init(&tcmd_cfg);
	if (err) {
		A_DBG("couldn't init nl80211!: %s\n", strerror(-err));
		return err;
	}
#endif

	return 0;
}

#else
/* get driver ep from tcmd ep */
static int tcmd_set_ep(uint32_t *driv_ep, enum tcmd_ep ep)
{
#ifdef WLAN_API_NL80211
	return nl80211_set_ep(driv_ep, ep);
#endif
}

void tcmd_response_cb(void *buf, int len)
{
	tcmd_cfg.timeout = true;
	tcmd_reset_timer(&tcmd_cfg);
	tcmd_cfg.docommand_rx_cb(buf, len);
}

int tcmd_init(char *iface, void (*rx_cb)(void *buf, int len), ...)
{
	int err;
	enum tcmd_ep ep;
	va_list ap;
	va_start(ap, rx_cb);
	ep = va_arg(ap, enum tcmd_ep);
	va_end(ap);

	strlcpy(tcmd_cfg.iface, iface, sizeof(tcmd_cfg.iface));
	tcmd_cfg.docommand_rx_cb = rx_cb;
	tcmd_cfg.rx_cb = tcmd_response_cb;
	err = tcmd_set_ep(&tcmd_cfg.ep, ep);
	if (err)
		return err;

	tcmd_cfg.sev.sigev_notify = SIGEV_THREAD;
	tcmd_cfg.sev.sigev_notify_function = tcmd_expire;
	timer_create(CLOCK_REALTIME, &tcmd_cfg.sev, &tcmd_cfg.timer);

#ifdef WLAN_API_NL80211
	err = nl80211_init(&tcmd_cfg);
	if (err) {
		A_DBG("couldn't init nl80211!: %s\n", strerror(-err));
		return err;
	}
#endif

	return 0;
}
int tcmd_tx_start( void )
{
	return nl80211_tcmd_start(&tcmd_cfg);
}

int tcmd_tx_stop( void )
{
	return nl80211_tcmd_stop(&tcmd_cfg);
}
int tcmd_tx_init(char *iface, void (*rx_cb)(void *buf, int len))
{
	return tcmd_init(iface, rx_cb, TCMD_EP_TCMD);
}
#endif
