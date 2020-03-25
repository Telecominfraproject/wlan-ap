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
#include "synclist.h"
#include "ovsdb_sync.h"

#include "nm2.h"

/*
 * ===========================================================================
 *  IP_Interface table
 * ===========================================================================
 */
static ovsdb_table_t table_DHCPv6_Client;

static void callback_DHCPv6_Client(
        ovsdb_update_monitor_t *mon,
        struct schema_DHCPv6_Client *old,
        struct schema_DHCPv6_Client *new);

static bool nm2_dhcpv6_client_update(
        struct nm2_dhcpv6_client *dc6,
        struct schema_DHCPv6_Client *schema);

static ds_tree_t nm2_dhcpv6_client_list = DS_TREE_INIT(ds_str_cmp, struct nm2_dhcpv6_client, dc6_tnode);

struct nm2_dhcpv6_client *nm2_dhcpv6_client_get(ovs_uuid_t *uuid);
static void nm2_dhcpv6_client_release(struct nm2_dhcpv6_client *rel);
static void nm2_dhcpv6_client_send_opts_update(uuidset_t *us, enum uuidset_event type, reflink_t *obj);
static void nm2_dhcpv6_client_set(struct nm2_dhcpv6_client *dc6, bool enable);
static inet_dhcp6_client_notify_fn_t nm2_dhcpv6_client_notify;
static synclist_fn_t dhcpv6_client_recv_opt_sync;

/* DCHPv6 received option */
struct dhcpv6_client_recv_opt
{
    int                         ropt_tag;
    char                       *ropt_data;
    ovs_uuid_t                  ropt_uuid;
    synclist_node_t             ropt_snode;
};

/* Comparator for nm2_dhcpv6_client_recv_opt */
static inline int dhcpv6_client_recv_opt_cmp(void *_a, void *_b)
{
    int rc;

    struct dhcpv6_client_recv_opt *a = _a;
    struct dhcpv6_client_recv_opt *b = _b;

    rc = a->ropt_tag - b->ropt_tag;
    if (rc != 0) return rc;

    return strcmp(a->ropt_data, b->ropt_data);
}

/*
 * Initialize table monitors
 */
void nm2_dhcpv6_client_init(void)
{
    LOG(INFO, "Initializing NM DHCPv6_Client monitoring.");

    OVSDB_TABLE_INIT_NO_KEY(DHCPv6_Client);

    /*
     * Do not listen to the "received_options" column as we're updating it ourselves.
     * Every time a column changes, _version is updated -- ignore that as well.
     */
    char *filter[] =
    {
                "-",
                "_version",
                SCHEMA_COLUMN(DHCPv6_Client, received_options),
                NULL
    };

    OVSDB_TABLE_MONITOR_F(DHCPv6_Client, filter);
}

/*
 * OVSDB monitor update callback for DHCPv6_Client
 */
void callback_DHCPv6_Client(
        ovsdb_update_monitor_t *mon,
        struct schema_DHCPv6_Client *old,
        struct schema_DHCPv6_Client *new)
{
    struct nm2_dhcpv6_client *dc6;

    switch (mon->mon_type)
    {
        case OVSDB_UPDATE_NEW:
            /* Insert case */
            dc6 = nm2_dhcpv6_client_get(&new->_uuid);
            reflink_ref(&dc6->dc6_reflink, 1);
            break;

        case OVSDB_UPDATE_MODIFY:
            /* Update case */
            dc6 = ds_tree_find(&nm2_dhcpv6_client_list, new->_uuid.uuid);
            if (dc6 == NULL)
            {
                LOG(ERR, "dhcpv6_client: DHCPv6_Client with uuid %s not found in cache. Cannot update.", new->_uuid.uuid);
                return;
            }
            break;

        case OVSDB_UPDATE_DEL:
            dc6 = ds_tree_find(&nm2_dhcpv6_client_list, old->_uuid.uuid);
            if (dc6 == NULL)
            {
                LOG(ERR, "dhcpv6_client: DHCPv6_Client with uuid %s not found in cache. Cannot delete.", old->_uuid.uuid);
                return;
            }

            nm2_dhcpv6_client_set(dc6, false);

            /* Decrease the reference count */
            reflink_ref(&dc6->dc6_reflink, -1);
            return;

        default:
            LOG(ERR, "dhcpv6_client: Monitor update error.");
            return;
    }

    if (!nm2_dhcpv6_client_update(dc6, new))
    {
        LOG(ERR, "dhcpv6_client: Unable to parse DHCPv6_Client schema.");
    }

    nm2_dhcpv6_client_set(dc6, true);
}

/*
 * Release DHCPv6_Client object */
