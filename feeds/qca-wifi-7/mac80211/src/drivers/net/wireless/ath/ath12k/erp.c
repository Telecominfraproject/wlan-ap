/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */
#include <linux/mutex.h>
#include <linux/pci.h>
#include <linux/workqueue.h>
#include <net/netlink.h>
#include "core.h"
#include "vendor.h"
#include "dp_rx.h"
#include "erp.h"
#include "debug.h"
#include "debugfs.h"
#include "pci.h"
#include "mac.h"

#if LINUX_VERSION_IS_GEQ(6,7,0)
static const struct netlink_range_validation
#else
static struct netlink_range_validation
#endif
ath12k_vendor_erp_config_trigger_range = {
	.min = 1,
	.max = BIT(QCA_WLAN_VENDOR_TRIGGER_TYPE_MAX) - 1,
};

static const struct nla_policy
ath12k_vendor_erp_config_policy[QCA_WLAN_VENDOR_ATTR_ERP_CONFIG_MAX + 1] = {
	[QCA_WLAN_VENDOR_ATTR_ERP_CONFIG_IFINDEX] = {.type = NLA_U32},
	[QCA_WLAN_VENDOR_ATTR_ERP_CONFIG_TRIGGER] =
		NLA_POLICY_FULL_RANGE(NLA_U32, &ath12k_vendor_erp_config_trigger_range),
	[QCA_WLAN_VENDOR_ATTR_ERP_CONFIG_PCIE_REMOVE] = { .type = NLA_FLAG},
	[QCA_WLAN_VENDOR_ATTR_ERP_CONFIG_PCIE_SPEED_WIDTH] = { .type = NLA_FLAG},
};

static const struct nla_policy
ath12k_vendor_erp_policy[QCA_WLAN_VENDOR_ATTR_ERP_MAX + 1] = {
	[QCA_WLAN_VENDOR_ATTR_ERP_ENTER_START] = { .type = NLA_FLAG},
	[QCA_WLAN_VENDOR_ATTR_ERP_ENTER_COMPLETE] = { .type = NLA_FLAG},
	[QCA_WLAN_VENDOR_ATTR_ERP_CONFIG] =
		NLA_POLICY_NESTED(ath12k_vendor_erp_config_policy),
	[QCA_WLAN_VENDOR_ATTR_ERP_EXIT] = { .type = NLA_FLAG },
};

enum ath12k_erp_pcie_rescan {
	ATH12K_ERP_PCIE_RESCAN_INVALID,
	ATH12K_ERP_PCIE_RESCAN_STARTED,
	ATH12K_ERP_PCIE_RESCAN_COMPLETE,
};

struct ath12k_erp_pci_dev {
	int num_pdev;
	int num_pdev_remove_req;
	struct pci_dev *dev;
	struct pci_dev *root;
	struct pci_bus *bus;
	u16 speed;
	u16 width;
};

struct ath12k_erp_active_ar {
	struct ath12k *ar;
	enum ath12k_routing_pkt_type trigger;
};

struct ath12k_erp_pcie_config {
	struct work_struct work;
	struct ath12k_erp_pci_dev pci[ATH12K_MAX_SOCS];
	u8 enter_cnt;
	u8 exit_cnt;
};

struct ath12k_erp_pcie_config erp_pcie_config = { 0 };
struct workqueue_struct *erp_pcie_config_workqueue = NULL;

struct ath12k_erp_state_machine {
	bool initialized;
	/* Protects the structure members */
	struct mutex lock;
	enum ath12k_erp_states state;
	struct ath12k_erp_active_ar active_ar;
	struct dentry *erp_dir;
};

static struct ath12k_erp_state_machine erp_sm = {};

static void ath12k_erp_reset_state(void)
{
	erp_sm.state = ATH12K_ERP_OFF;
	memset(&erp_sm.active_ar, 0, sizeof(erp_sm.active_ar));
}

