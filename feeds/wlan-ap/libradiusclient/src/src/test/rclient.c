/* SPDX-License-Identifier: BSD-3-Clause */
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <netinet/in.h>
#include <string.h>
#include <signal.h>
#include <time.h>
#include <syslog.h>
#include <stdarg.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/sysctl.h>

#include "eloop_libradius.h"
#include "accounting.h"
#include "radius_client.h"
#include "radius_debug.h"
#include "radius.h"
#include "radiusd.h"
#include "ap.h"
#include "sta_info.h"
#include "rclient.h"
#include "eap_defs.h"

int debug_level;
cp_cs_cb_t   *cp_cb_ptr;

extern char *optarg;
extern int optind, optopt;

static void config_free_radius(struct radius_server *servers,int num_servers);
static int hostapd_config_read_radius_addr(struct radius_server **server,
                                int *num_server, const char *val, 
                                int def_port,
                                struct radius_server **curr_serv, 
                                RadiusServerPriority priority);

struct radius_user * radius_users_config_read(const char *fname,int *num_users);

static void handle_term(int sig, void *eloop_ctx, void *signal_ctx)
{
    printf("Signal %d received - terminating\n", sig);
    eloop_terminate();
}


void handle_reload(int sig, void *eloop_ctx, void *signal_ctx)
{
    printf("Signal %d received - reloading configuration\n", sig);
}


static void handle_usr2(int sig, void *eloop_ctx, void *signal_ctx)
{
    printf("Signal %d received - handle user2 \n", sig);
}

static void config_free_radius(struct radius_server *servers,int num_servers)
{
    int i;

    for (i = 0; i < num_servers; i++) {
        free(servers[i].shared_secret);
    }
    free(servers);
}

static int
hostapd_config_read_radius_addr(struct radius_server **server,
                                int *num_server, const char *val, 
                                int def_port,
                                struct radius_server **curr_serv, 
                                RadiusServerPriority priority)
{
    struct radius_server *nserv;
    int ret;
    static int server_index = 1;

    nserv = realloc(*server, (*num_server + 1) * sizeof(*nserv));
    if (nserv == NULL)
        return -1;

    *server = nserv;
    nserv = &nserv[*num_server];
    (*num_server)++;
    (*curr_serv) = nserv;

    memset(nserv, 0, sizeof(*nserv));
    nserv->port = def_port;
    ret =  hostapd_parse_ip_addr(val, &nserv->addr);
    nserv->index = server_index++;
    nserv->priority = priority;

    return ret;
}

struct radius_servers * radius_config_read(const char *fname)
{
    struct radius_servers  *radius;
    FILE *f;
    char buf[256], *pos;
    int line = 0, j;
    int errors = 0;
    int radius_primary=0,radius_backupone=0,radius_backuptwo=0,radius_backupthree=0;
    int acct_primary = 0, acct_backupone=0,acct_backuptwo=0,acct_backupthree=0;

    f = fopen(fname, "r");
    if (f == NULL) {
        printf("Could not open configuration file '%s' for reading.\n",
               fname);
        return NULL;
    }

    radius = malloc(sizeof(*cp_cb_ptr->radiusd.conf->radius));
    if (radius == NULL) {
        printf("Failed to allocate memory for radius data.\n");
        free(radius);
        return NULL;
    }
    memset(radius, 0, sizeof(*cp_cb_ptr->radiusd.conf->radius));

    if (radius == NULL) {
        fclose(f);
        return NULL;
    }

