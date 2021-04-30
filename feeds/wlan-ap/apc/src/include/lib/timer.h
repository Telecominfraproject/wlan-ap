/* SPDX-License-Identifier: BSD-3-Clause */
#ifndef _APC_TIMER_H_
#define _APC_TIMER_H_

#include <time.h>

#include "lib/resource.h"

typedef struct _timer
{
    void (*hook)(struct _timer *);
    void *data;
    unsigned randomize; /* Amount of randomization */
    unsigned recurrent; /* Timer recurrence */
    time_t expires;     /* 0=inactive */
} timer;


timer * tm_new( void );
void tm_start( timer *, unsigned after );
void tm_stop( timer * );

static inline timer * tm_new_set( void (*hook)(struct _timer *), void *data, unsigned rand, unsigned rec )
{
    timer *t = tm_new( );
    t->hook = hook;
    t->data = data;
    t->randomize = rand;
    t->recurrent = rec;
    return t;
}

static inline void tm_free(timer *t)
{
    free(t);
}

#endif
