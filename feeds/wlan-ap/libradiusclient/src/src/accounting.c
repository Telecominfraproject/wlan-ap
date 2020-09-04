/* SPDX-License-Identifier: BSD-3-Clause */

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <netinet/in.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <sys/socket.h>


#include "radiusd.h"
#include "radius_debug.h"
#include "radius.h"
#include "radius_client.h"
#include "ap.h"
#include "eloop_libradius.h"
#include "accounting.h"

#if 0
#include "driver.h"
#include "sta_info.h"

const char *radius_mode_txt(hostapd *hapd);
int radius_sta_rate(struct hostapd_data *hapd, struct sta_info *sta);
#endif


static struct radius_msg * accounting_msg(radiusd *radd, struct sta_info *sta,
                                          int status_type)
{
  struct radius_msg *msg;
  char buf[128];
  u8 *val;
  size_t len;

  msg = radius_msg_new(RADIUS_CODE_ACCOUNTING_REQUEST,
  		     radius_client_get_id(radd->radius_client));
  if (msg == NULL) 
  {
    radius_debug_print(RADIUSD_DEBUG_MSGDUMPS, 
                       "Could not create net RADIUS packet\n");
    return NULL;
  }

  radius_debug_print(RADIUSD_DEBUG_MSGDUMPS, "%s: msg: %p, data: %p, len: %d\n",
                     __FUNCTION__,msg,sta,sizeof(sta));

  radius_msg_make_authenticator(msg, (u8 *) sta, sizeof(sta));

  snprintf(buf, sizeof(buf), "%08X-%08X",
           sta->acct_session_id_hi, sta->acct_session_id_lo);

  if (!radius_msg_add_attr(msg, RADIUS_ATTR_ACCT_SESSION_ID,
                           (u8 *) buf, strlen(buf))) 
  {
    radius_debug_print(RADIUSD_DEBUG_MSGDUMPS, 
                       "Could not add Acct-Session-Id\n");
    goto fail;
  }

  if (!radius_msg_add_attr_int32(msg, RADIUS_ATTR_ACCT_STATUS_TYPE,
                                 status_type)) 
  {
  	radius_debug_print(RADIUSD_DEBUG_MSGDUMPS, 
                       "Could not add Acct-Status-Type\n");
  	goto fail;
  }

  if (!radius_msg_add_attr_int32(msg, RADIUS_ATTR_ACCT_AUTHENTIC,
                                 RADIUS_ACCT_AUTHENTIC_RADIUS)) 
  {
    radius_debug_print(RADIUSD_DEBUG_MSGDUMPS, "Could not add Acct-Authentic\n");
    goto fail;
  }

  val = sta->identity;
  len = sta->identity_len;

  if (!val) 
  {
    snprintf(buf, sizeof(buf), RADIUS_ADDR_FORMAT,MAC2STR(sta->addr));
    val = (u8 *) buf;
    len = strlen((char *) val);
  }

  if (!radius_msg_add_attr(msg, RADIUS_ATTR_USER_NAME, val, len)) 
  {
    radius_debug_print(RADIUSD_DEBUG_MSGDUMPS, "Could not add User-Name\n");
    goto fail;
  }

  if (radd->conf->own_ip_addr.af == AF_INET &&
      !radius_msg_add_attr(msg, RADIUS_ATTR_NAS_IP_ADDRESS,
                           (u8 *) &radd->conf->own_ip_addr.u.v4, 4)) 
  {
    radius_debug_print(RADIUSD_DEBUG_MSGDUMPS, 
                       "Could not add NAS-IP-Address\n");
    goto fail;
  }

  if (radd->conf->own_ip_addr.af == AF_INET6 &&
      !radius_msg_add_attr(msg, RADIUS_ATTR_NAS_IPV6_ADDRESS,
                           (u8 *) &radd->conf->own_ip_addr.u.v6, 16)) 
  {
    radius_debug_print(RADIUSD_DEBUG_MSGDUMPS, 
                       "Could not add NAS-IPv6-Address\n");
    goto fail;
  }

  if (radd->conf->nas_identifier &&
      !radius_msg_add_attr(msg, RADIUS_ATTR_NAS_IDENTIFIER,
           (u8 *) radd->conf->nas_identifier,
           strlen(radd->conf->nas_identifier))) 
  {
    radius_debug_print(RADIUSD_DEBUG_MSGDUMPS, "Could not add NAS-Identifier\n");
    goto fail;
  }

  if (!radius_msg_add_attr_int32(msg, RADIUS_ATTR_NAS_PORT, sta->sta_id)) 
  {
    radius_debug_print(RADIUSD_DEBUG_MSGDUMPS, "Could not add NAS-Port\n");
    goto fail;
  }

  snprintf(buf, sizeof(buf), RADIUS_802_1X_ADDR_FORMAT,
           MAC2STR(radd->own_addr));

  if (!radius_msg_add_attr(msg, RADIUS_ATTR_CALLED_STATION_ID,
                           (u8 *) buf, strlen(buf))) 
  {
    radius_debug_print (RADIUSD_DEBUG_MSGDUMPS, 
                        "Could not add Called-Station-Id\n");
    goto fail;
  }

  snprintf(buf, sizeof(buf), RADIUS_802_1X_ADDR_FORMAT,MAC2STR(sta->addr));

  if (!radius_msg_add_attr(msg, RADIUS_ATTR_CALLING_STATION_ID,
                           (u8 *) buf, strlen(buf))) 
  {
    radius_debug_print(RADIUSD_DEBUG_MSGDUMPS, 
                       "Could not add Calling-Station-Id\n");
    goto fail;
  }

  if (!radius_msg_add_attr_int32(msg, RADIUS_ATTR_NAS_PORT_TYPE,
                                 RADIUS_NAS_PORT_TYPE_IEEE_802_11)) 
  {
    radius_debug_print(RADIUSD_DEBUG_MSGDUMPS, "Could not add NAS-Port-Type\n");
    goto fail;
  }
#if 0
  snprintf(buf, sizeof(buf), "CONNECT %d%sMbps %s",
           radius_sta_rate(hapd, sta) / 2,
           (radius_sta_rate(hapd, sta) & 1) ? ".5" : "",
           radius_mode_txt(hapd));

  if (!radius_msg_add_attr(msg, RADIUS_ATTR_CONNECT_INFO,
                           (u8 *) buf, strlen(buf))) 
  {
    radius_debug_print(RADIUSD_DEBUG_MSGDUMPS, 
                       "Could not add Connect-Info\n");
    goto fail;
  }
#endif

  return msg;

 fail:
  radius_msg_free(msg);
  free(msg);
  return NULL;
}