    while (fgets(buf, sizeof(buf), f)) {
        line++;

        if (buf[0] == '#')
            continue;
        pos = buf;
        while (*pos != '\0') {
            if (*pos == '\n') {
                *pos = '\0';
                break;
            }
            pos++;
        }
        if (buf[0] == '\0')
            continue;

        pos = strchr(buf, '=');
        if (pos == NULL) {
            printf("Line %d: invalid line '%s'\n", line, buf);
            errors++;
            continue;
        }
        *pos = '\0';
        pos++;

        if (strcmp(buf, "auth_server_addr_primary") == 0) {
            if (hostapd_config_read_radius_addr(
                    &radius->auth_servers,
                    &radius->num_auth_servers, pos, 1812,
                    &radius->auth_server, RADIUS_SERVER_PRIMARY)) {
                printf("Line %d: invalid IP address '%s'\n",
                       line, pos);
                errors++;
            }
            radius_primary = 1;
        }else if (strcmp(buf, "auth_server_addr_backupone") == 0) {
            if (hostapd_config_read_radius_addr(
                    &radius->auth_servers,
                    &radius->num_auth_servers, pos, 1812,
                    &radius->auth_server, RADIUS_SERVER_BACKUPONE)) {
                printf("Line %d: invalid IP address '%s'\n",
                       line, pos);
                errors++;
            }
            radius_backupone = 1;
        } else if (strcmp(buf, "auth_server_addr_backuptwo") == 0) {
            if (hostapd_config_read_radius_addr(
                    &radius->auth_servers,
                    &radius->num_auth_servers, pos, 1812,
                    &radius->auth_server, RADIUS_SERVER_BACKUPTWO)) {
                printf("Line %d: invalid IP address '%s'\n",
                       line, pos);
                errors++;
            }
	        radius_backuptwo = 1;
        }else if (strcmp(buf, "auth_server_addr_backupthree") == 0) {
            if (hostapd_config_read_radius_addr(
                    &radius->auth_servers,
                    &radius->num_auth_servers, pos, 1812,
                    &radius->auth_server, RADIUS_SERVER_BACKUPTHREE)) {
                printf("Line %d: invalid IP address '%s'\n",
                       line, pos);
                errors++;
            }
	        radius_backupthree = 1;
        } else if (radius->auth_server &&
               strcmp(buf, "auth_server_port") == 0) {
            radius->auth_server->port = atoi(pos);
        } else if (radius_primary &&
               strcmp(buf, "auth_server_shared_secret_primary") == 0) {
            int len = strlen(pos);
            if (len == 0) {
                /* RFC 2865, Ch. 3 */
                printf("Line %d: empty shared secret is not "
                       "allowed.\n", line);
                errors++;
            }
	        for(j = 0; j < radius->num_auth_servers; j++) {
	            if(radius->auth_servers[j].priority == RADIUS_SERVER_PRIMARY) {
                   radius->auth_servers[j].shared_secret = (u8 *) strdup(pos);
                   radius->auth_servers[j].shared_secret_len = len;
                }
            }
        } else if (radius_backupone &&
               strcmp(buf, "auth_server_shared_secret_backupone") == 0) {
            int len = strlen(pos);
            if (len == 0) {
                /* RFC 2865, Ch. 3 */
                printf("Line %d: empty shared secret is not "
                       "allowed.\n", line);
                errors++;
            }
	        for(j = 0; j < radius->num_auth_servers; j++) {
	            if(radius->auth_servers[j].priority == RADIUS_SERVER_BACKUPONE) {
                   radius->auth_servers[j].shared_secret = (u8 *) strdup(pos);
                   radius->auth_servers[j].shared_secret_len = len;
                }
            }
        } else if (radius_backuptwo &&
               strcmp(buf, "auth_server_shared_secret_backuptwo") == 0) {
            int len = strlen(pos);
            if (len == 0) {
                /* RFC 2865, Ch. 3 */
                printf("Line %d: empty shared secret is not "
                       "allowed.\n", line);
                errors++;
            }
	        for(j = 0; j < radius->num_auth_servers; j++) {
	            if(radius->auth_servers[j].priority == RADIUS_SERVER_BACKUPTWO) {
                   radius->auth_servers[j].shared_secret = (u8 *) strdup(pos);
                   radius->auth_servers[j].shared_secret_len = len;
                }
            }
        }else if (radius_backupthree&&
               strcmp(buf, "auth_server_shared_secret_backupthree") == 0) {
            int len = strlen(pos);
            if (len == 0) {
                /* RFC 2865, Ch. 3 */
                printf("Line %d: empty shared secret is not "
                       "allowed.\n", line);
                errors++;
            }
	        for(j = 0; j < radius->num_auth_servers; j++) {
	            if(radius->auth_servers[j].priority == RADIUS_SERVER_BACKUPTHREE) {
                   radius->auth_servers[j].shared_secret = (u8 *) strdup(pos);
                   radius->auth_servers[j].shared_secret_len = len;
                }
            }
        }else if (strcmp(buf, "acct_server_addr_primary") == 0) {
            if (hostapd_config_read_radius_addr(
                    &radius->acct_servers,
                    &radius->num_acct_servers, pos, 1813,
                    &radius->acct_server, RADIUS_SERVER_PRIMARY)) {
                printf("Line %d: invalid IP address '%s'\n",
                       line, pos);
                errors++;
            }
	        acct_primary = 1;
        } else if (strcmp(buf, "acct_server_addr_backupone") == 0) {
            if (hostapd_config_read_radius_addr(
                    &radius->acct_servers,
                    &radius->num_acct_servers, pos, 1813,
                    &radius->acct_server, RADIUS_SERVER_BACKUPONE)) {
                printf("Line %d: invalid IP address '%s'\n",
                       line, pos);
                errors++;
            }
	    acct_backupone = 1;
        } else if (strcmp(buf, "acct_server_addr_backuptwo") == 0) {
            if (hostapd_config_read_radius_addr(
                    &radius->acct_servers,
                    &radius->num_acct_servers, pos, 1813,
                    &radius->acct_server, RADIUS_SERVER_BACKUPTWO)) {
                printf("Line %d: invalid IP address '%s'\n",
                       line, pos);
                errors++;
            }
	    acct_backuptwo = 1;
        }else if (strcmp(buf, "acct_server_addr_backupthree") == 0) {
            if (hostapd_config_read_radius_addr(
                    &radius->acct_servers,
                    &radius->num_acct_servers, pos, 1813,
                    &radius->acct_server, RADIUS_SERVER_BACKUPTHREE)) {
                printf("Line %d: invalid IP address '%s'\n",
                       line, pos);
                errors++;
            }
            acct_backupthree = 1;
        } else if (radius->acct_server &&
               strcmp(buf, "acct_server_port") == 0) {
            radius->acct_server->port = atoi(pos);
        } else if (acct_primary &&
               strcmp(buf, "acct_server_shared_secret_primary") == 0) {
            int len = strlen(pos);
            if (len == 0) {
                /* RFC 2865, Ch. 3 */
                printf("Line %d: empty shared secret is not "
                       "allowed.\n", line);
                errors++;
            }
	    for(j = 0; j < radius->num_acct_servers; j++) {
	      if(radius->acct_servers[j].priority == RADIUS_SERVER_PRIMARY) {
                   radius->acct_servers[j].shared_secret = (u8 *) strdup(pos);
                   radius->acct_servers[j].shared_secret_len = len;
                }
            }
        } else if (acct_backupone &&
               strcmp(buf, "acct_server_shared_secret_backupone") == 0) {
            int len = strlen(pos);
            if (len == 0) {
                /* RFC 2865, Ch. 3 */
                printf("Line %d: empty shared secret is not "
                       "allowed.\n", line);
                errors++;
            }
	    for(j = 0; j < radius->num_acct_servers; j++) {
	      if(radius->acct_servers[j].priority == RADIUS_SERVER_BACKUPONE) {
                   radius->acct_servers[j].shared_secret = (u8 *) strdup(pos);
                   radius->acct_servers[j].shared_secret_len = len;
                }
            }
        } else if (acct_backuptwo &&
               strcmp(buf, "acct_server_shared_secret_backuptwo") == 0) {
            int len = strlen(pos);
            if (len == 0) {
                /* RFC 2865, Ch. 3 */
                printf("Line %d: empty shared secret is not "
                       "allowed.\n", line);
                errors++;
            }
	    for(j = 0; j < radius->num_acct_servers; j++) {
	      if(radius->acct_servers[j].priority == RADIUS_SERVER_BACKUPTWO) {
                   radius->acct_servers[j].shared_secret = (u8 *) strdup(pos);
                   radius->acct_servers[j].shared_secret_len = len;
                }
            }
        } else if (acct_backupthree &&
               strcmp(buf, "acct_server_shared_secret_backupthree") == 0) {
            int len = strlen(pos);
            if (len == 0) {
                /* RFC 2865, Ch. 3 */
                printf("Line %d: empty shared secret is not "
                       "allowed.\n", line);
                errors++;
            }
	    for(j = 0; j < radius->num_acct_servers; j++) {
	      if(radius->acct_servers[j].priority == RADIUS_SERVER_BACKUPTHREE) {
                   radius->acct_servers[j].shared_secret = (u8 *) strdup(pos);
                   radius->acct_servers[j].shared_secret_len = len;
                }
            }
        } else if (strcmp(buf, "radius_retry_primary_interval") == 0) {
            radius->retry_primary_interval = atoi(pos);
        }else if (strcmp(buf, "radius_current") == 0) {
            radius->currentServer = atoi(pos);
        } else {
            printf("Line %d: unknown configuration item '%s'\n",
                   line, buf);
            errors++;
        }
    }

