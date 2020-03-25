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

#include "log.h"
#include "cev.h"
#include "json_util.h"

#define MODULE_ID LOG_MODULE_ID_CEV

static json_rpc_response_t cev_ovsdb_callback;
static void cev_sleep_cb(struct ev_loop *loop, ev_timer *w, int revents);

/* Suspend a CEV function */
void CRT(cev_wait, cev_wait_t *wait)
{
    CRT_BEGIN();

    if (!wait->cw_pending)
    {
        wait->cw_cev = (cev_t *)__crt;
        CRT_YIELD();
        wait->cw_cev = NULL;
    }

    wait->cw_pending = false;

    CRT_END();
}

/* Resume a waiting CEV task */
void cev_signal(cev_wait_t *wait, bool pend)
{
    wait->cw_pending = pend;

    if (wait->cw_cev != NULL)
    {
        /* Wake-up co-routine if its waiting */
        CEV_RUN(wait->cw_cev);
    }
}

/**
 * Suspend the current co-routine for @p ms milliseconds; returns true if the
 * timer expired, false if was abruptly woken up by cev_wakeup()
 */
bool CRT(cev_sleep, int ms)
{
    cev_t *cev = (cev_t *)__crt;

    CRT_BEGIN();

    if (ms > 0)
    {
        ev_timer_init(&cev->cev_sleep_timer, cev_sleep_cb, (double)ms / 1000.0, 0.0);
        cev->cev_sleep_timer.data = cev;
        ev_timer_start(EV_DEFAULT, &cev->cev_sleep_timer);
    }

    CRT_CALL_RET((cev_wait, &cev->cev_sleep_wobj), false);

    if (ms > 0)
    {
        ev_timer_stop(EV_DEFAULT, &cev->cev_sleep_timer);
    }

    CRT_END();

    return cev->cev_sleep_expired;
}

void cev_sleep_cb(struct ev_loop *loop, ev_timer *w, int revents)
{
    (void)loop;
    (void)revents;

    cev_t *cev = (cev_t *)(w->data);

    cev->cev_sleep_expired = true;

    /* Wake-up sleeping co-routine */
    cev_signal(&cev->cev_sleep_wobj, true);
}

/* Abruptly wake-up co-routine that is sleeping with cev_sleep() */
void cev_wakeup(cev_t *cev)
{
    cev->cev_sleep_expired = false;

    /* Wake-up sleeping co-routine */
    cev_signal(&cev->cev_sleep_wobj, true);
}

/* CEV version of ovsdb_echo_call() */
json_t *CRT(cev_ovsdb_echo, char *txt)
{
    cev_t *cev = (cev_t *)__crt;

    CRT_BEGIN();

    if (!ovsdb_echo_call(cev_ovsdb_callback, __crt, txt, NULL))
    {
        CRT_EXIT(CRT_ERROR);
    }

    CRT_YIELD(NULL);

    if (cev->ovsdb_tran_call.is_error)
    {
        CRT_EXIT(CRT_ERROR);
    }

    CRT_END();

    if (CRT_MY_STATUS() != CRT_OK)
    {
        /* Return NULL on error */
        return NULL;
    }

    return json_incref(cev->ovsdb_tran_call.result);
}

/* CEV version of ovsdb_tran_call() */
json_t *CRT(cev_ovsdb_transact, char *table, ovsdb_tro_t oper, json_t *where, json_t *row)
{
    cev_t *cev = (cev_t *)__crt;

    CRT_BEGIN();

    if (!ovsdb_tran_call(cev_ovsdb_callback, cev, table, oper, where, row))
    {
        CRT_EXIT(CRT_ERROR);
    }

    /* Wait for response */
    CRT_YIELD(NULL);

    if (cev->ovsdb_tran_call.is_error)
    {
        CRT_EXIT(CRT_ERROR);
    }

    CRT_END();

    if (CRT_MY_STATUS() != CRT_OK)
    {
        /* Return NULL on error */
        return NULL;
    }

    return json_incref(cev->ovsdb_tran_call.result);
}

