/* SPDX-License-Identifier: BSD-3-Clause */


#ifndef ACCOUNTING_H
#define ACCOUNTING_H

/********** List of all the Headers *******************/
#include "common.h"
#include "ap.h"
#include "radiusd.h"

/***********************************************************************/
/* ******************** ENUM TYPEDEFS *********************************  */

/***********************************************************************/
/* ********************** DEFINES ********************************** */

/***********************************************************************/
/* *************************  Structures ************************** */
/* Accounting message retransmit list */
struct accounting_list 
{
  struct radius_msg *msg;
  time_t first_try;
  time_t next_try;
  int attempts;
  int next_wait;
  struct accounting_list *next;
};

/***********************************************************************/
/* ************************ Prototype Definitions **************************/

void accounting_sta_start(radiusd *radd, struct sta_info *sta);
void accounting_sta_stop(radiusd *radd, struct sta_info *sta);
void accounting_sta_get_id(struct radius_data *radd, struct sta_info *sta);
int accounting_init(radiusd *radd);
void accounting_deinit(radiusd *radd);
/* int accounting_reconfig(radiusd *radd, struct hostapd_config *oldconf); */
int accounting_reconfig(radiusd *radd);

/***********************************************************************/

#endif /* ACCOUNTING_H */
