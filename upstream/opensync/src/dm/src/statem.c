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

#include <stdarg.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <netinet/in.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <jansson.h>

#include <ev.h>

#include "log.h"
#include "ovsdb.h"
#include "statem.h"

#include "dm.h"

/* state watcher */
state_ext_t wstate;

state_tbl_t state_trans_table [] =
{
    {STATE_INIT         ,    act_check_ovsdb},
    {STATE_CHECK_ID     ,    act_check_id},
    {STATE_INSERT_ID    ,    act_insert_entity},
    {STATE_UPDATE_ID    ,    act_update_entity},
    {STATE_START_MNG    ,    act_init_managers},
    {STATE_IDLE         ,    NULL},
    {STATE_ERROR        ,    NULL},
};


/*
 * Invoked on every state change, main state change handler
 *
 * It calls proper state handler it it is defined. If there is no
 * state change action defined, nothing is invoked
 */
void state_handler(EV_P_ ev_timer * w, int events)
{
    /* make compiler happy */
    (void)events;
    bool result;

    state_ext_t * pwstate = (state_ext_t*)w;

    LOG(DEBUG, "DM SM: state set  state=%d", pwstate->state);

    /* check if there is a mismatch between transition table and state ID */
    if (state_trans_table[pwstate->state].state != pwstate->state)
    {
        STATE_TRANSIT(STATE_ERROR);
        return;
    }

    /* if there is action function for this state define, execute it */
    if (state_trans_table[pwstate->state].action != NULL)
    {
        LOG(DEBUG, "Executing action  state=%d", pwstate->state);
        result = state_trans_table[pwstate->state].action();
        if (!result) {
            LOG(ERROR, "Error executing action state=%d", pwstate->state);
        }
    }
}


/*
 * Initialize state machine
 */
bool init_statem()
{

    /* initialize state timer, the timeout value is irrelevant as this timer
     * is never initialized
     */
    ev_timer_init(&wstate.wtimer , state_handler, 1, 0);

    /* execute initial action */
    STATE_TRANSIT(STATE_INIT);

    return true;
}
