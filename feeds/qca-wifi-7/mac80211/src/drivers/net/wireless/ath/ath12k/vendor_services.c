// SPDX-License-Identifier: BSD-3-Clause-Clear
/*
 *  Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */
#include "core.h"
#include "vendor_services.h"
#include "telemetry_agent_if.h"
#include "debug.h"

#define AGENT_NOTIFY_EVENT_INIT	0
#define AGENT_NOTIFY_EVENT_DEINIT 1

static struct ath12k_vendor_service_info vendor_info;
static bool resources_created;

/* These macros directly access the fields of the provided vendor_info structure.
 * This is necessary for functions like ath12k_telemetry_destroy_resources that
 * are called during module unload and need direct access to the wiphy/wdev pointers.
 */
#define GET_WIPHY(vendor_info) ((vendor_info).wiphy)
#define GET_WDEV(vendor_info) ((vendor_info).wdev)

static struct wiphy *ath12k_vendor_get_wiphy(void)
{
	struct wiphy *wiphy;

	spin_lock(&vendor_info.vendor_lock);
	wiphy = vendor_info.wiphy;
	spin_unlock(&vendor_info.vendor_lock);

	return wiphy;
}

struct wireless_dev *ath12k_vendor_get_wdev(void)
{
	struct wireless_dev *wdev;

	spin_lock(&vendor_info.vendor_lock);
	wdev = vendor_info.wdev;
	spin_unlock(&vendor_info.vendor_lock);

	return wdev;
}

struct ath12k_vendor_work {
	struct work_struct work;
	struct ath12k_vendor_service_info *vendor_info;
};

static int
(*ath12k_vendor_service_init[ATH12K_RM_MAX_SERVICE])(
	struct ath12k_hw *ah,
	struct ath12k_vendor_service_info *info
);

static int
(*ath12k_vendor_service_deinit[ATH12K_RM_MAX_SERVICE])(
	struct ath12k_hw *ah,
	struct ath12k_vendor_service_info *info
);

/* Helper functions for thread-safe vendor_info operations */
static void ath12k_vendor_set_init_done(bool done)
{
	spin_lock(&vendor_info.vendor_lock);
	vendor_info.is_vendor_init_done = done;
	spin_unlock(&vendor_info.vendor_lock);
}

static bool ath12k_vendor_get_init_done(void)
{
	bool done;

	spin_lock(&vendor_info.vendor_lock);
	done = vendor_info.is_vendor_init_done;
	spin_unlock(&vendor_info.vendor_lock);

	return done;
}

static void ath12k_vendor_set_service_enabled(u8 service_id, bool enabled)
{
	if (service_id >= ATH12K_RM_MAX_SERVICE) {
		ath12k_err(NULL, "service_id is out of range %d\n", service_id);
		return;
	}

	spin_lock(&vendor_info.vendor_lock);
	vendor_info.service_enabled[service_id] = enabled;
	spin_unlock(&vendor_info.vendor_lock);
}

static void ath12k_vendor_set_dynamic_service_bit(u8 service_id, bool set)
{
	if (service_id >= ATH12K_RM_MAX_SERVICE) {
		ath12k_err(NULL, "service_id is out of range %d\n", service_id);
		return;
	}

	spin_lock(&vendor_info.vendor_lock);
	if (set)
		set_bit(service_id, &vendor_info.dynamic_svc_bitmask);
	else
		clear_bit(service_id, &vendor_info.dynamic_svc_bitmask);
	spin_unlock(&vendor_info.vendor_lock);
}

static void ath12k_vendor_update_service_state(u8 service_id, bool enabled,
					       bool init_done, bool is_dynamic,
					       bool set_dynamic_bit)
{
	if (service_id >= ATH12K_RM_MAX_SERVICE) {
		ath12k_err(NULL, "service_id is out of range %d\n", service_id);
		return;
	}

	spin_lock(&vendor_info.vendor_lock);
	vendor_info.service_enabled[service_id] = enabled;
	vendor_info.is_vendor_init_done = init_done;

	if (is_dynamic) {
		if (set_dynamic_bit)
			set_bit(service_id, &vendor_info.dynamic_svc_bitmask);
		else
			clear_bit(service_id, &vendor_info.dynamic_svc_bitmask);
	}
	spin_unlock(&vendor_info.vendor_lock);
}

static void ath12k_vendor_check_and_set_init_done(u8 service_id, bool *should_queue_work)
{
	spin_lock(&vendor_info.vendor_lock);
	if (service_id == ATH12K_RM_MAIN_SERVICE ||
	    test_bit(service_id, &vendor_info.dynamic_svc_bitmask)) {
		vendor_info.is_vendor_init_done = true;
		*should_queue_work = true;
	}
	spin_unlock(&vendor_info.vendor_lock);
}

static int ath12k_vendor_service_common(struct ath12k_hw *ah,
					struct ath12k_vendor_service_info *info,
					const u8 event_type, bool enable)
{
	if (ath12k_telemetry_notify_vendor_app_event(event_type, info->id,
						     info->service_data))
		return -EINVAL;

	if (event_type == AGENT_NOTIFY_EVENT_DEINIT)
		ath12k_vendor_update_service_state(info->id, enable, false,
						   false, false);
	else
		ath12k_vendor_set_service_enabled(info->id, enable);

	ath12k_dbg(NULL, ATH12K_DBG_RM,
		   "vendor svc %s id: %d Enable: %d\n",
		   (event_type == AGENT_NOTIFY_EVENT_INIT) ? "init" : "deinit",
		   info->id, enable);

	return 0;
}

static int ath12k_vendor_service_common_init(struct ath12k_hw *ah,
					     struct ath12k_vendor_service_info *info)
{
	if (ath12k_vendor_is_service_enabled(info->id) &&
	    ath12k_vendor_service_common(ah, info, AGENT_NOTIFY_EVENT_DEINIT, false))
		return -EINVAL;

	return ath12k_vendor_service_common(ah, info, AGENT_NOTIFY_EVENT_INIT,
					    true);
}

static int ath12k_vendor_service_common_deinit(struct ath12k_hw *ah,
					       struct ath12k_vendor_service_info *info)
{
	if (!ath12k_vendor_is_service_enabled(info->id))
		return 0;

	return ath12k_vendor_service_common(ah, info,
					    AGENT_NOTIFY_EVENT_DEINIT, false);
}

