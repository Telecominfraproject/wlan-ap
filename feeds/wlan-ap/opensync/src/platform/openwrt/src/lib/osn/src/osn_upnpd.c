/* SPDX-License-Identifier: BSD-3-Clause */

#include <stdlib.h>
#include "osn_upnp.h"

static int dummy_osn_upnp;

osn_upnp_t *osn_upnp_new(const char *ifname)
{
    return (osn_upnp_t *)&dummy_osn_upnp;
}

bool osn_upnp_del(osn_upnp_t *self)
{
    return true;
}

bool osn_upnp_set(osn_upnp_t *self, enum osn_upnp_mode mode)
{
    return true;
}

bool osn_upnp_get(osn_upnp_t *self, enum osn_upnp_mode *mode)
{
    *mode = 0;

    return true;
}

bool osn_upnp_start(osn_upnp_t *self)
{
    return false;
}

bool osn_upnp_stop(osn_upnp_t *self)
{
    return true;
}

