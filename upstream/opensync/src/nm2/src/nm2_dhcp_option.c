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

#include "ovsdb_table.h"
#include "os_util.h"
#include "reflink.h"

#include "nm2.h"

/*
 * ===========================================================================
 *  IP_Interface table
 * ===========================================================================
 */
static ovsdb_table_t table_DHCP_Option;

static void callback_DHCP_Option(
        ovsdb_update_monitor_t *mon,
        struct schema_DHCP_Option *old,
        struct schema_DHCP_Option *new);

static bool nm2_dhcp_option_update(
        struct nm2_dhcp_option *dco,
        struct schema_DHCP_Option *schema);

static ds_tree_t nm2_dhcp_option_list = DS_TREE_INIT(ds_str_cmp, struct nm2_dhcp_option, dco_tnode);

struct nm2_dhcp_option *nm2_dhcp_option_get(const ovs_uuid_t *uuid);
static void nm2_dhcp_option_release(struct nm2_dhcp_option *rel);

/*
 * Initialize table monitors
 */
void nm2_dhcp_option_init(void)
{
    LOG(INFO, "Initializing NM DHCP_Option monitoring.");

    OVSDB_TABLE_INIT_NO_KEY(DHCP_Option);
    OVSDB_TABLE_MONITOR(DHCP_Option, false);
}

/*
 * OVSDB monitor update callback for DHCP_Option
 */
void callback_DHCP_Option(
        ovsdb_update_monitor_t *mon,
        struct schema_DHCP_Option *old,
        struct schema_DHCP_Option *new)
{
    struct nm2_dhcp_option *dco;

    switch (mon->mon_type)
    {
        case OVSDB_UPDATE_NEW:
            /* Insert case */
            dco = nm2_dhcp_option_get(&new->_uuid);
            reflink_ref(&dco->dco_reflink, 1);
            break;

        case OVSDB_UPDATE_MODIFY:
            /* Update case */
            dco = ds_tree_find(&nm2_dhcp_option_list, new->_uuid.uuid);
            if (dco == NULL)
            {
                LOG(ERR, "dhcp_option: DHCP_Option with uuid %s not found in cache. Cannot update.", new->_uuid.uuid);
                return;
            }
            break;

        case OVSDB_UPDATE_DEL:
            dco = ds_tree_find(&nm2_dhcp_option_list, old->_uuid.uuid);
            if (dco == NULL)
            {
                LOG(ERR, "dhcp_option: DHCP_Option with uuid %s not found in cache. Cannot delete.", old->_uuid.uuid);
                return;
            }

            /* Decrease the reference count */
            reflink_ref(&dco->dco_reflink, -1);
            return;

        default:
            LOG(ERR, "dhcp_option: Monitor update error.");
            return;
    }

    if (!nm2_dhcp_option_update(dco, new))
    {
        LOG(ERR, "dhcp_option: Unable to parse DHCP_Option schema.");
    }
}

/*
 * Release DHCP_Option object
 */
void nm2_dhcp_option_release(struct nm2_dhcp_option *dco)
{
    LOG(TRACE, "DHCP_Option: Releasing.");

    dco->dco_valid = false;
    reflink_signal(&dco->dco_reflink);

    reflink_fini(&dco->dco_reflink);

    ds_tree_remove(&nm2_dhcp_option_list, dco);

    free(dco);
}

/*
 * DHCP_Option reflink callback
 */
void nm2_dhcp_option_ref_fn(reflink_t *obj, reflink_t *sender)
{
    struct nm2_dhcp_option *dco;

    dco = CONTAINER_OF(obj, struct nm2_dhcp_option, dco_reflink);

    if (sender == NULL)
    {
        LOG(INFO, "dhcp_option: Reference count of object "PRI(reflink_t)" reached 0.",
                FMT(reflink_t, dco->dco_reflink));
        nm2_dhcp_option_release(dco);
    }
}

/*
 * Get a reference to the nm2_dhcp_option structure associated
 * with uuid.
 */
struct nm2_dhcp_option *nm2_dhcp_option_get(const ovs_uuid_t *uuid)
{
    struct nm2_dhcp_option *dco;

    dco = ds_tree_find(&nm2_dhcp_option_list, (void *)uuid->uuid);
    if (dco == NULL)
    {
        /* Allocate a new dummy structure and insert it into the cache */
        dco = calloc(1, sizeof(struct nm2_dhcp_option));
        dco->dco_uuid = *uuid;

        reflink_init(&dco->dco_reflink, "DHCP_Option");
        reflink_set_fn(&dco->dco_reflink, nm2_dhcp_option_ref_fn);

        ds_tree_insert(&nm2_dhcp_option_list, dco, dco->dco_uuid.uuid);
    }

    return dco;
}

/*
 * Get reflink to DHCP_Option with UUID
 */
reflink_t *nm2_dhcp_option_getref(const ovs_uuid_t *uuid)
{
    struct nm2_dhcp_option *dco;

    dco = nm2_dhcp_option_get(uuid);
    if (dco == NULL)
    {
        return NULL;
    }

    return &dco->dco_reflink;
}

/*
 * Update a struct nm2_dhcp_option from the schema
 */
bool nm2_dhcp_option_update(
        struct nm2_dhcp_option *dco,
        struct schema_DHCP_Option *schema)
{
    bool retval = false;

    /*
     * The DHCP_Option is changing, flag it as invalid and notify listeners
     */
    if (dco->dco_valid)
    {
        dco->dco_valid = false;
        reflink_signal(&dco->dco_reflink);
    }

    dco->dco_uuid = schema->_uuid;
    dco->dco_enable = schema->enable;
    dco->dco_tag = schema->tag;

    if (strscpy(dco->dco_version, schema->version, sizeof(dco->dco_version)) < 0)
    {
        LOG(ERR, "dhcp_option: Version string too long: %s.", schema->version);
        goto error;
    }

    if (strscpy(dco->dco_type, schema->type, sizeof(dco->dco_type)) < 0)
    {
        LOG(ERR, "dhcp_option: Type string too long: %s.", schema->type);
        goto error;
    }

    if (strscpy(dco->dco_value, schema->value, sizeof(dco->dco_value)) < 0)
    {
        LOG(ERR, "dhcp_option: Value string too long: %s.", schema->value);
        goto error;
    }

    /*
     * Parsing successful, notify listeners that we have a valid structure now
     */
    dco->dco_valid = true;
    reflink_signal(&dco->dco_reflink);

    retval = true;

error:
    return retval;
}