void ath12k_vendor_create_resources(struct ath12k_vendor_service_info *vendor_info,
				    const u8 id)
{
	struct wiphy *wiphy = GET_WIPHY(*vendor_info);
	struct ieee80211_hw *hw = wiphy_to_ieee80211_hw(wiphy);
	struct ath12k_hw *ah = hw->priv;
	struct ath12k_hw_group *ag = ah->radio->ab->ag;

	if (id >= ATH12K_RM_MAX_SERVICE)
		return;

	/* Only create resources if they haven't been created yet */
	if (!resources_created) {
		ath12k_telemetry_create_resources(ag);
		resources_created = true;
		ath12k_dbg(NULL, ATH12K_DBG_RM,
			   "Created ta resources using service id : %d\n", id);
	} else {
		ath12k_dbg(NULL, ATH12K_DBG_RM,
			   "Resources already created, skipping for service id : %d\n",
			   id);
	}
}

void ath12k_vendor_destroy_resources(struct ath12k_vendor_service_info *vendor_info,
				     const u8 id)
{
	struct wiphy *wiphy = GET_WIPHY(*vendor_info);
	struct ieee80211_hw *hw = wiphy_to_ieee80211_hw(wiphy);
	struct ath12k_hw *ah = hw->priv;
	struct ath12k_hw_group *ag = ah->radio->ab->ag;

	if (id >= ATH12K_RM_MAX_SERVICE)
		return;

	/* Only destroy resources if they have been created */
	if (resources_created) {
		ath12k_telemetry_destroy_resources(ag);
		resources_created = false;
		ath12k_dbg(NULL, ATH12K_DBG_RM,
			   "Destroyed telemetry resources using id: %d\n", id);
	} else {
		ath12k_dbg(NULL, ATH12K_DBG_RM,
			   "Resources not created or already destroyed, skipping for service id : %d\n",
			   id);
	}
}

static int ath12k_vendor_main_service_deinit(struct ath12k_hw *ah,
					     struct ath12k_vendor_service_info *info)
{
	struct ath12k_vendor_service_info tmp_info = {0};
	int id;

	if (!info)
		return -EINVAL;

	ath12k_vendor_set_init_done(false);

	for (id = 0; id < ATH12K_RM_MAX_SERVICE; id++) {
		tmp_info.id = id;
		if (ath12k_vendor_service_common(ah, &tmp_info,
						 AGENT_NOTIFY_EVENT_DEINIT,
						 false))
			ath12k_err(NULL,
				    "vendor service: %d failed to deinit", id);
	}

	return 0;
}

static int ath12k_vendor_main_service_init(struct ath12k_hw *ah,
					   struct ath12k_vendor_service_info *info)
{
	bool is_init_done;

	if (!info)
		return -EINVAL;

	is_init_done = ath12k_vendor_get_init_done();

	ath12k_dbg(NULL, ATH12K_DBG_RM,
		   "main svc init started for id: %d Enable: %d\n",
		   info->id, vendor_info.service_enabled[info->id]);

	if (ath12k_telemetry_is_agent_loaded())
		ath12k_vendor_destroy_resources(&vendor_info, info->id);

	if (is_init_done) {
		if (ath12k_vendor_service_common(ah, info,
						 AGENT_NOTIFY_EVENT_DEINIT,
						 false))
			return -EINVAL;

		ath12k_vendor_set_init_done(false);
	}

	ath12k_vendor_create_resources(&vendor_info, info->id);
	if (ath12k_vendor_service_common(ah, info,
					 AGENT_NOTIFY_EVENT_INIT, true))
		return -EINVAL;

	return 0;
}

static int ath12k_vendor_dynamic_service_init(struct ath12k_hw *ah,
					      struct ath12k_vendor_service_info *info)
{
	if (ath12k_telemetry_is_agent_loaded())
		ath12k_vendor_destroy_resources(&vendor_info, info->id);

	ath12k_vendor_set_dynamic_service_bit(info->id, true);

	ath12k_vendor_create_resources(&vendor_info, info->id);

	if (ath12k_telemetry_dynamic_app_init_deinit_notify(AGENT_NOTIFY_EVENT_INIT,
							    info->id,
							    info->service_data,
							    info->is_container_app))
		return -EINVAL;

	ath12k_vendor_set_service_enabled(info->id, true);

	ath12k_dbg(NULL, ATH12K_DBG_RM, "Dynamic Init service ID: %d Enabled: %d\n",
		   info->id, true);
	return 0;
}

static int ath12k_vendor_dynamic_service_deinit(struct ath12k_hw *ah,
						struct ath12k_vendor_service_info *info)
{
	ath12k_vendor_set_dynamic_service_bit(info->id, false);

	if (ath12k_telemetry_dynamic_app_init_deinit_notify(AGENT_NOTIFY_EVENT_DEINIT,
							    info->id,
							    info->service_data,
							    info->is_container_app))
		return -EINVAL;

	ath12k_vendor_update_service_state(info->id, false, false, false, false);

	ath12k_dbg(NULL, ATH12K_DBG_RM, "Dynamic De-Init service ID: %d Enabled: %d\n",
		   info->id, false);
	return 0;
}

