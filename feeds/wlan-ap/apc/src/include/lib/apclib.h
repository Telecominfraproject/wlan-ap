/* SPDX-License-Identifier: BSD-3-Clause */
#ifndef _APC_LIB_H_
#define _APC_LIB_H_

#include "conf/types.h"
#include "timer.h"

#ifdef HAVE_ALLOCA_H
#include <alloca.h>
#else
#include <stdlib.h>
#endif

/* Ugly structure offset handling macros */

#define OFFSETOF(s, i) ((size_t) &((s *)0)->i)
#define SKIP_BACK(s, i, p) ((s *)((char *)p - OFFSETOF(s, i)))

/* Utility macros */

#define MIN_(a,b) (((a)<(b))?(a):(b))
#define MAX_(a,b) (((a)>(b))?(a):(b))

#ifndef PARSER
#undef MIN
#undef MAX
#define MIN(a,b) MIN_(a,b)
#define MAX(a,b) MAX_(a,b)
#endif

#define ABS(a)   ((a)>=0 ? (a) : -(a))

#ifndef NULL
#define NULL ((void *) 0)
#endif

#define IP_VERSION 4

/* Macros for gcc attributes */

#define NORET __attribute__((noreturn))
#define UNUSED __attribute__((unused))


/* Microsecond time */

typedef s64 btime;

#define S_	*1000000
#define MS_	*1000
#define US_	*1
#define TO_S	/1000000
#define TO_MS	/1000
#define TO_US	/1

#ifndef PARSER
#define S	S_
#define MS	MS_
#define US	US_
#endif

/* Pseudorandom numbers */

u32 random_u32(void);

#endif
