/* SPDX-License-Identifier: BSD-3-Clause */

#include "netifd.h"
#include "inet_conf.h"
#include "dhcp_fingerprint.h"
#include "ovsdb_sync.h"
#include "json_util.h"

#include "ds_tree.h"
#include "synclist.h"


struct dhcp_lease_node
{
	struct osn_dhcp_server_lease    dl_lease;   /* Lease data */
	dhcp_fp_data_t                  dl_fp_data; /* Fingerprint decode results */
	bool                            dl_sync;    /* Present on synclist */
	bool                            dl_updated; /* Update pending */
	ds_tree_node_t                  dl_tnode;   /* tree node */
	synclist_node_t                 dl_snode;   /* synclist node */
};

void *dhcp_lease_synclist_data = NULL;

static synclist_fn_t osn_dhcp_server_lease_sync;
static ds_key_cmp_t osn_dhcp_server_lease_cmp;

static bool __netifd_dhcp_lease_notify(
		void *data,
		bool released,
		struct osn_dhcp_server_lease *dl,
		dhcp_fp_data_t *fp_data);

/* ds_tree comparator */
int osn_dhcp_server_lease_cmp(void *_a, void *_b)
{
	int rc;

	struct osn_dhcp_server_lease *a = _a;
	struct osn_dhcp_server_lease *b = _b;

	rc = osn_mac_addr_cmp(&a->dl_hwaddr, &b->dl_hwaddr);
	if (rc != 0) return rc;

	rc = osn_ip_addr_cmp(&a->dl_ipaddr, &b->dl_ipaddr);
	if (rc != 0) return rc;

	return 0;
}

/* Synclist comparator */
int osn_dhcp_server_lease_sync_cmp(void *_a, void *_b)
{
	struct osn_dhcp_server_lease *a = _a;
	struct osn_dhcp_server_lease *b = _b;

	return osn_mac_addr_cmp(&a->dl_hwaddr, &b->dl_hwaddr);
}

/* Synclist handler */
void *osn_dhcp_server_lease_sync(synclist_t *sync, void *_old, void *_new)
{
	(void)sync;
	struct dhcp_lease_node *pold = _old;
	struct dhcp_lease_node *pnew = _new;

	if (_old == NULL) {
		/* New element */
		pnew->dl_sync = true;

		/* Call the original lease handler */
		__netifd_dhcp_lease_notify(dhcp_lease_synclist_data, false, &pnew->dl_lease, &pnew->dl_fp_data);

		return _new;
	}
	else if (_new == NULL) {
		pold->dl_sync = false;

		/* Call the original lease handler */
		__netifd_dhcp_lease_notify(dhcp_lease_synclist_data, true, &pold->dl_lease, &pold->dl_fp_data);

		/* Element removal */
		return NULL;
	}

	/* Compare the two elements, if pnew is "fresher", replace it */
	if (pnew->dl_lease.dl_leasetime > pold->dl_lease.dl_leasetime) {
		return pnew;
	}

	/*
	 * Comapre hostname, vendorclass and fingerprint, issue an update if they
	 * differ
	 */
	if (pold->dl_updated) {
		pold->dl_updated = false;
		__netifd_dhcp_lease_notify(dhcp_lease_synclist_data, false, &pold->dl_lease, &pold->dl_fp_data);
	}

	return pold;
}

/*
 * Global list of DHCP leases, indexed by MAC+IPADDR
 */
static ds_tree_t dhcp_lease_list = DS_TREE_INIT(osn_dhcp_server_lease_cmp, struct dhcp_lease_node, dl_tnode);

static synclist_t dhcp_lease_synclist = SYNCLIST_INIT(
		osn_dhcp_server_lease_sync_cmp,
		struct dhcp_lease_node,
		dl_snode,
		osn_dhcp_server_lease_sync);

/*
 * Hijack the netifd_dhcp_lease_notify function to handle unique macs properly.
 */
bool netifd_dhcp_lease_notify(
		void *data,
		bool released,
		struct osn_dhcp_server_lease *dl)
{
	struct dhcp_lease_node *node;