static int
ath12k_vendor_send_init_response(const u8 service_id,
				 const int vendor_resp_len,
				 const struct ath12k_vendor_soc_device_info *soc_info,
				 const struct ath12k_vendor_link_info *link_info)
{
	struct wiphy *wiphy = GET_WIPHY(vendor_info);
	u8 category = QCA_WLAN_VENDOR_ATTR_GENERIC_CATEGORY_APP_INIT;
	struct sk_buff *vendor_event;
	struct nlattr *nl_soc_info = NULL, *nl_per_soc_info = NULL;
	struct nlattr *nl_link_info = NULL;
	struct nlattr *nl_per_link_info = NULL;

	if (!wiphy) {
		ath12k_err(NULL, "vendor: wireless phy or device doesn't exist");
		return -1;
	}

	vendor_event =
		cfg80211_vendor_event_alloc(wiphy, NULL, vendor_resp_len,
					    QCA_NL80211_VENDOR_SUBCMD_RM_GENERIC_INDEX,
					    GFP_ATOMIC);
	if (!vendor_event) {
		ath12k_err(NULL, "vendor: SKB alloc failed for 6 GHz power mode evt\n");
		return -1;
	}

	ath12k_dbg(NULL, ATH12K_DBG_RM,
		   "Build Generic App init attributes for link info entry");
	if (nla_put_u8(vendor_event,
		       QCA_WLAN_VENDOR_ATTR_RM_GENERIC_CATEGORY, category)) {
		ath12k_err(NULL, "vendor: Vendor Attr RM generic category put failed");
		goto error;
	}
	if (nla_put_u8(vendor_event,
		       QCA_WLAN_VENDOR_ATTR_RM_GENERIC_SERVICE_ID, service_id)) {
		ath12k_err(NULL, "vendor: Vendor Attr RM generic service id put failed");
		goto error;
	}
	if (nla_put_u8(vendor_event,
		       QCA_WLAN_VENDOR_ATTR_RM_GENERIC_DYNAMIC_INIT_CONF,
		       vendor_info.init_config_type)) {
		ath12k_err(NULL, "vendor: Vendor Attr RM generic dynamic init conf put failed");
		goto error;
	}
	ath12k_dbg(NULL, ATH12K_DBG_RM,
		   "Send Vendor Generic Response for Category: %d service_id: %d\n",
		   category, service_id);
	if (nla_put_u8(vendor_event,
		       QCA_WLAN_VENDOR_ATTR_RM_GENERIC_APP_VERSION,
		       vendor_info.app_info.app_version)){
		ath12k_err(NULL, " Fails to put QCA_WLAN_VENDOR_ATTR_RM_GENERIC_APP_VERSION");
		goto error;
	}
	if (nla_put_u8(vendor_event,
		       QCA_WLAN_VENDOR_ATTR_RM_GENERIC_DRIVER_VERSION,
		       vendor_info.app_info.driver_version)){
		ath12k_err(NULL, "Fails to put QCA_WLAN_VENDOR_ATTR_RM_GENERIC_APP_VERSION");
		goto error;
	}

	nl_soc_info = nla_nest_start(vendor_event,
				     QCA_WLAN_VENDOR_ATTR_RM_GENERIC_SOC_DEVICE_INFO);
	if (!nl_soc_info) {
		ath12k_err(NULL, "Fails to put QCA_WLAN_VENDOR_ATTR_RM_GENERIC_SOC_DEVICE_INFO");
		goto error;
	}

	nl_per_soc_info = nla_nest_start(vendor_event, soc_info->soc_id);
	if (!nl_per_soc_info) {
		ath12k_err(NULL, "Fails to put per soc info for soc:%d",
			   soc_info->soc_id);
		goto error;
	}
	ATH12K_VENDOR_PUT(vendor_event, u8,
			  QCA_WLAN_VENDOR_ATTR_SOC_DEVICE_SOC_ID,
			  soc_info->soc_id);

	nl_link_info = nla_nest_start(vendor_event,
				      QCA_WLAN_VENDOR_ATTR_SOC_DEVICE_LINK_INFO);
	if (!nl_link_info) {
		ath12k_err(NULL, "Fails to put soc device link info soc:%d",
			   soc_info->soc_id);
		goto error;
	}

	nl_per_link_info = nla_nest_start(vendor_event, link_info->hw_link_id);
	if (!nl_per_link_info) {
		ath12k_err(NULL, "failed to put soc per link info soc:%d",
			   soc_info->soc_id);
		goto error;
	}

	ATH12K_VENDOR_PUT(vendor_event, u16,
			  QCA_WLAN_VENDOR_ATTR_LINK_INFO_HW_LINK_ID,
			  link_info->hw_link_id);
	if (nla_put(vendor_event,
		    QCA_WLAN_VENDOR_ATTR_LINK_MAC,
		    6, (void *)link_info->link_mac_addr)) {
		ath12k_err(NULL, "Fails to put mac addr for hw link id :%d soc:%d",
			   link_info->hw_link_id, soc_info->soc_id);
		goto error;
	}

	ATH12K_VENDOR_PUT(vendor_event, u8, QCA_WLAN_VENDOR_ATTR_LINK_CHAN_BW,
			  ath12k_nl_chan_bw_to_qca_vendor_chan_bw(link_info->chan_bw));
	ATH12K_VENDOR_PUT(vendor_event, u16, QCA_WLAN_VENDOR_ATTR_LINK_CHAN_FREQ,
			  link_info->chan_freq);
	ATH12K_VENDOR_PUT(vendor_event, u8, QCA_WLAN_VENDOR_ATTR_LINK_TX_CHAIN_MASK,
			  link_info->tx_chain_mask);
	ATH12K_VENDOR_PUT(vendor_event, u8, QCA_WLAN_VENDOR_ATTR_LINK_RX_CHAIN_MASK,
			  link_info->rx_chain_mask);
	nla_nest_end(vendor_event, nl_per_link_info);
	nla_nest_end(vendor_event, nl_link_info);

	ATH12K_VENDOR_PUT(vendor_event, u8,
			  QCA_WLAN_VENDOR_ATTR_SOC_DEVICE_NUM_LINKS, 1);
	nla_nest_end(vendor_event, nl_per_soc_info);
	nla_nest_end(vendor_event, nl_soc_info);
	ATH12K_VENDOR_PUT(vendor_event, u8,
			  QCA_WLAN_VENDOR_ATTR_RM_GENERIC_NUM_SOC_DEVICES, 1);

	cfg80211_vendor_event(vendor_event, GFP_ATOMIC);
	return 0;
error:
	kfree_skb(vendor_event);
	return -1;
}

static int
ath12k_vendor_send_link_info(struct ath12k_vendor_soc_device_info *soc_info,
			     struct ath12k_vendor_link_info *link_info,
			     u8 service_id)
{
	int vendor_data_len =
		nla_total_size(sizeof(struct ath12k_vendor_soc_device_info));

	return ath12k_vendor_send_init_response(service_id, vendor_data_len,
						soc_info, link_info);
}

