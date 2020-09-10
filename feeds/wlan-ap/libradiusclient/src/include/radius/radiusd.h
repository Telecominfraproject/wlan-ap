/* SPDX-License-Identifier: BSD-3-Clause */

#ifndef RADIUSD_H
#define RADIUSD_H

/********** List of all the Headers *******************/
#include "common.h"
#include<stdarg.h>

/***********************************************************************/
/* ******************** ENUM TYPEDEFS *********************************  */
#ifndef ETH_ALEN
#define ETH_ALEN 6
#endif

#define MAC2STR(a) (a)[0], (a)[1], (a)[2], (a)[3], (a)[4], (a)[5]
#define MACSTR "%02x:%02x:%02x:%02x:%02x:%02x"
#define STA_HASH_SIZE 256
#define STA_HASH(sta) (sta[5])

/***********************************************************************/
/* ********************** DEFINES ********************************** */

/***********************************************************************/
/* *************************  Structures ************************** */

struct radius_user 
{
  u8 *user_name;
  u8 *password;
  struct radius_user *next;
};

struct radius_config 
{
  enum 
  {
   RADIUSD_LEVEL_DEBUG_VERBOSE = 0,
   RADIUSD_LEVEL_DEBUG = 1,
   RADIUSD_LEVEL_INFO = 2,
   RADIUSD_LEVEL_NOTICE = 3,
   RADIUSD_LEVEL_WARNING = 4
  } logger_syslog_level, logger_stdout_level;

  unsigned int logger_syslog; /* module bitfield */
  unsigned int logger_stdout; /* module bitfield */
  enum 
  { 
    RADIUSD_DEBUG_NO = 0, 
    RADIUSD_DEBUG_MINIMAL = 1,
    RADIUSD_DEBUG_VERBOSE = 2,
    RADIUSD_DEBUG_MSGDUMPS = 3 
  } debug; /* debug verbosity level */

  int max_num_sta; /* maximum number of STAs in station table */

  /* RADIUS Authentication and Accounting servers in priority order */
  struct hostapd_ip_addr own_ip_addr;
  struct radius_servers *radius;
  char *nas_identifier;
  unsigned int invalid_server_addr; /* radiusAccClientInvalidServerAddresses MIB Data */
};

typedef struct radius_data 
{
  struct radius_config   *conf;
  u8 own_addr[ETH_ALEN];
  int num_sta; /* number of entries in sta_list */
  struct radius_user *user_list; /* user info list head */
  int num_users;
  struct sta_info *sta_list; /* STA info list head */
  struct sta_info *sta_hash[STA_HASH_SIZE];
  struct radius_client_data *radius_client;
  int radius_client_reconfigured;
  u32 acct_session_id_hi, acct_session_id_lo;
} radiusd;

/***********************************************************************/
/* ************************ Prototype Definitions **************************/

/***********************************************************************/
#endif /* RADIUSD_H */
