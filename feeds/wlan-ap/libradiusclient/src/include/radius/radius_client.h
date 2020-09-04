/* SPDX-License-Identifier: BSD-3-Clause */

#ifndef RADIUS_CLIENT_H
#define RADIUS_CLIENT_H

struct radius_msg;

/********** List of all the Headers *******************/
#include "common.h"

/***********************************************************************/
/* ******************** ENUM TYPEDEFS *********************************  */
typedef enum 
{
  RADIUS_AUTH,
  RADIUS_ACCT
} RadiusType;

typedef enum 
{
  RADIUS_SERVER_PRIMARY=1,
  RADIUS_SERVER_BACKUPONE,
  RADIUS_SERVER_BACKUPTWO,
  RADIUS_SERVER_BACKUPTHREE,
} RadiusServerPriority;

enum 
{ 
  RADIUS_VENDOR_ATTR_LVL7_ACL_DOWN = 120,
  RADIUS_VENDOR_ATTR_LVL7_ACL_UP,
  RADIUS_VENDOR_ATTR_LVL7_DS_POLICY_DOWN,
  RADIUS_VENDOR_ATTR_LVL7_DS_POLICY_UP
};

enum 
{ 
  RADIUS_VENDOR_ATTR_WISPR_BW_MAX_UP = 7,
  RADIUS_VENDOR_ATTR_WISPR_BW_MAX_DOWN
};

/* To identify the TLV fields */
enum
{
  AP_QOS_CLIENT_ADDR = 0,
  AP_QOS_BW_MAX_UP,
  AP_QOS_BW_MAX_DOWN,
  AP_QOS_ACL_TYPE_UP,
  AP_QOS_ACL_TYPE_DOWN,
  AP_QOS_ACL_NAME_UP,
  AP_QOS_ACL_NAME_DOWN,
  AP_QOS_POLICY_UP,
  AP_QOS_POLICY_DOWN,
  AP_QOS_IFACE_NAME

};

enum
{
  APQOS_GLOBAL =50
};

enum
{
  APQOS_GLOBAL_RADIUS_PARAMETERS= 62
};

typedef enum
{
  CLIENT_QOS_ACL_TYPE_NONE = 0,
  CLIENT_QOS_ACL_TYPE_IP,
  CLIENT_QOS_ACL_TYPE_MAC,
  CLIENT_QOS_ACL_TYPE_IPV6
} CLIENT_QOS_ACL_TYPE_t;

typedef enum
{
  CLIENT_QOS_DS_POLICY_TYPE_NONE = 0,
  CLIENT_QOS_DS_POLICY_TYPE_IN,
  CLIENT_QOS_DS_POLICY_TYPE_OUT
} CLIENT_QOS_DS_POLICY_TYPE_t;

typedef enum
{
  CLIENT_QOS_DIR_DOWN = 1,
  CLIENT_QOS_DIR_UP
} CLIENT_QOS_DIR_t;


typedef enum 
{
  RADIUS_RX_PROCESSED,
  RADIUS_RX_QUEUED,
  RADIUS_RX_UNKNOWN,
  RADIUS_RX_INVALID_AUTHENTICATOR
} RadiusRxResult;

/***********************************************************************/
/* ********************** DEFINES ********************************** */
#define BUF_LEN_MAX                   1024
#define RADIUS_VENDOR_ID_LVL7         6132
#define RADIUS_VENDOR_ID_WISPR        14122
#define CLIENT_QOS_DIR_MAX            2
#define CLIENT_QOS_ACL_NAME_MAX       31
#define CLIENT_QOS_DS_POLICY_NAME_MAX 31

#define NET_QOS 25 /* same as the one in sysctl.h */

/***********************************************************************/
/* *************************  Structures ************************** */
struct radius_attr_vendor 
{
  u8 vendor_type;
  u8 vendor_length;
} __attribute__ ((packed));

typedef struct
{
  u8  dot1xValid; /* true value indicates maxRate value retrieved via 802.1x */
  u32 maxRate;    /* bits-per-second */
} client_qos_bw_t;

typedef struct
{
  u8   aclType;
  char aclName[CLIENT_QOS_ACL_NAME_MAX+1];
} client_qos_acl_t;

typedef struct
{
  u8   policyType;
  char policyName[CLIENT_QOS_DS_POLICY_NAME_MAX+1];
} client_qos_ds_t;

typedef struct
{
  client_qos_bw_t   bw[CLIENT_QOS_DIR_MAX];
  client_qos_acl_t  acl[CLIENT_QOS_DIR_MAX];
  client_qos_ds_t   ds[CLIENT_QOS_DIR_MAX];
} client_qos_t;

struct radius_server 
{
  /* MIB prefix for shared variables:
   * @ = radiusAuth or radiusAcc depending on the type of the server */
  struct hostapd_ip_addr addr; /* @ServerAddress */
  int port; /* @ClientServerPortNumber */
  u8 *shared_secret;
  size_t shared_secret_len;

