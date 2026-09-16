#ifndef __DEBUG_H__
#define __DEBUG_H__

#include <stdio.h>
#define DEBUG 0
#if defined(DEBUG) && DEBUG > 0
 #define DEBUG_PRINT(fmt, args...) fprintf(stderr, "$$%d:%s:%d:%s():" fmt, \
    (int)time(NULL), __FILE__, __LINE__, __func__, ##args)
#else
 #define DEBUG_PRINT(fmt, args...) /* Don't do anything in release builds */
#endif

#endif // __DEBUG_H__