void accounting_sta_start(radiusd *radd, struct sta_info *sta)
{
  struct radius_msg *msg;

  if (sta->acct_session_started)
  {
    return;
  }

  time(&sta->acct_session_start);

  if (!radd->conf->radius->acct_server)
  {
    return;
  }

  msg = accounting_msg(radd, sta, RADIUS_ACCT_STATUS_TYPE_START);

  if (msg)
  {
    radius_client_send(radd->radius_client, msg, RADIUS_ACCT, sta->addr);
  }

  sta->acct_session_started = 1;
}


void accounting_sta_stop(radiusd *radd, struct sta_info *sta)
{
  struct radius_msg *msg;
  int cause = sta->acct_terminate_cause;
#if 0
  struct hostap_sta_driver_data data;
#endif

  if (!radd->conf->radius->acct_server)
  {
    return;
  }

  if (!sta->acct_session_started)
  {
    return;
  }
  sta->acct_session_started = 0;

  msg = accounting_msg(radd, sta, RADIUS_ACCT_STATUS_TYPE_STOP);
  if (!msg) 
  {
    radius_debug_print(RADIUSD_DEBUG_MSGDUMPS, 
                       "Could not create RADIUS Accounting message\n");
    return;
  }

  if (!radius_msg_add_attr_int32(msg, RADIUS_ATTR_ACCT_SESSION_TIME,
                                 time(NULL) - sta->acct_session_start)) 
  {
    radius_debug_print(RADIUSD_DEBUG_MSGDUMPS, 
                       "Could not add Acct-Session-Time\n");
    goto fail;
  }
#if 0
  if (hostapd_read_sta_data(hapd, &data, sta->addr) == 0) 
  {
  	if (!radius_msg_add_attr_int32(msg,
                                   RADIUS_ATTR_ACCT_INPUT_PACKETS,
                                   data.rx_packets)) 
    {
  	  radius_debug_print(RADIUSD_DEBUG_MSGDUMPS, 
                         "Could not add Acct-Input-Packets\n");
  	  goto fail;
  	}

    if (!radius_msg_add_attr_int32(msg,RADIUS_ATTR_ACCT_OUTPUT_PACKETS,
                                   data.tx_packets)) 
    {
  	  radius_debug_print(RADIUSD_DEBUG_MSGDUMPS, 
                         "Could not add Acct-Output-Packets\n");
  	  goto fail;
  	}
  	if (!radius_msg_add_attr_int32(msg,RADIUS_ATTR_ACCT_INPUT_OCTETS,
                                   data.rx_bytes)) 
    {
  	  radius_debug_print(RADIUSD_DEBUG_MSGDUMPS, "Could not add Acct-Input-Octets\n");
  	  goto fail;
  	}
  	if (!radius_msg_add_attr_int32(msg,RADIUS_ATTR_ACCT_OUTPUT_OCTETS,
                                   data.tx_bytes)) 
    {
  	  radius_debug_print(RADIUSD_DEBUG_MSGDUMPS, "Could not add Acct-Output-Octets\n");
  	  goto fail;
  	}
  }
#endif

  if (eloop_terminated())
  {
    cause = RADIUS_ACCT_TERMINATE_CAUSE_ADMIN_REBOOT;
  }

  if (cause &&
      !radius_msg_add_attr_int32(msg, RADIUS_ATTR_ACCT_TERMINATE_CAUSE,cause)) 
  {
    radius_debug_print(RADIUSD_DEBUG_MSGDUMPS, "Could not add Acct-Terminate-Cause\n");
    goto fail;
  }

  if (msg)
  {
    radius_client_send(radd->radius_client, msg, RADIUS_ACCT, sta->addr);
  }
  return;

fail:
  radius_msg_free(msg);
  free(msg);
}

