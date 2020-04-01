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
 * ===========================================================================
 *  DNSMASQ provides the implementation for the following OpenSync APIs:
 *      - DHCPv6 server
 *      - Router Advertisement
 * ===========================================================================
 */

#include <errno.h>

#include "osn_types.h"
#include "osn_dhcpv6.h"

#include "ds.h"
#include "ds_tree.h"
#include "ds_dlist.h"
#include "const.h"
#include "log.h"
#include "util.h"
#include "daemon.h"
#include "evx.h"

/*
 * ===========================================================================
 *  DHCPv6 Server
 * ===========================================================================
 */
#define DNSMASQ_SERVER_PATH                 "/usr/sbin/dnsmasq"
#define DNSMASQ_SERVER_DEBOUNCE_TIME        0.3 /* 300ms */
#define DNSMASQ_CONFIG_FILE                 "/var/etc/dnsmasq6.conf"
#define DNSMASQ_PID_FILE                    "/var/run/dnsmasq/dnsmasq6.pid"
#define DNSMASQ_LEASE_FILE                  "/tmp/dhcp6.leases"

#define DNSMASQ_REALLOC_ADD                 16  /* Number of elements to add each time an array is reallocated */

#define DHCP_OPTIONS_MAX 256

/*
 * ===========================================================================
 *  DHCPv6 server
 * ===========================================================================
 */
struct dhcpv6_server_prefix
{
    struct osn_dhcpv6_server_prefix     dp_prefix;
    ds_tree_node_t                      dp_tnode;
};

struct dhcpv6_server_option
{
    int                                 do_tag;
    char                               *do_data;
    ds_tree_node_t                      do_tnode;
};

struct dhcpv6_server_lease
{
    struct osn_dhcpv6_server_lease      dl_lease;
    ds_tree_node_t                      dl_tnode;
};

/*
 * There's a single global instance of dnsmasq, we need to connect the various
 * configuration structures so we can create a single config file.
 *
 * This is a combined structure for Router Advertisement and DHCPv6 server
 */
struct osn_dhcpv6_server
{
    /* DHCPv6 fields */
    char                        d6s_ifname[C_IFNAME_LEN];
    bool                        d6s_prefix_delegation;
    ds_tree_t                   d6s_prefixes;
    char                       *d6s_options[DHCP_OPTIONS_MAX];
    struct osn_dhcpv6_server_status
                                d6s_status;         /* Cached server status */
    osn_dhcpv6_server_status_fn_t
                               *d6s_notify_fn;      /* Notification callback */
    ds_tree_t                   d6s_leases;
    ds_tree_node_t              d6s_tnode;
    void                       *d6s_data;           /* Private data */
};

static ds_tree_t dhcpv6_server_list = DS_TREE_INIT(ds_str_cmp, struct osn_dhcpv6_server, d6s_tnode);

static ev_stat dhcpv6_server_lease_stat;            /* Lease file watcher */
static ev_debounce dhcpv6_server_lease_debounce;    /* Lease file debounce timer */

static daemon_t dnsmasq6_server_daemon;
static struct ev_debounce dnsmasq6_server_debounce;

/*
 * Private functions
 */

static void dhcpv6_server_lease_stat_fn(struct ev_loop *loop, ev_stat *w, int revent);
static void dhcpv6_server_lease_debounce_fn(struct ev_loop *loop, ev_debounce *w, int revent);
static struct osn_dhcpv6_server *dhcpv6_server_find_by_prefix(struct osn_ip6_addr *addr);

static void dhcpv6_server_status_lease_add(
        struct osn_dhcpv6_server_status *d6s,
        struct osn_dhcpv6_server_lease *lease);

static bool dnsmasq6_server_apply(void);

static ds_key_cmp_t dhcpv6_server_lease_cmp;

/*
 * ===========================================================================
 *  DHCPv6 Server Public API
 * ===========================================================================
 */

/*
 * Create new DHCPv6 server instance
 */
