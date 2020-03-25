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

#include <ev.h>
#include <ds.h>
#include <stdio.h>
#include <unistd.h>

#include "log.h"
#include "pasync.h"

#define MODULE_ID LOG_MODULE_ID_PASYNC

/* single request type */
typedef struct
{                       /* order is important!          */
    ev_io iowatch;      /* fd watcher                   */
    int id;             /* request id                   */
    bool isinit;        /* if watcher is initialized    */
    FILE * stream;      /* FILE object created by popen */
    int fd;             /* file descriptor              */
    pasync_cb * cb;     /* callback to be invoked       */
    char * buff;        /* message buffer               */
    int buff_sz;        /* buffer size                  */
    int msg_sz;         /* number of bytes in message   */

} req_pasync_t;

#define PASYNC_BUFF     1024    /* default buffer size */


/*
 * Initialize single pasync request
 */
static
req_pasync_t * pasync_init_req()
{
    req_pasync_t * req = NULL;

    req = malloc(sizeof(req_pasync_t));
    if (NULL != req)
    {
        memset(req, 0, sizeof(req_pasync_t));
        req->buff = malloc(PASYNC_BUFF);
        memset(req->buff, 0, PASYNC_BUFF);
        req->buff_sz = PASYNC_BUFF;
        req->isinit = false;
    }

    return req;
}


/*
 * Cleanup memory object - de initialize request
 */
static
void pasync_deinit_req(req_pasync_t * req)
{
    if (NULL != req)
    {
        if (req->buff)
            free(req->buff);

        if (req->stream != NULL)
            pclose(req->stream);

        free(req);
    }
}


/*
 * Common callback function (for all pending requests)
 */
static
void pasync_watcher_cb(EV_P_ ev_io *w, int revents)
{

    req_pasync_t * req = NULL;
    int rbytes = 0;
    void * tmpbuff = NULL;

    LOG(DEBUG, "pasync_watcher_cb entered");

    /* get the pointer to the req_async_t object */
    req = (req_pasync_t*)w;

    rbytes = read(req->fd, req->buff + req->msg_sz, req->buff_sz - req->msg_sz);

    LOG(DEBUG, "rbytes: %d", rbytes);

    if (rbytes > 0)
    {
        req->msg_sz += rbytes;

        if (rbytes == req->buff_sz)
        {
            /* increase buffer size for one buffer quantum */
            tmpbuff = realloc(req->buff, req->buff_sz + PASYNC_BUFF);

            if (tmpbuff != NULL)
            {
                req->buff = tmpbuff;
                req->buff_sz += PASYNC_BUFF;
            }
            else
            {
                /* this is to signal error and trigger
                 * stop watching for the io event */
                rbytes = -1;
            }
        }
    }

    /* stop io watcher, call callback and memory cleanup */
    if (rbytes <= 0)
    {
        ev_io_stop(loop, &req->iowatch);

        /* call callback */
        req->cb(req->id, req->buff, req->msg_sz);

        pasync_deinit_req(req);
    }

}


/*
 * Open the process, and get its output in callback
 */
bool pasync_ropen(struct ev_loop *loop,
                  int id,
                  const char * cmd,
                  pasync_cb * cb)

{

    req_pasync_t * req = NULL;

    if ((NULL == loop) || (cb == NULL) || (cmd == NULL))
    {
        goto ropen_end;
    }

    req = pasync_init_req();
    if (NULL == req)
        goto ropen_end;

    /* copy input parameters to request object */
    req->cb = cb;
    req->id = id;

    req->stream = popen(cmd, "r");

    if (NULL == req->stream)
    {
        goto ropen_end;
    }

    /* according to man pages, no reason for error checking here */
    req->fd = fileno(req->stream);

    /* first initialize iowatch object */
    ev_io_init(&req->iowatch, pasync_watcher_cb, req->fd, EV_READ);
    req->isinit = true;

    /* start watching */
    ev_io_start(loop, &req->iowatch);

    return true;

ropen_end:
    LOG(ERR, "ERR ropen");
    if (req != NULL)
    {
        if (req->buff != NULL)
            free(req->buff);

        if (req->stream != NULL)
            pclose(req->stream);

        free(req);
    }
    return false;
}


/*
 * Not yet implemented, always returns false
 */
bool pasync_wopen(struct ev_loop * loop,
                  int id,
                  const char * cmd,
                  pasync_cb * cb)
{
    return false;
}
