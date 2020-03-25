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

/*
 * evlib.h extended types
 */
#ifndef __EVEXT__H__
#define __EVEXT__H__

typedef struct
{
    struct ev_loop  * loop;     /* Loop object */
    ev_timer        * w;        /* ev_timer object, used for timeouts */
    void *          udata;      /* pointer to user data, general type */

} evrpc_t;

/* pluming macro usually to be used in all rpc callback method
 * It helps to extract all data without a lot of typing
 *
 * INPUT  X; ev_timer object name, usually w
 *        Y: callback user data object, usually data
 *
 * NOTE: if additional flexibility is needed, additional parameters
 *       may be added
 **/
#define RPCCB_UNPACK_DATA(X,Y)  evrpc_t  * erpc;            \
                                struct ev_loop * loop;      \
                                ev_timer * X;               \
                                                            \
                                erpc = (evrpc_t*)Y;         \
                                loop = erpc->loop;          \
                                X = erpc->w                 \



/* pluming macro usually to be used before ovsdb_method_call or
 * ovsdb_method_json functions
 * It helps to pack all data without a lot of typing
 *
 * INPUT  X; ev_timer object name
 *        Y: user data object, could be NULL as well
 *
 **/
#define RPCCB_PACK_DATA(X,Y)    erpc->loop = loop;          \
                                erpc->w = X;                \
                                erpc->udata = Y             \

#endif /*  _EVEXT__H__ */

