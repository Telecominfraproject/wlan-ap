/* SPDX-License-Identifier: BSD-3-Clause */

#ifndef RADIUS_DEBUG_H
#define RADIUS_DEBUG_H


extern int debug_level;

/********** List of all the Headers *******************/
#include<stdarg.h>
#include <netinet/in.h>

#include "common.h"
#include "radiusd.h"

/***********************************************************************/
/* ******************** ENUM TYPEDEFS *********************************  */

/***********************************************************************/
/* ********************** DEFINES ********************************** */
#ifdef RADIUS_CLIENT_DEBUG
#define RADIUS_DEBUG(level, args...) \
do { \
    if (debug_level >= (level)) \
        printf(args); \
} while (0)
#else
#define RADIUS_DEBUG(levels, args...)
#define printf(...)
#endif

/***********************************************************************/
/* *************************  Structures ************************** */

/***********************************************************************/
/* ************************ Prototype Definitions **************************/
void radius_debug_level_set(int level);
void radius_debug_print(int level, const char *fmt, ...);

void radiusd_logger(const struct radius_data *radd, const u8 *addr,int level,
                    const char *fmt,...) __attribute__ ((format (printf, 4, 5)));

int hostapd_parse_ip_addr(const char *txt, struct hostapd_ip_addr *addr);
const char * hostapd_ip_txt(const struct hostapd_ip_addr *addr, char *buf,
                            size_t buflen);
int hostapd_ip_diff(struct hostapd_ip_addr *a, struct hostapd_ip_addr *b);

/***********************************************************************/
#endif /* RADIUS_DEBUG_H */