    fclose(f);
    
    if (errors) {
        printf("%d errors found in configuration file '%s'\n",
               errors, fname);
        config_free_radius(radius->auth_servers, radius->num_auth_servers);
        radius->auth_servers = NULL;
        config_free_radius(radius->acct_servers, radius->num_acct_servers);
        radius->acct_servers = NULL;
        free(radius);
        radius = NULL;
    }

    return radius;
}

static void config_free_users(struct radius_user *users,int num_users)
{
    struct radius_user *curr_user;
    while(users != NULL) 
    {
      curr_user = users;
      users = users->next;
      free(curr_user->user_name);
      free(curr_user->password);
      free(curr_user);
    }
}

static int
radius_add_user(struct radius_user **user,
                int *num_users, char *user_name,
                char *user_pwd,
                struct radius_user **curr_usr )
{
    struct radius_user *nusr;
    int ret = 0;

    nusr = malloc(sizeof(struct radius_user));
    if (nusr == NULL)
        return -1;

    memset(nusr, 0, sizeof(*nusr));
    nusr->user_name = (u8 *) strdup(user_name);
    nusr->password = (u8 *) strdup(user_pwd);
    nusr->next = *user;
    (*num_users)++;
    *user = nusr;
    return ret;
}

struct radius_user * radius_users_config_read(const char *fname,int *num_users)
{
    struct radius_user  *user = NULL;
    FILE *f;
    char buf[256], *pos;
    char user_name[128],user_pwd[128];
    int line = 0;
    int errors = 0;