  /* Dynamic (not from configuration file) MIB data */
  int index; /* @ServerIndex */
  RadiusServerPriority priority;  /* server index or priority per bss */
  int round_trip_time; /* @ClientRoundTripTime; in hundredths of a
                        * second */
  u32 requests; /* @Client{Access,}Requests */
  u32 retransmissions; /* @Client{Access,}Retransmissions */
  u32 access_accepts; /* radiusAuthClientAccessAccepts */
  u32 access_rejects; /* radiusAuthClientAccessRejects */
  u32 access_challenges; /* radiusAuthClientAccessChallenges */
  u32 responses; /* radiusAccClientResponses */
  u32 malformed_responses; /* @ClientMalformed{Access,}Responses */
  u32 bad_authenticators; /* @ClientBadAuthenticators */
  u32 timeouts; /* @ClientTimeouts */
  u32 unknown_types; /* @ClientUnknownTypes */
  u32 packets_dropped; /* @ClientPacketsDropped */
  /* @ClientPendingRequests: length of hapd->radius->msgs for matching
   * msg_type */
  struct timeval last_acct_start_time;   /* last time the accounting server is started */
  struct timeval last_auth_start_time;   /* last time the auth server is started */
  int discontinuity_time; /* @ClientCounterDiscontinuity; in hundredths of a
                        * second */
};

struct radius_servers 
{
  /* RADIUS Authentication and Accounting servers in priority order */
  struct radius_server *auth_servers, *auth_server;
  int num_auth_servers;
  struct radius_server *acct_servers, *acct_server;
  int num_acct_servers;
  int currentServer;
  int retry_primary_interval;
  int acct_interim_interval;
  int msg_dumps;
  int auth_failover;
  /* A2W change start */
  unsigned int a2w_signature;
  unsigned int my_ip;
  /* A2W change end */
};

/* RADIUS message retransmit list */
struct radius_msg_list 
{
  u8 addr[ETH_ALEN];/* STA/client address; used to find RADIUS messages
                          * for the same STA. */
  struct radius_msg *msg;
  RadiusType msg_type;
  time_t first_try;
  time_t next_try;
  int attempts;
  int next_wait;
  struct timeval last_attempt;
  u8 *shared_secret;
  size_t shared_secret_len;
  /* TODO: server config with failover to backup server(s) */
  struct radius_msg_list *next;
};

struct radius_rx_handler 
{
  RadiusRxResult (*handler)(struct radius_msg *msg,
  			  struct radius_msg *req,
  			  u8 *shared_secret, size_t shared_secret_len,
  			  void *data);
  void *data;
};

struct radius_client_data 
{
  void *ctx;
  struct radius_servers *conf;
  int auth_serv_sock; /* socket for authentication RADIUS messages */
  int acct_serv_sock; /* socket for accounting RADIUS messages */
  int auth_serv_sock6;
  int acct_serv_sock6;
  int auth_sock; /* currently used socket */
  int acct_sock; /* currently used socket */
  struct radius_rx_handler *auth_handlers;
  size_t num_auth_handlers;
  struct radius_rx_handler *acct_handlers;
  size_t num_acct_handlers;
  struct radius_msg_list *msgs;
  size_t num_msgs;
  u8 next_radius_identifier;
#if 0
  u32 acct_session_id_hi;
  u32 acct_session_id_lo;
  /* TODO: remove separate acct list */
  int acct_retransmit_list_len;
  struct accounting_list *acct_retransmit_list;
#endif
};

/***********************************************************************/
/* ************************ Prototype Definitions **************************/
int radius_client_register(struct radius_client_data *radius, RadiusType msg_type,
                           RadiusRxResult (*handler)
                           (struct radius_msg *msg, struct radius_msg *req,
                            u8 *shared_secret, size_t shared_secret_len,
                            void *data),void *data);
int radius_client_unregister(struct radius_client_data *radius, RadiusType msg_type,
                             RadiusRxResult (*handler)
                             (struct radius_msg *msg, struct radius_msg *req,
                              u8 *shared_secret, size_t shared_secret_len,
                              void *data));
int radius_client_send(struct radius_client_data *radius, struct radius_msg *msg,
                       RadiusType msg_type, const u8 *addr);
u8 radius_client_get_id(struct radius_client_data *radius);

void radius_client_flush(struct radius_client_data *radius);
struct radius_client_data *
radius_client_init(void *ctx, struct radius_servers *conf);
void radius_client_deinit(struct radius_client_data *radius);
struct radius_client_data *
radius_client_reconfig(struct radius_client_data *old, void *ctx,
                       struct  radius_servers *oldconf,
                       struct radius_servers *newconf);

int ap_qos_params_sysctl_send (void * pString, int strLen, int sysctlDirType, int sysctlCmdType);

int qos_tlv_field_add (char ** ppSrcBuf, unsigned short * pOffset, unsigned char fieldType,
                       void * pFieldValue, unsigned short fieldLen);

void client_qos_params_sysctl_send (unsigned char * clientAddr, char * ifaceName,
                                    client_qos_t *pClientQoS);

/***********************************************************************/
#endif /* RADIUS_CLIENT_H */