osn_dhcpv6_server_t *osn_dhcpv6_server_new(const char *ifname)
{
    static bool first_run = true;

    osn_dhcpv6_server_t *self;

    if (first_run)
    {
        LOG(INFO, "dhcpv6_server: Global initialization.");
        /* Global initialization */
        ev_stat_init(&dhcpv6_server_lease_stat, dhcpv6_server_lease_stat_fn, DNSMASQ_LEASE_FILE, 0.0);
        ev_debounce_init(&dhcpv6_server_lease_debounce, dhcpv6_server_lease_debounce_fn, 0.3);

        /* Start monitoring the lease file */
        ev_stat_start(EV_DEFAULT, &dhcpv6_server_lease_stat);

        first_run = false;
    }

    self = ds_tree_find(&dhcpv6_server_list, (void *)ifname);
    if (self != NULL)
    {
        LOG(ERR, "dhcpv6_server: Attempting to configure multiple DHCPv6 servers on interface %s.", ifname);
        return NULL;
    }

    self = calloc(1, sizeof(*self));
    if (STRSCPY(self->d6s_ifname, ifname) < 0)
    {
        LOG(ERR, "dhcpv6_server: Interface name too long: %s", ifname);
        goto error;
    }

    ds_tree_init(&self->d6s_prefixes, osn_ip6_addr_cmp, struct dhcpv6_server_prefix, dp_tnode);
    ds_tree_init(&self->d6s_leases, dhcpv6_server_lease_cmp, struct dhcpv6_server_lease, dl_tnode);

    /* Insert the object into the global list */
    ds_tree_insert(&dhcpv6_server_list, self, self->d6s_ifname);

    /* Initialize the status structure */
    self->d6s_status.d6st_iface = self->d6s_ifname;

    return self;

error:
    if (self != NULL) free(self);

    return NULL;
}

/*
 * Delete a  DHCPv6 Server instance
 */
bool osn_dhcpv6_server_del(osn_dhcpv6_server_t *self)
{
    struct dhcpv6_server_prefix *prefix;
    struct dhcpv6_server_lease *lease;
    ds_tree_iter_t iter;
    int tag;

    ds_tree_remove(&dhcpv6_server_list, self);

    /* Clear the prefixes list */
    ds_tree_foreach_iter(&self->d6s_prefixes, prefix, &iter)
    {
        ds_tree_iremove(&iter);
        free(prefix);
    }

    /* Clear options */
    for (tag = 0; tag < DHCP_OPTIONS_MAX; tag++)
    {
        free(self->d6s_options[tag]);
        self->d6s_options[tag] = NULL;
    }

    /* Clear the leases list */
    ds_tree_foreach_iter(&self->d6s_leases, lease, &iter)
    {
        ds_tree_iremove(&iter);
        free(lease);
    }

    /* Clear status structure */
    if (self->d6s_status.d6st_leases != NULL)
    {
        free(self->d6s_status.d6st_leases);
    }

    /* Schedule a reconfiguration so the current config is removed from the system settings */
    if (!dnsmasq6_server_apply())
    {
        LOG(ERR, "dhcpv6_server: Unable to apply dnsmasq configuration.");
        return false;
    }

    return true;
}

bool osn_dhcpv6_server_apply(osn_dhcpv6_server_t *self)
{
    (void)self;

    if (!dnsmasq6_server_apply())
    {
        LOG(ERR, "dhcpv6_server: Unable to apply dnsmasq configuration.");
        return false;
    }

    return true;
}

bool osn_dhcpv6_server_set(osn_dhcpv6_server_t *self, bool prefix_delegation)
{
    self->d6s_prefix_delegation = prefix_delegation;
    return true;
}

/*
 * Add the prefix to the pool that the DHCPv6 server will be serving
 */
bool osn_dhcpv6_server_prefix_add(
        osn_dhcpv6_server_t *self,
        struct osn_dhcpv6_server_prefix *prefix)
{
    struct dhcpv6_server_prefix *node;

    node = ds_tree_find(&self->d6s_prefixes, &prefix->d6s_prefix);
    if (node == NULL)
    {
        node = calloc(1, sizeof(*node));
        node->dp_prefix = *prefix;
        ds_tree_insert(&self->d6s_prefixes, node, &node->dp_prefix.d6s_prefix);
    }
    else
    {
        node->dp_prefix = *prefix;
    }

    return true;
}

/*
 * Remove the prefix from the DHCPv6 server list
 */
bool osn_dhcpv6_server_prefix_del(
        osn_dhcpv6_server_t *self,
        struct osn_dhcpv6_server_prefix *prefix)
{
    struct dhcpv6_server_prefix *node;

    node = ds_tree_find(&self->d6s_prefixes, &prefix->d6s_prefix);
    if (node == NULL)
    {
        LOG(ERR, "dhcpv6_server: %s: Unable to remove prefix: "PRI_osn_ip6_addr,
                self->d6s_ifname,
                FMT_osn_ip6_addr(prefix->d6s_prefix));
        return false;
    }

    ds_tree_remove(&self->d6s_prefixes, node);
    free(node);

    return true;
}

/*
 * Set DHCPv6 options
 */
bool osn_dhcpv6_server_option_send(
        osn_dhcpv6_server_t *self,
        int tag,
        const char *data)
{
    if (tag <= 0 || tag >= DHCP_OPTIONS_MAX)
    {
        LOG(ERR, "dhcpv6_server: %s: Invalid tag: %d (data: %s).",
                self->d6s_ifname,
                tag,
                data);
        return false;
    }

    if (self->d6s_options[tag] != NULL)
    {
        free(self->d6s_options[tag]);
        self->d6s_options[tag] = NULL;
    }

    if (data != NULL) self->d6s_options[tag] = strdup(data);

    return true;
}

