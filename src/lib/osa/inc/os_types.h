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

#ifndef OS_TYPES_H_INCLUDED
#define OS_TYPES_H_INCLUDED

#include <inttypes.h>
#include "const.h"

#define PRI_os_macaddr_t        "%02X:%02X:%02X:%02X:%02X:%02X"
#define PRI_os_macaddr_lower_t  "%02x:%02x:%02x:%02x:%02x:%02x"
#define PRI_os_macaddr_plain_t  "%02X%02X%02X%02X%02X%02X"
#define FMT_os_macaddr_t(x)     (x).addr[0], (x).addr[1], (x).addr[2], (x).addr[3], (x).addr[4], (x).addr[5]
#define FMT_os_macaddr_pt(x)    (x)->addr[0], (x)->addr[1], (x)->addr[2], (x)->addr[3], (x)->addr[4], (x)->addr[5]

#define PRI_os_ipaddr_t         "%d.%d.%d.%d"
#define FMT_os_ipaddr_t(x)      (x).addr[0], (x).addr[1], (x).addr[2], (x).addr[3]

/* Plain MAC string takes exactly 13 chars. Because alignment it is better
 * to use 16 bytes instead
 */
#define OS_MACSTR_PLAIN_SZ      (16)

/* Non plain MAC string takes exactly 18 chars.
 */
#define OS_MACSTR_SZ            (18)

/**
 * Avoid using fixed-length arrays in typedefs; this leads to all sorts of problems.
 * Instead, wrap it inside an anonymous struct.
 *
 * The main problem is that arrays are passed to function arguments as references and not
 * as values. If we wrap this into a typedef, the user WILL NOT know it's passing a reference.
 */
typedef struct { uint8_t addr[6]; } os_macaddr_t;
typedef struct { uint8_t addr[4]; } os_ipaddr_t;

#endif /* OS_TYPES_H_INCLUDED */