static void ath12k_vendor_link_initialize_process(struct work_struct *work)
{
	struct ath12k_vendor_work *vendor_work =
		container_of(work, struct ath12k_vendor_work, work);
	struct ath12k_vendor_service_info *vendor_info = vendor_work->vendor_info;
	struct ath12k_vendor_soc_device_info *soc_info, *tmp;
	struct ath12k_vendor_link_info *link_info;
	int ret, id;
	bool service_sent = false;

	spin_lock(&vendor_info->vendor_lock);
	list_for_each_entry_safe(soc_info, tmp, &vendor_info->soc_list, list) {
		link_info = &soc_info->link_info[0];
		/* Send to all enabled services */
		service_sent = false;
		/* First check if main service is enabled */
		if (vendor_info->service_enabled[ATH12K_RM_MAIN_SERVICE]) {
			ret = ath12k_vendor_send_link_info(soc_info, link_info,
							   ATH12K_RM_MAIN_SERVICE);
			if (ret) {
				ath12k_err(NULL, "Failed to send link info to main service due to NL issue for soc: %d link: %d",
					   soc_info->soc_id, link_info->hw_link_id);
			} else {
				ath12k_dbg(NULL, ATH12K_DBG_RM,
					   "Sent link info for soc: %d hw link id: %d to main service",
					   soc_info->soc_id, link_info->hw_link_id);
				service_sent = true;
			}
		}

		 /* TODO: Currently, any async event notifies to userspace via
		  * vendor NL path and to notify about link state changes,
		  * leverage the generic NL path to notify userspace, this will
		  * avoid having this async workqueue to handle such scenarios
		  * this requires changes to all the stack
		  * For now, use below method to notify to all enabled
		  * services by checking for any dynamic services
		  */
		for (id = 0; id < ATH12K_RM_MAX_SERVICE; id++) {
			if (id != ATH12K_RM_MAIN_SERVICE &&
			    test_bit(id, &vendor_info->dynamic_svc_bitmask) &&
			    vendor_info->service_enabled[id]) {
				ret = ath12k_vendor_send_link_info(soc_info,
								   link_info, id);
				if (ret) {
					ath12k_err(NULL, "Failed to send link info to service %d due to NL issue for soc: %d link: %d",
						   id, soc_info->soc_id,
						   link_info->hw_link_id);
				} else {
					ath12k_dbg(NULL, ATH12K_DBG_RM,
						   "Sent link info for soc: %d hw link id: %d to service %d",
						   soc_info->soc_id,
						   link_info->hw_link_id, id);
					service_sent = true;
				}
			}
		}

		if (!service_sent)
			ath12k_dbg(NULL, ATH12K_DBG_RM,
				   "No enabled services found for link info for soc: %d hw link id: %d",
				   soc_info->soc_id, link_info->hw_link_id);

		list_del(&soc_info->list);
		kfree(soc_info);
	}
	spin_unlock(&vendor_info->vendor_lock);

	kfree(vendor_work);
}

static bool ath12k_vendor_check_work_queue_ready(void)
{
	bool ready;

	spin_lock(&vendor_info.vendor_lock);
	ready = vendor_info.is_vendor_init_done && vendor_info.wq;
	spin_unlock(&vendor_info.vendor_lock);

	return ready;
}

static int ath12k_vendor_queue_vendor_work(void)
{
	struct ath12k_vendor_work *vendor_work;
	bool queued;
	struct workqueue_struct *wq;

	if (!ath12k_vendor_check_work_queue_ready()) {
		ath12k_err(NULL, "failed to schedule vendor work as workqueue not initialized");
		return -EINVAL;
	}

	vendor_work = kmalloc(sizeof(*vendor_work), GFP_ATOMIC);
	if (!vendor_work)
		return -ENOMEM;

	INIT_WORK(&vendor_work->work, ath12k_vendor_link_initialize_process);
	vendor_work->vendor_info = &vendor_info;

	/* Get workqueue reference without holding lock to avoid deadlock
	 * since the work function also acquires the same lock
	 */
	spin_lock(&vendor_info.vendor_lock);
	wq = vendor_info.wq;
	spin_unlock(&vendor_info.vendor_lock);

	queued = queue_work(wq, &vendor_work->work);

	if (!queued) {
		kfree(vendor_work);
		ath12k_dbg(NULL, ATH12K_DBG_RM, "Work already queued, skipping duplicate");
		return -EBUSY;
	}

	return 0;
}

void ath12k_vendor_services_init(void)
{
	if (!ath12k_mlo_capable)
		return;

	memset(&vendor_info, 0, sizeof(struct ath12k_vendor_service_info));
	resources_created = false;

	INIT_LIST_HEAD(&vendor_info.soc_list);
	spin_lock_init(&vendor_info.vendor_lock);

	vendor_info.wq = create_singlethread_workqueue("ath12k_vendor_wq");
	if (!vendor_info.wq) {
		ath12k_err(NULL, "vendor: failed to create vendor_wq for link processing");
		return;
	}

	/* Initialize the service function array */
	ath12k_vendor_service_init[ATH12K_RM_MAIN_SERVICE] =
		ath12k_vendor_main_service_init;
	ath12k_vendor_service_deinit[ATH12K_RM_MAIN_SERVICE] =
		ath12k_vendor_main_service_deinit;
	ath12k_vendor_service_init[ATH12K_VENDOR_APP_ENERGY_SERVICE] =
		ath12k_vendor_dynamic_service_init;
	ath12k_vendor_service_deinit[ATH12K_VENDOR_APP_ENERGY_SERVICE] =
		ath12k_vendor_dynamic_service_deinit;
	ath12k_vendor_service_init[ATH12K_VENDOR_APP_ERP_SERVICE] =
		ath12k_vendor_service_common_init;
	ath12k_vendor_service_deinit[ATH12K_VENDOR_APP_ERP_SERVICE] =
		ath12k_vendor_service_common_deinit;
	ath12k_vendor_service_init[ATH12K_VENDOR_APP_QOS_OPTIMIZER] =
		ath12k_vendor_dynamic_service_init;
	ath12k_vendor_service_deinit[ATH12K_VENDOR_APP_QOS_OPTIMIZER] =
		ath12k_vendor_dynamic_service_deinit;
	/* Initialize other serives as needed */
}
EXPORT_SYMBOL(ath12k_vendor_services_init);

void ath12k_vendor_services_deinit(void)
{
	/* Clean up resources if needed */
	struct ath12k_vendor_soc_device_info *soc_info, *tmp;
	struct ath12k_vendor_service_info info = {0};
	int id;

	if (!ath12k_mlo_capable)
		return;

	info.id = ATH12K_RM_MAIN_SERVICE;
	if (ath12k_vendor_service_deinit[info.id] &&
	    ath12k_vendor_get_init_done())
		ath12k_vendor_service_deinit[info.id](NULL, &info);

	for (id = 0; id < ATH12K_RM_MAX_SERVICE; id++) {
		ath12k_vendor_service_init[id] = NULL;
		ath12k_vendor_service_deinit[id] = NULL;
	}

	flush_workqueue(vendor_info.wq);
	destroy_workqueue(vendor_info.wq);
	list_for_each_entry_safe(soc_info, tmp, &vendor_info.soc_list, list) {
		list_del(&soc_info->list);
		kfree(soc_info);
	}
	memset(&vendor_info, 0, sizeof(struct ath12k_vendor_service_info));
}
EXPORT_SYMBOL(ath12k_vendor_services_deinit);