    f = fopen(fname, "r");
    if (f == NULL) {
        printf("Could not open configuration file '%s' for reading.\n",
               fname);
        return NULL;
    }

    while (fgets(buf, sizeof(buf), f)) {
        line++;

        if (buf[0] == '#')
            continue;
        pos = buf;
        while (*pos != '\0') {
            if (*pos == '\n') {
                *pos = '\0';
                break;
            }
            pos++;
        }
        if (buf[0] == '\0')
            continue;

        pos = strchr(buf, '=');
        if (pos == NULL) {
            printf("Line %d: invalid line '%s'\n", line, buf);
            errors++;
            continue;
        }
        *pos = '\0';
        pos++;

        if (strcmp(buf, "user_name") == 0) {
            memset(user_name,0,128);
            strcpy(user_name,pos); 
        }else if (strcmp(buf, "user_password") == 0) {
            memset(user_pwd,0,128);
            strcpy(user_pwd,pos); 
            if (radius_add_user(&user,num_users,user_name,user_pwd,&user)) {
                printf("User add to user list failed \n");
                errors++;
            }
        } else {
            printf("Line %d: unknown configuration item '%s'\n",
                   line, buf);
            errors++;
        }
    }

    fclose(f);
    
    if (errors) {
        printf("%d errors found in configuration file '%s'\n",
               errors, fname);
        config_free_users(user,*num_users);
        free(user);
        user = NULL;
    }

    return user;
}