void nm2_dhcpv6_client_release(struct nm2_dhcpv6_client *dc6)
{
    reflink_t *ref;

    LOG(TRACE, "DHCPv6_Client: Releasing.");

    if (dc6->dc6_valid)
    {
        ref = nm2_ip_interface_getref(&dc6->dc6_ip_interface_uuid);
        if (ref == NULL)
        {
            LOG(ERR, "dhcpv6_client: Unable to acquire reference to IP_Interface:%s (releas).",
                    dc6->dc6_ip_interface_uuid.uuid);
        }
        else
        {
            reflink_disconnect(&dc6->dc6_ip_interface_reflink, ref);
        }
    }

    /* Invalidate this address so it gets removed, notify listeners */
    dc6->dc6_valid = false;
    reflink_signal(&dc6->dc6_reflink);

    uuidset_fini(&dc6->dc6_send_options);
    uuidset_fini(&dc6->dc6_received_options);

    reflink_fini(&dc6->dc6_ip_interface_reflink);

    reflink_fini(&dc6->dc6_reflink);

    ds_tree_remove(&nm2_dhcpv6_client_list, dc6);

    free(dc6);
}

/*
 * DHCPv6_Client reflink callback
 */
void nm2_dhcpv6_client_ref_fn(reflink_t *obj, reflink_t *sender)
{
    struct nm2_dhcpv6_client *dc6;

    dc6 = CONTAINER_OF(obj, struct nm2_dhcpv6_client, dc6_reflink);

    if (sender == NULL)
    {
        LOG(INFO, "dhcp6_client: Reference count of object "PRI(reflink_t)" reached 0.",
                FMT(reflink_t, dc6->dc6_reflink));
        nm2_dhcpv6_client_release(dc6);
    }
}

/*
 * DHCPv6_Client.ip_interface reflink callback
 *
 * This is called when ip_interface is created/updated
 */
void nm2_dhcpv6_client_ip_interface_ref_fn(reflink_t *obj, reflink_t *sender)
{
    struct nm2_dhcpv6_client *dc6;
    (void)dc6;

    dc6 = CONTAINER_OF(obj, struct nm2_dhcpv6_client, dc6_ip_interface_reflink);

    if (sender == NULL) return;

    /* Parent interface was updated, push new settings */
    nm2_dhcpv6_client_set(dc6, true);

    return;
}

/*
 * Get a reference to the nm2_dhcpv6_client structure associated
 * with uuid.
 *
 * This function increases the reference count of the structure by 1 and
 * must be freed using nm2_ipv6_address_put().
 */
struct nm2_dhcpv6_client *nm2_dhcpv6_client_get(ovs_uuid_t *uuid)
{
    struct nm2_dhcpv6_client *dc6;

    dc6 = ds_tree_find(&nm2_dhcpv6_client_list, uuid->uuid);

    if (dc6 != NULL) return dc6;

    /* Allocate a new dummy structure and insert it into the cache */
    dc6 = calloc(1, sizeof(struct nm2_dhcpv6_client));
    dc6->dc6_uuid = *uuid;

    reflink_init(&dc6->dc6_reflink, "DHCPv6_Client");
    reflink_set_fn(&dc6->dc6_reflink, nm2_dhcpv6_client_ref_fn);

    reflink_init(&dc6->dc6_ip_interface_reflink, "DHCPv6_Client.ip_interface");
    reflink_set_fn(&dc6->dc6_ip_interface_reflink, nm2_dhcpv6_client_ip_interface_ref_fn);

    /*
     * Initialize UUID references
     */
    uuidset_init(
            &dc6->dc6_send_options,
            "DHCPv6_Client.send_options",
            nm2_dhcp_option_getref,
            nm2_dhcpv6_client_send_opts_update);

    /* Initialzie received options synclist */
    synclist_init(
            &dc6->dc6_received_list,
            dhcpv6_client_recv_opt_cmp,
            struct dhcpv6_client_recv_opt,
            ropt_snode,
            dhcpv6_client_recv_opt_sync);

    ds_tree_insert(&nm2_dhcpv6_client_list, dc6, dc6->dc6_uuid.uuid);

    return dc6;
}

/*
 * DHCPv6_Client.send_options update
 */
