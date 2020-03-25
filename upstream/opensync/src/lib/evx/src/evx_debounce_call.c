/*
Copyright (c) 2015, Plume Design Inc. All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
   1. Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
   2. Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
   3. Neither the name of the Plume Design Inc. nor the
      names of its contributors may be used to endorse or promote products
      derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL Plume Design Inc. BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include <string.h>
#include <time.h>
#include <evx.h>
#include <evx_debounce_call.h>
#include <ds_dlist.h>
#include <const.h>

#define EVX_DEBOUNCE_SEC 1.0

#define EVX_DEBOUNCE_MIN_MSEC 1000
#define EVX_DEBOUNCE_MAX_MSEC 3000

struct evx_debounce_call {
	struct ds_dlist_node list;
	ev_debounce ev;
	void (*func)(const char *arg);
	char *arg;
};

static ds_dlist_t g_evx_debounce_call_list = DS_DLIST_INIT(struct evx_debounce_call, list);

static bool
evx_debounce_call_same_arg(const char *a, const char *b)
{
	if (a == b && a == NULL)
		return true;
	if ((a && !b) || (!a && b))
		return false;
	return strcmp(a, b) == 0;
}

static struct evx_debounce_call *
evx_debounce_call_lookup(void (*func)(const char *arg), const char *arg)
{
	struct evx_debounce_call *i;
	ds_dlist_foreach(&g_evx_debounce_call_list, i)
		if (i->func == func && evx_debounce_call_same_arg(i->arg, arg))
			return i;
	return NULL;
}

static void
evx_debounce_fn(EV_P_ ev_debounce *ev, int revents)
{
	struct evx_debounce_call *i;
	i = container_of(ev, struct evx_debounce_call, ev);
	ev_debounce_stop(EV_DEFAULT_ &i->ev);
	ds_dlist_remove(&g_evx_debounce_call_list, i);
	i->func(i->arg);
	if (i->arg)
		free(i->arg);
	free(i);
}

void
evx_debounce_call(void (*func)(const char *arg), const char *arg)
{
	struct evx_debounce_call *i;
	if (!(i = evx_debounce_call_lookup(func, arg))) {
		if (!(i = calloc(1, sizeof(*i))))
			return;
		i->func = func;
		if (arg)
			i->arg = strdup(arg);
		ds_dlist_insert_tail(&g_evx_debounce_call_list, i);
		ev_debounce_init(&i->ev, evx_debounce_fn, EVX_DEBOUNCE_SEC);
	}
	ev_debounce_start(EV_DEFAULT_ &i->ev);
}

void
evx_debounce_rn_call(void (*func)(const char *arg), const char *arg)
{
	static unsigned int seed = 1;
	struct evx_debounce_call *i;
	if (!(i = evx_debounce_call_lookup(func, arg))) {
		if (!(i = calloc(1, sizeof(*i))))
			return;
		i->func = func;
		if (arg)
			i->arg = strdup(arg);
		ds_dlist_insert_tail(&g_evx_debounce_call_list, i);

		srand(++seed ^ time(NULL));
		double delay = (double)((rand() % (EVX_DEBOUNCE_MAX_MSEC-EVX_DEBOUNCE_MIN_MSEC))+ EVX_DEBOUNCE_MAX_MSEC);
		ev_debounce_init(&i->ev, evx_debounce_fn, delay/1000);
	}
	ev_debounce_start(EV_DEFAULT_ &i->ev);
}