bool ath12k_vendor_is_service_enabled(const u8 svc_id)
{
	if (svc_id >= ATH12K_RM_MAX_SERVICE)
		return false;

	return vendor_info.service_enabled[svc_id] ? true : false;
}

static int ath12k_vendor_set_wireless_references(struct wiphy *wiphy,
					     struct wireless_dev *wdev)
{
	spin_lock(&vendor_info.vendor_lock);
	vendor_info.wiphy = wiphy;
	vendor_info.wdev = wdev;
	spin_unlock(&vendor_info.vendor_lock);

	return 0;
}

int ath12k_vendor_initialize_service(struct wiphy *wiphy,
				 struct wireless_dev *wdev,
				 struct ath12k_vendor_service_info *info)
{
	int ret;

	if (!info)
		return -EINVAL;

	if (info->id >= ATH12K_RM_MAX_SERVICE) {
		ath12k_err(NULL, "Invalid sevice id received: %d\n", info->id);
		return -EINVAL;
	}

	if (!ath12k_vendor_service_init[info->id] ||
	    !ath12k_vendor_service_deinit[info->id]) {
		ath12k_dbg(NULL, ATH12K_DBG_RM,
			   "Service id : %d not supported now\n", info->id);
		return -EOPNOTSUPP;
	}

	ath12k_vendor_set_wireless_references(wiphy, wdev);

	switch (info->init_config_type) {
	case QCA_WLAN_VENDOR_DYNAMIC_INIT_CONF_RM_APP_START:
	case QCA_WLAN_VENDOR_DYNAMIC_INIT_CONF_SERVICE_START:
	case QCA_WLAN_VENDOR_DYNAMIC_INIT_CONF_CONT_SERVICE_START:
		/* Set container flag only for container service start */
		info->is_container_app =
			(info->init_config_type ==
			 QCA_WLAN_VENDOR_DYNAMIC_INIT_CONF_CONT_SERVICE_START);
		ret = ath12k_vendor_service_init[info->id](NULL, info);
		if (info->is_container_app)
			ath12k_info(NULL, "App init called for containerized service ID: %d\n",
				    info->id);
		break;
	case QCA_WLAN_VENDOR_DYNAMIC_INIT_CONF_SERVICE_STOP:
	case QCA_WLAN_VENDOR_DYNAMIC_INIT_CONF_CONT_SERVICE_STOP:
		info->is_container_app =
			(info->init_config_type ==
			 QCA_WLAN_VENDOR_DYNAMIC_INIT_CONF_CONT_SERVICE_START);
		ret = ath12k_vendor_service_deinit[info->id](NULL, info);
		break;
	default:
		ath12k_err(NULL, "Invalid config init type: %d\n",
			   info->init_config_type);
		return -EINVAL;
	};

	return ret;
}

int ath12k_vendor_generic_app_init_reply(struct sk_buff *vendor_event)
{
	struct wiphy *wiphy = ath12k_vendor_get_wiphy();
	struct ieee80211_hw *hw = wiphy_to_ieee80211_hw(wiphy);
	struct ath12k_hw *ah = hw->priv;
	struct ath12k_hw_group *ag = ah->radio->ab->ag;
	struct ath12k_base *ab;
	struct nlattr *soc_info = NULL, *per_soc_info = NULL;
	struct nlattr *link_info = NULL;
	struct nlattr *per_link_info = NULL;
	struct ath12k *ar;
	int num_socs = 0;
	int num_hw_links;
	int soc_id, hw_link_id;

	/* Note this function is expected to be called with at least 1 valid
	 * interface
	 */
	if (nla_put_u8(vendor_event,
		       QCA_WLAN_VENDOR_ATTR_RM_GENERIC_APP_VERSION,
		       vendor_info.app_info.app_version)){
		ath12k_err(NULL, " Fails to put QCA_WLAN_VENDOR_ATTR_RM_GENERIC_APP_VERSION");
		return -EINVAL;
	}

	if (nla_put_u8(vendor_event,
		       QCA_WLAN_VENDOR_ATTR_RM_GENERIC_DRIVER_VERSION,
		       vendor_info.app_info.driver_version)){
		ath12k_err(NULL, "Fails to put QCA_WLAN_VENDOR_ATTR_RM_GENERIC_APP_VERSION");
		return -EINVAL;
	}

	soc_info = nla_nest_start(vendor_event,
				  QCA_WLAN_VENDOR_ATTR_RM_GENERIC_SOC_DEVICE_INFO);
	if (!soc_info) {
		ath12k_err(NULL, "Fails to put QCA_WLAN_VENDOR_ATTR_RM_GENERIC_SOC_DEVICE_INFO");
		return -1;
	}

	for (soc_id = 0; soc_id < ag->num_devices; soc_id++) {
		u8 total_vdevs;

		ab = ag->ab[soc_id];
		total_vdevs = ath12k_core_get_total_num_vdevs(ab);

		spin_lock_bh(&ab->base_lock);
		if (ab->free_vdev_map == (1LL << (ab->num_radios * total_vdevs)) - 1) {
			spin_unlock_bh(&ab->base_lock);
			continue;
		}
		spin_unlock_bh(&ab->base_lock);
		num_socs++;

		per_soc_info = nla_nest_start(vendor_event, soc_id);
		if (!per_soc_info) {
			ath12k_err(NULL, "Fails to put per soc info for soc:%d",
				   soc_id);
			return -1;
		}

		ath12k_vendor_put_ab_soc_id(vendor_event, ab);

		link_info = nla_nest_start(vendor_event,
					   QCA_WLAN_VENDOR_ATTR_SOC_DEVICE_LINK_INFO);
		if (!link_info) {
			ath12k_err(ab, "Fails to put soc device link info soc:%d",
				   ab->device_id);
			return -1;
		}

		num_hw_links = 0;
		for (hw_link_id = 0; hw_link_id < ab->num_radios; hw_link_id++) {
			ar = ab->pdevs[hw_link_id].ar;
			if (!ar)
				continue;

			lockdep_assert_wiphy(ath12k_ar_to_hw(ar)->wiphy);
			if (ar->num_started_vdevs == 0)
				continue;
			per_link_info = nla_nest_start(vendor_event,
						       hw_link_id);
			if (!per_link_info) {
				ath12k_err(ab, "Fails to put per link info :%d for soc:%d",
					   ar->pdev->hw_link_id, ab->device_id);
				return -1;

			}
			ath12k_vendor_put_ar_hw_link_id(vendor_event, ar);
			ath12k_vendor_put_ar_link_mac_addr(vendor_event, ar);
			ath12k_vendor_put_ar_chan_info(vendor_event, ar);
			ath12k_vendor_put_ar_nss_chains(vendor_event, ar);
			nla_nest_end(vendor_event, per_link_info);
			num_hw_links++;
		}
		nla_nest_end(vendor_event, link_info);
		ath12k_vendor_put_ab_num_links(vendor_event, ab, num_hw_links);
		nla_nest_end(vendor_event, per_soc_info);
	}
	nla_nest_end(vendor_event, soc_info);

	ATH12K_VENDOR_PUT(vendor_event, u8,
			  QCA_WLAN_VENDOR_ATTR_RM_GENERIC_NUM_SOC_DEVICES,
			  num_socs);

	return 0;
}

