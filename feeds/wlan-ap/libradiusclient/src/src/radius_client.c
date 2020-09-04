/* SPDX-License-Identifier: BSD-3-Clause */

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <netinet/in.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <errno.h>

#include "radiusd.h"
#include "radius_debug.h"
#include "radius.h"
#include "radius_client.h"
#include "eloop_libradius.h"

/* Defaults for RADIUS retransmit values (exponential backoff) */
#define RADIUS_CLIENT_FIRST_WAIT 3 /* seconds */
#define RADIUS_CLIENT_MAX_WAIT 120 /* seconds */
#define RADIUS_CLIENT_MAX_RETRIES 10 /* maximum number of retransmit attempts
				      * before entry is removed from retransmit
				      * list */
#define RADIUS_CLIENT_MAX_ENTRIES 30 /* maximum number of entries in retransmit
				      * list (oldest will be removed, if this
				      * limit is exceeded) */

#if 0
#define RADIUS_CLIENT_NUM_FAILOVER 4 /* try to change RADIUS server after this
                                      * many failed retry attempts */
#endif 
#define RADIUS_CLIENT_NUM_FAILOVER 2 /* try to change RADIUS server after this
                                      * many failed retry attempts */


static void radius_client_receive(int sock, void *eloop_ctx, void *sock_ctx);

static int
radius_change_server(struct radius_client_data *radius,
                     struct radius_server *nserv,
                     struct radius_server *oserv,
                     int sock, int sock6, int auth);

static int radius_client_init_acct(struct radius_client_data *radius);
static int radius_client_init_auth(struct radius_client_data *radius);

static void radius_server_update_acct_counters(struct radius_server *nserv,
                                               struct radius_server *oserv,
                                               int num);

static void radius_client_msg_free(struct radius_msg_list *req)
{
  radius_msg_free(req->msg);
  free(req->msg);
  free(req);
}


int radius_client_register(struct radius_client_data *radius,
                           RadiusType msg_type,
                           RadiusRxResult (*handler)(struct radius_msg *msg,
                                                     struct radius_msg *req,
                                                     u8 *shared_secret,
                                                     size_t shared_secret_len,
                                                     void *data),
                           void *data)
{
  struct radius_rx_handler **handlers, *newh;
  size_t *num;

  if (msg_type == RADIUS_ACCT) 
  {
  	handlers = &radius->acct_handlers;
  	num = &radius->num_acct_handlers;
  } 
  else 
  {
  	handlers = &radius->auth_handlers;
  	num = &radius->num_auth_handlers;
  }

  newh = (struct radius_rx_handler *)
         realloc(*handlers,(*num + 1) * sizeof(struct radius_rx_handler));

  if (newh == NULL)
  {
    return -1;
  }

  newh[*num].handler = handler;
  newh[*num].data = data;
  (*num)++;
  *handlers = newh;

  return 0;
}


int radius_client_unregister(struct radius_client_data *radius,
                             RadiusType msg_type,
                             RadiusRxResult (*handler)
                             (struct radius_msg *msg, struct radius_msg *req,
                              u8 *shared_secret, size_t shared_secret_len,
                              void *data))
{
  struct radius_rx_handler *handlers;
  size_t *num;
  int i;

  if (radius == NULL)
  {
    return -1;
  }

  if (msg_type == RADIUS_ACCT) 
  {
  	handlers = radius->acct_handlers;
  	num = &radius->num_acct_handlers;
  } 
  else 
  {
  	handlers = radius->auth_handlers;
  	num = &radius->num_auth_handlers;
  }

  for (i = 0; i < *num; i++) 
  {
  	if (handler == handlers[i].handler)
    {
      break;
    }
  }

  if (i >= *num)
  {
    return -1;
  }

  memmove(&handlers[i], &handlers[i + 1],
          (*num - i - 1) * sizeof(struct radius_rx_handler));
  (*num)--;

  return 0;
}

static void radius_client_handle_send_error(struct radius_client_data *radius,
                                            int s, RadiusType msg_type)
{
#ifndef CONFIG_NATIVE_WINDOWS
  int _errno = errno;
  radiusd_logger(radius->ctx, NULL,
                 RADIUSD_LEVEL_DEBUG, "send[RADIUS]");
  if (_errno == ENOTCONN || _errno == EDESTADDRREQ || _errno == EINVAL ||
      _errno == EBADF || s == -1 ) 
  {
    
    radiusd_logger(radius->ctx, NULL,RADIUSD_LEVEL_INFO, 
                   "Send failed - maybe interface status changed -" 
                   " try to connect again");
    if (msg_type == RADIUS_ACCT)
    {
      radius_client_init_acct(radius);
    }
    else
    {
      radius_client_init_auth(radius);
    }
  }
#endif /* CONFIG_NATIVE_WINDOWS */
}


static int radius_client_retransmit(struct radius_client_data *radius,
				    struct radius_msg_list *entry, time_t now)
{
  struct radius_servers *conf = radius->conf;
  int s, res;
 /*  radiusd *radd = radius->ctx; */

  if (entry->msg_type == RADIUS_ACCT) 
  {
  	s = radius->acct_sock;
  	if(entry->attempts == 0)
    {
      conf->acct_server->requests++;
    }
    else 
    {
      conf->acct_server->timeouts++;
      conf->acct_server->retransmissions++;
    }

  } 
  else 
  {
  	s = radius->auth_sock;
    if (entry->attempts == 0)
    {
      conf->auth_server->requests++;
    }
    else 
    {
      conf->auth_server->timeouts++;
      conf->auth_server->retransmissions++;
    }
  }

  /* retransmit; remove entry if too many attempts */
  entry->attempts++;
  radiusd_logger(radius->ctx, entry->addr, 
                 RADIUSD_LEVEL_DEBUG, "Resending RADIUS message (id=%d)",
                 entry->msg->hdr->identifier);
  gettimeofday(&entry->last_attempt, NULL);
 
  if (s >= 0)
  {
    res = send(s, entry->msg->buf, entry->msg->buf_used, 0);
  }
  else 
  {
    res = -1;
  }

  if (res < 0) 
  {
    radius_client_handle_send_error(radius, s, entry->msg_type);
  }


