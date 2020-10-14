/* SPDX-License-Identifier: BSD-3-Clause */

#ifndef _REDIR_DEBUG_H_
#define _REDIR_DEBUG_H_

#define D_NORMAL  1
#ifdef BCM_AP_ENTERPRISE

#if 1

#include <libwcf_debug.h>

//#define wlog(...) wc_put_logline( (APP_LOG_REDIR | DEBUG_LEVEL_1), __VA_ARGS__ )
//#define wdbg( level, ... ) wc_put_logline( level, __VA_ARGS__ )

#else

//#//define wlog(...) printf(__VA_ARGS__)
//#define wdbg(level, ...)
//   if (level) printf(__VA_ARGS__)


#endif

#else
//#define wlog(...) printf(__VA_ARGS__)
//#define wdbg(level, ...)
 //  if (level) printf(__VA_ARGS__)

#endif

//#define linetest() wlog("%s %d \n", __func__, __LINE__)

#endif