static int ath12k_vendor_get_len_vendor_generic_response(void)
{
	int len = 0;

	len += nla_total_size(sizeof(struct ath12k_vendor_soc_device_info)) +
	       nla_total_size(sizeof(struct ath12k_vendor_generic_peer_assoc_event));
	return len;
}

static int ath12k_vendor_generic_report_assoc_info(struct sk_buff *vendor_event,
						   void *gen_data)
{
	struct ath12k_vendor_generic_peer_assoc_event *peer_event;
	struct ath12k_vendor_mld_peer_link_entry *mld_link_entry;
	struct nlattr *nl_mld_link_entry;
	struct nlattr *nl_per_link_entry;
	u8 index = 0;

	peer_event = (struct ath12k_vendor_generic_peer_assoc_event *)gen_data;

	if (nla_put(vendor_event, QCA_WLAN_VENDOR_ATTR_RM_GENERIC_MLD_MAC_ADDR,
		    6, &peer_event->mld_mac_addr[0])) {
		ath12k_err(NULL, "Fails to put QCA_WLAN_VENDOR_ATTR_RM_GENERIC_MLD_MAC_ADDR");
		return -ENOMEM;
	}

	if (nla_put_u8(vendor_event,
		       QCA_WLAN_VENDOR_ATTR_RM_GENERIC_ASSOC_NUM_LINKS,
		       peer_event->num_links)) {
		ath12k_err(NULL,
			   "Fails to put RM Generic Assoc Num Links");
		return -ENOMEM;
	}

	nl_mld_link_entry =
		nla_nest_start(vendor_event,
			       QCA_WLAN_VENDOR_ATTR_RM_GENERIC_ASSOC_PEER_LINK_ENTRY);

	if (!nl_mld_link_entry) {
		ath12k_err(NULL, "Fails to start nested attr QCA_WLAN_VENDOR_ATTR_RM_GENERIC_ASSOC_PEER_LINK_ENTRY");
		return -ENOMEM;
	}

	for (index = 0; index < peer_event->num_links; index++) {
		nl_per_link_entry = nla_nest_start(vendor_event, index);

		if (!nl_per_link_entry) {
			ath12k_err(NULL, "Fails to start nested attr QCA_WLAN_VENDOR_ATTR_RM_GENERIC_ASSOC_PEER_LINK_ENTRY");
			return -ENOMEM;
		}

		mld_link_entry = &peer_event->link_entry[index];

		if (nla_put_u16(vendor_event,
				QCA_WLAN_VENDOR_ATTR_MLO_PEER_LINK_HW_LINK_ID,
				mld_link_entry->hw_link_id) ||
		    nla_put_u8(vendor_event,
			       QCA_WLAN_VENDOR_ATTR_MLO_PEER_LINK_VDEV_ID,
			       mld_link_entry->vdev_id) ||
		    nla_put(vendor_event,
			    QCA_WLAN_VENDOR_ATTR_MLO_PEER_LINK_AP_MLD_MAC,
			    6, &mld_link_entry->ap_mld_mac_addr[0]) ||
		    nla_put(vendor_event,
			    QCA_WLAN_VENDOR_ATTR_MLO_PEER_LINK_PEER_MAC,
			    6, &mld_link_entry->link_mac_addr[0]) ||
		    nla_put_u8(vendor_event,
			       QCA_WLAN_VENDOR_ATTR_MLO_PEER_LINK_MLO_LINK_ID,
			       mld_link_entry->link_id) ||
		    nla_put_u8(vendor_event,
			       QCA_WLAN_VENDOR_ATTR_MLO_PEER_LINK_IS_ASSOC_LINK,
			       mld_link_entry->is_assoc_link) ||
		    nla_put_u8(vendor_event,
			       QCA_WLAN_VENDOR_ATTR_MLO_PEER_LINK_CHAN_BW,
			       mld_link_entry->chan_bw) ||
		    nla_put_u16(vendor_event,
				QCA_WLAN_VENDOR_ATTR_MLO_PEER_LINK_CHAN_FREQ,
				mld_link_entry->chan_freq) ||
		    nla_put_u8(vendor_event,
			       QCA_WLAN_VENDOR_ATTR_MLO_PEER_LINK_BAND_CAP,
			       mld_link_entry->band_cap) ||
		    nla_put_u8(vendor_event,
			       QCA_WLAN_VENDOR_ATTR_MLO_PEER_LINK_RSSI,
			       mld_link_entry->link_rssi) ||
		    nla_put_u16(vendor_event,
				QCA_WLAN_VENDOR_ATTR_MLO_PEER_LINK_EFF_CHAN_BW,
				mld_link_entry->eff_chan_bw) ||
		    nla_put_u16(vendor_event,
				QCA_WLAN_VENDOR_ATTR_MLO_PEER_LINK_CAPS,
				mld_link_entry->peer_capa_flags)) {
			ath12k_err(NULL, "Fails to put per mld link entry attrs");
			return -ENOMEM;
		}
		nla_nest_end(vendor_event, nl_per_link_entry);
	}

	nla_nest_end(vendor_event, nl_mld_link_entry);

	ath12k_dbg(NULL, ATH12K_DBG_RM, "Notify vendor app on assoc\n");
	return 0;
}