/*
 * Add static lease
 */
bool osn_dhcpv6_server_lease_add(
        osn_dhcpv6_server_t *self,
        struct osn_dhcpv6_server_lease *lease)
{
    struct dhcpv6_server_lease *node;

    node = ds_tree_find(&self->d6s_leases, lease);
    if (node == NULL)
    {
        node = calloc(1, sizeof(*node));
        node->dl_lease = *lease;
        ds_tree_insert(&self->d6s_leases, node, &node->dl_lease);
    }
    else
    {
        node->dl_lease = *lease;
    }

    return true;
}

/*
 * Remove static lease
 */
bool osn_dhcpv6_server_lease_del(
        osn_dhcpv6_server_t *self,
        struct osn_dhcpv6_server_lease *lease)
{
    struct dhcpv6_server_lease *node;

    node = ds_tree_find(&self->d6s_leases, lease);
    if (node == NULL)
    {
        LOG(ERR, "dhcpv6_server: %s: Unable to remove lease: "PRI_osn_ip6_addr,
                self->d6s_ifname,
                FMT_osn_ip6_addr(lease->d6s_addr));
        return false;
    }

    ds_tree_remove(&self->d6s_leases, node);
    free(node);

    return true;
}

/*
 * Set the notification/status update
 */
bool osn_dhcpv6_server_status_notify(
        osn_dhcpv6_server_t *self,
        osn_dhcpv6_server_status_fn_t *fn)
{
    self->d6s_notify_fn = fn;

    return true;
}

/* Set/get private data */
void osn_dhcpv6_server_data_set(osn_dhcpv6_server_t *self, void *data)
{
    self->d6s_data = data;
}

void *osn_dhcpv6_server_data_get(osn_dhcpv6_server_t *self)
{
    return self->d6s_data;
}

/*
 * ===========================================================================
 *  Router Advertisement public API
 * ===========================================================================
 */
struct osn_ip6_radv
{
    char                        ra_ifname[C_IFNAME_LEN];
    struct osn_ip6_radv_options ra_opts;
    ds_dlist_t                  ra_prefixes;
    ds_dlist_t                  ra_rdnss;
    ds_dlist_t                  ra_dnssl;
    ds_tree_node_t              ra_tnode;
};

struct ra_server_prefix
{
    osn_ip6_addr_t              rp_prefix;
    ds_dlist_t                  rp_dnode;
    bool                        rp_autonomous;
    bool                        rp_onlink;
};

struct ra_server_rdnss
{
    osn_ip6_addr_t              rd_rdnss;
    ds_dlist_t                  rd_dnode;
};

struct ra_server_dnssl
{
    char                        rd_dnssl[256];
    ds_dlist_t                  rd_dnode;
};

static ds_tree_t ra_server_list = DS_TREE_INIT(ds_str_cmp, osn_ip6_radv_t, ra_tnode);

osn_ip6_radv_t *osn_ip6_radv_new(const char *ifname)
{
    osn_ip6_radv_t *self;

    self = ds_tree_find(&ra_server_list, (void *)ifname);
    if (self != NULL)
    {
        LOG(ERR, "osn_ip6_radv: Attempting to configure multiple RA servers on interface %s.", ifname);
        goto error;
    }

    self = calloc(1, sizeof(*self));
    if (STRSCPY(self->ra_ifname, ifname) < 0)
    {
        LOG(ERR, "osn_ip6_radv: Interface name too long: %s", ifname);
        goto error;
    }

    ds_dlist_init(&self->ra_prefixes, struct ra_server_prefix, rp_dnode);
    ds_dlist_init(&self->ra_rdnss, struct ra_server_rdnss, rd_dnode);
    ds_dlist_init(&self->ra_dnssl, struct ra_server_dnssl, rd_dnode);

    ds_tree_insert(&ra_server_list, self, self->ra_ifname);

    return self;

error:
    if (self != NULL) free(self);
    return NULL;
}

bool osn_ip6_radv_del(osn_ip6_radv_t *self)
{
    ds_dlist_iter_t iter;
    void *node;

    /* Clear the prefixes list */
    ds_dlist_foreach_iter(&self->ra_prefixes, node, iter)
    {
        ds_dlist_iremove(&iter);
        free(node);
    }

    /* Clear the RDNSS list */
    ds_dlist_foreach_iter(&self->ra_rdnss, node, iter)
    {
        ds_dlist_iremove(&iter);
        free(node);
    }

    /* Clear the DNSSL list */
    ds_dlist_foreach_iter(&self->ra_dnssl, node, iter)
    {
        ds_dlist_iremove(&iter);
        free(node);
    }

    ds_tree_remove(&ra_server_list, self);
    free(self);

    /* Restart service */
    if (!dnsmasq6_server_apply())
    {
        LOG(ERR, "osn_ip6_radv: Error applying dnsmasq configuration (radv del).");
    }

    return true;
}