  entry->next_try = now + entry->next_wait;
  entry->next_wait *= 2;
  if (entry->next_wait > RADIUS_CLIENT_MAX_WAIT)
  {
    entry->next_wait = RADIUS_CLIENT_MAX_WAIT;
  }
  if (entry->attempts >= RADIUS_CLIENT_MAX_RETRIES) 
  {
    radius_debug_print(RADIUSD_DEBUG_MSGDUMPS, "Removing un-ACKed RADIUS message due to too many "
                                               "failed retransmit attempts\n"); 
  	return 1;
  }

  return 0;
}

static void radius_client_timer(void *eloop_ctx, void *timeout_ctx)
{
  struct radius_client_data *radius = eloop_ctx;
  struct radius_servers *conf = radius->conf;
  time_t now, first;
  struct radius_msg_list *entry, *prev, *tmp;
  int auth_failover = 0, acct_failover = 0;
  int acct_change_server = 0;
  char abuf[50];

  entry = radius->msgs;
  if (!entry)
  {
    return;
  }

  time(&now);
  first = 0;

  prev = NULL;
  while (entry) 
  {
  	if (now >= entry->next_try &&
  	    radius_client_retransmit(radius, entry, now)) 
    {
      if (prev)
      {
        prev->next = entry->next;
      }
      else
      {
        radius->msgs = entry->next;
      }

      tmp = entry;
      entry = entry->next;
      radius_client_msg_free(tmp);
      radius->num_msgs--;
      continue;
  	}

  	if (entry->attempts > RADIUS_CLIENT_NUM_FAILOVER ) 
    {
  		if (entry->msg_type == RADIUS_ACCT)
        {
          acct_failover++;
        }
  		else
        {
          auth_failover++;
        }
  	}

    /* if (entry->attempts == 4) */
    if (entry->attempts == RADIUS_CLIENT_NUM_FAILOVER) 
    {
  	  radiusd_logger(radius->ctx, NULL,
  		       RADIUSD_LEVEL_INFO, "Possible issue "
  		       "with RADIUS server connection - "
  		       "no reply received for first three "
  		       "attempts");
  	}

  	if (first == 0 || entry->next_try < first)
    {
      first = entry->next_try;
    }

  	prev = entry;
  	entry = entry->next;
  }

  if (radius->msgs) 
  {
  	if (first < now)
    {
      first = now;
    }
    eloop_register_timeout(first - now, 0,
                           radius_client_timer, radius, NULL);
  	 radiusd_logger(radius->ctx, NULL,
                    RADIUSD_LEVEL_DEBUG, "Next RADIUS client "
                                         "retransmit in %ld seconds",
                    (long int) (first - now));
  }

  if (auth_failover && conf->num_auth_servers > 1) 
  {
  	struct radius_server *next, *old;
  	old = conf->auth_server;
  	radiusd_logger(radius->ctx, NULL,
  		       RADIUSD_LEVEL_NOTICE,
  		       "No response from Authentication server "
  		       "%s:%d - failover",
  		        hostapd_ip_txt(&old->addr, abuf, sizeof(abuf)),
  		        old->port);

  	for (entry = radius->msgs; entry; entry = entry->next) 
    {
      if (entry->msg_type == RADIUS_AUTH)
      {
        old->timeouts++;
      }
    }

  	next = old + 1;
  	if (next > &(conf->auth_servers[conf->num_auth_servers - 1]))
    {
      next = conf->auth_servers;
    }
  	conf->auth_server = next;
    conf->currentServer = next->priority; 
  	/* add accounting failover as well since accounting request 
  	   to be sent to same authentication server */ 
  	acct_change_server = 1; 
  	radius_change_server(radius, next, old,
                         radius->auth_serv_sock, 
                         radius->auth_serv_sock6, 1);
  }
  else
  {
    conf->auth_failover = auth_failover;
  }

#if 1 /* LVL7 BEGIN  */
  if ((acct_change_server || acct_failover)  && conf->num_acct_servers > 1) 
  {
#else 
  if (acct_failover && conf->num_acct_servers > 1) 
  {
#endif /* LVL7 END */
    struct radius_server *next, *old;
	  old = conf->acct_server;
    radiusd_logger(radius->ctx, NULL,
  	       RADIUSD_LEVEL_NOTICE,
  	       "No response from Accounting server "
  	       "%s:%d - failover",
  	       hostapd_ip_txt(&old->addr, abuf, sizeof(abuf)),
  	       old->port);
    for (entry = radius->msgs; entry; entry = entry->next) 
    {
      if (entry->msg_type == RADIUS_ACCT)
      {
        old->timeouts++;
      }
    }

	  /* return only if acct failover and not acct change server */
	  if(acct_failover && !acct_change_server) 
    {
	    return;
	  }
	  radiusd_logger(radius->ctx, NULL,
		       RADIUSD_LEVEL_NOTICE,
		       "No response from Accounting server "
		       "%s:%d - failover",
		       hostapd_ip_txt(&old->addr, abuf, sizeof(abuf)),
		       old->port);

	  next = old + 1;
    if (next > &conf->acct_servers[conf->num_acct_servers - 1])
    {
      next = conf->acct_servers;
    }
	  conf->acct_server = next;
    conf->currentServer = next->priority;
    radius_change_server(radius, next, old,
			     radius->acct_serv_sock, 
			     radius->acct_serv_sock6,0);
  }
}

static void radius_client_update_timeout(struct radius_client_data *radius)
{
  time_t first, now;
  struct radius_msg_list *entry;

  eloop_cancel_timeout(radius_client_timer, radius, NULL);

  if (radius->msgs == NULL) 
  {
    return;
  }

  first = 0;
  for (entry = radius->msgs; entry; entry = entry->next) 
  {
    if (first == 0 || entry->next_try < first)
    {
      first = entry->next_try;
    }
  }
  time(&now);

  if (first < now)
  {
    first = now;
  }

  eloop_register_timeout(first - now, 0, radius_client_timer, radius,NULL);
  radiusd_logger(radius->ctx, NULL,
                 RADIUSD_LEVEL_DEBUG, "Next RADIUS client retransmit in"
                                      " %ld seconds", (long int) (first - now));
}



static void radius_client_list_add(struct radius_client_data *radius,
				   struct radius_msg *msg,
				   RadiusType msg_type, u8 *shared_secret,
				   size_t shared_secret_len, const u8 *addr)
{
  struct radius_msg_list *entry, *prev;

  if (eloop_terminated()) 
  {
  	/* No point in adding entries to retransmit queue since event
  	 * loop has already been terminated. */
  	radius_msg_free(msg);
  	free(msg);
  	return;
  }

  entry = malloc(sizeof(*entry));
  if (entry == NULL) 
  {
  	printf("Failed to add RADIUS packet into retransmit list\n");
  	radius_msg_free(msg);
  	free(msg);
  	return;
  }

  memset(entry, 0, sizeof(*entry));
  if (addr)
  {
    memcpy(entry->addr, addr, ETH_ALEN);
  }
  entry->msg = msg;
  entry->msg_type = msg_type;
  entry->shared_secret = shared_secret;
  entry->shared_secret_len = shared_secret_len;
  gettimeofday(&entry->last_attempt, NULL);
  entry->first_try = entry->last_attempt.tv_sec;
  entry->next_try = entry->first_try + RADIUS_CLIENT_FIRST_WAIT;
  entry->attempts = 1;
  entry->next_wait = RADIUS_CLIENT_FIRST_WAIT * 2;
  entry->next = radius->msgs;
  radius->msgs = entry;
  radius_client_update_timeout(radius);

  if (radius->num_msgs >= RADIUS_CLIENT_MAX_ENTRIES) 
  {
  	printf("Removing the oldest un-ACKed RADIUS packet due to "
  	       "retransmit list limits.\n");
  	prev = NULL;
  	while (entry->next) 
    {
  	  prev = entry;
  	  entry = entry->next;
  	}
  	if (prev) 
    {
  	  prev->next = NULL;
  	  radius_client_msg_free(entry);
    }
  } 
  else
  {
    radius->num_msgs++;
  }
}


int radius_client_send(struct radius_client_data *radius,
		       struct radius_msg *msg,
		       RadiusType msg_type, const u8 *addr)
{
  struct  radius_servers *conf = radius->conf;
  u8 *shared_secret;
  size_t shared_secret_len;
  char *name;
  int s, res;

  if (!conf->auth_server && msg_type == RADIUS_AUTH)
  {
    radius_msg_free(msg);
    free(msg);
    return -2;
  }

  if (!conf->acct_server && msg_type == RADIUS_ACCT)
  {
    radius_msg_free(msg);
    free(msg);
    return -2;
  }


  if (msg_type == RADIUS_ACCT) 
  {
  	shared_secret = conf->acct_server->shared_secret;
  	shared_secret_len = conf->acct_server->shared_secret_len;
  	radius_msg_finish_acct(msg, shared_secret, shared_secret_len);
  	name = "accounting";
  	s = radius->acct_sock;
  	conf->acct_server->requests++;
  } 
  else 
  {
  	shared_secret = conf->auth_server->shared_secret;
  	shared_secret_len = conf->auth_server->shared_secret_len;
  	radius_msg_finish(msg, shared_secret, shared_secret_len);
  	name = "authentication";
  	s = radius->auth_sock;
  	conf->auth_server->requests++;
  }
  radiusd_logger(radius->ctx, NULL,
                 RADIUSD_LEVEL_DEBUG, "Sending RADIUS message to %s server",
                 name);
  if (conf->msg_dumps)
  {
    radius_msg_dump(msg);
  }

  if (s >= 0)
  {
    res = send(s, msg->buf, msg->buf_used, 0);
  }
  else 
  {
    res = -1;
  }

  if (res < 0) 
  {
  	radius_client_handle_send_error(radius, s, msg_type);
  }

  radius_client_list_add(radius, msg, msg_type, shared_secret,
                         shared_secret_len, addr);

  return res;
}

static void radius_client_receive(int sock, void *eloop_ctx, void *sock_ctx)
{
  struct radius_client_data *radius =  eloop_ctx;
  struct radius_servers *conf = radius->conf;
  RadiusType msg_type = (RadiusType) sock_ctx;
  int len, i, roundtrip;
  unsigned char buf[3000];
  struct radius_msg *msg;
  struct radius_rx_handler *handlers;
  size_t num_handlers;
  struct radius_msg_list *req, *prev_req;
  struct radius_server *rconf;
  struct timeval now;
  int invalid_authenticator = 0;
  struct sockaddr_in *serv;
  struct sockaddr_in6 *serv6; 
  struct sockaddr_storage from;
  char abuf[128];

  radiusd *radd = radius->ctx;

  int fromLen = sizeof(from);

  if (msg_type == RADIUS_ACCT) 
  {
    handlers = radius->acct_handlers;
    num_handlers = radius->num_acct_handlers;
    rconf = conf->acct_server;
  } 
  else 
  {
    handlers = radius->auth_handlers;
    num_handlers = radius->num_auth_handlers;
    rconf = conf->auth_server;
  }

  len = recvfrom(sock, buf, sizeof(buf), MSG_DONTWAIT, (struct sockaddr *)&from,
  		(socklen_t *)&fromLen);

  if (len < 0) 
  {
  	if (msg_type == RADIUS_ACCT)
    {
      radiusd_logger(radius->ctx, NULL,
                 RADIUSD_LEVEL_DEBUG_VERBOSE, "recv[RADIUS-ACCT]");
    }
  	else
    {
      radiusd_logger(radius->ctx, NULL,
                 RADIUSD_LEVEL_DEBUG_VERBOSE, "recv[RADIUS-AUTH]");
    }
  	return;
  }

  radiusd_logger(radius->ctx, NULL,
                 RADIUSD_LEVEL_DEBUG, "Received %d bytes from RADIUS "
                                      "server", len);
  if (len == sizeof(buf)) 
  {
  	radius_debug_print(RADIUSD_DEBUG_MSGDUMPS, "Possibly too long UDP frame for our buffer - "
  	       "dropping it\n"); 
  	rconf->malformed_responses++;
  	return;
  }

  msg = radius_msg_parse(buf, len);
  if (msg == NULL) 
  {
  	radius_debug_print(RADIUSD_DEBUG_MSGDUMPS, "Parsing incoming RADIUS frame failed\n");
  	rconf->malformed_responses++;
  	return;
  }

  /* Check the source addresses and port of the transmitter.
   * If not from the configured radius server, drop the packet 
   * and update the MIB data. */
  if(from.ss_family == AF_INET) 
  {
  	serv = (struct sockaddr_in *) &from;		
  	if(serv->sin_addr.s_addr != rconf->addr.u.v4.s_addr) 
    {
  	  radiusd_logger(radius->ctx, NULL,
  		  RADIUSD_LEVEL_INFO,
  		  "Received RADIUS packets from unknown address : %s",
  		  inet_ntoa(serv->sin_addr));
          radd->conf->invalid_server_addr++;

  	  goto fail;
  	}
  } 
  else if (from.ss_family == AF_INET6) 
  {
  	serv6 = (struct sockaddr_in6 *) &from;		
  	if (memcmp(serv6->sin6_addr.s6_addr, rconf->addr.u.v6.s6_addr, 
  			sizeof(serv6->sin6_addr.s6_addr)) != 0) 
    {
  	  radiusd_logger(radius->ctx, NULL,
  			RADIUSD_LEVEL_INFO,
  			"Received RADIUS packets from unknown address : %s",
  			 inet_ntop(AF_INET6, serv6->sin6_addr.s6_addr,abuf, sizeof(abuf)));
           radd->conf->invalid_server_addr++;
  	  goto fail;
  	}
  }
  else 
  {
  	radiusd_logger(radius->ctx, NULL,
  		RADIUSD_LEVEL_INFO,
  		"Received RADIUS packets from unknown address : ");
       radd->conf->invalid_server_addr++; 
  	goto fail;
  }
  radiusd_logger(radius->ctx, NULL,
                     RADIUSD_LEVEL_DEBUG, "Received RADIUS message");
  if (conf->msg_dumps)
  {
    radius_msg_dump(msg);
  }

  switch (msg->hdr->code) 
  {
      case RADIUS_CODE_ACCESS_ACCEPT:
              rconf->access_accepts++;
              break;
      case RADIUS_CODE_ACCESS_REJECT:
              rconf->access_rejects++;
              break;
      case RADIUS_CODE_ACCESS_CHALLENGE:
              rconf->access_challenges++;
              break;
      case RADIUS_CODE_ACCOUNTING_RESPONSE:
              rconf->responses++;
              break;
  }

  prev_req = NULL;
  req = radius->msgs;
  while (req) 
  {
  	/* TODO: also match by src addr:port of the packet when using
  	 * alternative RADIUS servers (?) */
  	if (req->msg_type == msg_type &&
  	    req->msg->hdr->identifier == msg->hdr->identifier)
  		break;

  	prev_req = req;
  	req = req->next;
  }

  if (req == NULL) 
  {
  	 radiusd_logger(radius->ctx, NULL,
                             RADIUSD_LEVEL_DEBUG,
  		      "No matching RADIUS request found "
  		      "(type=%d id=%d) - dropping packet",
  		      msg_type, msg->hdr->identifier);
              rconf->packets_dropped++;
  	goto fail;
  }

  gettimeofday(&now, NULL);
  roundtrip = (now.tv_sec - req->last_attempt.tv_sec) * 100 
      	        + (now.tv_usec - req->last_attempt.tv_usec) / 10000;
  radiusd_logger(radius->ctx, NULL,
                     RADIUSD_LEVEL_DEBUG,
                     "Received RADIUS packet matched with a pending "
                     "request, round trip time %d.%02d sec",
                     roundtrip / 100, roundtrip % 100);
  rconf->round_trip_time = roundtrip;


  /* Remove ACKed RADIUS packet from retransmit list */
  if (prev_req)
  {
    prev_req->next = req->next;
  }
  else
  {
     radius->msgs = req->next;
  }
  radius->num_msgs--;

  for (i = 0; i < num_handlers; i++) 
  {
  	RadiusRxResult res;
  	res = handlers[i].handler(msg, req->msg,
  				  req->shared_secret,
  				  req->shared_secret_len,
  				  handlers[i].data);
  	switch (res) 
    {
    	case RADIUS_RX_PROCESSED:
    		radius_msg_free(msg);
    		free(msg);
    		/* continue */
    	case RADIUS_RX_QUEUED:
    		radius_client_msg_free(req);
    		return;
    	case RADIUS_RX_INVALID_AUTHENTICATOR:
                        invalid_authenticator++;
                        /* continue */
    	case RADIUS_RX_UNKNOWN:
    		/* continue with next handler */
    		break;
  	}
  }

  if (invalid_authenticator)
  {
    rconf->bad_authenticators++;
  }
  else
  {
    rconf->unknown_types++;
  }

  radius_debug_print(RADIUSD_DEBUG_MSGDUMPS, "No RADIUS RX handler found (type=%d code=%d id=%d) - dropping " 
         "packet\n", msg_type, msg->hdr->code, msg->hdr->identifier);
  rconf->packets_dropped++;
  radius_client_msg_free(req);

 fail:
	radius_msg_free(msg);
	free(msg);
}


u8 radius_client_get_id(struct radius_client_data *radius)
{
  struct radius_msg_list *entry, *prev, *remove_ptr;
  u8 id = radius->next_radius_identifier++;

  /* remove entries with matching id from retransmit list to avoid
   * using new reply from the RADIUS server with an old request */
  entry = radius->msgs;
  prev = NULL;
  while (entry) 
  {
  	if (entry->msg->hdr->identifier == id) 
    {
      radiusd_logger(radius->ctx, entry->addr,RADIUSD_LEVEL_DEBUG,
                     "Removing pending RADIUS message, "
                     "since its id (%d) is reused", id);
  	  if (prev)
      {
        prev->next = entry->next;
      }

      else
      {
        radius->msgs = entry->next;
      }
  	  remove_ptr = entry;
  	} 
    else 
    {
  	  remove_ptr = NULL;
  	  prev = entry;
  	}
  	entry = entry->next;

  	if (remove_ptr)
    {
      radius_client_msg_free(remove_ptr);
    }
  }
  return id;
}


void radius_client_flush(struct radius_client_data *radius)
{
  struct radius_msg_list *entry, *prev;

  if (!radius)
  {
    return;
  }

  eloop_cancel_timeout(radius_client_timer, radius, NULL);

  entry = radius->msgs;
  radius->msgs = NULL;
  radius->num_msgs = 0;
  while (entry) 
  {
  	prev = entry;
  	entry = entry->next;
  	radius_client_msg_free(prev);
  }
}

void radius_client_update_acct_msgs(struct radius_client_data *radius,
                                    u8 *shared_secret,
                                    size_t shared_secret_len)
{
  struct radius_msg_list *entry;

  if (!radius)
  {
    return;
  }

  for (entry = radius->msgs; entry; entry = entry->next) 
  {
    if (entry->msg_type == RADIUS_ACCT) 
    {
      entry->shared_secret = shared_secret;
      entry->shared_secret_len = shared_secret_len;
      radius_msg_finish_acct(entry->msg, shared_secret,
                             shared_secret_len);
    }
  }
}

static int
radius_change_server(struct radius_client_data *radius,
		     struct radius_server *nserv,
		     struct radius_server *oserv,
		     int sock, int sock6, int auth)
{
  struct sockaddr_in serv;
  struct sockaddr_in6 serv6; 
  struct sockaddr *addr;
  int sel_sock;
  char abuf[128];
  socklen_t addrlen;
  struct radius_msg_list *entry;


#if 1 /* Remove this call because the eventual call to vsyslog can sometimes
       * cause a hang during a hostapd reload with many (20+) VAPs.  Need to
       * figure out the root cause.
       */
  radiusd_logger(radius->ctx, NULL, RADIUSD_LEVEL_INFO,
  	       "%s server %s:%d",
  	       auth ? "Authentication" : "Accounting",
  	       hostapd_ip_txt(&nserv->addr, abuf, sizeof(abuf)),
  	       nserv->port);
#endif

  if (!oserv || nserv->shared_secret_len != oserv->shared_secret_len ||
      memcmp(nserv->shared_secret, oserv->shared_secret,
  	   nserv->shared_secret_len) != 0) 
  {
  	/* Pending RADIUS packets used different shared
  	 * secret, so they would need to be modified. Could
  	 * update all message authenticators and
  	 * User-Passwords, etc. and retry with new server. For
  	 * now, just drop all pending packets. */
  	if(auth)
    {
      radius_client_flush(radius);
    }
  	else 
    {
  	  radius_client_update_acct_msgs(radius, nserv->shared_secret,
                                     nserv->shared_secret_len);
  	}
  } 

  /* Reset retry counters for the new server */
  for (entry = radius->msgs; entry; entry = entry->next) 
  {
    if ((auth && entry->msg_type != RADIUS_AUTH) ||
        (!auth && entry->msg_type != RADIUS_ACCT))
    {
      continue;
    }
    entry->next_try = entry->first_try + RADIUS_CLIENT_FIRST_WAIT;
  	entry->attempts = 0;
  	entry->next_wait = RADIUS_CLIENT_FIRST_WAIT * 2;
  }

  if (radius->msgs) 
  {
    eloop_cancel_timeout(radius_client_timer, radius, NULL);
  	eloop_register_timeout(RADIUS_CLIENT_FIRST_WAIT, 0,
                           radius_client_timer, radius, NULL);
  }

	
  switch (nserv->addr.af) 
  {
    case AF_INET:
            memset(&serv, 0, sizeof(serv));
            serv.sin_family = AF_INET;
            serv.sin_addr.s_addr = nserv->addr.u.v4.s_addr;
            serv.sin_port = htons(nserv->port);
            addr = (struct sockaddr *) &serv;
            addrlen = sizeof(serv);
            sel_sock = sock;
            break;
    case AF_INET6:
            memset(&serv6, 0, sizeof(serv6));
            serv6.sin6_family = AF_INET6;
            memcpy(&serv6.sin6_addr, &nserv->addr.u.v6,
                      sizeof(struct in6_addr));
            serv6.sin6_port = htons(nserv->port);
            addr = (struct sockaddr *) &serv6;
            addrlen = sizeof(serv6);
            sel_sock = sock6;
            break;
    default:
            return -1;
  }

  if (connect(sel_sock, addr, addrlen) < 0) 
  {
    if (auth)
    {
       radiusd_logger(radius->ctx, NULL,
                 RADIUSD_LEVEL_DEBUG_VERBOSE, "connect[radius-auth]");
    }
	else
    {
       radiusd_logger(radius->ctx, NULL,
                 RADIUSD_LEVEL_DEBUG_VERBOSE, "connect[radius-acct]");
    }
	printf("Failed to connect to RADIUS server - will retry later\n");
	/* Do not return error code, since we do not want to abort
	 * hostapd starting. connect() will be retried if send()
	 * fails with ENOTCONN. */
    /*eloop_unregister_read_sock(sel_sock);*/
    if (auth)
    {
      if (sel_sock == radius->auth_serv_sock)
      {
        radius->auth_serv_sock = -1;
      }
      else if (sel_sock == radius->auth_serv_sock6)
      {
        radius->auth_serv_sock6 = -1;
      }
    }
    else
    {
      if (sel_sock == radius->acct_serv_sock)
      {
        radius->acct_serv_sock = -1;
      }
      else if (sel_sock == radius->acct_serv_sock6)
      {
        radius->acct_serv_sock6 = -1;
      }
    }
    close(sel_sock);
	return -1;
  }

  if (auth)
  {
    radius->auth_sock = sel_sock;
  }
  else
  {
    radius->acct_sock = sel_sock;
  }
  if (auth)
  {
    if(nserv->last_auth_start_time.tv_sec == 0) 
    {
      nserv->discontinuity_time = 0;
    }
    gettimeofday(&nserv->last_auth_start_time, NULL);
          
  } 
  else
  {
    if(nserv->last_acct_start_time.tv_sec == 0) 
    {
      nserv->discontinuity_time = 0;
    }
    gettimeofday(&nserv->last_acct_start_time, NULL);
  }
     
  return 0;
}


static void radius_retry_primary_timer(void *eloop_ctx, void *timeout_ctx)
{
  struct radius_client_data *radius = eloop_ctx;
  struct radius_servers *conf = radius->conf;
  struct radius_server *oserv;

  if (radius->auth_sock >= 0 && conf->auth_servers &&
      conf->auth_server != conf->auth_servers) 
  {
  	oserv = conf->auth_server;
  	conf->auth_server = conf->auth_servers;
  	radius_change_server(radius, conf->auth_server, oserv,
  			     radius->auth_serv_sock, 
  			     radius->auth_serv_sock6,1);
  }

  if (radius->acct_sock >= 0 && conf->acct_servers &&
      conf->acct_server != conf->acct_servers) 
  {
  	oserv = conf->acct_server;
  	conf->acct_server = conf->acct_servers;
  	radius_change_server(radius, conf->acct_server, oserv,
  			     radius->acct_serv_sock, 
  			     radius->acct_serv_sock6, 0);
  }

  if (conf->retry_primary_interval)
  {
    eloop_register_timeout(conf->retry_primary_interval, 0,
                           radius_retry_primary_timer, radius, NULL);
  }
}

static int radius_client_init_auth(struct radius_client_data *radius)
{
  struct radius_servers *conf = radius->conf;
  int ok = 0;

  if (radius->auth_serv_sock >= 0)
  {
    eloop_unregister_read_sock(radius->auth_serv_sock);
    if (radius->auth_serv_sock == radius->auth_sock)
    {
      radius->auth_sock = -1;
    }
    close(radius->auth_serv_sock);
    radius->auth_serv_sock = -1;
  }
  radius->auth_serv_sock = socket(PF_INET, SOCK_DGRAM, 0);

  if (radius->auth_serv_sock < 0)
  {
     radiusd_logger(radius->ctx, NULL,
                 RADIUSD_LEVEL_DEBUG_VERBOSE, "socket[PF_INET,SOCK_DGRAM]");
  }
  else
  {
    if( conf->a2w_signature == 0xA22A2AA2 )
    {
      struct sockaddr_in s;

      memset( (char *)&s, 0, sizeof( s ) );
      s.sin_family = AF_INET;
      s.sin_addr.s_addr = conf->my_ip;
      s.sin_port = htons( 33333 );
      if( bind( radius->auth_serv_sock, (struct sockaddr *)&s, sizeof( s ) ) < 0 )
      {
        radiusd_logger(radius->ctx, NULL, RADIUSD_LEVEL_DEBUG, 
                       "bind error %d (%x)", errno, s.sin_addr.s_addr );
        close(radius->auth_serv_sock);
        radius->auth_serv_sock = -1;
        return -1;
      }
    }
    ok++;
  }

  if (radius->auth_serv_sock6 >= 0)
  {
    eloop_unregister_read_sock(radius->auth_serv_sock6);
    if (radius->auth_serv_sock6 == radius->auth_sock)
    {
      radius->auth_sock = -1;
    }
    close(radius->auth_serv_sock6);
    radius->auth_serv_sock6 = -1;
  }
  radius->auth_serv_sock6 = socket(PF_INET6, SOCK_DGRAM, 0);
  if (radius->auth_serv_sock6 < 0)
  {
     radiusd_logger(radius->ctx, NULL,
                 RADIUSD_LEVEL_DEBUG_VERBOSE, "socket[PF_INET6,SOCK_DGRAM]");
  }
  else
  {
    ok++;
  }

  if (ok == 0)
  {
    return -1;
  }

  if (conf->currentServer != 0)
  {
    int count_index = 1;
    conf->auth_server  = NULL;
    while (count_index <= conf->num_auth_servers)
    {
      struct radius_server *radius_server = &conf->auth_servers[count_index-1];
      if (radius_server->priority == conf->currentServer)
      {
        conf->auth_server = radius_server; 
        break;
      }
      count_index++;
    }
    if (conf->auth_server  == NULL)
    {
      conf->auth_server = conf->auth_servers;
      conf->currentServer = 1;
    }
  }

  
  radius_change_server(radius, conf->auth_server, NULL,
                       radius->auth_serv_sock, radius->auth_serv_sock6,1);

 
  if (radius->auth_serv_sock >= 0 &&
      eloop_register_read_sock(radius->auth_serv_sock,
                               radius_client_receive, radius,
                               (void *) RADIUS_AUTH)) 
  {
    printf("Could not register read socket for authentication server\n");
    return -1;
  }

  if (radius->auth_serv_sock6 >= 0 &&
      eloop_register_read_sock(radius->auth_serv_sock6,
                               radius_client_receive, radius,
                               (void *) RADIUS_AUTH)) 
  {
    printf("Could not register read socket for authentication server\n");
    return -1;
  }
#if 0 /* LVL7 BEGIN */
  if(conf->auth_server->last_acct_start_time.tv_sec == 0) 
  {
    conf->auth_server->discontinuity_time = 0;
  }
  gettimeofday(&conf->auth_server->last_acct_start_time, NULL);
#endif /* LVL7 END */

  return 0;
}

static int radius_client_init_acct(struct radius_client_data *radius)
{
  struct radius_servers *conf = radius->conf;
  int ok = 0;

  if (radius->acct_serv_sock >= 0)
  {
    eloop_unregister_read_sock(radius->acct_serv_sock);
    if (radius->acct_serv_sock == radius->acct_sock)
    {
      radius->acct_sock = -1;
    }
    close(radius->acct_serv_sock);
    radius->acct_serv_sock = -1;
  }

  radius->acct_serv_sock = socket(PF_INET, SOCK_DGRAM, 0);

  if (radius->acct_serv_sock < 0)
  {
     radiusd_logger(radius->ctx, NULL,
                 RADIUSD_LEVEL_DEBUG_VERBOSE, "socket[PF_INET,SOCK_DGRAM]");
  }
  else
  {
    ok++;
  }

  if (radius->acct_serv_sock6 >= 0)
  {
    eloop_unregister_read_sock(radius->acct_serv_sock6);
    if (radius->acct_serv_sock6 == radius->acct_sock)
    {
      radius->acct_sock = -1;
    }
    close(radius->acct_serv_sock6);
    radius->acct_serv_sock6 = -1;
  }
  radius->acct_serv_sock6 = socket(PF_INET6, SOCK_DGRAM, 0);
  if (radius->acct_serv_sock6 < 0)
  {
     radiusd_logger(radius->ctx, NULL,
                 RADIUSD_LEVEL_DEBUG_VERBOSE, "socket[PF_INET6,SOCK_DGRAM]");
  }
  else
  {
    ok++;
  }

  if (ok == 0)
  {
    return -1;
  }

  if (conf->currentServer != 0)
  {
    int count_index = 1;
    conf->acct_server  = NULL;
    while (count_index <= conf->num_acct_servers)
    {
      struct radius_server *radius_server = &conf->acct_servers[count_index-1];
      if (radius_server->priority == conf->currentServer)
      {
        conf->acct_server = radius_server; 
        break;
      }
      count_index++;
    }
    if (conf->acct_server  == NULL)
    {
      conf->acct_server = conf->acct_servers;
      conf->currentServer = 1;
    }
  }

 
  radius_change_server(radius, conf->acct_server, NULL,
                       radius->acct_serv_sock, radius->acct_serv_sock6,
                       0);

  if (radius->acct_serv_sock >= 0 &&
      eloop_register_read_sock(radius->acct_serv_sock,
                               radius_client_receive, radius,
                               (void *) RADIUS_ACCT)) 
  {
    printf("Could not register read socket for accounting server\n");
    return -1;
  }

  if (radius->acct_serv_sock6 >= 0 &&
      eloop_register_read_sock(radius->acct_serv_sock6,
                               radius_client_receive, radius,
                               (void *) RADIUS_ACCT)) 
  {
    printf("Could not register read socket for accounting server\n");
    return -1;
  }
#if 0 /* LVL7 BEGIN */
	if(conf->acct_server->last_acct_start_time.tv_sec == 0) 
    {
	  conf->acct_server->discontinuity_time = 0;
	}
	gettimeofday(&conf->acct_server->last_acct_start_time, NULL);
#endif /* LVL7 END */

    return 0;
}



struct radius_client_data *
radius_client_init(void *ctx, struct radius_servers *conf)
{
  struct radius_client_data *radius;

  radius = malloc(sizeof(struct radius_client_data));
  if (radius == NULL)
  {
    return NULL;
  }

  memset(radius, 0, sizeof(struct radius_client_data));
  radius->ctx = ctx;
  radius->conf = conf;
  radius->auth_serv_sock = radius->acct_serv_sock = 
  radius->auth_serv_sock6 = radius->acct_serv_sock6 =
  radius->auth_sock = radius->acct_sock = -1;

#if 0
  if (conf->auth_server &&
      radius_client_init_auth(radius)) 
  {
  	radius_client_deinit(radius);
  	return NULL;
  }

  if (conf->acct_server && 
      radius_client_init_acct(radius)) 
  {
  	radius_client_deinit(radius);
  	return NULL;
  }
#endif

  if ((conf->num_auth_servers != 0) && 
      radius_client_init_auth(radius))
  {
    radius_client_deinit(radius);
    return NULL;
  }

  if ((conf->num_acct_servers != 0) &&
      radius_client_init_acct(radius))
  {
    radius_client_deinit(radius);
    return NULL;
  }
    
  if (conf->retry_primary_interval)
  {
    eloop_register_timeout(conf->retry_primary_interval, 0,
                           radius_retry_primary_timer, radius, NULL);
  }

  return radius;
}


void radius_client_deinit(struct radius_client_data *radius)
{
  if (!radius)
  {
    return;
  }

  if (radius->auth_serv_sock > -1) 
  {
  	eloop_unregister_read_sock(radius->auth_serv_sock);
    close(radius->auth_serv_sock);
    radius->auth_serv_sock = -1;
  }

  if (radius->auth_serv_sock6 > -1) 
  {
  	eloop_unregister_read_sock(radius->auth_serv_sock6);
    close(radius->auth_serv_sock6);
    radius->auth_serv_sock6 = -1;
  }

  if (radius->acct_serv_sock > -1) 
  {
  	eloop_unregister_read_sock(radius->acct_serv_sock);
    close(radius->acct_serv_sock);
    radius->acct_serv_sock = -1;
  }

  if (radius->acct_serv_sock6 > -1) 
  {
  	eloop_unregister_read_sock(radius->acct_serv_sock6);
    close(radius->acct_serv_sock6);
    radius->acct_serv_sock6 = -1;
  }
  radius->auth_sock = -1;
  radius->acct_sock = -1;


  eloop_cancel_timeout(radius_retry_primary_timer, radius, NULL);

  radius_client_flush(radius);
  free(radius->auth_handlers);
  free(radius->acct_handlers);
  free(radius);
}

void radius_client_flush_auth(struct radius_client_data *radius, u8 *addr)
{
  struct radius_msg_list *entry, *prev, *tmp;

  prev = NULL;
  entry = radius->msgs;
  while (entry) 
  {
    if (entry->msg_type == RADIUS_AUTH &&
        memcmp(entry->addr, addr, ETH_ALEN) == 0) 
    {
      radiusd_logger(radius->ctx, addr,RADIUSD_LEVEL_DEBUG,
                                 "Removing pending RADIUS authentication"
                                 " message for removed client");

      if (prev)
              prev->next = entry->next;
      else
              radius->msgs = entry->next;

      tmp = entry;
      entry = entry->next;
      radius_client_msg_free(tmp);
      radius->num_msgs--;
      continue;
    }

    prev = entry;
    entry = entry->next;
  }
}

static int radius_client_dump_auth_server(char *buf, size_t buflen,
                                          struct radius_server *serv,
                                          struct radius_client_data *cli)
{
  int pending = 0;
  struct radius_msg_list *msg;
  char abuf[50];
  int addr_type;
  struct timeval now;

  if (cli) 
  {
    for (msg = cli->msgs; msg; msg = msg->next) 
    {
      if (msg->msg_type == RADIUS_AUTH)
      {
        pending++;
      }
    }
  }

  if (serv->addr.af == AF_INET) 
  {
    addr_type = 1;
  } 
  else if (serv->addr.af == AF_INET6) 
  {
    addr_type = 2;
  } 
  else 
  {
    addr_type = 0;
  }


  gettimeofday(&now, NULL);
  serv->discontinuity_time = (now.tv_sec -  serv->last_auth_start_time.tv_sec)/100;
 

  return snprintf(buf, buflen,
                  "%d %d %s %d %d %u %u %u %u %u %u %u %u %u %u %u %u\n",
                  serv->index,
                  addr_type,
                  hostapd_ip_txt(&serv->addr, abuf, sizeof(abuf)),
                  serv->port,
                  serv->round_trip_time,
                  serv->requests,
                  serv->retransmissions,
                  serv->access_accepts,
                  serv->access_rejects,
                  serv->access_challenges,
                  serv->malformed_responses,
                  serv->bad_authenticators,
                  pending,
                  serv->timeouts,
                  serv->unknown_types,
                  serv->packets_dropped,
                  serv->discontinuity_time);
}

static int radius_client_dump_acct_server(char *buf, size_t buflen,
                                          struct radius_server *serv,
                                          struct radius_client_data *cli)
{
  int pending = 0;
  struct radius_msg_list *msg;
  char abuf[50];
  int addr_type;
  struct timeval now;

  if (cli) 
  {
    for (msg = cli->msgs; msg; msg = msg->next) 
    {
      if (msg->msg_type == RADIUS_ACCT)
      {
        pending++;
      }
    }
   }

  if (serv->addr.af == AF_INET) 
  {
  	addr_type = 1;
  } 
  else if (serv->addr.af == AF_INET6) 
  {
  	addr_type = 2;
  } 
  else 
  {
  	addr_type = 0;
  }


  gettimeofday(&now, NULL);
  serv->discontinuity_time = (now.tv_sec -  serv->last_acct_start_time.tv_sec)/100;

  return snprintf(buf, buflen,"%d %d %s %d %d %u %u %u %u %u %u %u %u %u %u\n",
                  serv->index,
                  addr_type,
                  hostapd_ip_txt(&serv->addr, abuf, sizeof(abuf)),
                  serv->port,
                  serv->round_trip_time,
                  serv->requests,
                  serv->retransmissions,
                  serv->responses,
                  serv->malformed_responses,
                  serv->bad_authenticators,
                  pending,serv->timeouts,
                  serv->unknown_types,
                  serv->packets_dropped,
                  serv->discontinuity_time);
}


static int radius_servers_diff(struct radius_server *nserv,
			       struct radius_server *oserv,
			       int num)
{
  int i;

  for (i = 0; i < num; i++) 
  {
    if (hostapd_ip_diff(&nserv[i].addr, &oserv[i].addr) ||
        nserv[i].port != oserv[i].port ||
        nserv[i].shared_secret_len != oserv[i].shared_secret_len ||
        memcmp(nserv[i].shared_secret, oserv[i].shared_secret,
               nserv[i].shared_secret_len) != 0)
    {
      return 1;
    }
  }
  return 0;
}


static void radius_server_update_acct_counters(
                               struct radius_server *nserv,
                               struct radius_server *oserv,
                               int num)
{
  int i;

  for (i = 0; i < num; i++) 
  {
    if ((nserv != NULL) && (oserv != NULL)) 
    {
      if (hostapd_ip_diff(&nserv[i].addr, &oserv[i].addr) == 0) 
      { 
        nserv[i].round_trip_time = 0;
        nserv[i].requests = oserv[i].requests;
        nserv[i].retransmissions = oserv[i].retransmissions;
        nserv[i].responses = oserv[i].responses;
        nserv[i].malformed_responses = oserv[i].malformed_responses;
        nserv[i].bad_authenticators = oserv[i].bad_authenticators;
        nserv[i].timeouts = oserv[i].timeouts;
        nserv[i].unknown_types = oserv[i].unknown_types;
        nserv[i].packets_dropped = oserv[i].packets_dropped;
  	  }
    }
  }

  return;
}



struct radius_client_data *
radius_client_reconfig(struct radius_client_data *old, void *ctx,
                       struct radius_servers *oldconf,
                       struct radius_servers *newconf)
{
  radius_client_flush(old);
  radius_client_deinit(old);

  radius_server_update_acct_counters(newconf->acct_servers, 
                                     oldconf->acct_servers,
                                     newconf->num_acct_servers);

  return radius_client_init(ctx, newconf);

  if (newconf->retry_primary_interval != oldconf->retry_primary_interval ||
      newconf->num_auth_servers != oldconf->num_auth_servers ||
      newconf->num_acct_servers != oldconf->num_acct_servers ||
      radius_servers_diff(newconf->auth_servers, oldconf->auth_servers,
                          newconf->num_auth_servers) ||
      radius_servers_diff(newconf->acct_servers, oldconf->acct_servers,
                          newconf->num_acct_servers)) 
   {
      radiusd_logger(ctx, NULL,RADIUSD_LEVEL_DEBUG,
                     "Reconfiguring RADIUS client");
      radius_client_deinit(old);
      return radius_client_init(ctx, newconf);
   }

   return old;
}

int radius_client_get_mib(struct radius_client_data *radius, char *buf,
                          size_t buflen)
{
  struct radius_servers *conf = radius->conf;
  int i;
  struct radius_server *serv;
  int count = 0;

  if (conf->acct_servers) 
  {
    for (i = 0; i < conf->num_acct_servers; i++) 
    {
      serv = &conf->acct_servers[i];
      count += radius_client_dump_acct_server(buf + count, buflen - count, 
                                              serv,
                                              serv == conf->acct_server ?radius : NULL);
    }
  }
  return count;
}

int radius_auth_client_get_mib(struct radius_client_data *radius, char *buf,
                          size_t buflen)
{
  struct radius_servers *conf = radius->conf;
  int i;
  struct radius_server *serv;
  int count = 0;

   if (conf->auth_servers) 
   {
     for (i = 0; i < conf->num_auth_servers; i++) 
     {
       serv = &conf->auth_servers[i];
       count += radius_client_dump_auth_server(buf + count, buflen - count, 
                                               serv, 
                                               serv == conf->auth_server ?radius : NULL);
     }
  }
  return count;
}