void accounting_sta_get_id(struct radius_data *radd, struct sta_info *sta)
{
  sta->acct_session_id_lo = radd->acct_session_id_lo++;
  if (radd->acct_session_id_lo == 0) 
  {
    radd->acct_session_id_hi++;
  }
  sta->acct_session_id_hi = radd->acct_session_id_hi;
}


/* Process the RADIUS frames from Accounting Server */
static RadiusRxResult
accounting_receive( struct radius_msg *msg, struct radius_msg *req,
                    u8 *shared_secret, size_t shared_secret_len, void *data)
{
  if (msg->hdr->code != RADIUS_CODE_ACCOUNTING_RESPONSE) 
  {
    printf("Unknown RADIUS message code\n");
    return RADIUS_RX_UNKNOWN;
  }

  if (radius_msg_verify_acct(msg, shared_secret, shared_secret_len, req))
  {
    printf("Incoming RADIUS packet did not have correct "
  	       "Authenticator - dropped\n");
    return RADIUS_RX_INVALID_AUTHENTICATOR;
  }

  return RADIUS_RX_PROCESSED;
}


int accounting_init(radiusd *radd)
{
  static int first = 1;

  /* Acct-Session-Id should be unique over reboots. If reliable clock is
   * not available, this could be replaced with reboot counter, etc. */
  if (first) 
  {
    first = 0;
    radd->acct_session_id_hi = time(NULL);
  }

  if (radius_client_register(radd->radius_client, RADIUS_ACCT, 
                             accounting_receive,radd))
  {
    return -1;
  }

  return 0;
}

int accounting_remove_sta_all(struct sta_info *sta, void *data)
{
  struct radius_msg *msg;
  radiusd *radd = (radiusd*)data;

  if (!sta->acct_session_started)
  {
    return 0;
  }

  /* Inform RADIUS server that accounting will start/stop so that the
  * server can close old accounting sessions. */
  msg = accounting_msg(radd, sta,RADIUS_ACCT_STATUS_TYPE_ACCOUNTING_OFF);

  if (!msg)
  {
    return -1;
  }

  if (!radius_msg_add_attr_int32(msg, RADIUS_ATTR_ACCT_TERMINATE_CAUSE,
                                 RADIUS_ACCT_TERMINATE_CAUSE_NAS_REBOOT))
  {
    printf("Could not add Acct-Terminate-Cause\n");
    radius_msg_free(msg);
    free(msg);
    return -1;
  }

  radius_client_send(radd->radius_client, msg, RADIUS_ACCT, NULL);

  sta->acct_session_started = 0;

  return 0;
}

void accounting_deinit(radiusd *radd)
{
  /* radius_client_unregister(hapd, RADIUS_ACCT, accounting_receive);*/

  if (!radd->conf->radius->acct_server || radd->radius_client == NULL)
  {
    return;
  }

   /* ap_sta_for_each(hapd,accounting_remove_sta_all,hapd); */
}


/* int accounting_reconfig(radiusd *hapd, struct hostapd_config *oldconf) */
int accounting_reconfig(radiusd *radd)
{
  if (!radd->radius_client_reconfigured)
  {
    return 0;
  }

  accounting_deinit(radd);
  return accounting_init(radd);
}