void nm2_dhcpv6_client_send_opts_update(uuidset_t *us, enum uuidset_event type, reflink_t *remote)
{
    /* Dereference classes */
    struct nm2_dhcpv6_client *dc6 = CONTAINER_OF(us, struct nm2_dhcpv6_client, dc6_send_options);
    struct nm2_dhcp_option *dco = CONTAINER_OF(remote, struct nm2_dhcp_option, dco_reflink);

    bool add;

    switch (type)
    {
        case UUIDSET_NEW:
            if (!dco->dco_valid) return;
            add = true;
            break;

        case UUIDSET_MOD:
            add = dco->dco_valid;
            break;

        case UUIDSET_DEL:
            if (!dco->dco_valid) return;
            add = false;
            break;

        default:
            return;
    }

    struct nm2_iface *piface = nm2_ip_interface_iface_get(&dc6->dc6_ip_interface_uuid);
    if (piface == NULL)
    {
        LOG(ERR, "dhcv6_client: send_opts_update: Unable to resolve interface.");
        return;
    }

    LOG(DEBUG, "dhcpv6_client: %s: DEL/ADD[%d] DHCPv6 send option tag %d: %s",
            piface->if_name,
            add,
            dco->dco_tag,
            dco->dco_value);

    if (!inet_dhcp6_client_option_send(piface->if_inet, dco->dco_tag, add ? dco->dco_value : NULL))
    {
        LOG(ERR, "dhcpv6_client: %s: Error adding/removing(%d) send options with tag %d: %s.",
                piface->if_name,
                add,
                dco->dco_tag,
                dco->dco_value);
        return;
    }

    inet_commit(piface->if_inet);
}

/*
 * Update a struct nm2_dhcpv6_client from the schema
 */
bool nm2_dhcpv6_client_update(
        struct nm2_dhcpv6_client *dc6,
        struct schema_DHCPv6_Client *schema)
{
    reflink_t *ref;

    bool retval = false;

    /*
     * The DHCPv6_Client is changing, flag it as invalid and notify listeners
     */
    if (dc6->dc6_valid)
    {
        dc6->dc6_valid = false;
        reflink_signal(&dc6->dc6_reflink);

        /*
         * Remove reflink to IP_Interface
         */
        ref = nm2_ip_interface_getref(&dc6->dc6_ip_interface_uuid);
        if (ref == NULL)
        {
            LOG(ERR, "dhcpv6_client: Unable to acquire reference to IP_Interface:%s (delete)",
                    dc6->dc6_ip_interface_uuid.uuid);
        }
        else
        {
            reflink_disconnect(&dc6->dc6_ip_interface_reflink, ref);
        }
    }

    dc6->dc6_request_address = schema->request_address;
    dc6->dc6_request_prefixes = schema->request_prefixes;
    dc6->dc6_rapid_commit = schema->rapid_commit;
    dc6->dc6_renew = schema->renew;

    if (ARRAY_LEN(dc6->dc6_request_opts) < schema->request_options_len)
    {
        LOG(ERR, "dhcpv6_client: Request options too long.");
        goto error;
    }

    dc6->dc6_request_opts_len = schema->request_options_len;
    memcpy(
            dc6->dc6_request_opts,
            schema->request_options,
            sizeof(dc6->dc6_request_opts[0]) * schema->request_options_len);

    /*
     * Establish reflink to IP_Interface
     */
    dc6->dc6_ip_interface_uuid = schema->ip_interface;
    ref = nm2_ip_interface_getref(&dc6->dc6_ip_interface_uuid);
    if (ref == NULL)
    {
        LOG(ERR, "dhcpv6_client: Unable to acquire reference to IP_Interface:%s (add)",
                dc6->dc6_ip_interface_uuid.uuid);
        goto error;
    }

    /* Establish connection to IP_Interface */
    reflink_connect(&dc6->dc6_ip_interface_reflink, ref);

    uuidset_set(&dc6->dc6_received_options, schema->received_options, schema->received_options_len);
    uuidset_set(&dc6->dc6_send_options, schema->send_options, schema->send_options_len);

    /*
     * Parsing successful, notify listeners that we have a valid structure now
     */
    dc6->dc6_valid = true;
    reflink_signal(&dc6->dc6_reflink);

    retval = true;

error:
    return retval;
}