static u32 ath12k_erp_convert_trigger_bitmap(u32 bitmap)
{
	u32 new_bitmap = 0;
	u8 bit_set;

	WARN_ON(sizeof(bitmap) > sizeof(new_bitmap));

	while (bitmap) {
		bit_set = ffs(bitmap);
		if (bit_set > ATH12K_PKT_TYPE_MAX)
			break;

		bitmap &= ~BIT(bit_set - 1);

		switch (bit_set - 1) {
		case QCA_WLAN_VENDOR_TRIGGER_TYPE_ARP_IPV4:
			new_bitmap |= BIT(ATH12K_PKT_TYPE_ARP_IPV4);
			break;
		case QCA_WLAN_VENDOR_TRIGGER_TYPE_NS_IPV6:
			new_bitmap |= BIT(ATH12K_PKT_TYPE_NS_IPV6);
			break;
		case QCA_WLAN_VENDOR_TRIGGER_TYPE_IGMP_IPV4:
			new_bitmap |= BIT(ATH12K_PKT_TYPE_IGMP_IPV4);
			break;
		case QCA_WLAN_VENDOR_TRIGGER_TYPE_MLD_IPV6:
			new_bitmap |= BIT(ATH12K_PKT_TYPE_MLD_IPV6);
			break;
		case QCA_WLAN_VENDOR_TRIGGER_TYPE_DHCP_IPV4:
			new_bitmap |= BIT(ATH12K_PKT_TYPE_DHCP_IPV4);
			break;
		case QCA_WLAN_VENDOR_TRIGGER_TYPE_DHCP_IPV6:
			new_bitmap |= BIT(ATH12K_PKT_TYPE_DHCP_IPV6);
			break;
		case QCA_WLAN_VENDOR_TRIGGER_TYPE_DNS_TCP_IPV4:
			new_bitmap |= BIT(ATH12K_PKT_TYPE_DNS_TCP_IPV4);
			break;
		case QCA_WLAN_VENDOR_TRIGGER_TYPE_DNS_TCP_IPV6:
			new_bitmap |= BIT(ATH12K_PKT_TYPE_DNS_TCP_IPV6);
			break;
		case QCA_WLAN_VENDOR_TRIGGER_TYPE_DNS_UDP_IPV4:
			new_bitmap |= BIT(ATH12K_PKT_TYPE_DNS_UDP_IPV4);
			break;
		case QCA_WLAN_VENDOR_TRIGGER_TYPE_DNS_UDP_IPV6:
			new_bitmap |= BIT(ATH12K_PKT_TYPE_DNS_UDP_IPV6);
			break;
		case QCA_WLAN_VENDOR_TRIGGER_TYPE_ICMP_IPV4:
			new_bitmap |= BIT(ATH12K_PKT_TYPE_ICMP_IPV4);
			break;
		case QCA_WLAN_VENDOR_TRIGGER_TYPE_ICMP_IPV6:
			new_bitmap |= BIT(ATH12K_PKT_TYPE_ICMP_IPV6);
			break;
		case QCA_WLAN_VENDOR_TRIGGER_TYPE_TCP_IPV4:
			new_bitmap |= BIT(ATH12K_PKT_TYPE_TCP_IPV4);
			break;
		case QCA_WLAN_VENDOR_TRIGGER_TYPE_TCP_IPV6:
			new_bitmap |= BIT(ATH12K_PKT_TYPE_TCP_IPV6);
			break;
		case QCA_WLAN_VENDOR_TRIGGER_TYPE_UDP_IPV4:
			new_bitmap |= BIT(ATH12K_PKT_TYPE_UDP_IPV4);
			break;
		case QCA_WLAN_VENDOR_TRIGGER_TYPE_UDP_IPV6:
			new_bitmap |= BIT(ATH12K_PKT_TYPE_UDP_IPV6);
			break;
		case QCA_WLAN_VENDOR_TRIGGER_TYPE_IPV4:
			new_bitmap |= BIT(ATH12K_PKT_TYPE_IPV4);
			break;
		case QCA_WLAN_VENDOR_TRIGGER_TYPE_IPV6:
			new_bitmap |= BIT(ATH12K_PKT_TYPE_IPV6);
			break;
			break;
		case QCA_WLAN_VENDOR_TRIGGER_TYPE_EAP:
			new_bitmap |= BIT(ATH12K_PKT_TYPE_EAP);
			break;
		default:
			break;
		}
	}

       return new_bitmap;
}

static int ath12k_erp_set_pkt_filter(struct ath12k *ar, u32 bitmap,
				     enum ath12k_wmi_pkt_route_opcode op_code)
{
	struct ath12k_wmi_pkt_route_param param = {};

	lockdep_assert_held(&erp_sm.lock);

	if (!bitmap)
		return 0;

	if (op_code == ATH12K_WMI_PKTROUTE_ADD) {
		bitmap = ath12k_erp_convert_trigger_bitmap(bitmap);
		if (!bitmap) {
			ath12k_err(NULL, "invalid wake up trigger bitmap\n");
			return -EINVAL;
		}
	}