static int ath12k_vendor_generic_report_disassoc(struct sk_buff *vendor_event,
						 void *gen_data)
{
	struct ath12k_vendor_generic_peer_assoc_event *peer_event;
	struct ath12k_vendor_mld_peer_link_entry *mld_link_entry;
	struct nlattr *nl_mld_link_entry;
	struct nlattr *nl_per_link_entry;
	u8 index = 0;

	peer_event = (struct ath12k_vendor_generic_peer_assoc_event *)gen_data;

	if (nla_put(vendor_event, QCA_WLAN_VENDOR_ATTR_RM_GENERIC_MLD_MAC_ADDR,
		    6, &peer_event->mld_mac_addr[0])) {
		ath12k_err(NULL, "Fails to put QCA_WLAN_VENDOR_ATTR_RM_GENERIC_MLD_MAC_ADDR");
		return -ENOMEM;
	}

	if (nla_put_u8(vendor_event,
		       QCA_WLAN_VENDOR_ATTR_RM_GENERIC_ASSOC_NUM_LINKS,
		       peer_event->num_links)) {
		ath12k_err(NULL,
			   "Fails to put RM Generic Assoc Num Links");
		return -ENOMEM;
	}

	nl_mld_link_entry =
		nla_nest_start(vendor_event,
			       QCA_WLAN_VENDOR_ATTR_RM_GENERIC_ASSOC_PEER_LINK_ENTRY);

	if (!nl_mld_link_entry) {
		ath12k_err(NULL, "Fails to start nested attr QCA_WLAN_VENDOR_ATTR_RM_GENERIC_ASSOC_PEER_LINK_ENTRY");
		return -ENOMEM;
	}

	for (index = 0; index < peer_event->num_links; index++) {
		nl_per_link_entry = nla_nest_start(vendor_event, index);

		if (!nl_per_link_entry) {
			ath12k_err(NULL, "Fails to start nested attr QCA_WLAN_VENDOR_ATTR_RM_GENERIC_ASSOC_PEER_LINK_ENTRY");
			return -ENOMEM;
		}

		mld_link_entry = &peer_event->link_entry[index];

		if (nla_put_u16(vendor_event,
				QCA_WLAN_VENDOR_ATTR_MLO_PEER_LINK_HW_LINK_ID,
				mld_link_entry->hw_link_id) ||
		    nla_put(vendor_event,
			    QCA_WLAN_VENDOR_ATTR_MLO_PEER_LINK_PEER_MAC,
			    6, &mld_link_entry->link_mac_addr[0]) ||
		    nla_put_u8(vendor_event,
			       QCA_WLAN_VENDOR_ATTR_MLO_PEER_LINK_IS_ASSOC_LINK,
			       mld_link_entry->is_assoc_link)) {
			ath12k_err(NULL, "Fails to put per mld link entry attrs");
			return -ENOMEM;
		}
		nla_nest_end(vendor_event, nl_per_link_entry);
	}

	nla_nest_end(vendor_event, nl_mld_link_entry);

	ath12k_dbg(NULL, ATH12K_DBG_RM, "Notify vendor app on disassoc\n");
	return 0;
}

static void ath12k_vendor_generic_response(void *out_data,
					   u8 service_id, u8 category)
{
	struct wiphy *wiphy = ath12k_vendor_get_wiphy();
	int vendor_data_len = ath12k_vendor_get_len_vendor_generic_response();
	struct sk_buff *vendor_event;

	if (!wiphy) {
		ath12k_err(NULL, "wiphy doesn't exist");
		return;
	}

	vendor_event =
		cfg80211_vendor_event_alloc(wiphy, NULL, vendor_data_len,
					    QCA_NL80211_VENDOR_SUBCMD_RM_GENERIC_INDEX,
					    GFP_ATOMIC);

	if (!vendor_event) {
		ath12k_err(NULL, "skb allocation failed for RM generic command\n");
		return;
	}

	if (nla_put_u8(vendor_event,
		       QCA_WLAN_VENDOR_ATTR_RM_GENERIC_CATEGORY, category)) {
		ath12k_err(NULL, "failed to put RM generic category");
		goto error;
	}

	if (nla_put_u8(vendor_event,
		       QCA_WLAN_VENDOR_ATTR_RM_GENERIC_DYNAMIC_INIT_CONF,
		       vendor_info.init_config_type)) {
		ath12k_err(NULL, "failed to put RM generic dynamic init config");
		goto error;
	}

	switch (category) {
	case QCA_WLAN_VENDOR_ATTR_GENERIC_CATEGORY_INVALID:
		ath12k_err(NULL, "vendor: Invalid catergory received from telemetry agent");
		goto error;
	case QCA_WLAN_VENDOR_ATTR_GENERIC_CATEGORY_APP_INIT:
		if (nla_put_u8(vendor_event,
			       QCA_WLAN_VENDOR_ATTR_RM_GENERIC_SERVICE_ID, service_id)) {
			ath12k_err(NULL, "failed to put RM generic service id");
			goto error;
		}

		if (ath12k_vendor_generic_app_init_reply(vendor_event)) {
			ath12k_err(NULL, "vendor: Failed to response generic app init handshake");
			goto error;
		}
		break;
	case QCA_WLAN_VENDOR_ATTR_GENERIC_CATEGORY_ASSOC_NO_T2LM_INFO:
		fallthrough;
	case QCA_WLAN_VENDOR_ATTR_GENERIC_CATEGORY_ASSOC_WITH_T2LM_INFO:
		fallthrough;
	case QCA_WLAN_VENDOR_ATTR_GENERIC_CATEGORY_OMI_NO_T2LM_INFO:
		if (ath12k_vendor_generic_report_assoc_info(vendor_event,
							       out_data)) {
			ath12k_dbg(NULL, ATH12K_DBG_RM,
				   "vendor: failed to report assoc info to vendor app\n");
			goto error;
		}
		break;
	case QCA_WLAN_VENDOR_ATTR_GENERIC_CATEGORY_DISASSOC:
		if (ath12k_vendor_generic_report_disassoc(vendor_event,
							     out_data)) {
			ath12k_dbg(NULL, ATH12K_DBG_RM,
				   "vendor: failed to report disassoc info to vendor app\n");
			goto error;
		}
		break;
	default:
		ath12k_dbg(NULL, ATH12K_DBG_RM,
			   "vendor: Invalid catergory received from telemetry agent\n");
		goto error;
	}

	cfg80211_vendor_event(vendor_event, GFP_ATOMIC);
	return;
error:
	kfree(vendor_event);
}

