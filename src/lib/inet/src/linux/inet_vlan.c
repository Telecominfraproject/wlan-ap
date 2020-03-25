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
#include <sys/wait.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <errno.h>

#include <string.h>

#include "log.h"
#include "util.h"

#include "inet_unit.h"

#include "inet.h"
#include "inet_base.h"
#include "inet_vlan.h"

#include "execsh.h"


static bool inet_vlan_set_impl(
        inet_t *super,
        const char *ifparent,
        int vlanid);

bool inet_vlan_service_commit_impl(
        inet_base_t *super,
        enum inet_base_services srv,
        bool enable);

bool inet_vlan_interface_start(inet_vlan_t *self, bool enable);

/*
 * ===========================================================================
 *  Initialization
 * ===========================================================================
 */
inet_t *inet_vlan_new(const char *ifname)
{
    inet_vlan_t *self = NULL;

    self = malloc(sizeof(*self));
    if (self == NULL)
    {
        goto error;
    }

    if (!inet_vlan_init(self, ifname))
    {
        LOG(ERR, "inet_vif: %s: Failed to initialize interface instance.", ifname);
        goto error;
    }

    return (inet_t *)self;

error:
    if (self != NULL) free(self);
    return NULL;
}

bool inet_vlan_init(inet_vlan_t *self, const char *ifname)
{
    /* Initialize the parent class -- inet_eth */
    if (!inet_eth_init(&self->eth, ifname))
    {
        LOG(ERR, "inet_vlan: %s: Failed to instantiate class, parent class inet_eth_init() failed.", ifname);
        return false;
    }

    self->inet.in_vlan_set_fn = inet_vlan_set_impl;
    self->base.in_service_commit_fn = inet_vlan_service_commit_impl;

    return true;
}

/*
 * ===========================================================================
 *  VLAN class methods
 * ===========================================================================
 */
bool inet_vlan_set_impl(
        inet_t *super,
        const char *ifparent,
        int vlanid)
{
    inet_vlan_t *self = (void *)super;

    if ((strcmp(ifparent, self->vlan_parent) == 0) &&
        (self->vlan_id == vlanid))
    {
        return true;
    }

    if (strscpy(self->vlan_parent, ifparent, sizeof(self->vlan_parent)) < 0)
    {
        LOG(ERR, "inet_vlan: %s: Parent interface name too long: %s.",
                self->inet.in_ifname,
                ifparent);

        return false;
    }

    self->vlan_id = vlanid;

    /* Interface must be recreated, therefore restart the top service */
    return inet_unit_restart(self->base.in_units, INET_BASE_INTERFACE, false);
}

bool inet_vlan_service_commit_impl(
        inet_base_t *super,
        enum inet_base_services srv,
        bool enable)
{
    inet_vlan_t *self = (inet_vlan_t *)super;

    LOG(DEBUG, "inet_vlan: %s: Service %s -> %s.",
            self->inet.in_ifname,
            inet_base_service_str(srv),
            enable ? "start" : "stop");

    switch (srv)
    {
        case INET_BASE_INTERFACE:
            return inet_vlan_interface_start(self, enable);

        default:
            /* Delegate everything else to the parent class inet_eth() */
            return inet_eth_service_commit(super, srv, enable);
    }

    return true;
}

/*
 * ===========================================================================
 *  Commit and start/stop services
 * ===========================================================================
 */

/*
 * Built-in shell script for creating VLAN interfaces
 *
 * $1 - interface name
 * $2 - parent interface name
 * $3 - vlanid
 */
static char vlan_create_interface[] =
_S(
    if [ -e "/sys/class/net/$1" ];
    then
        ip link del "$1";
    fi;
    ip link add link "$2" name "$1" type vlan id "$3"
);

/*
 * Built-in shell script for deleting VLAN interfaces
 *
 * $1 - interface name, always return success
 */
static char vlan_delete_interface[] =
_S(
    [ ! -e "/sys/class/net/$1" ] && exit 0;
    ip link del "$1"
);


/*
 * Implement the INET_UNIT_INTERFACE service; this is responsible for
 * creating/destroying the interface
 */
bool inet_vlan_interface_start(inet_vlan_t *self, bool enable)
{
    int status;
    char svlanid[11];   /* Number of characters to represent an int */

    if (enable)
    {
        if (self->vlan_parent[0] == '\0')
        {
            LOG(INFO, "inet_vlan: %s: No parent interface was specified.", self->inet.in_ifname);
            return false;
        }

        if (self->vlan_id == 0)
        {
            LOG(INFO, "inet_vlan: %s: No vlan ID was specified.", self->inet.in_ifname);
            return false;
        }

        snprintf(svlanid, sizeof(svlanid), "%d", self->vlan_id);

        status = execsh_log(
                LOG_SEVERITY_INFO,
                vlan_create_interface,
                self->inet.in_ifname,
                self->vlan_parent,
                svlanid);
        if (!WIFEXITED(status) || WEXITSTATUS(status) != 0)
        {
            LOG(ERR, "inet_vlan: %s: Error creating VLAN interface.", self->inet.in_ifname);
            return false;
        }

        LOG(INFO, "inet_vlan: %s: VLAN interface was successfully created.", self->inet.in_ifname);
    }
    else
    {
        status = execsh_log(
                LOG_SEVERITY_INFO,
                vlan_delete_interface,
                self->inet.in_ifname);
        if (!WIFEXITED(status) || WEXITSTATUS(status) != 0)
        {
            LOG(ERR, "inet_vlan: %s: Error deleting VLAN interface.", self->inet.in_ifname);
        }

        LOG(INFO, "inet_vlan: %s: VLAN interface was successfully deleted.", self->inet.in_ifname);
    }

    return true;
}
