/* SPDX-License-Identifier: BSD-3-Clause */

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <string.h>
#include <time.h>
#include <stdarg.h>
#include <signal.h>
#include <time.h>
#include <syslog.h>

#include "radius_debug.h" 
#include "radiusd.h"

int debug_level = 4;

void radius_debug_level_set(int level)
{
  debug_level = level;
}

void radius_debug_print(int level, const char *fmt, ...)
{
  va_list argp;
  char buff[1000];

  va_start(argp, fmt);
  vsnprintf (buff, 1000, fmt, argp);

  RADIUS_DEBUG(level, buff);

  va_end(argp);
}

void radiusd_logger(const struct radius_data *radd, const u8 *addr,
                    int level, const char *fmt, ...)
{
  char *format, *module_str;
  int maxlen;
  va_list ap;
  int conf_stdout_level;

  maxlen = strlen(fmt) + 100;
  format = malloc(maxlen);
  if (!format)
      return;

  va_start(ap, fmt);

  if (radd && radd->conf) 
  {
    conf_stdout_level = radd->conf->logger_stdout_level;
  } 
  else 
  {
    conf_stdout_level = 0;
  }
  module_str = "RADIUS";

#if 0
    if (radd && radd->conf && addr)
        snprintf(format, maxlen, "%s: STA " MACSTR "%s%s: %s",
             radd->conf->iface, MAC2STR(addr),
             module_str ? " " : "", module_str, fmt);
    else if (radd && radd->conf)
        snprintf(format, maxlen, "%s:%s%s %s",
             radd->conf->iface, module_str ? " " : "",
             module_str, fmt);
     else if (addr)
        snprintf(format, maxlen, "STA " MACSTR "%s%s: %s",
             MAC2STR(addr), module_str ? " " : "",
             module_str, fmt);
    else

        snprintf(format, maxlen, "%s%s%s",
             module_str, module_str ? ": " : "", fmt);
#endif
  snprintf(format, maxlen, "%s:%s",module_str,fmt);

  if (level >= conf_stdout_level) 
  {
    vprintf(format, ap);
    printf("\n");
  }

#if 0
  if (level >= conf_syslog_level) 
  {
    int priority;
    switch (level) 
    {
      case RADIUSD_LEVEL_DEBUG_VERBOSE:
      case RADIUSD_LEVEL_DEBUG:
          priority = LOG_DEBUG;
          break;
      case RADIUSD_LEVEL_INFO:
          priority = LOG_INFO;
          break;
      case RADIUSD_LEVEL_NOTICE:
          priority = LOG_NOTICE;
          break;
      case RADIUSD_LEVEL_WARNING:
          priority = LOG_WARNING;
          break;
      default:
          priority = LOG_INFO;
          break;
    }
    /* vsyslog(priority, format, ap); */
  }
#endif
  free(format);
  va_end(ap);
}

int hostapd_parse_ip_addr(const char *txt, struct hostapd_ip_addr *addr)
{
  if (inet_aton(txt, &addr->u.v4)) 
  {
    addr->af = AF_INET;
    return 0;
  }

  if (inet_pton(AF_INET6, txt, &addr->u.v6) > 0) 
  {
    addr->af = AF_INET6;
    return 0;
  }

  return -1;
}


const char * hostapd_ip_txt(const struct hostapd_ip_addr *addr, char *buf,
                            size_t buflen)
{
  if (buflen == 0 || addr == NULL)
  {
    return NULL;
  }

  if (addr->af == AF_INET) 
  {
    snprintf(buf, buflen, "%s", inet_ntoa(addr->u.v4));
  } 
  else 
  {
    buf[0] = '\0';
  }
  if (addr->af == AF_INET6) 
  {
    if (inet_ntop(AF_INET6, &addr->u.v6, buf, buflen) == NULL)
    {
      buf[0] = '\0';
    }
  }
  return buf;
}

int hostapd_ip_diff(struct hostapd_ip_addr *a, struct hostapd_ip_addr *b)
{
  if (a == NULL && b == NULL)
  {
    return 0;
  }
  if (a == NULL || b == NULL)
  {
    return 1;
  }

  switch (a->af) 
  {
    case AF_INET:
      if (a->u.v4.s_addr != b->u.v4.s_addr)
      {
        return 1;
      }
      break;
    case AF_INET6:
      if (memcmp(&a->u.v6, &b->u.v6, sizeof(a->u.v6)) != 0)
      {
        return 1;
      }
      break;
  }

  return 0;
}