	param.opcode = op_code;
	param.meta_data = ATH12K_RX_PROTOCOL_TAG_START_OFFSET + bitmap;
	param.dst_ring = ATH12K_REO_RELEASE_RING;
	param.dst_ring_handler = ATH12K_WMI_PKTROUTE_USE_CCE;
	param.route_type_bmap = bitmap;
	if (ath12k_wmi_send_pdev_pkt_route(ar, &param)) {
		ath12k_err(NULL, "failed to set packet bitmap");
		return -EINVAL;
	}

	spin_lock_bh(&ar->ab->base_lock);
	if (op_code == ATH12K_WMI_PKTROUTE_ADD) {
		ar->erp_trigger_set = true;
		erp_sm.active_ar.trigger = bitmap;
	} else if (op_code == ATH12K_WMI_PKTROUTE_DEL) {
		ar->erp_trigger_set = false;
		erp_sm.active_ar.trigger = 0;
	}
	spin_unlock_bh(&ar->ab->base_lock);

	return 0;
}

static void ath12k_erp_config_pcie_speed_width(struct pci_dev *root,
					       struct ath12k_erp_pci_dev *pci,
					       bool enter)
{
	if (!root || !pci)
		return;

	if (enter) {
		if (pci->speed != 1 && pcie_set_link_speed(root, 1))
			ath12k_err(NULL, "failed to reduce PCIe speed\n");

		if (pci->width != 1 && pcie_set_link_width(root, 1))
			ath12k_err(NULL, "failed to reduce PCIe width\n");
	} else {
		if (pci->speed != 1 && pcie_set_link_speed(root, pci->speed))
			ath12k_err(NULL, "failed to reset PCIe speed\n");

		if (pci->width != 1 && pcie_set_link_width(root, pci->width))
			ath12k_err(NULL, "failed to reset PCIe width\n");
	}
}

static void ath12k_erp_enter_pcie_work(struct ath12k_erp_pcie_config *config,
				       u8 enter_cnt)
{
	struct ath12k_erp_pci_dev *pci;
	u8 i;

	for (i = 0; i < enter_cnt; i++) {
		pci = &config->pci[i];

		ath12k_erp_config_pcie_speed_width(pci->root, pci, true);

		if (pci->num_pdev_remove_req == pci->num_pdev) {
			pci_stop_and_remove_bus_device_locked(pci->root);
		}
	}

	mutex_lock(&erp_sm.lock);
	config->enter_cnt = 0;
	mutex_unlock(&erp_sm.lock);
}

static void ath12k_erp_exit_pcie_work(struct ath12k_erp_pcie_config *config,
				      u8 exit_cnt)
{
	struct ath12k_erp_pci_dev *pci;
	u8 i;

	for (i = 0; i < exit_cnt; i++) {
		pci = &config->pci[i];

		if (pci->num_pdev_remove_req == pci->num_pdev) {
			struct pci_dev *dev;

			pci_lock_rescan_remove();
			pci_rescan_bus(pci->bus);
			pci_unlock_rescan_remove();

			list_for_each_entry(dev, &pci->bus->devices, bus_list) {
				pci->root = pcie_find_root_port(dev);
				if (!pci->root) {
					ath12k_err(NULL, "failed to find PCIe root dev\n");
					continue;
				}

				ath12k_erp_config_pcie_speed_width(pci->root, pci, false);
			}
		} else {
			ath12k_erp_config_pcie_speed_width(pci->root, pci, false);
		}
	}

	mutex_lock(&erp_sm.lock);
	config->exit_cnt = 0;
	mutex_unlock(&erp_sm.lock);
}

static void ath12k_erp_pcie_work(struct work_struct *work)
{
	struct ath12k_erp_pcie_config *config =
		container_of(work,
			     struct ath12k_erp_pcie_config,
			     work);
	u8 enter_cnt, exit_cnt;

	mutex_lock(&erp_sm.lock);
	enter_cnt = config->enter_cnt;
	exit_cnt = config->exit_cnt;
	mutex_unlock(&erp_sm.lock);

	if (enter_cnt)
		ath12k_erp_enter_pcie_work(config, enter_cnt);
	else if (exit_cnt)
		ath12k_erp_exit_pcie_work(config, exit_cnt);
}

