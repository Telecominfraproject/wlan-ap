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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <glob.h>
#include <errno.h>

#include "evx.h"
#include "const.h"
#include "log.h"
#include "util.h"
#include "execsh.h"
#include "inet_igmp.h"

struct __inet_igmp
{
    char            igmp_ifname[C_IFNAME_LEN];       /* Interface name */
    bool            igmp;
    int  igmp_age;
    int   igmp_tsize;
};

static char set_igmp_snooping[] = _S(ovs-vsctl set Bridge "$1" mcast_snooping_enable="$2");
static char set_igmp_age[] = _S(ovs-vsctl set Bridge "$1" other_config:mcast-snooping-aging-time="$2");
static char set_igmp_tsize[] = _S(ovs-vsctl set Bridge "$1" other_config:mcast-snooping-table-size="$2");

bool inet_config_igmp_snooping(inet_igmp_t *self)
{
    int status;
    char cmdbuf[16] = {'\0'};

    LOG(INFO, "Enabling IGMP snooping on interface: %s", self->igmp_ifname);

    status = execsh_log(LOG_SEVERITY_INFO, set_igmp_snooping,
                        self->igmp_ifname, self->igmp ? "true" : "false");
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0)
    {
        LOG(ERR, "Error enabling IGMP snooping, command failed for %s",
                self->igmp_ifname);
        return false;
    }

    /* Aging time */
    if (self->igmp)
    {
        snprintf(cmdbuf, sizeof(cmdbuf), "%d", self->igmp_age);
        status = execsh_log(LOG_SEVERITY_INFO, set_igmp_age,
                            self->igmp_ifname, cmdbuf);
        if (!WIFEXITED(status) || WEXITSTATUS(status) != 0)
        {
            LOG(ERR, "Error setting IGMP aging time, command failed for %s",
                    self->igmp_ifname);
            return false;
        }
        memset(cmdbuf, '\0', strlen(cmdbuf));
    }

    /* Table size */
    if (self->igmp)
    {
        snprintf(cmdbuf, sizeof(cmdbuf), "%d", self->igmp_tsize);
        status = execsh_log(LOG_SEVERITY_INFO, set_igmp_tsize,
                            self->igmp_ifname, cmdbuf);
        if (!WIFEXITED(status) || WEXITSTATUS(status) != 0)
        {
            LOG(ERR, "Error setting IGMP table size, command failed for %s",
                    self->igmp_ifname);
            return false;
        }
    }

    return true;
}

/*
 * Start/stop the IGMP snooping service
 */
bool inet_igmp_start(inet_igmp_t *self)
{
    if (self->igmp)
        return inet_config_igmp_snooping(self);
    else
        return false;
}

bool inet_igmp_stop(inet_igmp_t *self)
{
    return inet_config_igmp_snooping(self);
}

/**
 * Initializer
 */
bool inet_igmp_init(inet_igmp_t *self, const char *ifname)
{
    memset(self, 0, sizeof(*self));

    if (strscpy(self->igmp_ifname, ifname, sizeof(self->igmp_ifname)) < 0)
    {
        LOG(ERR, "inet_igmp: %s: Interface name too long.", ifname);
        return false;
    }

    return true;
}

bool inet_igmp_set(inet_igmp_t *self, bool enable, int iage, int itsize)
{
    self->igmp = enable;

    if (self->igmp)
    {
        self->igmp_age = iage;
        self->igmp_tsize = itsize;
    }
    return true;
}

bool inet_igmp_get(inet_igmp_t *self, bool *igmp_enabled)
{
    *igmp_enabled = self->igmp;
    return true;
}

/*
 * ===========================================================================
 *  Public API
 * ===========================================================================
 */

/**
 * Constructor
 */
inet_igmp_t *inet_igmp_new(const char *ifname)
{
    inet_igmp_t *self = malloc(sizeof(inet_igmp_t));
    if (self == NULL) return NULL;

    if (!inet_igmp_init(self, ifname))
    {
        free(self);
        return NULL;
    }

    return self;
}

/**
 * Destructor
 */
bool inet_igmp_del(inet_igmp_t *self)
{
    bool retval = true;

    if (self->igmp)
    {
        retval = inet_igmp_stop(self);
    }

    free(self);

    return retval;
}

