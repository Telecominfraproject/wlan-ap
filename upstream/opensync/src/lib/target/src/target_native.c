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

#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <stdbool.h>
#include <stdio.h>
#include <fcntl.h>

#include <errno.h>

#include "ev.h"

#include "os.h"
#include "log.h"
#include "os_nif.h"
#include "os_types.h"
#include "util.h"
#include "os_util.h"
#include "os_regex.h"

#define MODULE_ID LOG_MODULE_ID_TARGET

/*
 *  TARGET definitions
 */

/*
 * Returns true if device serial name is correctly read
 */
bool target_serial_get(void *buff, size_t buffsz)
{
    /* prepare buffer       */
    memset(buff, 0, buffsz);

    os_macaddr_t mac;
    int n;

    /* get eth0 MAC address */
    if (true == os_nif_macaddr("eth0",  &mac))
    {
        /* convert this to string and set id & serial_number */
        n = snprintf(buff, buffsz, PRI(os_macaddr_plain_t), FMT(os_macaddr_t, mac));
        if (n == OS_MACSTR_PLAIN_SZ) {
            LOG(ERR, "buffer not large enough");
            return false;
        }
        return true;
    }
    // eth0 not found, find en* interface
    char interface[256];
    FILE *f;
    int r;
    *interface = 0;
    f = popen("cd /sys/class/net; ls -d en* | head -1", "r");
    if (!f) return false;
    r = fread(interface, 1, sizeof(interface), f);
    if (r > 0) {
        if (interface[r - 1] == '\n') r--;
        interface[r] = 0;
    }
    pclose(f);
    if (!*interface) return false;
    if (true == os_nif_macaddr(interface,  &mac))
    {
        n = snprintf(buff, buffsz, PRI(os_macaddr_plain_t), FMT(os_macaddr_t, mac));
        if (n == OS_MACSTR_PLAIN_SZ) {
            LOG(ERR, "buffer not large enough");
            return false;
        }
        return true;
    }

    return false;
}

void target_managers_restart(void)
{
}