	node = ds_tree_find(&dhcp_lease_list, dl);

	/*
	 * Update the global DHCP lease list first
	 */
	if (released) {
		if (node == NULL) {
			LOG(ERR, "dhcp_lease: Error removing non-existent lease: "PRI_osn_mac_addr":"PRI_osn_ip_addr,
					FMT_osn_mac_addr(dl->dl_hwaddr),
					FMT_osn_ip_addr(dl->dl_ipaddr));
			return true;
		}

		ds_tree_remove(&dhcp_lease_list, node);

		/* If an element exists on the synclist as well, remove it from there too */
		if (node->dl_sync) {
			synclist_del(&dhcp_lease_synclist, node);
		}

		free(node);
	}
	else if (node == NULL) {
		node = calloc(1, sizeof(struct dhcp_lease_node));
		node->dl_lease = *dl;
		dhcp_fp_db_lookup(&node->dl_fp_data, dl->dl_fingerprint);
		ds_tree_insert(&dhcp_lease_list, node, &node->dl_lease);
	}
	else {
		node->dl_lease = *dl;
		dhcp_fp_db_lookup(&node->dl_fp_data, dl->dl_fingerprint);
		node->dl_updated = true;
	}

	/*
	 * Now traverse the global lease list and collapse it to a synclist where
	 * the index is only the MAC address
	 */
	dhcp_lease_synclist_data = data;

	synclist_begin(&dhcp_lease_synclist);
	ds_tree_foreach(&dhcp_lease_list, node)
	{
		synclist_add(&dhcp_lease_synclist, node);
	}
	synclist_end(&dhcp_lease_synclist);

	return true;
}

bool __netifd_dhcp_lease_notify(
		void *data,
		bool released,
		struct osn_dhcp_server_lease *dl,
		dhcp_fp_data_t *fp_data)

{
	(void)data;

	bool ip_is_any = (osn_ip_addr_cmp(&dl->dl_ipaddr, &OSN_IP_ADDR_INIT) == 0);

	LOG(INFO, "dhcp_lease: %s DHCP lease: MAC:"PRI_osn_mac_addr" IP:"PRI_osn_ip_addr" Hostname:%s Time:%d%s",
			released ? "Released" : "Acquired",
					FMT_osn_mac_addr(dl->dl_hwaddr),
					FMT_osn_ip_addr(dl->dl_ipaddr),
					dl->dl_hostname,
					(int)dl->dl_leasetime,
					ip_is_any ? ", skipping" : "");

	if (ip_is_any) return true;

	/* Fill in the schema structure */
	struct schema_DHCP_leased_IP sdl;

	memset(&sdl, 0, sizeof(sdl));

	sdl.hwaddr_exists = true;
	snprintf(sdl.hwaddr, sizeof(sdl.hwaddr), PRI_osn_mac_addr, FMT_osn_mac_addr(dl->dl_hwaddr));

	sdl.inet_addr_exists = true;
	snprintf(sdl.inet_addr, sizeof(sdl.inet_addr), PRI_osn_ip_addr, FMT_osn_ip_addr(dl->dl_ipaddr));

	sdl.hostname_exists = true;
	strscpy(sdl.hostname, dl->dl_hostname, sizeof(sdl.hostname));

	sdl.fingerprint_exists = true;
	strscpy(sdl.fingerprint, dl->dl_fingerprint, sizeof(sdl.fingerprint));

	sdl.vendor_class_exists = true;
	strscpy(sdl.vendor_class, dl->dl_vendorclass, sizeof(sdl.vendor_class));

	sdl.subnet_mask_exists = true;
	snprintf(sdl.subnet_mask, sizeof(sdl.subnet_mask), PRI_osn_ip_addr, FMT_osn_ip_addr(dl->dl_subnetmask));

	sdl.gateway_exists = true;
	snprintf(sdl.gateway, sizeof(sdl.gateway), PRI_osn_ip_addr, FMT_osn_ip_addr(dl->dl_gateway));

	sdl.dhcp_server_exists = true;
	snprintf(sdl.dhcp_server, sizeof(sdl.dhcp_server), PRI_osn_ip_addr, FMT_osn_ip_addr(dl->dl_dhcpserver));

	sdl.primary_dns_exists = true;
	snprintf(sdl.primary_dns, sizeof(sdl.primary_dns), PRI_osn_ip_addr, FMT_osn_ip_addr(dl->dl_primarydns));

	sdl.secondary_dns_exists = true;
	snprintf(sdl.secondary_dns, sizeof(sdl.secondary_dns), PRI_osn_ip_addr, FMT_osn_ip_addr(dl->dl_secondarydns));

	sdl.db_status_exists = true;
	sdl.db_status = fp_data->db_status;

	sdl.device_name_exists = true;
	strscpy(sdl.device_name, fp_data->device_name, sizeof(sdl.device_name));

	sdl.device_type_exists = true;
	sdl.device_type = fp_data->device_type;

	sdl.manuf_id_exists = true;
	sdl.manuf_id = fp_data->manuf_id;

	/* A lease time of 0 indicates that this entry should be deleted */
	sdl.lease_time_exists = true;
	if (released) {
		sdl.lease_time = 0;
	}
	else {
		sdl.lease_time = (int)dl->dl_leasetime;
		/* OLPS-153: sdl.lease_time should never be 0 on first insert! */
		if (sdl.lease_time == 0) sdl.lease_time = -1;
	}

	if (!netifd_dhcp_table_update(&sdl)) {
		LOG(WARN, "dhcp_lease: Error processing DCHP lease entry "PRI_osn_mac_addr" ("PRI_osn_ip_addr"), %s)",
				FMT_osn_mac_addr(dl->dl_hwaddr),
				FMT_osn_ip_addr(dl->dl_ipaddr),
				dl->dl_hostname);
	}

	return true;
}