static int ath12k_erp_remove_pcie(struct wiphy *wiphy)
{
	struct ath12k_erp_pci_dev *pci;
	struct pci_dev *pci_dev;
	struct ath12k_base *ab;
	u8 i;

	ab = ath12k_core_get_ab_by_wiphy(wiphy, true);
	if (!ab || ab->hif.bus != ATH12K_BUS_PCI)
		return 0;

	pci_dev = ath12k_pci_get_dev_by_ab(ab);
	if (!pci_dev)
		return 0;

	for (i = 0; i < erp_pcie_config.enter_cnt; i++) {
		if (erp_pcie_config.pci[i].dev == pci_dev) {
			erp_pcie_config.pci[i].num_pdev_remove_req++;
			return 0;
		}
	}

	if (i >= ATH12K_MAX_SOCS) {
		ath12k_err(NULL, "configuration done for maximum allowed number of PCIes\n");
		return 0;
	}

	pci = &erp_pcie_config.pci[i];

	pci->root = pcie_find_root_port(pci_dev);
	if (!pci->root) {
		ath12k_err(NULL, "failed to find PCIe root dev\n");
		return 0;
	}

	if (ath12k_pci_get_link_status(pci->root, &pci->speed, &pci->width) < 0) {
		ath12k_err(NULL, "failed to get PCIe link status\n");
		pci->root = NULL;
		return 0;
	}

	pci->dev = pci_dev;
	pci->bus = pci->root->bus;
	pci->num_pdev = ab->num_radios;
	pci->num_pdev_remove_req = 1;
	erp_pcie_config.enter_cnt++;
	erp_pcie_config.exit_cnt++;

	return 0;
}

static void ath12k_erp_config_pcie_mlo(const struct wiphy *wiphy)
{
	struct ath12k_base *ab_list[ATH12K_MAX_SOCS] = { NULL }, *ab;
	struct ath12k_erp_pci_dev *pci;
	struct pci_dev *pci_dev;
	u8 i;

	lockdep_assert_wiphy(wiphy);
	lockdep_assert_held(&erp_sm.lock);

	erp_pcie_config.enter_cnt = ath12k_core_get_ab_list_by_wiphy(wiphy,
								     ab_list,
								     ATH12K_MAX_SOCS);

	if (!erp_pcie_config.enter_cnt)
		return;

	for (i = 0; i < erp_pcie_config.enter_cnt; i++) {
		ab = ab_list[i];

		if (!ab || ab->hif.bus != ATH12K_BUS_PCI)
			continue;

		pci_dev = ath12k_pci_get_dev_by_ab(ab);
		if (!pci_dev) {
			ath12k_warn(ab,
				   "no PCIe device associated with wiphy\n");
			continue;
		}

		pci = &erp_pcie_config.pci[erp_pcie_config.exit_cnt];

		pci->root = pcie_find_root_port(pci_dev);
		if (!pci->root) {
			ath12k_warn(ab, "failed to find PCIe root dev\n");
			continue;
		}

		if (ath12k_pci_get_link_status(pci->root, &pci->speed, &pci->width) < 0) {
			ath12k_warn(ab, "failed to get PCIe link status\n");
			pci->root = NULL;
			continue;
		}

		pci->dev = pci_dev;
		pci->bus = pci->root->bus;
		ath12k_erp_config_pcie_speed_width(pci->root, pci, true);

		erp_pcie_config.exit_cnt++;
	}

	erp_pcie_config.enter_cnt = 0;
}

static int ath12k_erp_config_active_ar(struct wiphy *wiphy, struct nlattr **attrs)
{
	struct ath12k_erp_pci_dev *pci;
	struct pci_dev *pci_dev;
	struct ieee80211_hw *hw = wiphy_to_ieee80211_hw(wiphy);
	struct ath12k_hw *ah = hw->priv;
	u32 trigger;
	struct ath12k *ar;

	lockdep_assert_wiphy(wiphy);
	lockdep_assert_held(&erp_sm.lock);

	ar = ah->radio;

	if (attrs[QCA_WLAN_VENDOR_ATTR_ERP_CONFIG_TRIGGER]) {
		trigger = nla_get_u32(attrs[QCA_WLAN_VENDOR_ATTR_ERP_CONFIG_TRIGGER]);
		if (ath12k_erp_set_pkt_filter(ar, trigger, ATH12K_WMI_PKTROUTE_ADD))
			return -EINVAL;

		erp_sm.active_ar.ar = ar;
	}

	if (!ar->ab->hif.bus == ATH12K_BUS_PCI ||
	    !attrs[QCA_WLAN_VENDOR_ATTR_ERP_CONFIG_PCIE_SPEED_WIDTH] ||
	    !nla_get_flag(attrs[QCA_WLAN_VENDOR_ATTR_ERP_CONFIG_PCIE_SPEED_WIDTH]))
		return 0;

	if (erp_pcie_config.enter_cnt >= ATH12K_MAX_SOCS) {
		ath12k_err(NULL, "configuration done for maximum allowed number of PCIes\n");
		return 0;
	}

	pci_dev = ath12k_pci_get_dev_by_ab(ar->ab);
	if (!pci_dev) {
		ath12k_warn(ar->ab, "no PCIe device associated with wiphy\n");
		return 0;
	}

	pci = &erp_pcie_config.pci[erp_pcie_config.enter_cnt];

	pci->root = pcie_find_root_port(pci_dev);
	if (!pci->root) {
		ath12k_err(ar->ab, "failed to find PCIe root dev\n");
		return 0;
	}

	if (ath12k_pci_get_link_status(pci->root, &pci->speed, &pci->width) < 0) {
		ath12k_err(NULL, "failed to get PCIe link status\n");
		pci->root = NULL;
		return 0;
	}

	pci->dev = pci_dev;
	pci->bus = pci->root->bus;
	pci->num_pdev = ar->ab->num_radios;
	pci->num_pdev_remove_req = 0;
	erp_pcie_config.enter_cnt++;
	erp_pcie_config.exit_cnt++;

	erp_sm.active_ar.ar = ar;
	return 0;
}

