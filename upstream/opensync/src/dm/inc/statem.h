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

#include <stdbool.h>
#include <ev.h>

/* States table, all states supported by dm */
typedef enum
{
    STATE_INIT,
    STATE_CHECK_ID,
    STATE_INSERT_ID,
    STATE_UPDATE_ID,
    STATE_START_MNG,
    STATE_IDLE,
    STATE_ERROR,
} state_t;


typedef bool on_state_change_t(void);

/* state extended type, included, ev_timer and
 * state related info
 */
typedef struct
{
    ev_timer wtimer;    /* state timer, never start it, just init it */
    state_t  state;     /* current system state                      */
}state_ext_t;


/* state, state action table    */
typedef struct
{
    state_t     state;
    on_state_change_t * action;
}state_tbl_t;

/* Call this macro in order to change state and trigger next state action */
#define STATE_TRANSIT( newstate )                                           \
                                    wstate.state = newstate;                \
                                    ev_feed_event(EV_DEFAULT_ &wstate.wtimer, 0);


/* This macro is to allow using state machine from all compilation units */
#define STATE_MACHINE_USE           extern  state_ext_t wstate;

/* state handlers so called actions */
bool act_check_ovsdb(void);
bool act_check_id (void);
bool act_insert_entity (void);
bool act_update_entity (void);
bool act_init_managers (void);