bool osn_ip6_radv_apply(osn_ip6_radv_t *self)
{
    (void)self;

    /* Restart global dnsmasq instance */
    if (!dnsmasq6_server_apply())
    {
        LOG(ERR, "osn_ip6_radv: Error applying dnsmasq configuration (radv apply).");
    }

    return true;
}

bool osn_ip6_radv_set(osn_ip6_radv_t *self, const struct osn_ip6_radv_options *opts)
{
    self->ra_opts = *opts;
    return true;
}

bool osn_ip6_radv_add_prefix(
        osn_ip6_radv_t *self,
        const osn_ip6_addr_t *prefix,
        bool autonomous,
        bool onlink)
{
    struct ra_server_prefix *node = calloc(1, sizeof(*node));
    node->rp_prefix = *prefix;
    node->rp_autonomous = autonomous;
    node->rp_onlink = onlink;

    ds_dlist_insert_head(&self->ra_prefixes, node);

    return true;
}

bool osn_ip6_radv_add_rdnss(osn_ip6_radv_t *self, const osn_ip6_addr_t *rdnss)
{
    struct ra_server_rdnss *node = calloc(1, sizeof(*node));
    node->rd_rdnss = *rdnss;
    ds_dlist_insert_tail(&self->ra_rdnss, node);

    return true;
}

bool osn_ip6_radv_add_dnssl(osn_ip6_radv_t *self, char *dnssl)
{
    struct ra_server_dnssl *node = calloc(1, sizeof(*node));
    if (strscpy(node->rd_dnssl, dnssl, sizeof(node->rd_dnssl)) < 0)
    {
        LOG(ERR, "osn_ip6_radv: Error adding DNSSL, string too long: %s", dnssl);
        free(node);
        return false;
    }

    ds_dlist_insert_tail(&self->ra_dnssl, node);

    return true;
}

const char *dnsmasq6_server_option6_encode(int tag, const char *value)
{
    static char buf[1024];

    switch (tag)
    {
        case 23:    /* DNS Recursive Name server */
        {
            char val[1024];
            char *pval;
            char *rdnss;
            int bufpos;
            int rc;

            /*
             * Value may be a space-separated list of DNS servers -- convert
             * it to a comma separated list. Enclose each IPv6 address with []
             */
            if (STRSCPY(val, value) < 0)
            {
                LOG(ERR, "dnsmasq: Error encoding RDNSS option: %s. String too long.", value);
                return value;
            }

            buf[0] = '\0';
            bufpos = 0;
            for (rdnss = strtok_r(val, " ", &pval);
                    rdnss != NULL;
                    rdnss = strtok_r(NULL, " ", &pval))
            {
                if (bufpos == 0)
                {
                    rc = snprintf(buf + bufpos, sizeof(buf) - bufpos, "[%s]", rdnss);
                }
                else
                {
                    rc = snprintf(buf + bufpos, sizeof(buf) - bufpos, ",[%s]", rdnss);
                }

                if (rc >= ((int)sizeof(buf) - bufpos))
                {
                    LOG(NOTICE, "dnsmasq: RDNSS string too long. Truncated from and including: %s", rdnss);
                    /* Truncate string */
                    buf[bufpos] = '\0';
                    break;
                }

                bufpos += rc;
            }

            return buf;
        }

        default:
            break;
    }

    return value;
}

/*
 * ===========================================================================
 *  DNSMASQ private functions
 * ===========================================================================
 */