static int ath12k_erp_config(struct wiphy *wiphy, struct nlattr *attrs)
{
	struct nlattr *tb[QCA_WLAN_VENDOR_ATTR_ERP_CONFIG_MAX + 1];
	int ret;

	lockdep_assert_wiphy(wiphy);

	ret = nla_parse_nested(tb, QCA_WLAN_VENDOR_ATTR_ERP_CONFIG_MAX,
			attrs, ath12k_vendor_erp_config_policy, NULL);
	if (ret) {
		ath12k_err(NULL, "failed to parse ErP parameters\n");
		return ret;
	}

	if (!tb[QCA_WLAN_VENDOR_ATTR_ERP_CONFIG_TRIGGER] &&
	    !tb[QCA_WLAN_VENDOR_ATTR_ERP_CONFIG_PCIE_REMOVE] &&
	    !tb[QCA_WLAN_VENDOR_ATTR_ERP_CONFIG_PCIE_SPEED_WIDTH]) {
		ath12k_err(NULL, "empty ErP parameters\n");
		return ret;
	}

	if (tb[QCA_WLAN_VENDOR_ATTR_ERP_CONFIG_TRIGGER] &&
	    tb[QCA_WLAN_VENDOR_ATTR_ERP_CONFIG_PCIE_REMOVE]) {
		ath12k_err(NULL,
			   "both wake up trigger and PCIe not allowed for a wiphy\n");
		return ret;
	}

	if (tb[QCA_WLAN_VENDOR_ATTR_ERP_CONFIG_TRIGGER] ||
	    tb[QCA_WLAN_VENDOR_ATTR_ERP_CONFIG_PCIE_SPEED_WIDTH])
		return ath12k_erp_config_active_ar(wiphy, tb);
	else
		return ath12k_erp_remove_pcie(wiphy);
}

static int ath12k_erp_enter_non_mlo(struct wiphy *wiphy, struct wireless_dev *wdev,
				    struct nlattr **attrs)
{
	int ret = -EINVAL;

	mutex_lock(&erp_sm.lock);

	if (nla_get_flag(attrs[QCA_WLAN_VENDOR_ATTR_ERP_ENTER_START])) {
		if (erp_sm.state != ATH12K_ERP_OFF) {
			ath12k_err(NULL, "driver is already in ErP mode\n");
			goto out;
		}

		if (erp_pcie_config.enter_cnt) {
			ath12k_err(NULL,
				   "PCIe configuration for previous entry has not completed yet\n");
			goto out;
		}

		if (erp_pcie_config.exit_cnt) {
			ath12k_err(NULL,
				   "PCIe configuration for previous exit has not completed yet\n");
			goto out;
		}

		erp_sm.state = ATH12K_ERP_ENTER_STARTED;
	} else if (erp_sm.state != ATH12K_ERP_ENTER_STARTED) {
		ath12k_err(NULL,
			   "driver has not started ErP mode configurations\n");
		goto out;
	}

	if (attrs[QCA_WLAN_VENDOR_ATTR_ERP_CONFIG]) {
		ret = ath12k_erp_config(wiphy,
					attrs[QCA_WLAN_VENDOR_ATTR_ERP_CONFIG]);
		if (ret)
			goto out;
	}

