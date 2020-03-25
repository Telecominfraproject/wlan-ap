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
#include <stdlib.h>
#include <time.h>

#include "os.h"
#include "log.h"
#include "os_nif.h"
#include "os_types.h"
#include "os_random.h"


#define MODULE_ID LOG_MODULE_ID_OSA


#define SEED_DEFAULT_IFNAME     "eth0"

/*
 * os_random_seed: Initialize seed using srandom() and srand().
 * Uses MAC address of interface to make seed more unique, since
 * it's possible that seed happens before NTP has sync'd time
 */
void
os_random_seed(const char *ifname)
{
    os_macaddr_t    mac;
    uint32_t        seed;

    if (!ifname)
        ifname = SEED_DEFAULT_IFNAME;

    // Start seed out with current date/time
    seed = (uint32_t)time(NULL);

    // Combine seed and MAC to get as random as possible seed
    // ...Have to cast ifname because os_nif doesn't use const!
    if (os_nif_macaddr((char *)ifname, &mac) == true) {
        seed = seed ^ mac.addr[5] << 8 * (((uint8_t)seed ^ mac.addr[5]) & 0x3);
        seed = seed ^ mac.addr[4] << 8 * (((uint8_t)seed ^ mac.addr[4]) & 0x3);
        seed = seed ^ mac.addr[3] << 8 * (((uint8_t)seed ^ mac.addr[3]) & 0x3);
        seed = seed ^ mac.addr[2] << 8 * (((uint8_t)seed ^ mac.addr[2]) & 0x3);
    }

    LOGD("Random seed value set to 0x%08X", seed);

    // Change seed for random() function
    srandom(seed);

    // Change seed for rand() function
    srand(seed);

    return;
}