bool dnsmasq6_server_write_config(void)
{
    FILE *f;
    osn_ip6_radv_t *ra;
    osn_dhcpv6_server_t *d6s;
    struct ra_server_prefix *ra_prefix;
    struct ra_server_rdnss *ra_rdnss;
    struct ra_server_dnssl *ra_dnssl;
    struct dhcpv6_server_prefix *d6s_prefix;
    struct dhcpv6_server_lease *d6s_lease;
    int tag;

    bool retval = false;

    f = fopen(DNSMASQ_CONFIG_FILE, "w");
    if (f == NULL)
    {
        LOG(ERR, "dnsmasq: Unable to open config file: %s", DNSMASQ_CONFIG_FILE);
        goto error;
    }

    /* Disable DNS */
    fprintf(f, "port=0\n");

    /* Lease file */
    fprintf(f, "dhcp-leasefile=%s\n", DNSMASQ_LEASE_FILE);

    /* Enable router advertisement */
    fprintf(f, "enable-ra\n");

    /*
     * Router Advertisement configuration
     */
    ds_tree_foreach(&ra_server_list, ra)
    {
        char mtu[32] = {0};
        char prfd_router[16] = {0};
        char radvt_intrvl[32] = {0};
        char router_lifetime[32] = {0};

        fprintf(f, "interface=%s\n", ra->ra_ifname);

        // mtu to be used by clients.for now we dont supporting sending mtus of other interfaces.
        if(ra->ra_opts.ra_mtu >= 1280)
            sprintf(mtu,",mtu:%d",ra->ra_opts.ra_mtu);

        switch(ra->ra_opts.ra_preferred_router)
        {
            case 0:
                    sprintf(prfd_router,  ",%s", "low");
                    break;
            case 2:
                    sprintf(prfd_router,  ",%s", "high");
                    break;
            default:
                    // medium - no need to specify.
                    break;
         }

        // RA packets interval.
        // If the value is < 4 it will be set to 4 by dnsmasq.
        // If the value is > 1800 it will be set to 1800 by dnsmasq.
        sprintf(radvt_intrvl, ",%d", ra->ra_opts.ra_max_adv_interval >= 0 ? ra->ra_opts.ra_max_adv_interval : 0);

        // RA router lifetime.if not specified it takes 3 * RA packets interval.
        if(ra->ra_opts.ra_default_lft >= 0)
            sprintf(router_lifetime, ",%d", ra->ra_opts.ra_default_lft);

        fprintf(f,"ra-param=%s%s%s%s%s\n",
                ra->ra_ifname,
                mtu,
                prfd_router,
                radvt_intrvl,
                router_lifetime);

        ds_dlist_foreach(&ra->ra_prefixes, ra_prefix)
        {
            char *ra_mode = NULL;

            /*
             * The following table was taken from:
             *      https://weirdfellow.wordpress.com/2014/09/05/dhcpv6-and-ra-with-dnsmasq/
             *
             * =========================================================================================================
             * | dnsmasq option              | Managed | Other   | Autonomous      | Comment
             * |                             |         | Config  |                 |
             * =========================================================================================================
             * |                             |   +     |         |      -          | Stateful DHCPv6 see below
             * ---------------------------------------------------------------------------------------------------------
             * | slaac,ra-names              |   +     |         |      +          | SLAAC enabled, stateful DHCPv6
             * ---------------------------------------------------------------------------------------------------------
             * | ra-stateless,ra-names       |   -     |   +     |      +          | Stateless DHCPv6
             * ---------------------------------------------------------------------------------------------------------
             * | ra-only                     |   -     |   -     |      +          | Stateless DHCPv6
             * ---------------------------------------------------------------------------------------------------------
             *
             * Note: When "Address Autoconfiguration" is false it means we have to run in stateful DHCPv6 mode. In this
             * a DHCPv6_Server table must exists and therefore the DHCPv6 code will have to add it below.
             */

            LOG(DEBUG, "osn_ip6_radv: Flags for prefix "PRI_osn_ip6_addr" managed:%d other_config:%d auto:%d.",
                    FMT_osn_ip6_addr(ra_prefix->rp_prefix),
                    ra->ra_opts.ra_managed,
                    ra->ra_opts.ra_other_config,
                    ra_prefix->rp_autonomous);

#if 0
WAR: ESW-3196: Assume the autonomous flag is true if its added to IPv6_RouteAdv
            if (!ra_prefix->rp_autonomous)
            {
                /* Skip this address -- it will probably be handled by the DHCPv6_Server instance */
                continue;
            }
            else
#endif
            if (ra->ra_opts.ra_managed)
            {
                ra_mode = "slaac,ra-names";
            }
            else if (ra->ra_opts.ra_other_config)
            {
                ra_mode = "ra-stateless,ra-names";
            }
            else
            {
                ra_mode = "ra-only";
            }

            fprintf(f, "dhcp-range=%s,::,constructor:%s,%s\n",
                    ra->ra_ifname,
                    ra->ra_ifname,
                    ra_mode);
        }

        /* Add RDNSS options */
        ds_dlist_foreach(&ra->ra_rdnss, ra_rdnss)
        {
            osn_ip6_addr_t rdnss;

            rdnss = ra_rdnss->rd_rdnss;
            rdnss.ia6_prefix = -1;
            rdnss.ia6_pref_lft = INT_MIN;
            rdnss.ia6_valid_lft = INT_MIN;

            fprintf(f, "dhcp-option=%s,option6:dns-server,["PRI_osn_ip6_addr"]\n",
                    ra->ra_ifname,
                    FMT_osn_ip6_addr(rdnss));
        }

        char dnssl[256];
        int dnssl_len;
        dnssl[0] = '\0';

        /* Build up the DNSSL option */
        dnssl_len = 0;
        ds_dlist_foreach(&ra->ra_dnssl, ra_dnssl)
        {
            if ((sizeof(dnssl) - dnssl_len) <= (strlen(ra_dnssl->rd_dnssl) + 1))
            {
                LOG(ERR, "osn_ip6_radv: Error adding DNSSL entry: %s. Line too long.", ra_dnssl->rd_dnssl);
                break;
            }

            dnssl_len += snprintf(dnssl + dnssl_len, sizeof(dnssl) - dnssl_len, "%s,", ra_dnssl->rd_dnssl);
        }

        /* Remove any trailing "," */
        strchomp(dnssl, ",");
        if(dnssl_len > 0)
        {
            fprintf(f, "dhcp-option=%s,option6:domain-search,%s\n",
                    ra->ra_ifname,
                    dnssl);
        }
    }

    /*
     * DHCPv6 Server
     */
    ds_tree_foreach(&dhcpv6_server_list, d6s)
    {
        fprintf(f, "interface=%s\n", d6s->d6s_ifname);

        /* Add prefixes */
        ds_tree_foreach(&d6s->d6s_prefixes, d6s_prefix)
        {
            char prfx_valid_timer[32] = {0};
            int  pf_vld_tmr = d6s_prefix->dp_prefix.d6s_prefix.ia6_valid_lft;

            if(pf_vld_tmr == -1)
                sprintf(prfx_valid_timer, ",%s", "infinite");
            else if(pf_vld_tmr > 0)
                sprintf(prfx_valid_timer, ",%d", pf_vld_tmr);

            fprintf(f, "dhcp-range=%s,::1,::FFFF:FFFF,constructor:%s%s\n",
                    d6s->d6s_ifname,
                    d6s->d6s_ifname,
                    prfx_valid_timer);
        }

        /* Write out options */
        for (tag = 0; tag < DHCP_OPTIONS_MAX; tag++)
        {
            if (d6s->d6s_options[tag] == NULL) continue;

            fprintf(f, "dhcp-option=%s,option6:%d,%s\n",
                    d6s->d6s_ifname,
                    tag,
                    dnsmasq6_server_option6_encode(tag, d6s->d6s_options[tag]));
        }

        /* Add static leases */
        ds_tree_foreach(&d6s->d6s_leases, d6s_lease)
        {
            fprintf(f, "dhcp-host=%s,uid:%s,["PRI_osn_ip6_addr"]\n",
                    d6s->d6s_ifname,
                    d6s_lease->dl_lease.d6s_duid,
                    FMT_osn_ip6_addr(d6s_lease->dl_lease.d6s_addr));

            if (d6s_lease->dl_lease.d6s_hostname[0] != '\0')
            {
                fprintf(f, ",%s", d6s_lease->dl_lease.d6s_hostname);
            }
            fprintf(f, "\n");
        }
    }

    retval = true;

error:
    if (f != NULL) fclose(f);

    if (retval)
    {
        if (!daemon_start(&dnsmasq6_server_daemon))
        {
            LOG(ERR, "dnsmasq: Error starting DNSMASQ process.");
            retval = false;
        }
    }

    return retval;
}