	if (nla_get_flag(attrs[QCA_WLAN_VENDOR_ATTR_ERP_ENTER_COMPLETE])) {
		erp_sm.state = ATH12K_ERP_ENTER_COMPLETE;
		mutex_unlock(&erp_sm.lock);

		queue_work(erp_pcie_config_workqueue, &erp_pcie_config.work);
		return 0;
	}

	mutex_unlock(&erp_sm.lock);
	return 0;

out:
	mutex_unlock(&erp_sm.lock);
	return ret;
}

static void ath12k_vendor_send_erp_trigger(struct wiphy *wiphy)
{
	struct sk_buff *skb;
	struct nlattr *erp_ath;

	if (!wiphy)
		return;

	wiphy_lock(wiphy);
	skb = cfg80211_vendor_event_alloc(wiphy, NULL, NLMSG_DEFAULT_SIZE,
					  QCA_NL80211_VENDOR_SUBCMD_RM_GENERIC_INDEX,
					  GFP_KERNEL);
	if (!skb) {
		ath12k_err(NULL, "No memory available to send ErP vendor event\n");
		wiphy_unlock(wiphy);
		return;
	}

	erp_ath = nla_nest_start(skb, QCA_WLAN_VENDOR_ATTR_RM_GENERIC_ERP);
	if (!erp_ath) {
		ath12k_err(NULL, "failed to put ErP parameters\n");
		kfree(skb);
		wiphy_unlock(wiphy);
		return;
	}

	if (nla_put_flag(skb, QCA_WLAN_VENDOR_ATTR_ERP_EXIT)) {
		ath12k_err(NULL, "Failure to put ErP vendor event flag\n");
		/* Allow this error case for now */
	}

	nla_nest_end(skb, erp_ath);
	cfg80211_vendor_event(skb, GFP_KERNEL);
	wiphy_unlock(wiphy);
}

void ath12k_erp_ssr_exit(struct work_struct *work)
{
	struct ath12k *ar = container_of(work, struct ath12k, ssr_erp_exit);
	struct ieee80211_hw *hw = ar->ah->hw;
	struct wiphy *wiphy = hw->wiphy;

	if (ath12k_erp_exit(wiphy, true))
		ath12k_err(NULL, "Fail to exit from erp mode after ssr\n");
}

int ath12k_erp_exit(struct wiphy *wiphy, bool send_event)
{
	struct ath12k_erp_active_ar *active_ar;
	int ret;

	if (!erp_sm.initialized) {
		ath12k_err(NULL, "ErP is not initialized\n");
		return -EOPNOTSUPP;
	}

	if (!send_event) {
		/* When exit is triggered by userspace, no need to send event
		 * back. Wiphy lock must already be acquired in this case. */
		lockdep_assert_wiphy(wiphy);
	}

	mutex_lock(&erp_sm.lock);
	if (erp_sm.state != ATH12K_ERP_ENTER_COMPLETE) {
		ath12k_err(NULL, "driver is not in ErP mode\n");
		mutex_unlock(&erp_sm.lock);
		return -EINVAL;
	}

	if (erp_pcie_config.enter_cnt) {
		ath12k_err(NULL,
			   "cannot start ErP exit as PCIe configuration for entry has not completed yet\n");
		mutex_unlock(&erp_sm.lock);
		return -EINVAL;
	}

	active_ar = &erp_sm.active_ar;
	if (active_ar->ar) {
		ret = ath12k_erp_set_pkt_filter(active_ar->ar,
				active_ar->trigger,
				ATH12K_WMI_PKTROUTE_DEL);
		if (ret) {
			ath12k_err(active_ar->ar->ab, "failed to reset pkt bitmap %d", ret);
			mutex_unlock(&erp_sm.lock);
			return ret;
		}
	}

	ath12k_erp_reset_state();

	if (!ath12k_mlo_capable) {
		mutex_unlock(&erp_sm.lock);

		queue_work(erp_pcie_config_workqueue, &erp_pcie_config.work);

		if (send_event)
			ath12k_vendor_send_erp_trigger(wiphy);
	} else {
		u8 i;

		for (i = 0; i < erp_pcie_config.exit_cnt; i++)
			ath12k_erp_config_pcie_speed_width(erp_pcie_config.pci[i].root,
							   &erp_pcie_config.pci[i],
							   false);

		erp_pcie_config.exit_cnt = 0;
		mutex_unlock(&erp_sm.lock);

		if (send_event)
			cfg80211_erp_trigger_exit(wiphy);
	}

	return 0;
}