bool netifd_dhcp_table_update(struct schema_DHCP_leased_IP *dlip)
{
	pjs_errmsg_t        perr;
	json_t             *where, *row, *cond;
	bool                ret;

	LOGT("dhcp_lease: Updating DHCP lease '%s'", dlip->hwaddr);

	/* OVSDB transaction where multi condition */
	where = json_array();

	cond = ovsdb_tran_cond_single("hwaddr", OFUNC_EQ, str_tolower(dlip->hwaddr));
	json_array_append_new(where, cond);

#if !defined(WAR_LEASE_UNIQUE_MAC)
	/*
	 * Additionally, the DHCP sniffing code may pick up random DHCP updates
	 * from across the network. It is imperative that there's always only 1 MAC
	 * present in the table.
	 */
	cond = ovsdb_tran_cond_single("inet_addr", OFUNC_EQ, dlip->inet_addr);
	json_array_append_new(where, cond);
#endif

	if (dlip->lease_time == 0) {
		// Released or expired lease... remove from OVSDB
		ret = ovsdb_sync_delete_where(SCHEMA_TABLE(DHCP_leased_IP), where);
		if (!ret) {
			LOGE("dhcp_lease: Updating DHCP lease %s, %s (Failed to remove entry)", dlip->inet_addr, dlip->hwaddr);
			return false;
		}

		LOGN("dhcp_lease: Removed DHCP lease '%s' with '%s' '%s' '%d'",
				dlip->hwaddr,
				dlip->inet_addr,
				dlip->hostname,
				dlip->lease_time);
	}
	else
	{
		// New/active lease, upsert it into OVSDB
		row = schema_DHCP_leased_IP_to_json(dlip, perr);
		if (!ovsdb_sync_upsert_where(SCHEMA_TABLE(DHCP_leased_IP), where, row, NULL)) {
			LOGE("Updating DHCP lease %s (Failed to insert entry)", dlip->hwaddr);
			return false;
		}

		LOGN("Updated DHCP lease '%s' with '%s' '%s' '%d'",
				dlip->hwaddr,
				dlip->inet_addr,
				dlip->hostname,
				dlip->lease_time);
	}

	return true;
}