/*
 * Write the configuration file and restart dnsmasq
 *
 * Note: Always call this through dhcpv6_server_apply()
 */
void __dnsmasq6_server_apply(struct ev_loop *loop, struct ev_debounce *ev, int revent)
{
    (void)loop;
    (void)ev;
    (void)revent;

    daemon_stop(&dnsmasq6_server_daemon);

    if (!dnsmasq6_server_write_config())
    {
        return;
    }

    daemon_start(&dnsmasq6_server_daemon);
}

/*
 * Schedule a debounced global configuration apply
 */
bool dnsmasq6_server_apply(void)
{
    static bool dnsmasq_init = false;

    if (!dnsmasq_init)
    {
        /* Create intermediate folders if they do not exists */
        char *var_folders[] = { "/var/etc", "/var/run", "/var/run/dnsmasq", NULL };
        char **pvar;

        for (pvar = var_folders; *pvar != NULL; pvar++)
        {
            if (mkdir(*pvar, 0755) != 0 && errno != EEXIST)
            {
                LOG(CRIT, "dhcpv6_server: Unable to create folder %s", *pvar);
                return false;
            }
        }

        if (!daemon_init(&dnsmasq6_server_daemon, DNSMASQ_SERVER_PATH, DAEMON_LOG_ALL))
        {
            LOG(ERR, "dhcpv6_server: Unable to initialize global daemon object.");
            return false;
        }

        /* Set the PID file location -- necessary to kill stale instances */
        if (!daemon_pidfile_set(&dnsmasq6_server_daemon, DNSMASQ_PID_FILE, false))
        {
            LOG(WARN, "dhcpv6_server: Error setting the PID file path.");
        }

        if (!daemon_restart_set(&dnsmasq6_server_daemon, true, 3.0, 10))
        {
            LOG(WARN, "dhcpv6_server: Error enabling daemon auto-restart on global instance.");
        }

        daemon_arg_add(&dnsmasq6_server_daemon, "--keep-in-foreground");
        daemon_arg_add(&dnsmasq6_server_daemon, "-C", DNSMASQ_CONFIG_FILE);
        daemon_arg_add(&dnsmasq6_server_daemon, "-x", DNSMASQ_PID_FILE);

        ev_debounce_init(
                &dnsmasq6_server_debounce,
                __dnsmasq6_server_apply,
               DNSMASQ_SERVER_DEBOUNCE_TIME);

        dnsmasq_init = true;
    }

    ev_debounce_start(EV_DEFAULT, &dnsmasq6_server_debounce);

    return true;
}