static ssize_t ath12k_write_erp_rescan_pcie(struct file *file,
					    const char __user *ubuf,
					    size_t count, loff_t *ppos)
{
	u32 state;
	int ret;

	if (!erp_sm.initialized) {
		ath12k_err(NULL, "erp is not initialized\n");
		return -EOPNOTSUPP;
	}

	ret = kstrtou32_from_user(ubuf, count, 0, &state);
	if (ret)
		return 0;

	if (state != ATH12K_ERP_PCIE_RESCAN_STARTED)
		return 0;

	if (ath12k_erp_exit(NULL, false))
		return 0;

	return count;
}

static ssize_t ath12k_read_erp_rescan_pcie(struct file *file,
					   char __user *ubuf,
					   size_t count, loff_t *ppos)
{
	int len = 0;
	char buf[4];

	if (!erp_sm.initialized) {
		ath12k_err(NULL, "erp is not initialized\n");
		return -EOPNOTSUPP;
	}

	mutex_lock(&erp_sm.lock);
	if (erp_sm.state == ATH12K_ERP_OFF)
		len += scnprintf(buf, sizeof(buf), "%u", ATH12K_ERP_PCIE_RESCAN_COMPLETE);
	else
		len += scnprintf(buf, sizeof(buf), "%u", ATH12K_ERP_PCIE_RESCAN_INVALID);
	mutex_unlock(&erp_sm.lock);

	return simple_read_from_buffer(ubuf, count, ppos, buf, len);
}

static const struct file_operations ath12k_fops_erp_rescan_pcie = {
	.write = ath12k_write_erp_rescan_pcie,
	.open = simple_open,
	.read = ath12k_read_erp_rescan_pcie,
};

int ath12k_erp_enter(struct ieee80211_hw *hw, struct ieee80211_vif *vif, int link_id,
		     struct cfg80211_erp_params *params)
{
	struct ath12k_vif *ahvif;
	struct ath12k_link_vif *arvif;
	struct ath12k *ar = NULL;
	struct ath12k_hw *ah = hw->priv;
	struct ath12k_hw_group *ag = ath12k_ah_to_ag(ah);

	lockdep_assert_wiphy(hw->wiphy);

	if (!ath12k_mlo_capable) {
		ath12k_err(NULL, "command not supported in non-MLO mode\n");
		return -EOPNOTSUPP;
	}

	if (!erp_sm.initialized) {
		ath12k_err(NULL, "Erp is not initialized\n");
		return -EOPNOTSUPP;
	}

	if (vif && params->trigger) {
		if (link_id < 0) {
			ath12k_err(NULL, "invalid link id for Erp enter operation\n");
			return -ENOLINK;
		}

		ahvif = ath12k_vif_to_ahvif(vif);
		arvif = ath12k_get_arvif_from_link_id(ahvif, link_id);
		if (!arvif || !arvif->is_created || !arvif->ar) {
			ath12k_info(NULL, "cannot set ErP trigger for the specified link\n");
			return -ENOLINK;
		}

		ar = arvif->ar;
	}

	mutex_lock(&erp_sm.lock);

	if (erp_sm.state != ATH12K_ERP_OFF) {
		ath12k_err(NULL, "driver is already in ErP mode\n");
		goto out;
	}

	if (erp_pcie_config.enter_cnt) {
		ath12k_err(NULL,
			   "PCIe configuration for previous entry has not completed yet\n");
		goto out;
	}

	if (erp_pcie_config.exit_cnt) {
		ath12k_err(NULL,
			   "PCIe configuration for previous exit has not completed yet\n");
		goto out;
	}

	if (ar && params->trigger) {
		if (ath12k_erp_set_pkt_filter(ar, params->trigger,
					      ATH12K_WMI_PKTROUTE_ADD))
			return -EINVAL;

		erp_sm.active_ar.ar = ar;
	}

	ath12k_erp_config_pcie_mlo(hw->wiphy);

	erp_sm.state = ATH12K_ERP_ENTER_COMPLETE;
	mutex_unlock(&erp_sm.lock);

	if (!ath12k_check_erp_power_down(ag) &&
	    ath12k_mac_validate_active_radio_count(ah))
		ath12k_core_cleanup_power_down_q6(ag);

	return 0;

out:
	mutex_unlock(&erp_sm.lock);
	return -EINVAL;
}

int ath12k_vendor_parse_rm_erp(struct wiphy *wiphy, struct wireless_dev *wdev,
			       struct nlattr *attrs)
{
	struct nlattr *tb[QCA_WLAN_VENDOR_ATTR_ERP_MAX + 1];
	int ret;

