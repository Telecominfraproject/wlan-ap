/* SPDX-License-Identifier: BSD-3-Clause */
#ifndef AP_H
#define AP_H

/********** List of all the Headers *******************/

/***********************************************************************/
/* ******************** ENUM TYPEDEFS *********************************  */
enum sta_client_qos_dir 
{
  STA_CLIENT_QOS_DIR_DOWN = 1,
  STA_CLIENT_QOS_DIR_UP
};

enum sta_client_qos_acl_type 
{
  STA_CLIENT_QOS_ACL_TYPE_NONE = 0, /* ACL not defined via RADIUS */
  STA_CLIENT_QOS_ACL_TYPE_IP   = 1, /* IPV4 ACL (numbered or named) */
  STA_CLIENT_QOS_ACL_TYPE_MAC  = 2, /* MAC ACL */
  STA_CLIENT_QOS_ACL_TYPE_IPV6 = 3  /* IPV6 ACL */
};

enum sta_client_qos_ds_policy_type 
{
  STA_CLIENT_QOS_DS_POLICY_TYPE_NONE = 0,  /* policy not defined via RADIUS */
  STA_CLIENT_QOS_DS_POLICY_TYPE_IN   = 1,  /* inbound policy definition */
  STA_CLIENT_QOS_DS_POLICY_TYPE_OUT  = 2   /* outbound policy definition */
};

/***********************************************************************/
/* ********************** DEFINES ********************************** */
#define STA_CLIENT_QOS_DIR_MAX            2
#define STA_CLIENT_QOS_ACL_NAME_MAX       31
#define STA_CLIENT_QOS_DS_POLICY_NAME_MAX 31

/***********************************************************************/
/* *************************  Structures ************************** */
struct sta_client_qos_bw
{
  u8           dot1xValid;  /* if true, max_rate value was obtained from 802.1X */
  unsigned int max_rate;    /* bits-per-second */
};

struct sta_client_qos_acl
{
  u8    acl_type;
  char  acl_name[STA_CLIENT_QOS_ACL_NAME_MAX+1];
};
struct sta_client_qos_ds
{
  u8    policy_type;
  char  policy_name[STA_CLIENT_QOS_DS_POLICY_NAME_MAX+1];
};

struct sta_info 
{
  struct sta_info *next, *hnext;
  u8 addr[6];
  u16 sta_id; 
  u8 *identity;
  size_t identity_len;
  int radius_identifier;
  struct radius_msg *last_recv_radius;
  u32 acct_session_id_hi;
  u32 acct_session_id_lo;
  time_t acct_session_start;
  int acct_session_started;
  int acct_terminate_cause; /* Acct-Terminate-Cause */
  /* Average TX/RX rate statistics */
  time_t last_txrx_stats_update;
  unsigned long last_tx_bytes;
  unsigned long last_rx_bytes;
  int prev_int_seconds;
  unsigned long prev_int_tx_bytes;
  unsigned long prev_int_rx_bytes;
  int vlan_id;
};

/***********************************************************************/
/* ************************ Prototype Definitions **************************/

/***********************************************************************/

#endif /* AP_H */