/*
 * Lease file changed, debounce it
 */
void dhcpv6_server_lease_stat_fn(struct ev_loop *loop, ev_stat *w, int revent)
{
    (void)loop;
    (void)w;
    (void)revent;

    ev_debounce_start(EV_DEFAULT, &dhcpv6_server_lease_debounce);
}

/*
 * Update dhcpv6_server structures with lease data and issue notification callbacks
 */
void dhcpv6_server_lease_debounce_fn(struct ev_loop *loop, ev_debounce *w, int revent)
{
    (void)loop;
    (void)w;
    (void)revent;

    FILE *fl;
    char buf[1024];
    struct osn_dhcpv6_server *d6s;

    LOG(INFO, "dhcpv6_server: Lease file changed: %s. Updating all leases.", DNSMASQ_LEASE_FILE);

    /* Process the lease file */
    fl = fopen(DNSMASQ_LEASE_FILE, "r");
    if (fl == NULL)
    {
        LOG(DEBUG, "dhcpv6_server: Error opening lease file: %s", DNSMASQ_LEASE_FILE);
        goto error;
    }

    /*
     * Clear lease status of all interfaces
     */
    ds_tree_foreach(&dhcpv6_server_list, d6s)
    {
        if (d6s->d6s_status.d6st_leases != NULL)
        {
            free(d6s->d6s_status.d6st_leases);
        }

        d6s->d6s_status.d6st_leases = NULL;
        d6s->d6s_status.d6st_leases_len = 0;
    }

    /*
     * The dnsmasq v6 lease file looks like the one below:
     *
     * duid 00:01:00:01:24:87:7d:3b:1e:0f:97:a4:55:e2
     *
     * 1559548085 366646381 1111:2222:3333:4444::a36e:2cf7 * * "*" 00:01:00:01:24:87:7d:92:86:64:15:da:94:6d
     * |          |         |                              | |  |  |
     *  \ Lease time        |                              | |  |  |
     *            |         |                              | |  |  |
     *            \ IAID as Big Endian decimal number      | |  |  |
     *                      |                              | |  |  |
     *                      \ IPv6 address                 | |  |  |
     *                                                     \ Hostname or '*' if unknown
     *                                                       |  |  |
     *                                                       \ Fingerprint, not used on IPv6
     *                                                          |  |
     *                                                          \ Vendorclass, not used on IPv6
     *                                                             |
     *                                                             \  DUID or '*' if unknown
     */
    while (fgets(buf, sizeof(buf), fl) != NULL)
    {
        struct osn_dhcpv6_server *d6s;
        char *pbuf;
        char *ptime;
        char *piaid;
        char *pipv6;
        char *phostname;
        char *pduid;

        /* Skip the duid line */
        if (strncmp(buf, "duid", strlen("duid")) == 0)
        {
            continue;
        }

        pbuf = buf;

        /* Lease time */
        ptime = strsep(&pbuf, " ");
        /* IAID */
        piaid = strsep(&pbuf, " "); (void)piaid;
        /* IPv6 Address */
        pipv6 = strsep(&pbuf, " ");
        /* Hostname */
        phostname = strsep(&pbuf, " ");
        /* Skip fingerprint */
        strsep(&pbuf, " ");
        /* Skip vendorclass */
        strsep(&pbuf, " ");
        /* DUID */
        pduid = strsep(&pbuf, " ");

        osn_ip6_addr_t addr6;

        if (ptime == NULL ||
                piaid == NULL ||
                pipv6 == NULL ||
                phostname == NULL ||
                pduid == NULL)
        {
            continue;
        }

        /* Parse the IPv6 address */
        if (!osn_ip6_addr_from_str(&addr6, pipv6))
        {
            LOG(ERR, "dhcpv6_server: lease_file: Error parsing IPv6 address"PRI_osn_ip6_addr", skipping.",
                    FMT_osn_ip6_addr(addr6));
            continue;
        }

        /* Find closes matching DNSMASQ server instance */
        d6s = dhcpv6_server_find_by_prefix(&addr6);
        if (d6s == NULL)
        {
            LOG(WARN, "dhcpv6_server: lease_file: Stale IPv6 entry: "PRI_osn_ip6_addr,
                    FMT_osn_ip6_addr(addr6));
            continue;
        }

        /* Fill in the lease structure */
        struct osn_dhcpv6_server_lease lease;
        memset(&lease, 0, sizeof(lease));

        lease.d6s_addr = OSN_IP6_ADDR_INIT;

        lease.d6s_addr = addr6;
        lease.d6s_hwaddr = OSN_MAC_ADDR_INIT;
        lease.d6s_leased_time = strtoul(ptime, NULL, 0);

        if (STRSCPY(lease.d6s_hostname, phostname) < 0)
        {
            LOG(WARN, "dhcpv6_server: lease_file: Hostname too long: %s", phostname);
            continue;
        }

        if (STRSCPY(lease.d6s_duid, pduid) < 0)
        {
            LOG(WARN, "dhcpv6_server: lease_file: DUID too long: %s", pduid);
            continue;
        }

        dhcpv6_server_status_lease_add(&d6s->d6s_status, &lease);
    }

    /* Notify all servers of notification update */
    ds_tree_foreach(&dhcpv6_server_list, d6s)
    {
        if (d6s->d6s_notify_fn == NULL) continue;

        d6s->d6s_notify_fn(d6s, &d6s->d6s_status);
    }

error:
    if (fl != NULL) fclose(fl);
}