	if (ath12k_mlo_capable) {
		ath12k_err(NULL,
			   "ErP vendor command support is available only for non-mlo mode\n");
		return -EOPNOTSUPP;
	}

	if (!erp_sm.initialized)
		return -EOPNOTSUPP;

	lockdep_assert_wiphy(wiphy);

	ret = nla_parse_nested(tb, QCA_WLAN_VENDOR_ATTR_ERP_MAX, attrs,
			       ath12k_vendor_erp_policy, NULL);
	if (ret) {
		ath12k_err(NULL, "failed to parse ErP attributes\n");
		return ret;
	}

	if (nla_get_flag(tb[QCA_WLAN_VENDOR_ATTR_ERP_ENTER_START]) ||
			nla_get_flag(tb[QCA_WLAN_VENDOR_ATTR_ERP_ENTER_COMPLETE]) ||
			tb[QCA_WLAN_VENDOR_ATTR_ERP_CONFIG])
		ret = ath12k_erp_enter_non_mlo(wiphy, wdev, tb);
	else if (nla_get_flag(tb[QCA_WLAN_VENDOR_ATTR_ERP_EXIT])) {
		ret = ath12k_erp_exit(wiphy, false);
	}

	return ret;
}

void ath12k_erp_handle_trigger(struct work_struct *work)
{
	struct ath12k *ar = container_of(work, struct ath12k, erp_handle_trigger_work);

	if (!ar->erp_trigger_set)
		return;

	(void)ath12k_erp_exit(ath12k_ar_to_hw(ar)->wiphy, true);
}

void ath12k_erp_handle_ssr(struct ath12k *ar)
{
	if (!erp_sm.initialized)
		return;

	mutex_lock(&erp_sm.lock);
	if (erp_sm.state == ATH12K_ERP_OFF || !erp_sm.active_ar.trigger ||
	    !erp_sm.active_ar.ar || ar != erp_sm.active_ar.ar) {
		mutex_unlock(&erp_sm.lock);
		return;
	}

	ath12k_erp_set_pkt_filter(ar, erp_sm.active_ar.trigger, ATH12K_WMI_PKTROUTE_ADD);
	mutex_unlock(&erp_sm.lock);

}

void ath12k_erp_init(void)
{
	if (erp_sm.initialized)
		return;

	mutex_init(&erp_sm.lock);
	ath12k_erp_reset_state();

	if (!ath12k_mlo_capable) {
		erp_sm.erp_dir = ath12k_debugfs_erp_create();
		if (!erp_sm.erp_dir || !IS_ERR(erp_sm.erp_dir))
			goto err_dir;

		debugfs_create_file("rescan_pcie", 0200, erp_sm.erp_dir,
				    NULL, &ath12k_fops_erp_rescan_pcie);

		erp_pcie_config_workqueue = create_singlethread_workqueue("ath12k_erp_wq");
		if (!erp_pcie_config_workqueue) {
			ath12k_err(NULL, "failed to initialize ErP work queue\n");
			goto err_workqueue;
		}

		INIT_WORK(&erp_pcie_config.work, ath12k_erp_pcie_work);
	}

	erp_sm.initialized = true;
	return;

err_workqueue:
	if (!ath12k_mlo_capable)
		debugfs_remove_recursive(erp_sm.erp_dir);
err_dir:
	erp_sm.erp_dir = NULL;
	mutex_destroy(&erp_sm.lock);
}
EXPORT_SYMBOL(ath12k_erp_init);

void ath12k_erp_deinit(void)
{
	if (!erp_sm.initialized)
		return;

	mutex_lock(&erp_sm.lock);

	if (!ath12k_mlo_capable) {
		erp_sm.erp_dir = NULL;

		cancel_work_sync(&erp_pcie_config.work);
		destroy_workqueue(erp_pcie_config_workqueue);
		erp_pcie_config_workqueue = NULL;
	}

	ath12k_erp_reset_state();
	erp_sm.initialized = false;

	mutex_unlock(&erp_sm.lock);
	mutex_destroy(&erp_sm.lock);
}
EXPORT_SYMBOL(ath12k_erp_deinit);

enum ath12k_erp_states ath12k_erp_get_sm_state(void)
{
	u32 erp_state;

	mutex_lock(&erp_sm.lock);
	erp_state = erp_sm.state;
	mutex_unlock(&erp_sm.lock);

	return erp_state;
}