void nm2_dhcpv6_client_set(struct nm2_dhcpv6_client *dc6, bool enable)
{
    struct nm2_iface *piface;
    bool rc;
    int tag;
    int ii;

    piface = nm2_ip_interface_iface_get(&dc6->dc6_ip_interface_uuid);
    if (piface == NULL)
    {
        LOG(TRACE, "dhcpv6_client: Parent interface with uuid %s not rady.",
                dc6->dc6_ip_interface_uuid.uuid);
        return;
    }

    rc = inet_dhcp6_client(
            piface->if_inet,
            enable,
            dc6->dc6_request_address,
            dc6->dc6_request_prefixes,
            dc6->dc6_rapid_commit);
    if (!rc)
    {
        LOG(ERR, "dhcpv6_client: %s: Unable to set configuration.",
                piface->if_name);
        return;
    }

    /*
     * Update requested options
     */
    for (tag = 0; tag < DHCP_OPTION_MAX; tag++)
    {
        /* Scan the array in dc6_request_opts and check if tag is present */
        for (ii = 0; ii < dc6->dc6_request_opts_len; ii++)
        {
            if (dc6->dc6_request_opts[ii] == tag)
            {
                break;
            }
        }

        /*
         * If the tag was found, the "ii < dc6->dc6_request_opts_len" condition
         * will be true, false otherwise
         */
        inet_dhcp6_client_option_request(
                piface->if_inet,
                tag,
                ii < dc6->dc6_request_opts_len);
    }

    inet_dhcp6_client_notify(piface->if_inet, nm2_dhcpv6_client_notify, dc6);

    /* Commit configuration */
    inet_commit(piface->if_inet);
}

void nm2_dhcpv6_client_notify(void *ctx, struct osn_dhcpv6_client_status *status)
{
    struct nm2_dhcpv6_client *dc6 = ctx;

    int ii;

    synclist_begin(&dc6->dc6_received_list);
    for (ii = 0; ii < ARRAY_LEN(status->d6c_recv_options); ii++)
    {
        struct dhcpv6_client_recv_opt ropts;

        if (status->d6c_recv_options[ii] == NULL) continue;

        ropts.ropt_tag = ii;
        ropts.ropt_data = status->d6c_recv_options[ii];

        (void)synclist_add(&dc6->dc6_received_list, &ropts);
    }
    synclist_end(&dc6->dc6_received_list);
}


void *dhcpv6_client_recv_opt_sync(synclist_t *list, void *_old, void *_new)
{
    struct dhcpv6_client_recv_opt *item;
    struct nm2_dhcpv6_client *dc6;
    struct schema_DHCP_Option dco;
    pjs_errmsg_t msg;
    bool insert;
    bool rc;

    struct dhcpv6_client_recv_opt *ropt = NULL;

    /* Insert case */
    if (_old == NULL)
    {
        insert = true;
        item = _new;
    }
    /* Remove case */
    else if (_new == NULL)
    {
        insert = false;
        item = _old;
    }
    /* Update case or invalid */
    else
    {
        return _old;
    }

    dc6 = CONTAINER_OF(list, struct nm2_dhcpv6_client, dc6_received_list);

    if (!insert)
    {
        ovsdb_sync_mutate_uuid_set(
                SCHEMA_TABLE(DHCPv6_Client),
                ovsdb_where_uuid("_uuid", dc6->dc6_uuid.uuid),
                SCHEMA_COLUMN(DHCPv6_Client, received_options),
                OTR_DELETE,
                item->ropt_uuid.uuid);

        /* Delete the option */
        free(item->ropt_data);
        free(item);
        return NULL;
    }

    /* Do not allow empty data */
    if (item->ropt_data[0] == '\0')
    {
        LOG(NOTICE, "dhcpv6_client: Received empty DHCPv6 option tag %d.", item->ropt_tag);
        return NULL;
    }

    ropt = calloc(1, sizeof(*ropt));
    ropt->ropt_tag = item->ropt_tag;
    ropt->ropt_data = strdup(item->ropt_data);

    memset(&dco, 0, sizeof(dco));
    dco.enable = true;
    strscpy(dco.type, "rx", sizeof(dco.type));
    strscpy(dco.version, "v6", sizeof(dco.type));
    dco.tag = ropt->ropt_tag;
    strscpy(dco.value, ropt->ropt_data, sizeof(dco.value));

    json_t *jopt = schema_DHCP_Option_to_json(&dco, msg);
    if (jopt == NULL)
    {
        LOG(ERR, "dhcpv6_client: Unable to create JSON for received option: %d:%s", item->ropt_tag, item->ropt_data);
        goto exit;
    }

    /* Insert a new DHCP_Option entry with parent DHCPv6_Client */
    rc = ovsdb_sync_insert_with_parent(
            SCHEMA_TABLE(DHCP_Option),
            jopt,
            &ropt->ropt_uuid,
            SCHEMA_TABLE(DHCPv6_Client),
            ovsdb_where_uuid("_uuid", dc6->dc6_uuid.uuid),
            SCHEMA_COLUMN(DHCPv6_Client, received_options));
    if (!rc)
    {
        LOG(ERR, "dhcpv6_client: Unable to insert DHCP_Option (%d) with parent DHCPv6_Client.",
                item->ropt_tag);
        goto exit;
    }

    return ropt;

exit:
    if (ropt != NULL)
    {
        free(ropt->ropt_data);
        free(ropt);
    }

    return NULL;
}