static int radius_md5_auth(cp_cs_cb_t *cp_cp_ptr, u8 *addr,
                           u8 *user, u8* password)
{
    struct radius_msg *msg;
    char buf[128];
    u8 radius_id;
    struct sta_info *sta;
    int ipaddr;
    struct eap_hdr eap;
    int len = 0 ;

    sta = auth_get_sta(&cp_cb_ptr->radiusd,addr);

    radius_id = radius_client_get_id(cp_cb_ptr->radiusd.radius_client);
    sta->radius_identifier = radius_id;
    msg = radius_msg_new(RADIUS_CODE_ACCESS_REQUEST, radius_id);
    if (msg == NULL)
        return -1;

    radius_msg_make_authenticator(msg, addr, ETH_ALEN);

    snprintf(buf, sizeof(buf), "%s", user);
    if (!radius_msg_add_attr(msg, RADIUS_ATTR_USER_NAME, (u8 *) buf,
                 strlen(buf))) {
        radius_debug_print(RADIUSD_DEBUG_MSGDUMPS, "Could not add User-Name\n");
        goto fail;
    }

    snprintf(buf, sizeof(buf), RADIUS_802_1X_ADDR_FORMAT,
         MAC2STR(addr));
    if (!radius_msg_add_attr(msg, RADIUS_ATTR_CALLED_STATION_ID,
                 (u8 *) buf, strlen(buf))) {
        radius_debug_print(RADIUSD_DEBUG_MSGDUMPS, "Could not add Called-Station-Id\n");
        goto fail;
    }

    snprintf(buf, sizeof(buf), RADIUS_802_1X_ADDR_FORMAT,
         MAC2STR(addr));
    if (!radius_msg_add_attr(msg, RADIUS_ATTR_CALLING_STATION_ID,
                 (u8 *) buf, strlen(buf))) {
        radius_debug_print(RADIUSD_DEBUG_MSGDUMPS, "Could not add Calling-Station-Id\n");
        goto fail;
    }

    ipaddr = inet_addr("10.27.128.45");
    if (!radius_msg_add_attr(msg, RADIUS_ATTR_NAS_IP_ADDRESS,
                       (u8 *)&ipaddr, 4 )) {
        radius_debug_print(RADIUSD_DEBUG_MSGDUMPS, "Could not add NAS-IP_address\n");
        goto fail;
    }
    if (!radius_msg_add_attr_int32(msg, RADIUS_ATTR_NAS_PORT_TYPE,
                       RADIUS_NAS_PORT_TYPE_IEEE_802_11)) {
        radius_debug_print(RADIUSD_DEBUG_MSGDUMPS, "Could not add NAS-Port-Type\n");
        goto fail;
    }

    snprintf(buf, sizeof(buf), "CONNECT 11Mbps 802.11b");
    if (!radius_msg_add_attr(msg, RADIUS_ATTR_CONNECT_INFO,
                 (u8 *) buf, strlen(buf))) {
        radius_debug_print(RADIUSD_DEBUG_MSGDUMPS, "Could not add Connect-Info\n");
        goto fail;
    }
    
    memset(&eap, 0, sizeof(eap));
    memset(buf, 0, sizeof(buf));
    eap.code = EAP_CODE_RESPONSE;
    eap.identifier = 0;
    len = sizeof(eap) + 1 + strlen(user);
    eap.length = htons(len);
    memcpy(buf,&eap,sizeof(eap));
    *(buf+sizeof(eap)) = EAP_TYPE_IDENTITY;
    memcpy((buf+(sizeof(eap)+sizeof(char))),user,strlen(user));

    if (!radius_msg_add_eap(msg, (u8 *) buf,len)) {
             radius_debug_print(RADIUSD_DEBUG_MSGDUMPS,"Failed to add EAP-Message attribute");
        goto fail;
    }


    radius_client_send(cp_cb_ptr->radiusd.radius_client, msg, RADIUS_AUTH, addr);
    return 0;

 fail:
    radius_msg_free(msg);
    free(msg);
    return -1;
}

static int radius_md5_challenge_resp(cp_cs_cb_t *cp_cb_ptr, u8 *addr,
                                     u8 *user, u8* password,
                                     u8 id, u8 *challenge, 
                                     u8 challenge_len, u8 *state,
                                     int state_len)
{
    struct radius_msg *msg;
    char buf[128];
    u8 radius_id;
    struct sta_info *sta;
    struct eap_hdr eap;
    int len = 0 ;
    u8 *pos;
    int ipaddr;

    sta = auth_get_sta(&cp_cb_ptr->radiusd,addr);

    radius_id = radius_client_get_id(cp_cb_ptr->radiusd.radius_client);
    sta->radius_identifier = radius_id;
    msg = radius_msg_new(RADIUS_CODE_ACCESS_REQUEST, radius_id);

    if (msg == NULL)
        return -1;

    radius_msg_make_authenticator(msg, addr, ETH_ALEN);

    snprintf(buf, sizeof(buf), "%s", user);
    if (!radius_msg_add_attr(msg, RADIUS_ATTR_USER_NAME, (u8 *) buf,
                 strlen(buf))) {
        radius_debug_print(RADIUSD_DEBUG_MSGDUMPS, "Could not add User-Name\n");
        goto fail;
    }

    ipaddr = inet_addr("10.27.128.45");
    if (!radius_msg_add_attr(msg, RADIUS_ATTR_NAS_IP_ADDRESS,
                       (u8 *)&ipaddr, 4 )) {
        radius_debug_print(RADIUSD_DEBUG_MSGDUMPS, "Could not add NAS-IP_address\n");
        goto fail;
    }
    
    if (!radius_msg_add_attr_int32(msg, RADIUS_ATTR_NAS_PORT_TYPE,
                       RADIUS_NAS_PORT_TYPE_IEEE_802_11)) {
        radius_debug_print(RADIUSD_DEBUG_MSGDUMPS, "Could not add NAS-Port-Type\n");
        goto fail;
    }
    if (!radius_msg_add_attr(msg, RADIUS_ATTR_STATE,
                 (u8 *) state, state_len)) {
        radius_debug_print(RADIUSD_DEBUG_MSGDUMPS, "Could not add stateInfo\n");
        goto fail;
    }

    memset(&eap, 0, sizeof(eap));
    memset(buf, 0, sizeof(buf));
    eap.code = EAP_CODE_RESPONSE;
    eap.identifier = id;
    len = sizeof(eap) + 1 + 1 + challenge_len;
    eap.length = htons(len);
    memcpy(buf,&eap,sizeof(eap));
    *(buf+sizeof(eap)) = EAP_TYPE_MD5;
    *(buf+(sizeof(eap)+sizeof(char))) = challenge_len;
    pos = (buf+(sizeof(eap)+ 2 * sizeof(char)));
    chap_md5(id, password,strlen(password), challenge, challenge_len, pos);

    if (!radius_msg_add_eap(msg, (u8 *) buf,len)) {
             radius_debug_print(RADIUSD_DEBUG_MSGDUMPS,"Failed to add EAP-Message attribute");
        goto fail;
    }

    radius_client_send(cp_cb_ptr->radiusd.radius_client, msg, RADIUS_AUTH, addr);
    return 0;

 fail:
    radius_msg_free(msg);
    free(msg);
    return -1;
}