void ath12k_telemetry_vendor_callback(u8 init,
				      u8 id,
				      u8 category)
{
	static int data;
	void *ptr = &data;
	bool should_queue_work = false;

	if (id >= ATH12K_RM_MAX_SERVICE)
		return;

	if (ath12k_telemetry_is_agent_loaded() && init == 0) {
		/* Set init flag for both main service and
		 * dynamic/containerized services
		 * Main service is not applicable in case of containerized
		 * services. Main service is only applicable in case RM based
		 * app, driver needs to support initialize handshake for
		 * both non-containerized based vendor app and containerized
		 * based vendor app
		 */
		ath12k_vendor_check_and_set_init_done(id, &should_queue_work);
		if (should_queue_work) {
			ath12k_dbg(NULL, ATH12K_DBG_RM,
				   "Received init event from ta for service %s (ID: %d)",
				   id == ATH12K_RM_MAIN_SERVICE ? "main" : "dynamic", id);
		}
	} else if (init != 0) {
		ath12k_dbg(NULL, ATH12K_DBG_RM,
			   "Received de-init event from ta for service ID: %d", id);
	}

	if (init == 0) {
		ath12k_vendor_generic_response(ptr, id, category);
		/* Queue vendor work for both main service and dynamic services */
		if (should_queue_work)
			ath12k_vendor_queue_vendor_work();
	} else {
		ath12k_telemetry_destroy_peer_agent_resources();
	}
}

int ath12k_vendor_send_assoc_event(void *event_data, u8 category)
{
	/* Only report if any of the vendor app is enabled */
	if (!ath12k_vendor_get_init_done())
		return -EOPNOTSUPP;

	/* Check if vendor serive app is enabled
	 * for any non-init event, send only 1 notification to vendor app.
	 * And this vendor app notifies to all enabled vendor services
	 */
	ath12k_vendor_generic_response(event_data,
				       ATH12K_RM_MAIN_SERVICE, category);
	ath12k_dbg(NULL, ATH12K_DBG_RM, "Sent assoc event to vendor app\n");

	return 0;
}

static void ath12k_vendor_report_link_info(struct ath12k_base *ab,
					   struct ath12k_link_vif *arvif)
{
	struct ath12k_vendor_soc_device_info *soc_info;
	struct ath12k_vendor_link_info *link_info;
	struct ieee80211_chanctx_conf *ctx = NULL;
	struct ath12k *ar = arvif->ar;
	int ret;

	lockdep_assert_wiphy(ath12k_ar_to_hw(ar)->wiphy);

	ctx = &arvif->chanctx;
	if (!ctx) {
		ath12k_warn(ab, "Failed to send link info due to no chan ctx");
		return;
	}

	/* Only report if vendor services are initialized */
	if (!ath12k_vendor_check_work_queue_ready()) {
		ath12k_dbg(ab, ATH12K_DBG_RM,
			   "failed to schedule vendor work as workqueue not initialized");
		return;
	}

	soc_info = kmalloc(sizeof(*soc_info), GFP_KERNEL);
	if (!soc_info)
		return;

	soc_info->soc_id = ath12k_get_ab_device_id(ab);
	soc_info->num_radios = ab->num_radios;

	link_info = &soc_info->link_info[0];
	link_info->hw_link_id = ar->pdev->hw_link_id;
	ether_addr_copy(link_info->link_mac_addr, arvif->bssid);
	link_info->chan_bw = ctx->def.width;
	link_info->chan_freq = ctx->def.chan->center_freq;
	link_info->tx_chain_mask = ar->pdev->cap.tx_chain_mask;
	link_info->rx_chain_mask = ar->pdev->cap.rx_chain_mask;

	spin_lock(&vendor_info.vendor_lock);
	list_add(&soc_info->list, &vendor_info.soc_list);
	spin_unlock(&vendor_info.vendor_lock);

	/* Queue work with any valid service ID - the work handler will send
	 * link info to all enabled services regardless of which service ID
	 * was used to queue the work
	 */
	ret = ath12k_vendor_queue_vendor_work();
	if (ret) {
		ath12k_dbg(ab, ATH12K_DBG_RM,
			   "Failed to queue link info work, cleaning up");
		spin_lock(&vendor_info.vendor_lock);
		list_del(&soc_info->list);
		kfree(soc_info);
		spin_unlock(&vendor_info.vendor_lock);
	} else {
		ath12k_dbg(ab, ATH12K_DBG_RM,
			   "Queued link info work successfully");
	}
}

int ath12k_vendor_link_state_update(const u8 mac_id,
				    struct ath12k_base *ab,
				    struct ath12k_link_vif *arvif,
				    enum ath12k_vendor_link_state new_state)
{
	struct ath12k_vendor_soc_device_info *soc_info;
	struct ath12k_vendor_link_info *link_info;
	u8 chip_id;

	if (!ab || !arvif)
		return -EINVAL;

	if (arvif->ahvif->vdev_type != WMI_VDEV_TYPE_AP)
		return -EOPNOTSUPP;

	if (!ath12k_telemetry_is_agent_loaded()) {
		ath12k_dbg(ab, ATH12K_DBG_RM,
			   "vendor: vendor services are not initialized");
		return -EINVAL;
	}

	if (new_state >= ATH12K_VENDOR_LINK_STATE_MAX)
		return -EINVAL;

	chip_id = ath12k_get_ab_device_id(ab);
	soc_info = &vendor_info.app_info.soc_info[chip_id];
	link_info = &soc_info->link_info[mac_id];

	link_info->state.prev_state = link_info->state.curr_state;
	link_info->state.curr_state = new_state;

	if (link_info->state.prev_state == ATH12K_VENDOR_LINK_STATE_ADDED &&
	    link_info->state.curr_state == ATH12K_VENDOR_LINK_STATE_ASSIGNED) {
		ath12k_vendor_report_link_info(ab, arvif);
		ath12k_dbg(ab, ATH12K_DBG_RM,
			   "Vendor link notification is sent for chip id: %d mac id: %d when prev: %d curr: %d state",
			   chip_id, mac_id,
			   link_info->state.prev_state,
			   link_info->state.curr_state);
	} else {
		/*
		 * For rest of the state transition, no action required
		 * Currently, RM doesn't support link removal for this
		 * particular notification
		 */
		ath12k_dbg(ab, ATH12K_DBG_RM,
			   "Vendor link notification is not reported for chip id: %d mac id: %d when prev: %d curr: %d state",
			   chip_id, mac_id,
			   link_info->state.prev_state,
			   link_info->state.curr_state);
	}

	return 0;
}