/* CEV version of ovsdb_tranmonitor() */
json_t *CRT(cev_ovsdb_monitor, int monid, char *table, int mon_flags, int argc, char *argv[])
{
    cev_t *cev = (cev_t *)__crt;

    CRT_BEGIN();

    if (!ovsdb_monit_call_argv(cev_ovsdb_callback, cev, monid, table, mon_flags, argc, argv))
    {
        CRT_EXIT(CRT_ERROR);
    }

    /* Wait for repsonse */
    CRT_YIELD(NULL);

    if (cev->ovsdb_tran_call.is_error)
    {
        CRT_EXIT(CRT_ERROR);
    }

    CRT_END();

    if (CRT_MY_STATUS() != CRT_OK)
    {
        /* Return NULL on error */
        return NULL;
    }

    return json_incref(cev->ovsdb_tran_call.result);
}


/* Parse a OVSDB transact response */
bool cev_ovsdb_response_parse(
        json_t *res,
        const char **uuid,
        const char **error,
        int *count)
{
    size_t ii;

    bool retval = false;

    *uuid = NULL;
    *error = NULL;
    *count = 0;

    for (ii = 0; ii < json_array_size(res); ii++)
    {
        json_t *jobj;
        json_t *jval;

        jobj = json_array_get(res, ii);

        if ((jval = json_object_get(jobj, "count")) != NULL)
        {
            if (count != NULL) *count = json_integer_value(jval);
        }
        else if ((jval = json_object_get(jobj, "error")) != NULL)
        {
            if (error != NULL) *error = json_string_value(jval);
        }
        else if ((jval = json_object_get(jobj, "uuid")) != NULL)
        {
            if (uuid != NULL) *uuid = json_string_value(json_array_get(jval, 1));
        }
        else
        {
            char *str = json_dumps(res, 0);
            LOG(ERR, "CEV OVSDB: Unknown TRANSACT response.::response=%s\n", str);
            json_free(str);
            goto error;
        }
    }

    retval = true;

error:

    json_decref(res);
    return retval;
}

void CRT(cev_ovsdb_upsert, char *table, json_t *where, json_t *row)
{
    CRT_BEGIN();

    json_t *res;
    const char *r_uuid;
    const char *r_error;
    int r_count;


    /* Issue an update call; ovsdb_tran_call() decreases the reference count
     * of row; work around it... */
    json_incref(row);
    CRT_CALL_VOID(res = cev_ovsdb_transact, table, OTR_UPDATE, where, row);
    if (CRT_CALL_STATUS() != CRT_OK)
    {
        LOG(ERR, "CEV OVSDB: Transaction update call failed.");
        CRT_EXIT(CRT_ERROR);
    }

    /* Parse the update response */
    if (!cev_ovsdb_response_parse(res, &r_uuid, &r_error, &r_count))
    {
        LOG(ERR, "CEV OVSDB: upsert(update) error parsing response.");
        CRT_EXIT(CRT_ERROR);
    }

    /* Do we have an error field? */
    if (r_error != NULL)
    {
        LOG(ERR, "CEV OVSDB: Update table error.::error=%s", r_error);
        CRT_EXIT(CRT_ERROR);
    }

    /* Update is successfull if the number of rows updated is greater than 1 */
    if (r_count >= 1)
    {
        CRT_EXIT(CRT_OK);
    }

    /* Issue an update call; again, increase the reference count */
    json_incref(row);
    CRT_CALL_VOID(res = cev_ovsdb_transact, table, OTR_INSERT, NULL, row);
    /* Upsert row references are exhausted by now, set it to NULL */
    if (CRT_CALL_STATUS() != CRT_OK)
    {
        LOG(ERR, "CEV OVSDB: Transaction update call failed.");
        CRT_EXIT(CRT_ERROR);
    }

    /* Parse the insert response */
    if (!cev_ovsdb_response_parse(res, &r_uuid, &r_error, &r_count))
    {
        LOG(ERR, "CEV OVSDB: upsert(update) error parsing response.");
        CRT_EXIT(CRT_ERROR);
    }

    /* Do we have an error field? */
    if (r_error != NULL)
    {
        LOG(ERR, "CEV OVSDB: Insert table error.::error=%s", r_error);
        CRT_EXIT(CRT_ERROR);
    }

    if (r_uuid == NULL)
    {
        LOG(ERR, "CEV OVSDB: Insert was successful, but there's no UUID field in the response.");
        CRT_EXIT(CRT_ERROR);
    }

    CRT_END();

    /* Cleanup */
    if (row !=  NULL) json_decref(row);
}


void cev_ovsdb_callback(int id, bool is_error, json_t *js, void *data)
{
    (void)id;

    cev_t *cev = (cev_t *)data;

    cev->ovsdb_tran_call.result = js;
    cev->ovsdb_tran_call.is_error = is_error;

    CEV_RUN(cev);
}