/* Process the RADIUS frames from Authentication Server */
static RadiusRxResult
radius_receive_process( struct radius_msg *msg, struct radius_msg *req,
            u8 *shared_secret, size_t shared_secret_len,
            void *data)
{
    struct radius_data *radd = data;
    struct sta_info *sta;
    u32 session_timeout = 0, termination_action = 0;
    int session_timeout_set;
    u8  buffer[64];
    u8  eap_type;
    const struct eap_hdr *hdr;
    const u8 *pos, *challenge;
    size_t challenge_len;
    u8 *eap = NULL;
    size_t eap_len;
    int state_len =0;

    if (radius_msg_verify(msg, shared_secret, shared_secret_len, req))
    {
      radius_debug_print(RADIUSD_DEBUG_MINIMAL,"Incoming RADIUS packet did"
  	         "not have correct Authenticator - dropped\n");
      return RADIUS_RX_UNKNOWN;
    }

    sta = ap_get_sta_radius_identifier(radd, msg->hdr->identifier);
    if (sta == NULL) {
        radius_debug_print(RADIUSD_DEBUG_MINIMAL, "RADIUS 802.1X: Could not "
                  "find matching station for this RADIUS message\n");
        return RADIUS_RX_UNKNOWN;
    }

    if (msg->hdr->code != RADIUS_CODE_ACCESS_ACCEPT &&
        msg->hdr->code != RADIUS_CODE_ACCESS_REJECT &&
        msg->hdr->code != RADIUS_CODE_ACCESS_CHALLENGE) {
        radiusd_logger(radd, sta->addr,RADIUSD_LEVEL_DEBUG,
                   "Unknown RADIUS message "
                   "code (%d)", msg->hdr->code);
        return RADIUS_RX_UNKNOWN;
    }

    sta->radius_identifier = -1;

    radius_debug_print(RADIUSD_DEBUG_MINIMAL,
              "RADIUS packet matching with station " MACSTR "\n",
              MAC2STR(sta->addr));

    if (sta->last_recv_radius) {
        radius_msg_free(sta->last_recv_radius);
        free(sta->last_recv_radius);
    }

    sta->last_recv_radius = msg;

    session_timeout_set =
        !radius_msg_get_attr_int32(msg, RADIUS_ATTR_SESSION_TIMEOUT,
                       &session_timeout);
    if (radius_msg_get_attr_int32(msg, RADIUS_ATTR_TERMINATION_ACTION,
                      &termination_action))
        termination_action = RADIUS_TERMINATION_ACTION_DEFAULT;

    switch (msg->hdr->code) {
    case RADIUS_CODE_ACCESS_ACCEPT:
        memset(buffer,0,sizeof(buffer));
        if (radius_msg_get_attr(msg,RADIUS_ATTR_USER_NAME,buffer,sizeof(buffer)))
        {
            int buffer_len = strlen((char *) buffer);
            if (sta->identity)
                free(sta->identity);
            sta->identity = (u8 *) malloc(buffer_len);
            if (sta->identity)
            {
                memcpy(sta->identity, buffer, buffer_len);
                sta->identity_len = buffer_len;
            }
        }
        accounting_sta_start(&cp_cb_ptr->radiusd,sta);
        accounting_sta_stop(&cp_cb_ptr->radiusd,sta);
        break;
    case RADIUS_CODE_ACCESS_REJECT:
        radiusd_logger(radd, sta->addr,
                   RADIUSD_LEVEL_INFO, "authentication server "
                   "rejected MD5 authentiation");
        break;
    case RADIUS_CODE_ACCESS_CHALLENGE:
        radiusd_logger(radd, sta->addr,
                   RADIUSD_LEVEL_INFO, "authentication server "
                   "sent Access Challenge message for MD5 authentiation");
        state_len = radius_msg_get_attr(msg,RADIUS_ATTR_STATE,buffer,sizeof(buffer));
        if (state_len > 0 )
        {
           radiusd_logger(radd,sta->addr,RADIUSD_LEVEL_INFO, 
           "State attribute: in EAP-Message in RADIUS packet");
           for(eap_len = 0; eap_len<state_len;eap_len++)
             radiusd_logger(radd,sta->addr,RADIUSD_LEVEL_INFO, 
                   "%x",buffer[eap_len]);

        }
        eap = radius_msg_get_eap(msg, &eap_len);
        if (eap == NULL) {
           radiusd_logger(radd,sta->addr,RADIUSD_LEVEL_INFO, 
           "No EAP-Message in RADIUS packet ");
        }
        else /* process eap message */
        {

           hdr = (struct eap_hdr *)eap;
           pos = (const u8 *) (hdr + 1);
           eap_type = *pos;
           pos++;
           challenge_len = *pos++;
           challenge = pos;

           if(eap_type == EAP_TYPE_MD5)
              radius_md5_challenge_resp(cp_cb_ptr, sta->addr,
                                         "wireless", "welcome123",
                                          /* "testqos", "testqos",*/
                                         hdr->identifier, challenge,
                                         challenge_len,buffer,state_len);
        }

        break;
    }
    return RADIUS_RX_QUEUED;
}

