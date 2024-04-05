/*
 * Copyright (c) 2017-2018 Qualcomm Technologies, Inc.
 *
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 */

#ifndef __COMDEF_H
#define __COMDEF_H

#define PACK(x)		x __attribute__((__packed__))

#define TRUE 1
#define FALSE 0
#define QSDK_BUILD	1
#define FEATURE_LE_DIAG 1

#include <stdint.h>

/*
 * type defs
 */

typedef uint8_t       boolean;    /* Boolean value type. */
typedef uint32_t      uint32;     /* Unsigned 32 bit value */
typedef uint16_t      uint16;     /* Unsigned 16 bit value */
typedef uint8_t       uint8;      /* Unsigned 8  bit value */
typedef int32_t	      int32;      /* Signed 32 bit value */
typedef int16_t	      int16;      /* Signed 16 bit value */
typedef int8_t	      int8;       /* Signed 8  bit value */
typedef uint64_t      uint64;
typedef uint8_t       byte;
typedef uint32_t      dword;
typedef uint16_t      word;

#if __SIZEOF_POINTER__ == 4
# define FPOS( type, field ) \
	/*lint -e545 */ ( (dword) &(( type *) 0)-> field)  /*lint +e545 */
#else
# define FPOS( type, field ) \
	 /*lint -e545 */ ((uint64) &(( type *) 0)-> field) /*lint +e545 */
#endif

#define FSIZ( type, field ) sizeof( ((type *) 0)->field )

#ifndef MAX
#define  MAX( x, y ) ( ((x) > (y)) ? (x) : (y) )
#endif

#ifndef MIN
#define  MIN( x, y ) ( ((x) < (y)) ? (x) : (y) )
#endif

#endif /* END _COMDEF_H */