/* Binary compare first *N-bits* of data in a and b */
int bitcmp(const void *_a, const void *_b, size_t nbits)
{
    const uint8_t *a = _a;
    const uint8_t *b = _b;

    int ncmp;
    int rc;
    uint8_t mask;

    /* memcpy() up to the last 8-bit boundary */
    ncmp = nbits >> 3;
    rc = memcmp(a, b, ncmp);
    if (rc != 0) return rc;

    /* Now what's left is to compare what's left after the last 8-bit boundary */
    mask = 0xFF << (8 + (ncmp << 3) - nbits);
    return (a[ncmp] & mask) - (b[ncmp] & mask);
}

/*
 * Find DHCPv6 server instance that most closely matches @p addr
 */
struct osn_dhcpv6_server *dhcpv6_server_find_by_prefix(struct osn_ip6_addr *addr)
{
    struct osn_dhcpv6_server *d6s;
    struct osn_dhcpv6_server *rc;
    struct dhcpv6_server_prefix *prefix;
    int plen;

    rc = NULL;
    plen = 0;

    /*
     * This function can be optimized by using a tree lookup
     */
    ds_tree_foreach(&dhcpv6_server_list, d6s)
    {
        ds_tree_foreach(&d6s->d6s_prefixes, prefix)
        {
            /* Filter out lighter prefixes */
            if (prefix->dp_prefix.d6s_prefix.ia6_prefix < plen) continue;

            /* Binary compare first bits of the two addresses (up to prefix bits) */
            if (bitcmp(&prefix->dp_prefix.d6s_prefix.ia6_addr,
                        &addr->ia6_addr,
                        prefix->dp_prefix.d6s_prefix.ia6_prefix) != 0)
            {
                /* No match, continue to the next entry */
                continue;
            }

            /* Match, update */
            rc = d6s;
            plen = prefix->dp_prefix.d6s_prefix.ia6_prefix;
        }
    }

    return rc;
}

/*
 * Add lease to the list of currently active leases
 */
void dhcpv6_server_status_lease_add(
        struct osn_dhcpv6_server_status *status,
        struct osn_dhcpv6_server_lease *lease)
{
    /* Resize the leases array */
    if ((status->d6st_leases_len % DNSMASQ_REALLOC_ADD) == 0)
    {
        /* Reallocate buffer */
        status->d6st_leases = realloc(
                status->d6st_leases,
                (status->d6st_leases_len + DNSMASQ_REALLOC_ADD) * sizeof(struct osn_dhcpv6_server_lease));
    }

    status->d6st_leases[status->d6st_leases_len++] = *lease;
}

int dhcpv6_server_lease_cmp(void *_a, void *_b)
{
    struct osn_dhcpv6_server_lease *a = _a;
    struct osn_dhcpv6_server_lease *b = _b;
    int rc;

    rc = osn_ip6_addr_cmp(&a->d6s_addr, &b->d6s_addr);
    if (rc != 0) return rc;

    rc = strcmp(a->d6s_duid, b->d6s_duid);
    if (rc != 0) return rc;

    rc = strcmp(a->d6s_hostname, b->d6s_hostname);
    if (rc != 0) return rc;

    rc = memcmp(&a->d6s_hwaddr, &b->d6s_hwaddr, sizeof(a->d6s_hwaddr));
    if (rc != 0) return rc;

    return 0;
}