static void usage(void)
{
      fprintf(stderr,
              "User space Radius Client test utility \n "
              "\n"
              "usage: radiusclientd [-hSdupMa] ...\n"
              "\n"
              "options:\n"
              "   -h   show this usage\n"
              "   -d   set debug level[0-4,more traces at lower debug level] \n"
              "   -S   radius authentication and accounting servers configuration file \n"
              "   -u   user name for MD5 authetication \n"
              "   -p   user password for MD5 authetication \n"
              "   -M   user name, password file for MD5 authentication of multiple users \n");
          exit(1);
}

int main(int argc, char *argv[])
{
    int ret = 1;
    int c;
    int errflg=0;
    u8 *server_file = NULL;
    u8 *users_file = NULL;
    u8 *user_name = NULL;
    u8 *user_pwd = NULL;
    char buf[2000];
    struct radius_user *user_ptr;
    u8 clnt_addr[ETH_ALEN] = {0x22,0x23,0x24,0x25,0x26,0x27};
    u8 ap_addr[ETH_ALEN] = {0x32,0x33,0x34,0x35,0x36,0x37};
    u8 ap_ip_addr[] = "10.132.27.55";


    for (;;) {
        c = getopt(argc, argv, "S:d:u:p::M:");
        if (c < 0)
            break;
        switch(c) {
           case 'h':
               usage();
               break;
            case 'S':
               server_file = optarg;
               if(server_file ==NULL) 
                 errflg++;
               break;
            case 'd':
               debug_level = atoi(optarg);
               break;
            case 'u':
               user_name = optarg;
               break;
            case 'p':
               user_pwd = optarg;
               break;
            case 'M':
               users_file = optarg;
               break;
            case ':':       /* -f or -o without operand */
               fprintf(stderr,"Option -%c requires an operand\n", optopt);
                errflg++;
               break;
             case '?':
               fprintf(stderr,"Unrecognized option: -%c\n", optopt);
               errflg++;
           }
         }

      if (errflg) {
          usage();
      }

      /* user name and password is required to test radius client, in case of 
       * multi user they are provided through the file 
       */
      if((user_name == NULL || user_pwd == NULL) &&
         (users_file == NULL)) 
      {
         printf("\n\nuser name and/or password are NULL \n\n");
         usage();
      }
      if(debug_level < 0 || debug_level > 4) 
      {
         printf("\n\nDebug level not in range \n\n");
         usage();
      }

    eloop_init(&buf);

    eloop_register_signal(SIGHUP, handle_reload, NULL);
    eloop_register_signal(SIGINT, handle_term, NULL);
    eloop_register_signal(SIGTERM, handle_term, NULL);
    eloop_register_signal(SIGUSR2, handle_usr2, NULL);


    cp_cb_ptr = (cp_cs_cb_t *)malloc(sizeof(cp_cs_cb_t));

    if (cp_cb_ptr == NULL)
    {
      radius_debug_print(RADIUSD_DEBUG_MSGDUMPS,"Unable to allocate memory");
      free(cp_cb_ptr);
      return ret;
    }
    /* Initialize the memory contents to zero */
    memset(cp_cb_ptr,0,sizeof(cp_cs_cb_t));

    cp_cb_ptr->radiusd.conf = ( struct radius_config *) malloc(sizeof( struct radius_config )); 

    if (cp_cb_ptr->radiusd.conf  == NULL)
    {
      radius_debug_print(RADIUSD_DEBUG_MSGDUMPS,"Unable to allocate memory");
      free(cp_cb_ptr);
      return ret;
    }

    /* Initialize debug level */
    cp_cb_ptr->radiusd.conf->logger_syslog_level = debug_level;
    cp_cb_ptr->radiusd.conf->logger_stdout_level = debug_level;
    cp_cb_ptr->radiusd.conf->logger_syslog = (unsigned int) -1;
    cp_cb_ptr->radiusd.conf->logger_stdout = (unsigned int) -1;


    memcpy(&cp_cb_ptr->radiusd.own_addr,ap_addr,ETH_ALEN);
    ret =  hostapd_parse_ip_addr(ap_ip_addr, &cp_cb_ptr->radiusd.conf->own_ip_addr);

    cp_cb_ptr->radiusd.conf->radius =  radius_config_read(server_file);

    if(cp_cb_ptr->radiusd.conf->radius == NULL) 
    {
      radius_debug_print(RADIUSD_DEBUG_MSGDUMPS, "RADIUS client initialization failed.\n");
      free(cp_cb_ptr->radiusd.conf);
      free(cp_cb_ptr);
      return ret;
    }

    cp_cb_ptr->radiusd.radius_client = radius_client_init(&cp_cb_ptr->radiusd, cp_cb_ptr->radiusd.conf->radius);

    if (cp_cb_ptr->radiusd.radius_client == NULL) 
    {
      radius_debug_print(RADIUSD_DEBUG_MSGDUMPS, "RADIUS client initialization failed.\n");
      return ret;
    }

    if( accounting_init(&cp_cb_ptr->radiusd) < 0)
    {
      radius_debug_print(RADIUSD_DEBUG_MSGDUMPS,"RADIUS accounting initialization failed.\n");
      return ret;
    }

    ap_sta_init(&cp_cb_ptr->radiusd);

    radius_client_register(cp_cb_ptr->radiusd.radius_client, RADIUS_AUTH, radius_receive_process, &cp_cb_ptr->radiusd);

    if(users_file != NULL) 
    {
      cp_cb_ptr->radiusd.user_list =  radius_users_config_read(users_file,&cp_cb_ptr->user_count);
    }
    else
    {
       if (radius_add_user(&cp_cb_ptr->radiusd.user_list,
                           &cp_cb_ptr->user_count,
                           user_name,user_pwd,&cp_cb_ptr->radiusd.user_list)) 
       {
         printf("User add to user list failed \n");
       }
    }

    if(cp_cb_ptr->radiusd.user_list == NULL) 
    {
      radius_debug_print(RADIUSD_DEBUG_MINIMAL,"Invoked radiusclientd with empty MD5 user list, RADIUS client exiting!!.\n");
      return ret;
    }        

    user_ptr = cp_cb_ptr->radiusd.user_list;

    while(user_ptr != NULL) 
    {
      radius_md5_auth(cp_cb_ptr,clnt_addr,
                      user_ptr->user_name,
                      user_ptr->password);
      user_ptr = user_ptr->next;

      if (clnt_addr[5] == 0xFF) /* Handles last byte overflow only */
      {
        clnt_addr[4]++;
        clnt_addr[5] = 0;
      }
      else
      {
        clnt_addr[5]++;
      }
    }
    
    eloop_run();

    radiusd_free_stas(&cp_cb_ptr->radiusd);
    config_free_users(cp_cb_ptr->radiusd.user_list,cp_cb_ptr->user_count);
    ap_sta_deinit(&cp_cb_ptr->radiusd);
    radius_client_deinit(cp_cb_ptr->radiusd.radius_client);
    accounting_deinit(&cp_cb_ptr->radiusd);
    eloop_destroy();

    return ret;
}
