/* SPDX-License-Identifier: BSD-3-Clause-Clear */
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */
#ifndef ATH12K_VENDOR_H
#define ATH12K_VENDOR_H

#define QCA_NL80211_VENDOR_ID 0x001374

#define INVALID_LINK_ID 0xFF
#define is_valid_link_id(X) !((X) >= INVALID_LINK_ID)

#define QCA_NL80211_AFC_REQ_RESP_BUF_MAX_SIZE          5000
#define QCA_WLAN_AFC_RESP_DESC_FIELD_START_OCTET       14
#define QCA_WLAN_AFC_RESP_DESC_FIELD_END_OCTET         30
#define ATF_OFFLOAD_MAX_PAYLOAD                                2048
#define ATF_OFFLOAD_STATS_DEFAULT_TIMEOUT              30
#define ATH12K_ATF_MAX_PEERS				512

#define INVALID_LINK_ID 0xFF
#define is_valid_link_id(X) !((X) >= INVALID_LINK_ID)

#define INVALID_RADIO_INDEX 0xFF

extern unsigned int ath12k_ppe_ds_enabled;
struct ath12k;
struct ath12k_hw;
struct ath12k_afc_info;
struct ath12k_afc_host_request;


struct ath12k_wifi_generic_params {
	u32 command;
	u32 value;
	void *data;
	u32 data_len;
	u32 length;
	u32 flags;
	u32 ifindex;
	u8 link_id;
	u8 radio_idx;
};

struct atf_peer_stat {
	u8 addr[ETH_ALEN];
	u32 atf_actual_airtime;
	u32 atf_peer_conf_airtime;
	u8 atf_group_index;
	u8 atf_ul_airtime;
	u32 atf_actual_duration;
	u32 atf_actual_ul_duration;
};

enum qca_nl80211_vendor_subcmds {
	/* Wi-Fi configuration subcommand */
	QCA_NL80211_VENDOR_SUBCMD_SET_WIFI_CONFIGURATION = 74,
	QCA_NL80211_VENDOR_SUBCMD_GET_WIFI_CONFIGURATION = 75,
	QCA_NL80211_VENDOR_SUBCMD_WIFI_PARAMS = 200,
	QCA_NL80211_VENDOR_SUBCMD_RM_GENERIC = 206,
	QCA_NL80211_VENDOR_SUBCMD_SCS_RULE_CONFIG = 218,
	QCA_NL80211_VENDOR_SUBCMD_AFC_EVENT = 222,
	QCA_NL80211_VENDOR_SUBCMD_AFC_RESPONSE = 223,
	QCA_NL80211_VENDOR_SUBCMD_SDWF_PHY_OPS = 235,
	QCA_NL80211_VENDOR_SUBCMD_SDWF_DEV_OPS = 236,
	QCA_NL80211_VENDOR_SUBCMD_PRI_LINK_MIGRATE = 256,
	QCA_NL80211_VENDOR_SUBCMD_SET_6GHZ_POWER_MODE = 258,
	QCA_NL80211_VENDOR_SUBCMD_POWER_MODE_CHANGE_COMPLETED = 259,
	QCA_NL80211_VENDOR_SUBCMD_ATF_OFFLOAD_OPS = 261,
	QCA_NL80211_VENDOR_SUBCMD_WLAN_TELEMETRY_WIPHY = 262,
	QCA_NL80211_VENDOR_SUBCMD_WLAN_TELEMETRY_WDEV = 263,
	QCA_NL80211_VENDOR_SUBCMD_AFC_CLEAR_PAYLOAD = 264,
	QCA_NL80211_VENDOR_SUBCMD_AFC_RESET = 265,
	QCA_NL80211_VENDOR_SUBCMD_IFACE_RELOAD = 267,
	QCA_NL80211_VENDOR_SUBCMD_SET_WIPHY_CONFIGURATION = 268,
	QCA_NL80211_VENDOR_SUBCMD_GET_WIPHY_CONFIGURATION = 269,
	QCA_NL80211_VENDOR_SUBCMD_AFC_GET_REG_EIRP = 290,
	QCA_NL80211_VENDOR_SUBCMD_240MHZ_INFO = 299,
	QCA_NL80211_VENDOR_SUBCMD_DCS_WLAN_INTERFERENCE_COMPUTE = 350,
	QCA_NL80211_VENDOR_SUBCMD_AFC_FETCH_POWER_EVENT = 352,
};

enum qca_nl80211_vendor_events {
	QCA_NL80211_VENDOR_SUBCMD_AFC_EVENT_INDEX = 0,
	QCA_NL80211_VENDOR_SUBCMD_6GHZ_PWR_MODE_EVT_IDX = 1,
	QCA_NL80211_VENDOR_SUBCMD_RM_GENERIC_INDEX = 2,
	QCA_NL80211_VENDOR_SUBCMD_WLAN_WIPHY_TELEMETRY_EVENT = 3,
	QCA_NL80211_VENDOR_SUBCMD_WLAN_WDEV_TELEMETRY_EVENT = 4,
	QCA_NL80211_VENDOR_SUBCMD_IFACE_RELOAD_INDEX = 5,
	QCA_NL80211_VENDOR_SUBCMD_SDWF_DEV_OPS_INDEX = 6,
	QCA_NL80211_VENDOR_SUBCMD_PRI_LINK_MIGRATE_INDEX = 7,
	QCA_NL80211_VENDOR_SUBCMD_DCS_WLAN_INTERFERENCE_COMPUTE_INDEX = 8,
	QCA_NL80211_VENDOR_SUBCMD_SCS_RULE_CONFIG_INDEX = 9,
};

/**
 * ath12k_vendor_send_power_update_complete - Send 6 GHz power update complete
 * vendor NL event to the application.
 *
 * @ar - Pointer to ar
 * @afc - Pointer to AFC info
 *
 * Return: 0 on success, negative error code on failure
 */
int
ath12k_vendor_send_power_update_complete(struct ath12k *ar,
					 struct ath12k_afc_info *afc);

/**
 * ath12k_send_afc_request - Send AFC request vendor NL event to the application
 * @ar: Pointer to ath12k structure
 * @afc_req: Pointer to AFC host request
 *
 * Return: 0 on success, negative error code on failure
 */
int ath12k_send_afc_request(struct ath12k *ar, struct ath12k_afc_host_request *afc_req);

/**
 * ath12k_send_afc_payload_reset - Send AFC payload reset vendor NL event
 * to userspace.
 *
 * @ar: Pointer to ar
 * Return: 0 on success, negative errno on failure.
 */
int ath12k_send_afc_payload_reset(struct ath12k *ar);

/**
 * Opclass, channel and EIRP information attribute length
 * Refer kernel doc explanation for attribute
 * QCA_WLAN_VENDOR_ATTR_AFC_RESP_OPCLASS_CHAN_EIRP_INFO
 * to understand the minimum length calculation.
+ */
#define QCA_WLAN_AFC_RESP_OPCLASS_CHAN_EIRP_INFO_MIN_LEN       \
	NLA_ALIGN((NLA_HDRLEN + sizeof(uint8_t)) +              \
		  ((3 * NLA_HDRLEN) + sizeof(uint8_t) +         \
		   sizeof(uint32_t)))

/**
 * Frequency/PSD information attribute length
 * Refer kernel doc explanation for attribute
 * QCA_WLAN_VENDOR_ATTR_AFC_RESP_FREQ_PSD_INFO
 * to understand the minimum length calculation.
 */
#define QCA_WLAN_AFC_RESP_FREQ_PSD_INFO_INFO_MIN_LEN           \
	NLA_ALIGN((3 * NLA_HDRLEN) +                            \
		  (2 * sizeof(uint16_t)) +                      \
		  sizeof(uint32_t))

/* enum qca_nl_afc_resp_type: Defines the format in which user space
 * application will send over the AFC response to driver.
 * @QCA_WLAN_VENDOR_ATTR_AFC_JSON_RESP: Payload in JSON format
 * @QCA_WLAN_VENDOR_ATTR_AFC_BIN_RESP: Payload in binary format
 * @QCA_WLAN_VENDOR_ATTR_AFC_INV_RESP: Invalid payload format
 */
enum ath12k_nl_afc_resp_type {
	QCA_WLAN_VENDOR_ATTR_AFC_JSON_RESP,
	QCA_WLAN_VENDOR_ATTR_AFC_BIN_RESP,
	QCA_WLAN_VENDOR_ATTR_AFC_INV_RESP,
};

enum qca_wlan_vendor_afc_response_attr {
	QCA_WLAN_VENDOR_ATTR_AFC_RESPONSE_DATA_TYPE = 1,
	QCA_WLAN_VENDOR_ATTR_AFC_RESPONSE_DATA,

	QCA_WLAN_VENDOR_ATTR_AFC_RESPONSE_MAX,
};

enum qca_nl_afc_event_type {
	QCA_WLAN_VENDOR_AFC_EXPIRY_EVENT,
	QCA_WLAN_VENDOR_AFC_POWER_UPDATE_COMPLETE_EVENT,
};

/**
 * enum qca_wlan_vendor_attr_afc_freq_psd_info: This enum is used with
 * nested attributes QCA_WLAN_VENDOR_ATTR_AFC_RESP_FREQ_PSD_INFO and
 * QCA_WLAN_VENDOR_ATTR_AFC_EVENT_FREQ_RANGE_LIST to update the frequency range
 * and PSD information.
 *
 * @QCA_WLAN_VENDOR_ATTR_AFC_FREQ_PSD_INFO_RANGE_START: Required and type is
 * u32. This attribute is used to indicate the start of the queried frequency
 * range in MHz.
 *
 * @QCA_WLAN_VENDOR_ATTR_AFC_FREQ_PSD_INFO_RANGE_END: Required and type is u32.
 * This attribute is used to indicate the end of the queried frequency range
 * in MHz.
 *
 * @QCA_WLAN_VENDOR_ATTR_AFC_FREQ_PSD_INFO_PSD: Required and type is u32.
 * This attribute will contain the PSD information for a single range as
 * specified by the QCA_WLAN_VENDOR_ATTR_AFC_FREQ_PSD_INFO_RANGE_START and
 * QCA_WLAN_VENDOR_ATTR_AFC_FREQ_PSD_INFO_RANGE_END attributes.
 *
 * The PSD power info (dBm/MHz) from user space should be multiplied
 * by a factor of 100 when sending to the driver to preserve granularity
 * up to 2 decimal places.
 * Example:
 *     PSD power value: 10.21 dBm/MHz
 *     Value to be updated in QCA_WLAN_VENDOR_ATTR_AFC_PSD_INFO: 1021.
 *
 * Note: QCA_WLAN_VENDOR_ATTR_AFC_FREQ_PSD_INFO_PSD attribute will be used only
 * with nested attribute QCA_WLAN_VENDOR_ATTR_AFC_RESP_FREQ_PSD_INFO and with
 * QCA_WLAN_VENDOR_ATTR_AFC_EVENT_FREQ_RANGE_LIST when
 * QCA_WLAN_VENDOR_ATTR_AFC_EVENT_TYPE is
 * QCA_WLAN_VENDOR_AFC_EVENT_TYPE_POWER_UPDATE_COMPLETE.
 *
 * The following set of attributes will be used to exchange frequency and
 * corresponding PSD information for AFC between the user space and the driver.
 */
enum qca_wlan_vendor_attr_afc_freq_psd_info {
	QCA_WLAN_VENDOR_ATTR_AFC_FREQ_PSD_INFO_INVALID = 0,
	QCA_WLAN_VENDOR_ATTR_AFC_FREQ_PSD_INFO_RANGE_START = 1,
	QCA_WLAN_VENDOR_ATTR_AFC_FREQ_PSD_INFO_RANGE_END = 2,
	QCA_WLAN_VENDOR_ATTR_AFC_FREQ_PSD_INFO_PSD = 3,

	QCA_WLAN_VENDOR_ATTR_AFC_FREQ_PSD_INFO_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_AFC_FREQ_PSD_INFO_MAX =
		QCA_WLAN_VENDOR_ATTR_AFC_FREQ_PSD_INFO_AFTER_LAST - 1,
};

/**
 * enum qca_wlan_vendor_attr_afc_chan_eirp_info: This enum is used with
 * nested attribute QCA_WLAN_VENDOR_ATTR_AFC_CHAN_LIST_INFO to update the
 * channel list and corresponding EIRP information.
 *
 * @QCA_WLAN_VENDOR_ATTR_AFC_CHAN_EIRP_INFO_CHAN_NUM: Required and type is u8.
 * This attribute is used to indicate queried channel from
 * the operating class indicated in QCA_WLAN_VENDOR_ATTR_AFC_OPCLASS_INFO.
 *
 * @QCA_WLAN_VENDOR_ATTR_AFC_CHAN_EIRP_INFO_EIRP: Optional and type is u32.
 * This attribute is used to configure the EIRP power info corresponding
 * to the channel number indicated in QCA_WLAN_VENDOR_ATTR_AFC_CHAN_NUM.
 * The EIRP power info(dBm) from user space should be multiplied
 * by a factor of 100 when sending to Driver to preserve granularity up to
 * 2 decimal places.
 * Example:
 *     EIRP power value: 34.23 dBm
 *     Value to be updated in QCA_WLAN_VENDOR_ATTR_AFC_EIRP_INFO: 3423.
 *
 * Note: QCA_WLAN_VENDOR_ATTR_AFC_EIRP_INFO attribute will only be used with
 * nested attribute QCA_WLAN_VENDOR_ATTR_AFC_RESP_OPCLASS_CHAN_EIRP_INFO and
 * with QCA_WLAN_VENDOR_ATTR_AFC_EVENT_OPCLASS_CHAN_LIST when
 * QCA_WLAN_VENDOR_ATTR_AFC_EVENT_TYPE is
 * QCA_WLAN_VENDOR_AFC_EVENT_TYPE_POWER_UPDATE_COMPLETE:
 *
 * The following set of attributes will be used to exchange Channel and
 * corresponding EIRP information for AFC between the user space and Driver.
 */
enum qca_wlan_vendor_attr_afc_chan_eirp_info {
	QCA_WLAN_VENDOR_ATTR_AFC_CHAN_EIRP_INFO_INVALID = 0,
	QCA_WLAN_VENDOR_ATTR_AFC_CHAN_EIRP_INFO_CHAN_NUM = 1,
	QCA_WLAN_VENDOR_ATTR_AFC_CHAN_EIRP_INFO_EIRP = 2,

	QCA_WLAN_VENDOR_ATTR_AFC_CHAN_EIRP_INFO_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_AFC_CHAN_EIRP_INFO_MAX =
	QCA_WLAN_VENDOR_ATTR_AFC_CHAN_EIRP_INFO_AFTER_LAST - 1,
};

/**
 * enum qca_wlan_vendor_attr_afc_opclass_info: This enum is used with nested
 * attributes QCA_WLAN_VENDOR_ATTR_AFC_RESP_OPCLASS_CHAN_EIRP_INFO and
 * QCA_WLAN_VENDOR_ATTR_AFC_REQ_OPCLASS_CHAN_INFO to update the operating class,
 * channel, and EIRP related information.
 *
 * @QCA_WLAN_VENDOR_ATTR_AFC_OPCLASS_INFO_OPCLASS: Required and type is u8.
 * This attribute is used to indicate the operating class, as listed under
 * IEEE Std 802.11-2020 Annex E Table E-4, for the queried channel list.
 *
 * @QCA_WLAN_VENDOR_ATTR_AFC_OPCLASS_INFO_CHAN_LIST: Array of nested attributes
 * for updating the channel number and EIRP power information.
 * It uses the attributes defined in
 * enum qca_wlan_vendor_attr_afc_chan_eirp_info.
 *
 * Operating class information packing format for
 * QCA_WLAN_VENDOR_ATTR_AFC_EVENT_OPCLASS_CHAN_INFO when
 * QCA_WLAN_VENDOR_ATTR_AFC_EVENT_TYPE is
 * QCA_WLAN_VENDOR_ATTR_AFC_EVENT_TYPE_EXPIRY.
 *
 * m - Total number of operating classes.
 * n, j - Number of queried channels for the corresponding operating class.
 *
 *  QCA_WLAN_VENDOR_ATTR_AFC_OPCLASS_INFO_OPCLASS[0]
 *  QCA_WLAN_VENDOR_ATTR_AFC_OPCLASS_INFO_CHAN_LIST[0]
 *      QCA_WLAN_VENDOR_ATTR_AFC_CHAN_EIRP_INFO_CHAN_NUM[0]
 *      .....
 *      QCA_WLAN_VENDOR_ATTR_AFC_CHAN_EIRP_INFO_CHAN_NUM[n - 1]
 *  ....
 *  QCA_WLAN_VENDOR_ATTR_AFC_OPCLASS_INFO_OPCLASS[m]
 *  QCA_WLAN_VENDOR_ATTR_AFC_OPCLASS_INFO_CHAN_LIST[m]
 *      QCA_WLAN_VENDOR_ATTR_AFC_CHAN_EIRP_INFO_CHAN_NUM[0]
 *      ....
 *      QCA_WLAN_VENDOR_ATTR_AFC_CHAN_EIRP_INFO_CHAN_NUM[j - 1]
 *
 * Operating class information packing format for
 * QCA_WLAN_VENDOR_ATTR_AFC_RESP_OPCLASS_CHAN_EIRP_INFO and
 * QCA_WLAN_VENDOR_ATTR_AFC_EVENT_OPCLASS_CHAN_INFO when
 * QCA_WLAN_VENDOR_ATTR_AFC_EVENT_TYPE is
 * QCA_WLAN_VENDOR_AFC_EVENT_TYPE_POWER_UPDATE_COMPLETE.
 *
 * m - Total number of operating classes.
 * n, j - Number of channels for the corresponding operating class.
 *
 *  QCA_WLAN_VENDOR_ATTR_AFC_OPCLASS_INFO_OPCLASS[0]
 *  QCA_WLAN_VENDOR_ATTR_AFC_OPCLASS_INFO_CHAN_LIST[0]
 *      QCA_WLAN_VENDOR_ATTR_AFC_CHAN_EIRP_INFO_CHAN_NUM[0]
 *      QCA_WLAN_VENDOR_ATTR_AFC_CHAN_EIRP_INFO_EIRP[0]
 *      .....
 *      QCA_WLAN_VENDOR_ATTR_AFC_CHAN_EIRP_INFO_CHAN_NUM[n - 1]
 *      QCA_WLAN_VENDOR_ATTR_AFC_CHAN_EIRP_INFO_EIRP[n - 1]
 *  ....
 *  QCA_WLAN_VENDOR_ATTR_AFC_OPCLASS_INFO_OPCLASS[m]
 *  QCA_WLAN_VENDOR_ATTR_AFC_OPCLASS_INFO_CHAN_LIST[m]
 *      QCA_WLAN_VENDOR_ATTR_AFC_CHAN_EIRP_INFO_CHAN_NUM[0]
 *      QCA_WLAN_VENDOR_ATTR_AFC_CHAN_EIRP_INFO_EIRP[0]
 *      ....
 *      QCA_WLAN_VENDOR_ATTR_AFC_CHAN_EIRP_INFO_CHAN_NUM[j - 1]
 *      QCA_WLAN_VENDOR_ATTR_AFC_CHAN_EIRP_INFO_EIRP[j - 1]
 *
 * The following set of attributes will be used to exchange operating class
 * information for AFC between the user space and the driver.
 */
enum qca_wlan_vendor_attr_afc_opclass_info {
	QCA_WLAN_VENDOR_ATTR_AFC_OPCLASS_INFO_INVALID = 0,
	QCA_WLAN_VENDOR_ATTR_AFC_OPCLASS_INFO_OPCLASS = 1,
	QCA_WLAN_VENDOR_ATTR_AFC_OPCLASS_INFO_CHAN_LIST = 2,

	QCA_WLAN_VENDOR_ATTR_AFC_OPCLASS_INFO_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_AFC_OPCLASS_INFO_MAX =
	QCA_WLAN_VENDOR_ATTR_AFC_OPCLASS_INFO_AFTER_LAST - 1,
};

/**
 * enum qca_wlan_vendor_afc_event_type: Defines values for AFC event type.
 * Attribute used by QCA_WLAN_VENDOR_ATTR_AFC_EVENT_TYPE attribute.
 *
 * @QCA_WLAN_VENDOR_AFC_EVENT_TYPE_EXPIRY: AFC expiry event sent from the
 * driver to userspace in order to query the new AFC power values.
 *
 * @QCA_WLAN_VENDOR_AFC_EVENT_TYPE_POWER_UPDATE_COMPLETE: Power update
 * complete event will be sent from the driver to userspace to indicate
 * processing of the AFC response.
 *
 * @QCA_WLAN_VENDOR_AFC_EVENT_TYPE_PAYLOAD_RESET: AFC payload reset event
 * will be sent from the driver to userspace to indicate last received
 * AFC response data has been cleared on the AP due to invalid data
 * in the QCA_NL80211_VENDOR_SUBCMD_AFC_RESPONSE.
 *
 * The following enum defines the different event types that will be
 * used by the driver to help trigger corresponding AFC functionality in user
 * space.
 */
enum qca_wlan_vendor_afc_event_type {
	QCA_WLAN_VENDOR_AFC_EVENT_TYPE_EXPIRY = 0,
	QCA_WLAN_VENDOR_AFC_EVENT_TYPE_POWER_UPDATE_COMPLETE = 1,
	QCA_WLAN_VENDOR_AFC_EVENT_TYPE_PAYLOAD_RESET = 2,
};

/**
 * enum qca_wlan_vendor_afc_evt_status_code: Defines values AP will use to
 * indicate AFC response status.
 * Enum used by QCA_WLAN_VENDOR_ATTR_AFC_EVENT_STATUS_CODE attribute.
 *
 * @QCA_WLAN_VENDOR_AFC_EVT_STATUS_CODE_SUCCESS: Success
 *
 * @QCA_WLAN_VENDOR_AFC_EVT_STATUS_CODE_TIMEOUT: Indicates AFC indication
 * command was not received within the expected time of the AFC expiry event
 * being triggered.
 *
 * @QCA_WLAN_VENDOR_AFC_EVT_STATUS_CODE_PARSING_ERROR: Indicates AFC data
 * parsing error by the driver.
 *
 * @QCA_WLAN_VENDOR_AFC_EVT_STATUS_CODE_LOCAL_ERROR: Indicates any other local
 * error.
 *
 * The following enum defines the status codes that the driver will use to
 * indicate whether the AFC data is valid or not.
 */
enum qca_wlan_vendor_afc_evt_status_code {
	QCA_WLAN_VENDOR_AFC_EVT_STATUS_CODE_SUCCESS = 0,
	QCA_WLAN_VENDOR_AFC_EVT_STATUS_CODE_TIMEOUT = 1,
	QCA_WLAN_VENDOR_AFC_EVT_STATUS_CODE_PARSING_ERROR = 2,
	QCA_WLAN_VENDOR_AFC_EVT_STATUS_CODE_LOCAL_ERROR = 3,
};

/**
 * enum qca_wlan_vendor_attr_afc_event: Defines attributes to be used with
 * vendor event QCA_NL80211_VENDOR_SUBCMD_AFC_EVENT. These attributes will
 * support sending only a single request to the user space at a time.
 *
 * @QCA_WLAN_VENDOR_ATTR_AFC_EVENT_TYPE: Required u8 attribute.
 * Used with event to notify the type of AFC event received.
 * Valid values are defined in enum qca_wlan_vendor_afc_event_type.
 *
 * @QCA_WLAN_VENDOR_ATTR_AFC_EVENT_AP_DEPLOYMENT: u8 attribute. Required when
 * QCA_WLAN_VENDOR_ATTR_AFC_EVENT_TYPE is QCA_WLAN_VENDOR_AFC_EVENT_TYPE_EXPIRY,
 * otherwise unused.
 *
 * This attribute is used to indicate the AP deployment type in the AFC request.
 * Valid values are defined in enum qca_wlan_vendor_afc_ap_deployment_type.
 *
 * @QCA_WLAN_VENDOR_ATTR_AFC_EVENT_REQ_ID: Required u32 attribute.
 * Unique request identifier generated by the AFC client for every
 * AFC expiry event trigger. See also QCA_WLAN_VENDOR_ATTR_AFC_RESP_REQ_ID.
 * The user space application is responsible for ensuring no duplicate values
 * are in-flight with the server, e.g., by delaying a request, should the same
 * value be received from different radios in parallel.
 *
 * @QCA_WLAN_VENDOR_ATTR_AFC_EVENT_AFC_WFA_VERSION: u32 attribute. Optional.
 * It is used when the QCA_WLAN_VENDOR_ATTR_AFC_EVENT_TYPE is
 * QCA_WLAN_VENDOR_AFC_EVENT_TYPE_EXPIRY, otherwise unused.
 *
 * This attribute indicates the AFC spec version information. This will
 * indicate the AFC version AFC client must use to query the AFC data.
 * Bits 15:0  - Minor version
 * Bits 31:16 - Major version
 *
 * @QCA_WLAN_VENDOR_ATTR_AFC_EVENT_MIN_DES_POWER: u16 attribute. Required when
 * QCA_WLAN_VENDOR_ATTR_AFC_EVENT_TYPE is QCA_WLAN_VENDOR_AFC_EVENT_TYPE_EXPIRY,
 * otherwise unused.
 * This attribute indicates the minimum desired power (in dBm) for
 * the queried spectrum.
 *
 * @QCA_WLAN_VENDOR_ATTR_AFC_EVENT_STATUS_CODE: u8 attribute. Required when
 * QCA_WLAN_VENDOR_ATTR_AFC_EVENT_TYPE is
 * QCA_WLAN_VENDOR_AFC_EVENT_TYPE_POWER_UPDATE_COMPLETE, otherwise unused.
 *
 * Valid values are defined in enum qca_wlan_vendor_afc_evt_status_code.
 * This attribute is used to indicate if there were any errors parsing the
 * AFC response.
 *
 * @QCA_WLAN_VENDOR_ATTR_AFC_EVENT_SERVER_RESP_CODE: s32 attribute. Required
 * when QCA_WLAN_VENDOR_ATTR_AFC_EVENT_TYPE is
 * QCA_WLAN_VENDOR_AFC_EVENT_TYPE_POWER_UPDATE_COMPLETE, otherwise unused.
 *
 * This attribute indicates the AFC response code. The AFC response codes are
 * in the following categories:
 * -1: General Failure.
 * 0: Success.
 * 100 - 199: General errors related to protocol.
 * 300 - 399: Error events specific to message exchange
 *            for the Available Spectrum Inquiry.
 *
 * @QCA_WLAN_VENDOR_ATTR_AFC_EVENT_EXP_DATE: u32 attribute. Required when
 * QCA_WLAN_VENDOR_ATTR_AFC_EVENT_TYPE is
 * QCA_WLAN_VENDOR_AFC_EVENT_TYPE_POWER_UPDATE_COMPLETE, otherwise unused.
 *
 * This attribute indicates the date until which the current response is
 * valid for in UTC format.
 * Date format: bits 7:0   - DD (Day 1-31)
 *              bits 15:8  - MM (Month 1-12)
 *              bits 31:16 - YYYY (Year)
 *
 * @QCA_WLAN_VENDOR_ATTR_AFC_EVENT_EXP_TIME: u32 attribute. Required when
 * QCA_WLAN_VENDOR_ATTR_AFC_EVENT_TYPE is
 * QCA_WLAN_VENDOR_AFC_EVENT_TYPE_POWER_UPDATE_COMPLETE, otherwise unused.
 *
 * This attribute indicates the time until which the current response is
 * valid for in UTC format.
 * Time format: bits 7:0   - SS (Seconds 0-59)
 *              bits 15:8  - MM (Minutes 0-59)
 *              bits 23:16 - HH (Hours 0-23)
 *              bits 31:24 - Reserved
 *
 * @QCA_WLAN_VENDOR_ATTR_AFC_EVENT_FREQ_RANGE_LIST: Array of nested attributes
 * for updating the list of frequency ranges to be queried.
 * Required when QCA_WLAN_VENDOR_ATTR_AFC_EVENT_TYPE is
 * QCA_WLAN_VENDOR_AFC_EVENT_TYPE_EXPIRY or
 * QCA_WLAN_VENDOR_AFC_EVENT_TYPE_POWER_UPDATE_COMPLETE, otherwise unused.
 * It uses the attributes defined in
 * enum qca_wlan_vendor_attr_afc_freq_psd_info.
 *
 * @QCA_WLAN_VENDOR_ATTR_AFC_EVENT_OPCLASS_CHAN_LIST: Array of nested attributes
 * for updating the list of operating classes and corresponding channels to be
 * queried.
 * Required when QCA_WLAN_VENDOR_ATTR_AFC_EVENT_TYPE is
 * QCA_WLAN_VENDOR_AFC_EVENT_TYPE_EXPIRY or
 * QCA_WLAN_VENDOR_AFC_EVENT_TYPE_POWER_UPDATE_COMPLETE, otherwise unused.
 * It uses the attributes defined in enum qca_wlan_vendor_attr_afc_opclass_info.
 *
 * @QCA_WLAN_VENDOR_ATTR_AFC_EVENT_HW_IDX: Required u32 attribute.
 * It notifies the hardware index for which the AFC request event is sent
 * to the AFC application.
 */
enum qca_wlan_vendor_attr_afc_event {
	QCA_WLAN_VENDOR_ATTR_AFC_EVENT_INVALID = 0,
	QCA_WLAN_VENDOR_ATTR_AFC_EVENT_TYPE = 1,
	QCA_WLAN_VENDOR_ATTR_AFC_EVENT_AP_DEPLOYMENT = 2,
	QCA_WLAN_VENDOR_ATTR_AFC_EVENT_REQ_ID = 3,
	QCA_WLAN_VENDOR_ATTR_AFC_EVENT_AFC_WFA_VERSION = 4,
	QCA_WLAN_VENDOR_ATTR_AFC_EVENT_MIN_DES_POWER = 5,
	QCA_WLAN_VENDOR_ATTR_AFC_EVENT_STATUS_CODE = 6,
	QCA_WLAN_VENDOR_ATTR_AFC_EVENT_SERVER_RESP_CODE = 7,
	QCA_WLAN_VENDOR_ATTR_AFC_EVENT_EXP_DATE = 8,
	QCA_WLAN_VENDOR_ATTR_AFC_EVENT_EXP_TIME = 9,
	QCA_WLAN_VENDOR_ATTR_AFC_EVENT_FREQ_RANGE_LIST = 10,
	QCA_WLAN_VENDOR_ATTR_AFC_EVENT_OPCLASS_CHAN_LIST = 11,
	QCA_WLAN_VENDOR_ATTR_AFC_EVENT_HW_IDX = 12,

	QCA_WLAN_VENDOR_ATTR_AFC_EVENT_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_AFC_EVENT_MAX =
	QCA_WLAN_VENDOR_ATTR_AFC_EVENT_AFTER_LAST - 1,
};

enum qca_wlan_vendor_attr_config {
	QCA_WLAN_VENDOR_ATTR_CONFIG_INVALID = 0,
	/* Unsigned 32-bit attribute for generic commands */
	QCA_WLAN_VENDOR_ATTR_CONFIG_GENERIC_COMMAND = 17,
	/* Unsigned 32-bit value attribute for generic commands */
	QCA_WLAN_VENDOR_ATTR_CONFIG_GENERIC_VALUE = 18,
	/* Unsigned 32-bit data attribute for generic command response */
	QCA_WLAN_VENDOR_ATTR_CONFIG_GENERIC_DATA = 19,
	/* Unsigned 32-bit length attribute for
	 * QCA_WLAN_VENDOR_ATTR_CONFIG_GENERIC_DATA
	 */
	QCA_WLAN_VENDOR_ATTR_CONFIG_GENERIC_LENGTH = 20,
	/* Unsigned 32-bit flags attribute for
	 * QCA_WLAN_VENDOR_ATTR_CONFIG_GENERIC_DATA
	 */
	QCA_WLAN_VENDOR_ATTR_CONFIG_GENERIC_FLAGS = 21,
	/* Unsigned 32-bit, specifies the interface index (netdev) for which the
	 * corresponding configurations are applied. If the interface index is
	 * not specified, the configurations are attributed to the respective
	 * wiphy.
	 */
	QCA_WLAN_VENDOR_ATTR_CONFIG_IFINDEX = 24,
	/* 8-bit unsigned value. Used to specify the MLO link ID of a link
	 * that is being configured. This attribute must be included in each
	 * record nested inside %QCA_WLAN_VENDOR_ATTR_CONFIG_MLO_LINKS, and
	 * may be included without nesting to indicate the link that is the
	 * target of other configuration attributes.
	 */
	QCA_WLAN_VENDOR_ATTR_CONFIG_MLO_LINK_ID = 99,
	/* 8-bit unsigned value to configure the interface offload type
	 *
	 * This attribute is used to configure the interface offload capability.
	 * User can configure software based acceleration, hardware based
	 * acceleration, or a combination of both using this option. More
	 * details on each option is described under the enum definition below.
	 * Uses enum qca_wlan_intf_offload_type for values.
	 */
	QCA_WLAN_VENDOR_ATTR_IF_OFFLOAD_TYPE = 120,
        /* 8-bit unsigned value. Used to specify the HW Radio Index of a wiphy
         * device that is being configured. This attribute may be included in
         * %QCA_NL80211_VENDOR_SUBCMD_SET_WIPHY_CONFIGURATION or
         * %QCA_NL80211_VENDOR_SUBCMD_GET_WIPHY_CONFIGURATION subcmds to
         * specify a particular Radio of the wiphy device.
         */
	QCA_WLAN_VENDOR_ATTR_CONFIG_RADIO_INDEX = 135,
	/* Keep last */
	QCA_WLAN_VENDOR_ATTR_CONFIG_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_CONFIG_MAX =
		QCA_WLAN_VENDOR_ATTR_CONFIG_AFTER_LAST - 1
};

enum qca_wlan_vendor_attr_tele_sdwfdelay_hist_bucket {
	QCA_WLAN_VENDOR_ATTR_TELE_SDWFDELAY_HIST_BUCKET_ID_0 = 1,
	QCA_WLAN_VENDOR_ATTR_TELE_SDWFDELAY_HIST_BUCKET_ID_1,
	QCA_WLAN_VENDOR_ATTR_TELE_SDWFDELAY_HIST_BUCKET_ID_2,
	QCA_WLAN_VENDOR_ATTR_TELE_SDWFDELAY_HIST_BUCKET_ID_3,
	QCA_WLAN_VENDOR_ATTR_TELE_SDWFDELAY_HIST_BUCKET_ID_4,
	QCA_WLAN_VENDOR_ATTR_TELE_SDWFDELAY_HIST_BUCKET_ID_5,
	QCA_WLAN_VENDOR_ATTR_TELE_SDWFDELAY_HIST_BUCKET_ID_6,
	QCA_WLAN_VENDOR_ATTR_TELE_SDWFDELAY_HIST_BUCKET_ID_7,
	QCA_WLAN_VENDOR_ATTR_TELE_SDWFDELAY_HIST_BUCKET_ID_8,
	QCA_WLAN_VENDOR_ATTR_TELE_SDWFDELAY_HIST_BUCKET_ID_9,
	QCA_WLAN_VENDOR_ATTR_TELE_SDWFDELAY_HIST_BUCKET_ID_10,
	QCA_WLAN_VENDOR_ATTR_TELE_SDWFDELAY_HIST_BUCKET_ID_11,
	QCA_WLAN_VENDOR_ATTR_TELE_SDWFDELAY_HIST_BUCKET_ID_12,

	QCA_WLAN_VENDOR_ATTR_TELE_SDWFDELAY_HIST_BUCKET_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_TELE_SDWFDELAY_HIST_BUCKET_MAX =
		QCA_WLAN_VENDOR_ATTR_TELE_SDWFDELAY_HIST_BUCKET_AFTER_LAST - 1,
};

enum qca_wlan_vendor_attr_tele_sdwfdelay_hist {
	QCA_WLAN_VENDOR_ATTR_TELE_SDWFDELAY_HIST_TYPE_SW_ENQEUE_DELAY = 1,
	QCA_WLAN_VENDOR_ATTR_TELE_SDWFDELAY_HIST_TYPE_HW_COMP_DELAY,
	QCA_WLAN_VENDOR_ATTR_TELE_SDWFDELAY_HIST_TYPE_REAP_STACK,
	QCA_WLAN_VENDOR_ATTR_TELE_SDWFDELAY_HIST_TYPE_HW_TX_COMP_DELAY,
	QCA_WLAN_VENDOR_ATTR_TELE_SDWFDELAY_HIST_TYPE_DELAY_PERCENTILE,
	QCA_WLAN_VENDOR_ATTR_TELE_SDWFDELAY_HIST_TYPE_HW_COMP_DELAY_TSF,
	QCA_WLAN_VENDOR_ATTR_TELE_SDWFDELAY_HIST_TYPE_HW_COMP_DELAY_JITTER_TSF,

	QCA_WLAN_VENDOR_ATTR_TELE_SDWFDELAY_HIST_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_TELE_SDWFDELAY_HIST_MAX =
		QCA_WLAN_VENDOR_ATTR_TELE_SDWFDELAY_HIST_AFTER_LAST - 1,
};

enum qca_wlan_vendor_attr_tele_sdwfdelay_hwdelay {
	QCA_WLAN_VENDOR_ATTR_TELE_SDWFDELAY_HWDELAY_MAXIMUM = 1,
	QCA_WLAN_VENDOR_ATTR_TELE_SDWFDELAY_HWDELAY_MINIMUM,
	QCA_WLAN_VENDOR_ATTR_TELE_SDWFDELAY_HWDELAY_AVERAGE,
	QCA_WLAN_VENDOR_ATTR_TELE_SDWFDELAY_HWDELAY_SUCCESS,
	QCA_WLAN_VENDOR_ATTR_TELE_SDWFDELAY_HWDELAY_FAILURE,
	QCA_WLAN_VENDOR_ATTR_TELE_SDWFDELAY_HWDELAY_INVALID_PKTS,
	QCA_WLAN_VENDOR_ATTR_TELE_SDWFDELAY_HWDELAY_HISTOGRAM,

	QCA_WLAN_VENDOR_ATTR_TELE_SDWFDELAY_HWDELAY_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_TELE_SDWFDELAY_HWDELAY_MAX =
		QCA_WLAN_VENDOR_ATTR_TELE_SDWFDELAY_HWDELAY_AFTER_LAST,
};

enum qca_wlan_vendor_attr_tele_sdwfdelay {
	QCA_WLAN_VENDOR_ATTR_TELE_SDWFDELAY_NETWORK_DELAY_AVG = 1,
	QCA_WLAN_VENDOR_ATTR_TELE_SDWFDELAY_SOFTWARE_DELAY_AVG,
	QCA_WLAN_VENDOR_ATTR_TELE_SDWFDELAY_HARDWARE_DELAY_AVG,
	QCA_WLAN_VENDOR_ATTR_TELE_SDWFDELAY_HWDELAY,
	QCA_WLAN_VENDOR_ATTR_TELE_SDWFDELAY_TID,
	QCA_WLAN_VENDOR_ATTR_TELE_SDWFDELAY_QUEUE_ID,

	QCA_WLAN_VENDOR_ATTR_TELE_SDWFDELAY_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_TELE_SDWFDELAY_MAX =
		QCA_WLAN_VENDOR_ATTR_TELE_SDWFDELAY_AFTER_LAST -1,
};

enum qca_wlan_vendor_attr_tele_sdwftx_advance_stats {
	QCA_WLAN_VENDOR_ATTR_TELE_SDWFTX_ADVANCE_STATS_SUCCESS_CNT = 1,
	QCA_WLAN_VENDOR_ATTR_TELE_SDWFTX_ADVANCE_STATS_FAILURE_CNT,

	QCA_WLAN_VENDOR_ATTR_TELE_SDWFTX_ADVANCE_STATS_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_TELE_SDWFTX_ADVANCE_STATS_MAX =
		QCA_WLAN_VENDOR_ATTR_TELE_SDWFTX_ADVANCE_STATS_AFTER_LAST -1,
};

enum qca_wlan_vendor_attr_tele_sdwftx_drop_res {
	QCA_WLAN_VENDOR_ATTR_TELE_SDWFTX_DROP_RES_CMD_REMOVE_MPDU = 1,
	QCA_WLAN_VENDOR_ATTR_TELE_SDWFTX_DROP_RES_CMD_REMOVE_TX,
	QCA_WLAN_VENDOR_ATTR_TELE_SDWFTX_DROP_RES_CMD_REMOVE_NOTX,
	QCA_WLAN_VENDOR_ATTR_TELE_SDWFTX_DROP_RES_CMD_REMOVE_AGED_FRAMES,
	QCA_WLAN_VENDOR_ATTR_TELE_SDWFTX_DROP_RES_CMD_REMOVE_REASON1,
	QCA_WLAN_VENDOR_ATTR_TELE_SDWFTX_DROP_RES_CMD_REMOVE_REASON2,
	QCA_WLAN_VENDOR_ATTR_TELE_SDWFTX_DROP_RES_CMD_REMOVE_REASON3,
	QCA_WLAN_VENDOR_ATTR_TELE_SDWFTX_DROP_RES_CMD_DISABLE_QUEUE,
	QCA_WLAN_VENDOR_ATTR_TELE_SDWFTX_DROP_RES_CMD_TILL_NONMATCHING,
	QCA_WLAN_VENDOR_ATTR_TELE_SDWFTX_DROP_RES_THRESHOLD_DROP,
	QCA_WLAN_VENDOR_ATTR_TELE_SDWFTX_DROP_RES_LINK_DESC_UNAVAIL_DROP,
	QCA_WLAN_VENDOR_ATTR_TELE_SDWFTX_DROP_RES_INVALID_MSDU_OR_DROP,
	QCA_WLAN_VENDOR_ATTR_TELE_SDWFTX_DROP_RES_MULTICAST_DROP,
	QCA_WLAN_VENDOR_ATTR_TELE_SDWFTX_DROP_RES_INVALID_RR,

	QCA_WLAN_VENDOR_ATTR_TELE_SDWFTX_DROP_RES_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_TELE_SDWFTX_DROP_RES_MAX =
		QCA_WLAN_VENDOR_ATTR_TELE_SDWFTX_DROP_RES_AFTER_LAST - 1,
};


enum qca_wlan_vendor_attr_tele_sdwftx_pkt_type_mcs {
	QCA_WLAN_VENDOR_ATTR_TELE_SDWFTX_PKT_TYPE_MCS_IDX0 = 1,
	QCA_WLAN_VENDOR_ATTR_TELE_SDWFTX_PKT_TYPE_MCS_IDX1,
	QCA_WLAN_VENDOR_ATTR_TELE_SDWFTX_PKT_TYPE_MCS_IDX2,
	QCA_WLAN_VENDOR_ATTR_TELE_SDWFTX_PKT_TYPE_MCS_IDX3,
	QCA_WLAN_VENDOR_ATTR_TELE_SDWFTX_PKT_TYPE_MCS_IDX4,
	QCA_WLAN_VENDOR_ATTR_TELE_SDWFTX_PKT_TYPE_MCS_IDX5,
	QCA_WLAN_VENDOR_ATTR_TELE_SDWFTX_PKT_TYPE_MCS_IDX6,
	QCA_WLAN_VENDOR_ATTR_TELE_SDWFTX_PKT_TYPE_MCS_IDX7,
	QCA_WLAN_VENDOR_ATTR_TELE_SDWFTX_PKT_TYPE_MCS_IDX8,
	QCA_WLAN_VENDOR_ATTR_TELE_SDWFTX_PKT_TYPE_MCS_IDX9,
	QCA_WLAN_VENDOR_ATTR_TELE_SDWFTX_PKT_TYPE_MCS_IDX10,
	QCA_WLAN_VENDOR_ATTR_TELE_SDWFTX_PKT_TYPE_MCS_IDX11,
	QCA_WLAN_VENDOR_ATTR_TELE_SDWFTX_PKT_TYPE_MCS_IDX12,
	QCA_WLAN_VENDOR_ATTR_TELE_SDWFTX_PKT_TYPE_MCS_IDX13,
	QCA_WLAN_VENDOR_ATTR_TELE_SDWFTX_PKT_TYPE_MCS_IDX14,
	QCA_WLAN_VENDOR_ATTR_TELE_SDWFTX_PKT_TYPE_MCS_IDX15,
	QCA_WLAN_VENDOR_ATTR_TELE_SDWFTX_PKT_TYPE_MCS_IDX16,

	QCA_WLAN_VENDOR_ATTR_TELE_SDWFTX_PKT_TYPE_MCS_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_TELE_SDWFTX_PKT_TYPE_MCS_MAX =
		QCA_WLAN_VENDOR_ATTR_TELE_SDWFTX_PKT_TYPE_MCS_AFTER_LAST - 1,
};

enum qca_wlan_vendor_attr_tele_sdwftx_pkt_type {
	QCA_WLAN_VENDOR_ATTR_TELE_SDWFTX_PKT_TYPE_80211_A = 1,
	QCA_WLAN_VENDOR_ATTR_TELE_SDWFTX_PKT_TYPE_80211_B,
	QCA_WLAN_VENDOR_ATTR_TELE_SDWFTX_PKT_TYPE_80211_N,
	QCA_WLAN_VENDOR_ATTR_TELE_SDWFTX_PKT_TYPE_80211_AC,
	QCA_WLAN_VENDOR_ATTR_TELE_SDWFTX_PKT_TYPE_80211_AX,
	QCA_WLAN_VENDOR_ATTR_TELE_SDWFTX_PKT_TYPE_80211_BE,

	QCA_WLAN_VENDOR_ATTR_TELE_SDWFTX_PKT_TYPE_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_TELE_SDWFTX_PKT_TYPE_MAX =
		QCA_WLAN_VENDOR_ATTR_TELE_SDWFTX_PKT_TYPE_AFTER_LAST - 1,
};

enum qca_wlan_vendor_attr_tele_sdwftx {
	QCA_WLAN_VENDOR_ATTR_TELE_SDWFTX_SUCCESS = 1,
	QCA_WLAN_VENDOR_ATTR_TELE_SDWFTX_FAILED,
	QCA_WLAN_VENDOR_ATTR_TELE_SDWFTX_INGRESS,
	QCA_WLAN_VENDOR_ATTR_TELE_SDWFTX_DROP_RES,
	QCA_WLAN_VENDOR_ATTR_TELE_SDWFTX_QUEUE_DEPTH,
	QCA_WLAN_VENDOR_ATTR_TELE_SDWFTX_THROUGHPUT,
	QCA_WLAN_VENDOR_ATTR_TELE_SDWFTX_INGRESS_RATE,
	QCA_WLAN_VENDOR_ATTR_TELE_SDWFTX_MIN_THROUGHPUT,
	QCA_WLAN_VENDOR_ATTR_TELE_SDWFTX_MAX_THROUGHPUT,
	QCA_WLAN_VENDOR_ATTR_TELE_SDWFTX_AVG_THROUGHPUT,
	QCA_WLAN_VENDOR_ATTR_TELE_SDWFTX_PKT_ERROR_RATE,
	QCA_WLAN_VENDOR_ATTR_TELE_SDWFTX_RETRY_PERCENTAGE,
	QCA_WLAN_VENDOR_ATTR_TELE_SDWFTX_RETRY_PKTS_CNT,
	QCA_WLAN_VENDOR_ATTR_TELE_SDWFTX_TOTAL_RETRIES_CNT,
	QCA_WLAN_VENDOR_ATTR_TELE_SDWFTX_MULTIPLE_RETRIES_CNT,
	QCA_WLAN_VENDOR_ATTR_TELE_SDWFTX_FAILED_RETRIES_CNT,
	QCA_WLAN_VENDOR_ATTR_TELE_SDWFTX_REINJECT_PKTS,
	QCA_WLAN_VENDOR_ATTR_TELE_SDWFTX_PKT_TYPE,
	QCA_WLAN_VENDOR_ATTR_TELE_SDWFTX_SERVICE_INTERVAL,
	QCA_WLAN_VENDOR_ATTR_TELE_SDWFTX_BURST_SIZE,
	QCA_WLAN_VENDOR_ATTR_TELE_SDWFTX_TID,
	QCA_WLAN_VENDOR_ATTR_TELE_SDWFTX_QUEUE_ID,

	QCA_WLAN_VENDOR_ATTR_TELE_SDWFTX_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_TELE_SDWFTX_MAX =
		QCA_WLAN_VENDOR_ATTR_TELE_SDWFTX_AFTER_LAST - 1,
};

/**
 * enum qca_wlan_vendor_attr_afc_response: Defines attributes to be used
 * with vendor command QCA_NL80211_VENDOR_SUBCMD_AFC_RESPONSE. These attributes
 * will support sending only a single AFC response to the driver at a time.
 *
 * @QCA_WLAN_VENDOR_ATTR_AFC_RESP_DATA: Type is NLA_STRING. Required attribute.
 * This attribute will be used to send a single Spectrum Inquiry response object
 * from the 'availableSpectrumInquiryResponses' array object from the response
 * JSON.
 *
 * @QCA_WLAN_VENDOR_ATTR_AFC_RESP_TIME_TO_LIVE: Required u32 attribute.
 *
 * This attribute indicates the period (in seconds) for which the response
 * data received is valid for.
 *
 * @QCA_WLAN_VENDOR_ATTR_AFC_RESP_REQ_ID: Required u32 attribute.
 *
 * This attribute indicates the request ID for which the corresponding
 * response is being sent for. See also QCA_WLAN_VENDOR_ATTR_AFC_EVENT_REQ_ID.
 *
 * @QCA_WLAN_VENDOR_ATTR_AFC_RESP_EXP_DATE: Required u32 attribute.
 *
 * This attribute indicates the date until which the current response is
 * valid for in UTC format.
 * Date format: bits 7:0   - DD (Day 1-31)
 *              bits 15:8  - MM (Month 1-12)
 *              bits 31:16 - YYYY (Year)
 *
 * @QCA_WLAN_VENDOR_ATTR_AFC_RESP_EXP_TIME: Required u32 attribute.
 *
 * This attribute indicates the time until which the current response is
 * valid for in UTC format.
 * Time format: bits 7:0   - SS (Seconds 0-59)
 *              bits 15:8  - MM (Minutes 0-59)
 *              bits 23:16 - HH (Hours 0-23)
 *              bits 31:24 - Reserved
 *
 * @QCA_WLAN_VENDOR_ATTR_AFC_RESP_AFC_SERVER_RESP_CODE: Required s32 attribute.
 *
 * This attribute indicates the AFC response code. The AFC response codes are
 * in the following categories:
 * -1: General Failure.
 * 0: Success.
 * 100 - 199: General errors related to protocol.
 * 300 - 399: Error events specific to message exchange
 *            for the Available Spectrum Inquiry.
 *
 * @QCA_WLAN_VENDOR_ATTR_AFC_RESP_FREQ_PSD_INFO: Array of nested attributes
 * for PSD info of all the queried frequency ranges. It uses the attributes
 * defined in enum qca_wlan_vendor_attr_afc_freq_psd_info. Required attribute.
 *
 * @QCA_WLAN_VENDOR_ATTR_AFC_RESP_OPCLASS_CHAN_EIRP_INFO: Array of nested
 * attributes for EIRP info of all queried operating class/channels. It uses
 * the attributes defined in enum qca_wlan_vendor_attr_afc_opclass_info and
 * enum qca_wlan_vendor_attr_afc_chan_eirp_info. Required attribute.
 *
 */
enum qca_wlan_vendor_attr_afc_response {
	QCA_WLAN_VENDOR_ATTR_AFC_RESP_INVALID = 0,
	QCA_WLAN_VENDOR_ATTR_AFC_RESP_DATA = 1,
	QCA_WLAN_VENDOR_ATTR_AFC_RESP_TIME_TO_LIVE = 2,
	QCA_WLAN_VENDOR_ATTR_AFC_RESP_REQ_ID = 3,
	QCA_WLAN_VENDOR_ATTR_AFC_RESP_EXP_DATE = 4,
	QCA_WLAN_VENDOR_ATTR_AFC_RESP_EXP_TIME = 5,
	QCA_WLAN_VENDOR_ATTR_AFC_RESP_AFC_SERVER_RESP_CODE = 6,
	QCA_WLAN_VENDOR_ATTR_AFC_RESP_FREQ_PSD_INFO = 7,
	QCA_WLAN_VENDOR_ATTR_AFC_RESP_OPCLASS_CHAN_EIRP_INFO = 8,
	QCA_WLAN_VENDOR_ATTR_AFC_RESP_HW_IDX = 9,

	QCA_WLAN_VENDOR_ATTR_AFC_RESP_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_AFC_RESP_MAX =
	QCA_WLAN_VENDOR_ATTR_AFC_RESP_AFTER_LAST - 1,
};

/**
 * enum qca_wlan_vendor_6ghz_power_modes: Defines values of power modes a 6GHz
 * radio can operate in.
 * Enum used by QCA_WLAN_VENDOR_ATTR_6GHZ_REG_POWER_MODE attribute.
 *
 * @QCA_WLAN_VENDOR_6GHZ_PWR_MODE_AP_LPI: LPI AP
 *
 * @QCA_WLAN_VENDOR_6GHZ_PWR_MODE_AP_SP: SP AP
 *
 * @QCA_WLAN_VENDOR_6GHZ_PWR_MODE_AP_VLP: VLP AP
 *
 * @QCA_WLAN_VENDOR_6GHZ_PWR_MODE_REGULAR_CLIENT_LPI: LPI Regular Client
 *
 * @QCA_WLAN_VENDOR_6GHZ_PWR_MODE_REGULAR_CLIENT_SP: SP Regular Client
 *
 * @QCA_WLAN_VENDOR_6GHZ_PWR_MODE_REGULAR_CLIENT_VLP: VLP Regular Client
 *
 * @QCA_WLAN_VENDOR_6GHZ_PWR_MODE_SUBORDINATE_CLIENT_LPI: LPI Subordinate Client
 *
 */
enum qca_wlan_vendor_6ghz_power_modes {
	QCA_WLAN_VENDOR_6GHZ_PWR_MODE_AP_LPI = 0,
	QCA_WLAN_VENDOR_6GHZ_PWR_MODE_AP_SP = 1,
	QCA_WLAN_VENDOR_6GHZ_PWR_MODE_AP_VLP = 2,
	QCA_WLAN_VENDOR_6GHZ_PWR_MODE_REGULAR_CLIENT_LPI = 3,
	QCA_WLAN_VENDOR_6GHZ_PWR_MODE_REGULAR_CLIENT_SP = 4,
	QCA_WLAN_VENDOR_6GHZ_PWR_MODE_REGULAR_CLIENT_VLP = 5,
	QCA_WLAN_VENDOR_6GHZ_PWR_MODE_SUBORDINATE_CLIENT_LPI = 6,
	QCA_WLAN_VENDOR_6GHZ_PWR_MODE_SUBORDINATE_CLIENT_SP = 7,
	QCA_WLAN_VENDOR_6GHZ_PWR_MODE_SUBORDINATE_CLIENT_VLP = 8,
};

/**
 * enum qca_wlan_vendor_client_type - Client types for 6 GHz operation
 * @QCA_WLAN_VENDOR_CLIENT_TYPE_DEFAULT: Default client type operating under standard rules.
 * @QCA_WLAN_VENDOR_CLIENT_TYPE_SUBORDINATE: Subordinate client type, typically operating under
 *                                           a controlling AP.
 * @QCA_WLAN_VENDOR_CLIENT_TYPE_MAX: Maximum value placeholder for bounds checking and validation.
 */
enum qca_wlan_vendor_client_type {
	QCA_WLAN_VENDOR_CLIENT_TYPE_DEFAULT = 0,
	QCA_WLAN_VENDOR_CLIENT_TYPE_SUBORDINATE = 1,
	QCA_WLAN_VENDOR_CLIENT_TYPE_MAX,
};

/**
 * enum qca_wlan_vendor_set_6ghz_power_mode - Used by the vendor command
 * QCA_NL80211_VENDOR_SUBCMD_SET_6GHZ_POWER_MODE command to configure the
 * 6 GHz power mode.
 *
 * QCA_WLAN_VENDOR_ATTR_6GHZ_REG_POWER_MODE: Unsigned 8-bit integer representing
 * the 6 GHz power mode
 */
enum qca_wlan_vendor_set_6ghz_power_mode {
	QCA_WLAN_VENDOR_ATTR_6GHZ_REG_POWER_MODE_INVALID = 0,
	QCA_WLAN_VENDOR_ATTR_6GHZ_REG_POWER_MODE = 1,
	QCA_WLAN_VENDOR_ATTR_6GHZ_LINK_ID = 2,

	/* Keep last */
	QCA_WLAN_VENDOR_ATTR_6GHZ_REG_POWER_MODE_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_6GHZ_REG_POWER_MODE_MAX =
		QCA_WLAN_VENDOR_ATTR_6GHZ_REG_POWER_MODE_AFTER_LAST - 1,
};

/**
 * enum qca_wlan_vendor_attr_reg_eirp - Vendor attributes for regulatory EIRP handling
 * @QCA_WLAN_VENDOR_ATTR_REG_EIRP_INVALID: Invalid attribute (placeholder).
 * @QCA_WLAN_VENDOR_ATTR_REG_EIRP_POWER_TYPE: Attribute indicating the AP power mode type
 *                                            (e.g., LPI, SP, VLP) for 6 GHz regulatory
 *                                            configuration.
 * @QCA_WLAN_VENDOR_ATTR_REG_EIRP_CLIENT_TYPE: Attribute indicating the client type
 *                                             (e.g., default or subordinate) for 6 GHz
 *                                             regulatory configuration.
 * @QCA_WLAN_VENDOR_ATTR_REG_EIRP_MAX: Maximum attribute index (internal use for bounds checking).
 */
enum qca_wlan_vendor_attr_reg_eirp {
	QCA_WLAN_VENDOR_ATTR_REG_EIRP_INVALID = 0,
	QCA_WLAN_VENDOR_ATTR_REG_EIRP_POWER_TYPE,
	QCA_WLAN_VENDOR_ATTR_REG_EIRP_CLIENT_TYPE,
	QCA_WLAN_VENDOR_ATTR_REG_EIRP_MAX,
};

/**
 * enum qca_wlan_vendor_attr_reg_eirp_update - Vendor attributes for EIRP update
 * @QCA_WLAN_VENDOR_ATTR_REG_EIRP_UPDATE_INVALID: Invalid attribute (placeholder).
 * @QCA_WLAN_VENDOR_ATTR_REG_EIRP_UPDATE_CENTER_FREQ: Attribute representing the center
 *                                                    frequency (in MHz) of the channel.
 * @QCA_WLAN_VENDOR_ATTR_REG_EIRP_UPDATE_CHAN_NUM: Attribute representing the hardware channel
 *                                                 number.
 * @QCA_WLAN_VENDOR_ATTR_REG_EIRP_UPDATE_TX_POWER: Attribute representing the maximum regulatory
 *                                                 transmit power (in dBm).
 * @QCA_WLAN_VENDOR_ATTR_REG_EIRP_UPDATE_MAX: Maximum attribute index (used for validation and
 *                                            bounds checking).
 */
enum qca_wlan_vendor_attr_reg_eirp_update {
	QCA_WLAN_VENDOR_ATTR_REG_EIRP_UPDATE_INVALID = 0,
	QCA_WLAN_VENDOR_ATTR_REG_EIRP_UPDATE_CENTER_FREQ,
	QCA_WLAN_VENDOR_ATTR_REG_EIRP_UPDATE_CHAN_NUM,
	QCA_WLAN_VENDOR_ATTR_REG_EIRP_UPDATE_TX_POWER,
	QCA_WLAN_VENDOR_ATTR_REG_EIRP_UPDATE_MAX,
};

/**
 * enum qca_wlan_vendor_attr_240mhz_info - Represents the vendor specific
 * 240MHz information. This enum is used by
 * %QCA_NL80211_VENDOR_SUBCMD_240MHZ_INFO
 *
 * @QCA_WLAN_VENDOR_ATTR_240MHZ_BEAMFORMEE_SS: u8 mandatory attribute.
 * This is for the beamformee SS capability to indicate the maximum number of
 * spatial streams that the STA can receive in an EHT sounding NDP for 240 MHz.
 * The range of the vale is from 3 to 7.
 *
 * @QCA_WLAN_VENDOR_ATTR_240MHZ_NUM_SOUNDING_DIMENSIONS: u8 mandatory attribute.
 * This indicates the maximum value of the TXVECTOR parameter NUM_STS
 * supported by the beamformer for an EHT sounding NDP.
 *
 * @QCA_WLAN_VENDOR_ATTR_240MHZ_NON_OFDMA_UL_MUMIMO: flag optional attribute.
 * If present, this indicates the support for non-OFDMA UL MU-MIMO reception of
 * an EHT TB PPDU, for PPDU with 240MHz.
 *
 * @QCA_WLAN_VENDOR_ATTR_240MHZ_MU_BEAMFORMER: flag optional attribute.
 * If present, this indicates the support for non-OFDMA DL MU-MIMO transmission
 * and the required MU sounding, for PPDU 240MHz.
 *
 * @QCA_WLAN_VENDOR_ATTR_240MHZ_MCS_MAP: u8 array of size 3, mandatory
 * attribute. This indicates the maximum number of spatial streams supported for
 * reception and the maximum number of spatial streams that the STA can
 * transmit, for each MCS value, in a PPDU 240MHz.
 */
enum qca_wlan_vendor_attr_240mhz_info {
	QCA_WLAN_VENDOR_ATTR_240MHZ_INVALID = 0,
	QCA_WLAN_VENDOR_ATTR_240MHZ_BEAMFORMEE_SS = 1,
	QCA_WLAN_VENDOR_ATTR_240MHZ_NUM_SOUNDING_DIMENSIONS = 2,
	QCA_WLAN_VENDOR_ATTR_240MHZ_NON_OFDMA_UL_MUMIMO = 3,
	QCA_WLAN_VENDOR_ATTR_240MHZ_MU_BEAMFORMER = 4,
	QCA_WLAN_VENDOR_ATTR_240MHZ_MCS_MAP = 5,

	/* keep last */
	QCA_WLAN_VENDOR_ATTR_240MHZ_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_240MHZ_MAX =
	QCA_WLAN_VENDOR_ATTR_240MHZ_AFTER_LAST - 1,
};

/**
 * ath12k_vendor_send_6ghz_power_mode_update_complete - Send 6 GHz power mode
 * update vendor NL event to the application.
 *
 * @ar - Pointer to ar
 * @wdev - Pointer to wdev
 * @link_id - Link ID for which the power mode update is complete
 */
int
ath12k_vendor_send_6ghz_power_mode_update_complete(struct ath12k *ar,
						   struct wireless_dev *wdev,
						   u8 link_id);

enum qca_wlan_vendor_attr_sdwf_phy {
	QCA_WLAN_VENDOR_ATTR_SDWF_PHY_INVALID = 0,
	QCA_WLAN_VENDOR_ATTR_SDWF_PHY_OPERATION = 1,
	QCA_WLAN_VENDOR_ATTR_SDWF_PHY_SVC_PARAMS = 2,
	QCA_WLAN_VENDOR_ATTR_SDWF_PHY_SLA_SAMPLES_PARAMS = 3,
	QCA_WLAN_VENDOR_ATTR_SDWF_PHY_SLA_DETECT_PARAMS = 4,
	QCA_WLAN_VENDOR_ATTR_SDWF_PHY_SLA_THRESHOLD_PARAMS = 5,

	/* keep last */
	QCA_WLAN_VENDOR_ATTR_SDWF_PHY_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_SDWF_PHY_MAX =
	QCA_WLAN_VENDOR_ATTR_SDWF_PHY_AFTER_LAST - 1,
};

enum qca_wlan_vendor_sdwf_phy_oper {
	QCA_WLAN_VENDOR_SDWF_PHY_OPER_SVC_SET = 0,
	QCA_WLAN_VENDOR_SDWF_PHY_OPER_SVC_DEL = 1,
	QCA_WLAN_VENDOR_SDWF_PHY_OPER_SVC_GET = 2,
	QCA_WLAN_VENDOR_SDWF_PHY_OPER_SLA_SAMPLES_SET = 3,
	QCA_WLAN_VENDOR_SDWF_PHY_OPER_SLA_BREACH_DETECTION_SET = 4,
	QCA_WLAN_VENDOR_SDWF_PHY_OPER_SLA_THRESHOLD_SET = 5,
};

enum qca_wlan_vendor_attr_sdwf_svc {
	QCA_WLAN_VENDOR_ATTR_SDWF_SVC_INVALID = 0,
	QCA_WLAN_VENDOR_ATTR_SDWF_SVC_ID = 1,
	QCA_WLAN_VENDOR_ATTR_SDWF_SVC_MIN_TP = 2,
	QCA_WLAN_VENDOR_ATTR_SDWF_SVC_MAX_TP = 3,
	QCA_WLAN_VENDOR_ATTR_SDWF_SVC_BURST_SIZE = 4,
	QCA_WLAN_VENDOR_ATTR_SDWF_SVC_INTERVAL = 5,
	QCA_WLAN_VENDOR_ATTR_SDWF_SVC_DELAY_BOUND = 6,
	QCA_WLAN_VENDOR_ATTR_SDWF_SVC_MSDU_TTL = 7,
	QCA_WLAN_VENDOR_ATTR_SDWF_SVC_PRIO = 8,
	QCA_WLAN_VENDOR_ATTR_SDWF_SVC_TID = 9,
	QCA_WLAN_VENDOR_ATTR_SDWF_SVC_MSDU_RATE_LOSS = 10,
	QCA_WLAN_VENDOR_ATTR_SDWF_SVC_UL_SVC_INTERVAL = 11,
	QCA_WLAN_VENDOR_ATTR_SDWF_SVC_UL_MIN_TPUT = 12,
	QCA_WLAN_VENDOR_ATTR_SDWF_SVC_UL_MAX_LATENCY = 13,
	QCA_WLAN_VENDOR_ATTR_SDWF_SVC_UL_BURST_SIZE = 14,
	QCA_WLAN_VENDOR_ATTR_SDWF_SVC_UL_OFDMA_DISABLE = 15,
	QCA_WLAN_VENDOR_ATTR_SDWF_SVC_UL_MU_MIMO_DISABLE = 16,
	/* The below are used by MCC */
	QCA_WLAN_VENDOR_ATTR_SDWF_SVC_BUFFER_LATENCY_TOLERANCE = 17,
	QCA_WLAN_VENDOR_ATTR_SDWF_SVC_TX_TRIGGER_DSCP = 18,
	QCA_WLAN_VENDOR_ATTR_SDWF_SVC_TX_REPLACE_DSCP = 19,

	QCA_WLAN_VENDOR_ATTR_SDWF_SVC_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_SDWF_SVC_MAX =
	QCA_WLAN_VENDOR_ATTR_SDWF_SVC_AFTER_LAST - 1,
};

enum qca_wlan_vendor_attr_atf_offload_oper {
	QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_INVALID = 0,
	QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_OPERATION = 1,
	QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_RADIO_ID = 2,
	QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_ENABLE_DISABLE_CONFIG = 3,
	QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_SSID_GROUP_CONFIG = 4,
	QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_WMM_AC_CONFIG = 5,
	QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_PEER_CONFIG = 6,
	QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_ENABLE_DISABLE_STATS_CONFIG = 7,
	QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_ENABLE_DISABLE_STRICT_SCH_CONFIG = 8,
	QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_VO_DEDICATED_TIME_CONFIG = 9,
	QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_VI_DEDICATED_TIME_CONFIG = 10,
	QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_SCHED_DURATION_CONFIG = 11,
	QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_SSID_SCHED_POLICY = 12,
	QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_STATS_TIMEOUT = 13,
	QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_STATS = 14,

	/* keep last */
	QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_MAX =
		QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_AFTER_LAST - 1
};

enum qca_wlan_vendor_attr_atf_stats {
	QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_STATS_INVALID = 0,
	QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_RADIO_TX_BE_AIRTIME = 1,
	QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_RADIO_TX_BK_AIRTIME = 2,
	QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_RADIO_TX_VI_AIRTIME = 3,
	QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_RADIO_TX_VO_AIRTIME = 4,
	QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_RADIO_RX_BE_AIRTIME = 5,
	QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_RADIO_RX_BK_AIRTIME = 6,
	QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_RADIO_RX_VI_AIRTIME = 7,
	QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_RADIO_RX_VO_AIRTIME = 8,
	QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_PEER_STATS = 9,

	QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_STATS_LAST,
	QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_STATS_MAX =
		QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_STATS_LAST - 1,
};

enum qca_wlan_vendor_attr_atf_peer_stats {
	QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_PEER_STATS_INVALID = 0,
	QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_PEER_STATS_MAC = 1,
	QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_PEER_TX_BE_AIRTIME = 2,
	QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_PEER_TX_BK_AIRTIME = 3,
	QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_PEER_TX_VI_AIRTIME = 4,
	QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_PEER_TX_VO_AIRTIME = 5,
	QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_PEER_RX_BE_AIRTIME = 6,
	QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_PEER_RX_BK_AIRTIME = 7,
	QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_PEER_RX_VI_AIRTIME = 8,
	QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_PEER_RX_VO_AIRTIME = 9,

	QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_PEER_STATS_LAST,
	QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_PEER_STATS_MAX =
		QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_PEER_STATS_LAST - 1,
};

enum qca_wlan_vendor_attr_atf_offload_ssid_group_config {
	QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_SSID_GROUP_INVALID = 0,
	QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_SSID_GROUP_INDEX = 1,
	QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_SSID_GROUP_AIRTIME_CONFIGURED = 2,
	QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_SSID_GROUP_POLICY = 3,
	QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_SSID_GROUP_UNCONFIGURED_PEERS = 4,
	QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_SSID_GROUP_CONFIGURED_PEERS = 5,
	QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_SSID_GROUP_UNCONFIGURED_PEERS_AIRTIME = 6,

	/* keep last */
	QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_SSID_GROUP_LAST,
	QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_SSID_GROUP_MAX =
	QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_SSID_GROUP_LAST - 1,
};

enum qca_wlan_vendor_attr_atf_offload_peer_config {
	QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_PEER_INVALID = 0,
	QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_NUMBER_OF_PEERS = 1,
	QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_PEER_FULL_UPDATE = 2,
	QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_PEER_MORE = 3,
	QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_PEER_PAYLOAD = 4,

	/* keep last */
	QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_PEER_CONFIG_LAST,
	QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_PEER_CONFIG_MAX =
	QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_PEER_CONFIG_LAST - 1,
};

enum qca_wlan_vendor_attr_atf_offload_peer {
	QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_PEER_AIRTIME_INVALID = 0,
	QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_PEER_MAC = 1,
	QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_PEER_AIRTIME = 2,
	QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_PEER_GROUP_INDEX = 3,
	QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_PEER_CONFIGURED = 4,

	/* keep last */
	QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_PEER_AIRTIME_MAX,
	QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_PEER_AIRTIME_LAST =
	QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_PEER_AIRTIME_MAX - 1,
};

enum qca_wlan_vendor_atf_offload_sched_duration {
	QCA_WLAN_VENDOR_ATF_OFFLOAD_SCHED_DURATION_INVALID = 0,
	QCA_WLAN_VENDOR_ATF_OFFLOAD_SCHED_AC = 1,
	QCA_WLAN_VENDOR_ATF_OFFLOAD_SCHED_DURATION = 2,

	QCA_WLAN_VENDOR_ATF_OFFLOAD_SCHED_DURATION_LAST,
	QCA_WLAN_VENDOR_ATF_OFFLOAD_SCHED_DURATION_MAX =
		QCA_WLAN_VENDOR_ATF_OFFLOAD_SCHED_DURATION_LAST - 1,
};

enum qca_wlan_vendor_atf_offload_ssid_scheduling {
	QCA_WLAN_VENDOR_ATF_OFFLOAD_SSID_SCHED_INVALID = 0,
	QCA_WLAN_VENDOR_ATF_OFFLOAD_LINK_ID = 1,
	QCA_WLAN_VENDOR_ATF_OFFLOAD_SSID_SCHED = 2,

	/*keep last */
	QCA_WLAN_VENDOR_ATF_OFFLOAD_SSID_SCHED_LAST,
	QCA_WLAN_VENDOR_ATF_OFFLOAD_SSID_SCHED_MAX =
		QCA_WLAN_VENDOR_ATF_OFFLOAD_SSID_SCHED_LAST - 1,
};

enum qca_wlan_vendor_atf_offload_operations {
	QCA_WLAN_VENDOR_ATF_OFFLOAD_ENABLE_DISABLE = 0,
	QCA_WLAN_VENDOR_ATF_OFFLOAD_SSID_GROUP = 1,
	QCA_WLAN_VENDOR_ATF_OFFLOAD_WMM_AC = 2,
	QCA_WLAN_VENDOR_ATF_OFFLOAD_PEER = 3,
	QCA_WLAN_VENDOR_ATF_OFFLOAD_STATS_ENABLE_DISABLE = 4,
	QCA_WLAN_VENDOR_ATF_OFFLOAD_STRICT_SCH = 5,
	QCA_WLAN_VENDOR_ATF_OFFLOAD_VO_TIME = 6,
	QCA_WLAN_VENDOR_ATF_OFFLOAD_VI_TIME = 7,
	QCA_WLAN_VENDOR_ATF_OFFLOAD_SCHED = 8,
	QCA_WLAN_VENDOR_ATF_OFFLOAD_SSID_SCHEDULING = 9,
	QCA_WLAN_VENDOR_ATF_OFFLOAD_STATS_TIME_OUT = 10,
};

enum qca_wlan_vendor_attr_sdwf_sla_samples {
	QCA_WLAN_VENDOR_ATTR_SDWF_SLA_SAMPLES_INVALID = 0,
	QCA_WLAN_VENDOR_ATTR_SDWF_SLA_SAMPLES_MOVING_AVG_PKT = 1,
	QCA_WLAN_VENDOR_ATTR_SDWF_SLA_SAMPLES_MOVING_AVG_WIN = 2,
	QCA_WLAN_VENDOR_ATTR_SDWF_SLA_SAMPLES_SLA_NUM_PKT = 3,
	QCA_WLAN_VENDOR_ATTR_SDWF_SLA_SAMPLES_SLA_TIME_SEC = 4,

	/* keep last */
	QCA_WLAN_VENDOR_ATTR_SDWF_SLA_SAMPLES_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_SDWF_SLA_SAMPLES_MAX =
	QCA_WLAN_VENDOR_ATTR_SDWF_SLA_SAMPLES_AFTER_LAST - 1,
};

enum qca_wlan_vendor_attr_sdwf_sla_detect {
	QCA_WLAN_VENDOR_ATTR_SDWF_SLA_DETECT_INVALID = 0,
	QCA_WLAN_VENDOR_ATTR_SDWF_SLA_DETECT_PARAM = 1,
	QCA_WLAN_VENDOR_ATTR_SDWF_SLA_DETECT_MIN_TP = 2,
	QCA_WLAN_VENDOR_ATTR_SDWF_SLA_DETECT_MAX_TP = 3,
	QCA_WLAN_VENDOR_ATTR_SDWF_SLA_DETECT_BURST_SIZE = 4,
	QCA_WLAN_VENDOR_ATTR_SDWF_SLA_DETECT_INTERVAL = 5,
	QCA_WLAN_VENDOR_ATTR_SDWF_SLA_DETECT_DELAY_BOUND = 6,
	QCA_WLAN_VENDOR_ATTR_SDWF_SLA_DETECT_MSDU_TTL = 7,
	QCA_WLAN_VENDOR_ATTR_SDWF_SLA_DETECT_MSDU_RATE_LOSS = 8,
	QCA_WLAN_VENDOR_ATTR_SDWF_SLA_DETECT_PKT_ERROR_RATE = 9,
	QCA_WLAN_VENDOR_ATTR_SDWF_SLA_DETECT_MCS_MIN_THRESHOLD = 10,
	QCA_WLAN_VENDOR_ATTR_SDWF_SLA_DETECT_MCS_MAX_THRESHOLD = 11,
	QCA_WLAN_VENDOR_ATTR_SDWF_SLA_DETECT_RETRIES_THRESHOLD = 12,

	/* keep last */
	QCA_WLAN_VENDOR_ATTR_SDWF_SLA_DETECT_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_SDWF_SLA_DETECT_MAX =
	QCA_WLAN_VENDOR_ATTR_SDWF_SLA_DETECT_AFTER_LAST - 1,
};

enum qca_wlan_vendor_sdwf_sla_detect_param {
	QCA_WLAN_VENDOR_SDWF_SLA_DETECT_PARAM_NUM_PACKET,
	QCA_WLAN_VENDOR_SDWF_SLA_DETECT_PARAM_PER_SECOND,
	QCA_WLAN_VENDOR_SDWF_SLA_DETECT_PARAM_MOV_AVG,
	QCA_WLAN_VENDOR_SDWF_SLA_DETECT_PARAM_NUM_SECOND,
	QCA_WLAN_VENDOR_SDWF_SLA_DETECT_PARAM_MAX,
};

enum qca_wlan_vendor_attr_sdwf_sla_threshold {
	QCA_WLAN_VENDOR_ATTR_SDWF_SLA_THRESHOLD_INVALID = 0,
	QCA_WLAN_VENDOR_ATTR_SDWF_SLA_THRESHOLD_SVC_ID = 1,
	QCA_WLAN_VENDOR_ATTR_SDWF_SLA_THRESHOLD_MIN_TP = 2,
	QCA_WLAN_VENDOR_ATTR_SDWF_SLA_THRESHOLD_MAX_TP = 3,
	QCA_WLAN_VENDOR_ATTR_SDWF_SLA_THRESHOLD_BURST_SIZE = 4,
	QCA_WLAN_VENDOR_ATTR_SDWF_SLA_THRESHOLD_INTERVAL = 5,
	QCA_WLAN_VENDOR_ATTR_SDWF_SLA_THRESHOLD_DELAY_BOUND = 6,
	QCA_WLAN_VENDOR_ATTR_SDWF_SLA_THRESHOLD_MSDU_TTL = 7,
	QCA_WLAN_VENDOR_ATTR_SDWF_SLA_THRESHOLD_MSDU_RATE_LOSS = 8,
	QCA_WLAN_VENDOR_ATTR_SDWF_SLA_THRESHOLD_PKT_ERROR_RATE = 9,
	QCA_WLAN_VENDOR_ATTR_SDWF_SLA_THRESHOLD_MCS_MIN_THRESHOLD = 10,
	QCA_WLAN_VENDOR_ATTR_SDWF_SLA_THRESHOLD_MCS_MAX_THRESHOLD = 11,
	QCA_WLAN_VENDOR_ATTR_SDWF_SLA_THRESHOLD_RETRIES_THRESHOLD = 12,

	/* keep last */
	QCA_WLAN_VENDOR_ATTR_SDWF_SLA_THRESHOLD_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_SDWF_SLA_THRESHOLD_MAX =
	QCA_WLAN_VENDOR_ATTR_SDWF_SLA_THRESHOLD_AFTER_LAST - 1,
};

/**
 * enum qca_wlan_vendor_attr_rm_generic - Attributes required for vendor
 * command %QCA_NL80211_VENDOR_SUBCMD_RM_GENERIC to register a Resource Manager
 * with the driver.
 *
 * @QCA_WLAN_VENDOR_ATTR_RM_GENERIC_ERP: Nested attribute used for commands
 * and events related to ErP (Energy related Products),
 * see @enum qca_wlan_vendor_attr_erp_ath for details.
 */
enum qca_wlan_vendor_attr_rm_generic {
	QCA_WLAN_VENDOR_ATTR_RM_GENERIC_APP_VERSION = 1,
	QCA_WLAN_VENDOR_ATTR_RM_GENERIC_DRIVER_VERSION = 2,
	QCA_WLAN_VENDOR_ATTR_RM_GENERIC_NUM_SOC_DEVICES = 3,
	QCA_WLAN_VENDOR_ATTR_RM_GENERIC_SOC_DEVICE_INFO = 4,
	QCA_WLAN_VENDOR_ATTR_RM_GENERIC_TTLM_MAPPING = 5,
	QCA_WLAN_VENDOR_ATTR_RM_GENERIC_RELAYFS_FILE_NAME_PMLO = 6,
	QCA_WLAN_VENDOR_ATTR_RM_GENERIC_LINK_BW_NSS_CHANGE = 7,
	QCA_WLAN_VENDOR_ATTR_RM_GENERIC_RELAYFS_FILE_NAME_DETSCHED = 8,
	QCA_WLAN_VENDOR_ATTR_RM_GENERIC_CATEGORY = 9,
	QCA_WLAN_VENDOR_ATTR_RM_GENERIC_ASSOC_NUM_LINKS = 10,
	QCA_WLAN_VENDOR_ATTR_RM_GENERIC_ASSOC_PEER_LINK_ENTRY = 11,
	QCA_WLAN_VENDOR_ATTR_RM_GENERIC_ASSOC_TTLM_INFO = 12,
	QCA_WLAN_VENDOR_ATTR_RM_GENERIC_ERP = 13,
	QCA_WLAN_VENDOR_ATTR_RM_GENERIC_MLD_MAC_ADDR = 14,
	QCA_WLAN_VENDOR_ATTR_RM_GENERIC_SERVICE_ID = 15,
	QCA_WLAN_VENDOR_ATTR_RM_GENERIC_SERVICE_DATA = 16,
	QCA_WLAN_VENDOR_ATTR_RM_GENERIC_DYNAMIC_INIT_CONF = 17,

	/* keep last */
	QCA_WLAN_VENDOR_ATTR_RM_GENERIC_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_RM_GENERIC_MAX =
		QCA_WLAN_VENDOR_ATTR_RM_GENERIC_AFTER_LAST - 1,
};

/**
 * enum qca_wlan_vendor_attr_erp_ath - Parameters to support ErP in ath driver.
 *
 * @QCA_WLAN_VENDOR_ATTR_ERP_INVALID: Invalid attribute
 *
 * @QCA_WLAN_VENDOR_ATTR_ERP_ENTER_START: Flag, set to true will trigger
 * driver's entry into ErP mode.
 *
 * @QCA_WLAN_VENDOR_ATTR_ERP_ENTER_COMPLETE: Flag to indicate that ErP
 * parameter configuration is complete. This can be included along with flags
 * QCA_WLAN_VENDOR_ATTR_ERP_ENTER_START and
 * QCA_WLAN_VENDOR_ATTR_ERP_CONFIG.
 *
 * @QCA_WLAN_VENDOR_ATTR_ERP_CONFIG: Optional nested attribute for ErP
 * parameters. Flag QCA_WLAN_VENDOR_ATTR_ERP_ENTER_START must be sent
 * either before or when the first time this flag is included. See
 * @enum qca_wlan_vendor_attr_erp_ath_config for details.
 *
 * @QCA_WLAN_VENDOR_ATTR_ERP_EXIT: Flag, set to true will trigger exit from
 * ErP mode. Driver uses this flag to send vendor event
 * %QCA_NL80211_VENDOR_SUBCMD_RM_GENERIC to userspace upon receiving a packet
 * matching a previously configured filter. Userspace can also trigger driver's
 * exit from ErP using this flag.
 */
enum qca_wlan_vendor_attr_erp_ath {
	QCA_WLAN_VENDOR_ATTR_ERP_INVALID = 0,
	QCA_WLAN_VENDOR_ATTR_ERP_ENTER_START = 1,
	QCA_WLAN_VENDOR_ATTR_ERP_ENTER_COMPLETE = 2,
	QCA_WLAN_VENDOR_ATTR_ERP_CONFIG = 3,
	QCA_WLAN_VENDOR_ATTR_ERP_EXIT = 4,

	/* keep last */
	QCA_WLAN_VENDOR_ATTR_ERP_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_ERP_MAX = QCA_WLAN_VENDOR_ATTR_ERP_AFTER_LAST - 1,
};

/**
 * enum qca_wlan_vendor_trigger_types - Types of ErP wake up trigger
 */
enum qca_wlan_vendor_trigger_types {
	QCA_WLAN_VENDOR_TRIGGER_TYPE_ARP_IPV4,
	QCA_WLAN_VENDOR_TRIGGER_TYPE_NS_IPV6,
	QCA_WLAN_VENDOR_TRIGGER_TYPE_IGMP_IPV4,
	QCA_WLAN_VENDOR_TRIGGER_TYPE_MLD_IPV6,
	QCA_WLAN_VENDOR_TRIGGER_TYPE_DHCP_IPV4,
	QCA_WLAN_VENDOR_TRIGGER_TYPE_DHCP_IPV6,
	QCA_WLAN_VENDOR_TRIGGER_TYPE_DNS_TCP_IPV4,
	QCA_WLAN_VENDOR_TRIGGER_TYPE_DNS_TCP_IPV6,
	QCA_WLAN_VENDOR_TRIGGER_TYPE_DNS_UDP_IPV4,
	QCA_WLAN_VENDOR_TRIGGER_TYPE_DNS_UDP_IPV6,
	QCA_WLAN_VENDOR_TRIGGER_TYPE_ICMP_IPV4,
	QCA_WLAN_VENDOR_TRIGGER_TYPE_ICMP_IPV6,
	QCA_WLAN_VENDOR_TRIGGER_TYPE_TCP_IPV4,
	QCA_WLAN_VENDOR_TRIGGER_TYPE_TCP_IPV6,
	QCA_WLAN_VENDOR_TRIGGER_TYPE_UDP_IPV4,
	QCA_WLAN_VENDOR_TRIGGER_TYPE_UDP_IPV6,
	QCA_WLAN_VENDOR_TRIGGER_TYPE_IPV4,
	QCA_WLAN_VENDOR_TRIGGER_TYPE_IPV6,
	QCA_WLAN_VENDOR_TRIGGER_TYPE_EAP,
	QCA_WLAN_VENDOR_TRIGGER_TYPE_MAX,
};

/**
 * enum qca_wlan_vendor_attr_erp_ath_config - Parameters to support ErP.
 *
 * @QCA_WLAN_VENDOR_ATTR_ERP_CONFIG_INVALID: Invalid attribute
 *
 * @QCA_WLAN_VENDOR_ATTR_ERP_CONFIG_IFINDEX: (u32) Interface index. This is
 * a mandatory attribute for setting packet trigger for the designated wake up
 * interface along with %QCA_WLAN_VENDOR_ATTR_ERP_CONFIG_TRIGGER).
 *
 * @QCA_WLAN_VENDOR_ATTR_ERP_CONFIG_TRIGGER: (u32) Attribute used
 * to set wake-up trigger to bring the device out of ErP mode. This is bitmap
 * where each bit corresponds to the values defined in
 * enum qca_wlan_vendor_trigger_types.
 *
 * @QCA_WLAN_VENDOR_ATTR_ERP_CONFIG_PCIE_REMOVE: flag, set if the driver should
 * remove PCIe slot.
 */
enum qca_wlan_vendor_attr_erp_ath_config {
	QCA_WLAN_VENDOR_ATTR_ERP_CONFIG_INVALID = 0,
	QCA_WLAN_VENDOR_ATTR_ERP_CONFIG_IFINDEX = 1,
	QCA_WLAN_VENDOR_ATTR_ERP_CONFIG_TRIGGER = 2,
	QCA_WLAN_VENDOR_ATTR_ERP_CONFIG_PCIE_REMOVE = 3,
	QCA_WLAN_VENDOR_ATTR_ERP_CONFIG_PCIE_SPEED_WIDTH = 4,

	/* keep last */
	QCA_WLAN_VENDOR_ATTR_ERP_CONFIG_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_ERP_CONFIG_MAX =
		QCA_WLAN_VENDOR_ATTR_ERP_CONFIG_AFTER_LAST - 1,
};

enum qca_vendor_wlan_telemetry_attr {
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_HIERARCHY_TYPE = 1,
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_FEATURE,
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_STA_MAC,
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_REQUEST_ID,
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_LINK_ID,
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_SVC_ID,

	QCA_VENDOR_ATTR_WLAN_TELEMETRY_AFTER_LAST,
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_MAX =
		QCA_VENDOR_ATTR_WLAN_TELEMETRY_AFTER_LAST - 1,
};

enum qca_vendor_wlan_telemetry_feat_attr {
	QCA_VENDOR_ATTR_WLAN_FEAT_TX = 1,
	QCA_VENDOR_ATTR_WLAN_FEAT_RX,
	QCA_VENDOR_ATTR_WLAN_FEAT_SDWFTX,
	QCA_VENDOR_ATTR_WLAN_FEAT_SDWFDELAY,

	QCA_VENDOR_ATTR_WLAN_FEAT_AFTER_LAST,
	QCA_VENDOR_ATTR_WLAN_FEAT_MAX =
		QCA_VENDOR_ATTR_WLAN_FEAT_AFTER_LAST - 1,
};

enum qca_vendor_wlan_telemetry_event_attr {
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_OBJECT_EVENT = 1,
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_LINK_ID_EVENT,
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_REQUEST_ID_EVENT,
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_TX_STATS_EVENT,
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_RX_STATS_EVENT,
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_SVC_ID_EVENT,
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_SDWFTX_STATS_EVENT,
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_SDWFDELAY_STATS_EVENT,

	QCA_VENDOR_ATTR_WLAN_TELEMETRY_EVENT_AFTER_LAST,
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_EVENT_MAX =
		QCA_VENDOR_ATTR_WLAN_TELEMETRY_EVENT_AFTER_LAST - 1,
};

enum qca_vendor_wlan_telemetry_rx_stats_attr {
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_RXDMA_ERR_EVENT = 1,
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_REO_ERR_EVENT,
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_RX_WBM_SW_DROP_REASON_EVENT,
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_REO_SW_DROP_REASON_EVENT,
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_RX_PER_PKT_STATS_EVENT,

	QCA_VENDOR_ATTR_WLAN_TELEMETRY_RX_STATS_AFTER_LAST,
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_RX_STATS_MAX =
		QCA_VENDOR_ATTR_WLAN_TELEMETRY_RX_STATS_AFTER_LAST - 1,
};

enum qca_vendor_wlan_telemetry_tx_stats_attr {
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_TX_STATS_INVALID = 0,
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_TX_COMP_ERR_EVENT = 1,
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_TX_PER_PKT_STATS_EVENT,
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_TX_INGRESS_STATS_EVENT,

	QCA_VENDOR_ATTR_WLAN_TELEMETRY_TX_STATS_AFTER_LAST,
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_TX_STATS_MAX_EVENT =
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_TX_STATS_AFTER_LAST - 1,
};

enum qca_vendor_wlan_telemetry_reo_attr {
	QCA_VENDOR_ATTR_REO_ERR_QUEUE_DESC_ADDR_0 = 1,
	QCA_VENDOR_ATTR_REO_ERR_QUEUE_DESC_INVALID,
	QCA_VENDOR_ATTR_REO_ERR_AMPDU_IN_NON_BA,
	QCA_VENDOR_ATTR_REO_ERR_NON_BA_DUPLICATE,
	QCA_VENDOR_ATTR_REO_ERR_BA_DUPLICATE,
	QCA_VENDOR_ATTR_REO_ERR_REGULAR_FRAME_2K_JUMP,
	QCA_VENDOR_ATTR_REO_ERR_BAR_FRAME_2K_JUMP,
	QCA_VENDOR_ATTR_REO_ERR_REGULAR_FRAME_OOR,
	QCA_VENDOR_ATTR_REO_ERR_BAR_FRAME_OOR,
	QCA_VENDOR_ATTR_REO_ERR_BAR_FRAME_NO_BA_SESSION,
	QCA_VENDOR_ATTR_REO_ERR_BAR_FRAME_SN_EQUALS_SSN,
	QCA_VENDOR_ATTR_REO_ERR_PN_CHECK_FAILED,
	QCA_VENDOR_ATTR_REO_ERR_2K_ERROR_HANDLING_FLAG_SET,
	QCA_VENDOR_ATTR_REO_ERR_PN_ERROR_HANDLING_FLAG_SET,
	QCA_VENDOR_ATTR_REO_ERR_QUEUE_DESC_BLOCKED_SET,

	QCA_VENDOR_ATTR_REO_ERR_AFTER_LAST,
	QCA_VENDOR_ATTR_REO_ERR_MAX =
		QCA_VENDOR_ATTR_REO_ERR_AFTER_LAST - 1,
};

enum qca_vendor_wlan_telemetry_rxdma_attr {
	QCA_VENDOR_ATTR_RXDMA_ERR_OVERFLOW = 1,
	QCA_VENDOR_ATTR_RXDMA_ERR_MPDU_LENGTH,
	QCA_VENDOR_ATTR_RXDMA_ERR_FCS,
	QCA_VENDOR_ATTR_RXDMA_ERR_DECRYPT,
	QCA_VENDOR_ATTR_RXDMA_ERR_TKIP_MIC,
	QCA_VENDOR_ATTR_RXDMA_ERR_UNENCRYPTED,
	QCA_VENDOR_ATTR_RXDMA_ERR_MSDU_LEN,
	QCA_VENDOR_ATTR_RXDMA_ERR_MSDU_LIMIT,
	QCA_VENDOR_ATTR_RXDMA_ERR_WIFI_PARSE,
	QCA_VENDOR_ATTR_RXDMA_ERR_AMSDU_PARSE,
	QCA_VENDOR_ATTR_RXDMA_ERR_SA_TIMEOUT,
	QCA_VENDOR_ATTR_RXDMA_ERR_DA_TIMEOUT,
	QCA_VENDOR_ATTR_RXDMA_ERR_FLOW_TIMEOUT,
	QCA_VENDOR_ATTR_RXDMA_ERR_FLUSH_REQUEST,
	QCA_VENDOR_ATTR_RXDMA_AMSDU_FRAGMENT,
	QCA_VENDOR_ATTR_RXDMA_MULTICAST_ECHO,
	QCA_VENDOR_ATTR_RXDMA_AMSDU_ADDR_MISMATCH,
	QCA_VENDOR_ATTR_RXDMA_UNAUTH_WDS,
	QCA_VENDOR_ATTR_RXDMA_GROUPCAST_AMSDU_OR_WDS,

	QCA_VENDOR_ATTR_RXDMA_AFTER_LAST,
	QCA_VENDOR_ATTR_RXDMA_ERR_MAX =
		QCA_VENDOR_ATTR_RXDMA_AFTER_LAST - 1,
};

enum qca_vendor_wlan_telemetry_rx_wbm_sw_drop_reason {
	QCA_VENDOR_ATTR_WBM_SW_DROP_GET_SW_DESC_ERROR = 1,
	QCA_VENDOR_ATTR_WBM_SW_DROP_GET_SW_DESC_FROM_CK_ERROR,
	QCA_VENDOR_ATTR_WBM_SW_DROP_INVALID_PEER_ID_ERROR,
	QCA_VENDOR_ATTR_WBM_SW_DROP_DESC_PARSE_ERROR,
	QCA_VENDOR_ATTR_WBM_SW_DROP_INVALID_COOKIE,
	QCA_VENDOR_ATTR_WBM_SW_DROP_INVALID_PUSH_REASON,
	QCA_VENDOR_ATTR_WBM_SW_DROP_INVALID_HW_ID,
	QCA_VENDOR_ATTR_WBM_SW_DROP_NULL_PARTNER_DP,
	QCA_VENDOR_ATTR_WBM_SW_DROP_PROCESS_NULL_PARTNER_DP,
	QCA_VENDOR_ATTR_WBM_SW_DROP_NULL_PDEV,
	QCA_VENDOR_ATTR_WBM_SW_DROP_NULL_AR,
	QCA_VENDOR_ATTR_WBM_SW_DROP_CAC_RUNNING,
	QCA_VENDOR_ATTR_WBM_SW_DROP_SCATTER_GATHER,
	QCA_VENDOR_ATTR_WBM_SW_DROP_INVALID_NWIFI_HDR_LEN,
	QCA_VENDOR_ATTR_WBM_SW_DROP_REO_GENERIC,
	QCA_VENDOR_ATTR_WBM_SW_DROP_RXDMA_GENERIC,

	QCA_VENDOR_ATTR_WBM_SW_DROP_AFTER_LAST,
	QCA_VENDOR_ATTR_WBM_SW_DROP_MAX =
		QCA_VENDOR_ATTR_WBM_SW_DROP_AFTER_LAST - 1,
};

enum qca_vendor_wlan_telemetry_reo_sw_drop_reason {
	QCA_VENDOR_ATTR_REO_SUCCESS  = 1,
	QCA_VENDOR_ATTR_REO_SW_DROP_MISCELLANEOUS,
	QCA_VENDOR_ATTR_REO_SW_DROP_GET_SW_DESC_FROM_CK_ERROR,
	QCA_VENDOR_ATTR_REO_SW_DROP_GET_SW_DESC_ERROR,
	QCA_VENDOR_ATTR_REO_SW_DROP_REPLENISH,
	QCA_VENDOR_ATTR_REO_SW_DROP_PARTNER_DP_NA,
	QCA_VENDOR_ATTR_REO_SW_DROP_PDEV_NA,
	QCA_VENDOR_ATTR_REO_SW_DROP_LAST_MSDU_NOT_FOUND,
	QCA_VENDOR_ATTR_REO_SW_DROP_NWIFI_HDR_LEN_INVALID,
	QCA_VENDOR_ATTR_REO_SW_DROP_INVALID_MSDU_LEN,
	QCA_VENDOR_ATTR_REO_SW_DROP_MSDU_COALESCE_FAIL,
	QCA_VENDOR_ATTR_REO_SW_DROP_MPDU,
	QCA_VENDOR_ATTR_REO_SW_DROP_PPDU,
	QCA_VENDOR_ATTR_REO_SW_DROP_INVALID_PEER,

	QCA_VENDOR_ATTR_REO_SW_DROP_AFTER_LAST,
	QCA_VENDOR_ATTR_REO_SW_DROP_MAX =
		QCA_VENDOR_ATTR_REO_SW_DROP_AFTER_LAST - 1,
};

enum qca_vendor_wlan_telemetry_tcl_ring_attr {
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_TCL_RING_INVALID = 0,
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_TCL_RING_0,
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_TCL_RING_1,
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_TCL_RING_2,
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_TCL_RING_3,

	QCA_VENDOR_ATTR_WLAN_TELEMETRY_TCL_RING_AFTER_LAST,
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_TCL_RING_MAX =
		QCA_VENDOR_ATTR_WLAN_TELEMETRY_TCL_RING_AFTER_LAST - 1,
};

enum qca_vendor_wlan_telemetry_reo_ring_attr {
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_REO_RING_INVALID = 0,
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_REO_RING_0,
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_REO_RING_1,
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_REO_RING_2,
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_REO_RING_3,

	QCA_VENDOR_ATTR_WLAN_TELEMETRY_REO_RING_AFTER_LAST,
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_REO_RING_MAX =
		QCA_VENDOR_ATTR_WLAN_TELEMETRY_REO_RING_AFTER_LAST - 1,
};

enum qca_vendor_wlan_telemetry_tx_comp_err_types_attr {
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_TX_COMP_ERR_INVALID = 0,
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_TX_COMP_ERR_MISC,
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_TX_COMP_ERR_INVALID_DESC,
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_TX_COMP_ERR_INVALID_PDEV,
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_TX_COMP_ERR_INVALID_VIF,
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_TX_COMP_ERR_INVALID_PEER,
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_TX_COMP_ERR_INVALID_LINK_PEER,
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_TX_COMP_ERR_DESC_INUSE,

	QCA_VENDOR_ATTR_WLAN_TELEMETRY_TX_COMP_ERR_INVALID_AFTER_LAST,
	QCA_VENDOR_ATTR_WLAN_TELEMETRY_TX_COMP_ERR_INVALID_MAX =
		QCA_VENDOR_ATTR_WLAN_TELEMETRY_TX_COMP_ERR_INVALID_AFTER_LAST - 1
};

enum qca_vendor_wlan_telemetry_pkt_info {
	QCA_VENDOR_WLAN_TELEMETRY_ATTR_PKTINFO_PKTS = 1,
	QCA_VENDOR_WLAN_TELEMETRY_ATTR_PKTINFO_BYTES,

	/* keep last */
	QCA_VENDOR_WLAN_TELEMETRY_ATTR_PKTINFO_AFTER_LAST,
	QCA_VENDOR_WLAN_TELEMETRY_ATTR_PKTINFO_MAX =
		QCA_VENDOR_WLAN_TELEMETRY_ATTR_PKTINFO_AFTER_LAST - 1,
};

enum qca_vendor_wlan_telemetry_per_pkt_stats_rx_attr {
	QCA_VENDOR_ATTR_PER_PKT_STATS_RX_PKTINFO_RECV_FROM_REO = 1,
	QCA_VENDOR_ATTR_PER_PKT_STATS_RX_PKTINFO_TO_STACK,
	QCA_VENDOR_ATTR_PER_PKT_STATS_RX_PKTINFO_TO_STACK_FAST,
	QCA_VENDOR_ATTR_PER_PKT_STATS_RX_MCAST,
	QCA_VENDOR_ATTR_PER_PKT_STATS_RX_UCAST,
	QCA_VENDOR_ATTR_PER_PKT_STATS_RX_NON_AMSDU,
	QCA_VENDOR_ATTR_PER_PKT_STATS_RX_MSDU_PART_OF_AMSDU,
	QCA_VENDOR_ATTR_PER_PKT_STATS_RX_MPDU_RETRY,

	/* keep last */
	QCA_VENDOR_ATTR_PER_PKT_STATS_RX_AFTER_LAST,
	QCA_VENDOR_ATTR_PER_PKT_STATS_RX_MAX =
		QCA_VENDOR_ATTR_PER_PKT_STATS_RX_AFTER_LAST - 1,
};

enum qca_vendor_wlan_telemetry_stats_tx {
	QCA_VENDOR_ATTR_PER_PKT_STATS_TX_PKTINFO_COMP_PKT = 1,
	QCA_VENDOR_ATTR_PER_PKT_STATS_TX_PKTINFO_TX_SUCCESS,
	QCA_VENDOR_ATTR_PER_PKT_STATS_TX_FAILED,
	QCA_VENDOR_ATTR_PER_PKT_STATS_TX_WBM_REL_REASON,
	QCA_VENDOR_ATTR_PER_PKT_STATS_TX_TQM_REL_REASON,
	QCA_VENDOR_ATTR_PER_PKT_STATS_TX_RELEASE_SRC_NOT_TQM,
	QCA_VENDOR_ATTR_PER_PKT_STATS_TX_RETRY_COUNT,
	QCA_VENDOR_ATTR_PER_PKT_STATS_TX_TOTAL_MSDU_RETRIES,
	QCA_VENDOR_ATTR_PER_PKT_STATS_TX_MULTIPLE_RETRY_COUNT,
	QCA_VENDOR_ATTR_PER_PKT_STATS_TX_OFDMA,
	QCA_VENDOR_ATTR_PER_PKT_STATS_TX_AMSDU_CNT,
	QCA_VENDOR_ATTR_PER_PKT_STATS_TX_NON_AMSDU_CNT,
	QCA_VENDOR_ATTR_PER_PKT_STATS_TX_INVALID_LINK_ID_PKT_CNT,
	QCA_VENDOR_ATTR_PER_PKT_STATS_TX_MCAST,
	QCA_VENDOR_ATTR_PER_PKT_STATS_TX_UCAST,
	QCA_VENDOR_ATTR_PER_PKT_STATS_TX_BCAST,

	/* keep last */
	QCA_VENDOR_ATTR_PER_PKT_STATS_TX_AFTER_LAST,
	QCA_VENDOR_ATTR_PER_PKT_STATS_TX_MAX =
		QCA_VENDOR_ATTR_PER_PKT_STATS_TX_AFTER_LAST - 1,
};

enum qca_vendor_wlan_telemetry_htt_tx_comp_status {
	QCA_VENDOR_ATTR_WBM_REL_HTT_TX_COMP_STATUS_OK = 1,
	QCA_VENDOR_ATTR_WBM_REL_HTT_TX_COMP_STATUS_DROP,
	QCA_VENDOR_ATTR_WBM_REL_HTT_TX_COMP_STATUS_TTL,
	QCA_VENDOR_ATTR_WBM_REL_HTT_TX_COMP_STATUS_REINJ,
	QCA_VENDOR_ATTR_WBM_REL_HTT_TX_COMP_STATUS_INSPECT,
	QCA_VENDOR_ATTR_WBM_REL_HTT_TX_COMP_STATUS_MEC_NOTIFY,
	QCA_VENDOR_ATTR_WBM_REL_HTT_TX_COMP_STATUS_VDEVID_MISMATCH,

	/* keep last */
	QCA_VENDOR_ATTR_WBM_REL_HTT_TX_COMP_STATUS_AFTER_LAST,
	QCA_VENDOR_ATTR_WBM_REL_HTT_TX_COMP_STATUS_MAX =
		QCA_VENDOR_ATTR_WBM_REL_HTT_TX_COMP_STATUS_AFTER_LAST - 1,
};

enum qca_vendor_wlan_telemetry_wbm_tqm_rel_reason {
	QCA_VENDOR_ATTR_WBM_TQM_REL_REASON_FRAME_ACKED = 1,
	QCA_VENDOR_ATTR_WBM_TQM_REL_REASON_CMD_REMOVE_MPDU,
	QCA_VENDOR_ATTR_WBM_TQM_REL_REASON_CMD_REMOVE_TX,
	QCA_VENDOR_ATTR_WBM_TQM_REL_REASON_CMD_REMOVE_NOTX,
	QCA_VENDOR_ATTR_WBM_TQM_REL_REASON_CMD_REMOVE_AGED_FRAMES,
	QCA_VENDOR_ATTR_WBM_TQM_REL_REASON_CMD_REMOVE_RESEAON1,
	QCA_VENDOR_ATTR_WBM_TQM_REL_REASON_CMD_REMOVE_RESEAON2,
	QCA_VENDOR_ATTR_WBM_TQM_REL_REASON_CMD_REMOVE_RESEAON3,
	QCA_VENDOR_ATTR_WBM_TQM_REL_REASON_CMD_DISABLE_QUEUE,
	QCA_VENDOR_ATTR_WBM_TQM_REL_REASON_CMD_TILL_NONMATCHING,
	QCA_VENDOR_ATTR_WBM_TQM_REL_REASON_DROP_THRESHOLD,
	QCA_VENDOR_ATTR_WBM_TQM_REL_REASON_DROP_LINK_DESC_UNAVAIL,
	QCA_VENDOR_ATTR_WBM_TQM_REL_REASON_DROP_OR_INVALID_MSDU,
	QCA_VENDOR_ATTR_WBM_TQM_REL_REASON_MULTICAST_DROP,
	QCA_VENDOR_ATTR_WBM_TQM_REL_REASON_VDEV_MISMATCH_DROP,

	/* keep last */
	QCA_VENDOR_ATTR_WBM_TQM_REL_REASON_AFTER_LAST,
	QCA_VENDOR_ATTR_WBM_TQM_REL_REASON_MAX =
		QCA_VENDOR_ATTR_WBM_TQM_REL_REASON_AFTER_LAST - 1,
};

enum qca_vendor_wlan_telemetry_tx_ingress_stats {
	QCA_VENDOR_ATTR_TX_INGRESS_STATS_PKTINFO_RECV_FROM_STACK = 1,
	QCA_VENDOR_ATTR_TX_INGRESS_STATS_PKTINFO_ENQ_TO_HW,
	QCA_VENDOR_ATTR_TX_INGRESS_STATS_PKTINFO_ENQ_TO_HW_FAST,
	QCA_VENDOR_ATTR_TX_INGRESS_STATS_ENCAP_TYPE,
	QCA_VENDOR_ATTR_TX_INGRESS_STATS_ENCRYPT_TYPE,
	QCA_VENDOR_ATTR_TX_INGRESS_STATS_DESC_TYPE,
	QCA_VENDOR_ATTR_TX_INGRESS_STATS_DROP_TYPE,
	QCA_VENDOR_ATTR_TX_INGRESS_STATS_MCAST,

	QCA_VENDOR_ATTR_TX_INGRESS_STATS_AFTER_LAST,
	QCA_VENDOR_ATTR_TX_INGRESS_STATS_MAX =
	QCA_VENDOR_ATTR_TX_INGRESS_STATS_AFTER_LAST - 1,
};

enum qca_vendor_wlan_telemetry_tx_ingress_encap_type {
	QCA_VENDOR_ATTR_TX_INGRESS_ENCAP_TYPE_RAW = 1,
	QCA_VENDOR_ATTR_TX_INGRESS_ENCAP_TYPE_NATIVE_WIFI,
	QCA_VENDOR_ATTR_TX_INGRESS_ENCAP_TYPE_ETHERNET,
	QCA_VENDOR_ATTR_TX_INGRESS_ENCAP_TYPE_802_3,

	QCA_VENDOR_ATTR_TX_INGRESS_ENCAP_TYPE_AFTER_LAST,
	QCA_VENDOR_ATTR_TX_INGRESS_ENCAP_TYPE_MAX =
		QCA_VENDOR_ATTR_TX_INGRESS_ENCAP_TYPE_AFTER_LAST - 1,
};

enum qca_vendor_wlan_telemetry_tx_ingress_encrypt_type {
	QCA_VENDOR_ATTR_TX_INGRESS_ENCRYPT_TYPE_WEP_40 = 1,
	QCA_VENDOR_ATTR_TX_INGRESS_ENCRYPT_TYPE_WEP_104,
	QCA_VENDOR_ATTR_TX_INGRESS_ENCRYPT_TYPE_TKIP_NO_MIC,
	QCA_VENDOR_ATTR_TX_INGRESS_ENCRYPT_TYPE_WEP_128,
	QCA_VENDOR_ATTR_TX_INGRESS_ENCRYPT_TYPE_TKIP_MIC,
	QCA_VENDOR_ATTR_TX_INGRESS_ENCRYPT_TYPE_WAPI,
	QCA_VENDOR_ATTR_TX_INGRESS_ENCRYPT_TYPE_CCMP_128,
	QCA_VENDOR_ATTR_TX_INGRESS_ENCRYPT_TYPE_OPEN,
	QCA_VENDOR_ATTR_TX_INGRESS_ENCRYPT_TYPE_CCMP_256,
	QCA_VENDOR_ATTR_TX_INGRESS_ENCRYPT_TYPE_GCMP_128,
	QCA_VENDOR_ATTR_TX_INGRESS_ENCRYPT_TYPE_AES_GCMP_256,
	QCA_VENDOR_ATTR_TX_INGRESS_ENCRYPT_TYPE_WAPI_GCM_SM4,
	QCA_VENDOR_ATTR_TX_INGRESS_ENCRYPT_TYPE_AFTER_LAST,
	QCA_VENDOR_ATTR_TX_INGRESS_ENCRYPT_TYPE_MAX =
		QCA_VENDOR_ATTR_TX_INGRESS_ENCRYPT_TYPE_AFTER_LAST - 1,
};

enum qca_vendor_wlan_telemetry_tx_ingress_desc_type {
	QCA_VENDOR_ATTR_TX_INGRESS_DESC_TYPE_BUFFER = 1,
	QCA_VENDOR_ATTR_TX_INGRESS_DESC_TYPE_EXT_DESC,

	QCA_VENDOR_ATTR_TX_INGRESS_DESC_TYPE_AFTER_LAST,
	QCA_VENDOR_ATTR_TX_INGRESS_DESC_TYPE_MAX =
		QCA_VENDOR_ATTR_TX_INGRESS_DESC_TYPE_AFTER_LAST - 1,
};

enum qca_vendor_wlan_telemetry_tx_ingress_enq_error {
	QCA_VENDOR_ATTR_TX_INGRESS_ENQ_SUCCESS = 1,
	QCA_VENDOR_ATTR_TX_INGRESS_ENQ_DROP_MISC,
	QCA_VENDOR_ATTR_TX_INGRESS_ENQ_DROP_VIF_TYPE_MON,
	QCA_VENDOR_ATTR_TX_INGRESS_ENQ_DROP_INV_LINK,
	QCA_VENDOR_ATTR_TX_INGRESS_ENQ_DROP_INV_ARVIF,
	QCA_VENDOR_ATTR_TX_INGRESS_ENQ_DROP_MGMT_FRAME,
	QCA_VENDOR_ATTR_TX_INGRESS_ENQ_DROP_MAX_TX_LIMIT,
	QCA_VENDOR_ATTR_TX_INGRESS_ENQ_DROP_INV_PDEV,
	QCA_VENDOR_ATTR_TX_INGRESS_ENQ_DROP_INV_PEER,
	QCA_VENDOR_ATTR_TX_INGRESS_ENQ_DROP_CRASH_FLUSH,
	QCA_VENDOR_ATTR_TX_INGRESS_ENQ_DROP_NON_DATA_FRAME,
	QCA_VENDOR_ATTR_TX_INGRESS_ENQ_DROP_SW_DESC_NA,
	QCA_VENDOR_ATTR_TX_INGRESS_ENQ_DROP_ENCAP_RAW,
	QCA_VENDOR_ATTR_TX_INGRESS_ENQ_DROP_ENCAP_802_3,
	QCA_VENDOR_ATTR_TX_INGRESS_ENQ_DROP_DMA_ERR,
	QCA_VENDOR_ATTR_TX_INGRESS_ENQ_DROP_EXT_DESC_NA,
	QCA_VENDOR_ATTR_TX_INGRESS_ENQ_DROP_HTT_MDATA_ERR,
	QCA_VENDOR_ATTR_TX_INGRESS_ENQ_DROP_TCL_DESC_NA,
	QCA_VENDOR_ATTR_TX_INGRESS_ENQ_TCL_DESC_RETRY,
	QCA_VENDOR_ATTR_TX_INGRESS_ENQ_DROP_INV_ARVIF_FAST,
	QCA_VENDOR_ATTR_TX_INGRESS_ENQ_DROP_INV_PDEV_FAST,
	QCA_VENDOR_ATTR_TX_INGRESS_ENQ_DROP_MAX_TX_LIMIT_FAST,
	QCA_VENDOR_ATTR_TX_INGRESS_ENQ_DROP_INV_ENCAP_FAST,
	QCA_VENDOR_ATTR_TX_INGRESS_ENQ_DROP_BRIDGE_VDEV,
	QCA_VENDOR_ATTR_TX_INGRESS_ENQ_ARSTA_NA,

	QCA_VENDOR_ATTR_TX_INGRESS_ENQ_AFTER_LAST,
	QCA_VENDOR_ATTR_TX_INGRESS_ENQ_ERR_MAX =
		QCA_VENDOR_ATTR_TX_INGRESS_ENQ_AFTER_LAST - 1,
};

/* TODO: qca_wlan_genric_data, qca_wlan_set_params,
 * qca_wlan_get_params
 * These should be align with qca_wlan_vendor_attr_config
 * in qca-vendor.h
 * QCA_WLAN_VENDOR_ATTR_CONFIG_GENERIC_COMMAND
 * QCA_WLAN_VENDOR_ATTR_CONFIG_GENERIC_DATA
 *
 * It requires qca_nl80211_lib changes also in reading
 * responses
 */
enum qca_wlan_genric_data {
	QCA_WLAN_VENDOR_ATTR_GENERIC_PARAM_INVALID = 0,
	QCA_WLAN_VENDOR_ATTR_PARAM_DATA,
	QCA_WLAN_VENDOR_ATTR_PARAM_LENGTH,
	QCA_WLAN_VENDOR_ATTR_PARAM_FLAGS,

	/* keep last */
	QCA_WLAN_VENDOR_ATTR_GENERIC_PARAM_LAST,
	QCA_WLAN_VENDOR_ATTR_GENERIC_PARAM_MAX =
	QCA_WLAN_VENDOR_ATTR_GENERIC_PARAM_LAST - 1
};

enum qca_wlan_vendor_attr_iface_reload {
	QCA_WLAN_VENDOR_ATTR_IFACE_RELOAD_INVALID = 0,
	QCA_WLAN_VENDOR_ATTR_IFACE_RELOAD_LINKID = 1,

	/* Keep last */
	QCA_WLAN_VENDOR_ATTR_IFACE_RELOAD_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_IFACE_RELOAD_MAX =
	QCA_WLAN_VENDOR_ATTR_IFACE_RELOAD_AFTER_LAST - 1,
};

enum qca_vendor_vdev_param {
	QCA_WLAN_VENDOR_VDEV_PARAM_TEST = 0,
	QCA_WLAN_VENDOR_VDEV_PARAM_TEST_RELOAD = QCA_WLAN_VENDOR_VDEV_PARAM_TEST,

	/* Add new params above */
	QCA_WLAN_VENDOR_VDEV_PARAM_LAST,
	QCA_WLAN_VENDOR_VDEV_PARAM_MAX = QCA_WLAN_VENDOR_VDEV_PARAM_LAST - 1,
};

enum qca_vendor_radio_param {
	QCA_WLAN_VENDOR_RADIO_PARAM_TEST = 0,
	QCA_WLAN_VENDOR_RADIO_PARAM_TEST_RELOAD = QCA_WLAN_VENDOR_RADIO_PARAM_TEST,

	/* Add new params above */
	QCA_WLAN_VENDOR_RADIO_PARAM_LAST,
	QCA_WLAN_VENDOR_RADIO_PARAM_MAX = QCA_WLAN_VENDOR_RADIO_PARAM_LAST - 1,
};

enum qca_wlan_vendor_attr_sdwf_dev {
	QCA_WLAN_VENDOR_ATTR_SDWF_DEV_INVALID = 0,
	QCA_WLAN_VENDOR_ATTR_SDWF_DEV_OPERATION = 1,
	QCA_WLAN_VENDOR_ATTR_SDWF_DEV_DEF_Q_PARAMS = 2,
	QCA_WLAN_VENDOR_ATTR_SDWF_DEV_STREAMING_STATS_PARAMS = 3,
	QCA_WLAN_VENDOR_ATTR_SDWF_DEV_RESET_STATS = 4,
	QCA_WLAN_VENDOR_ATTR_SDWF_DEV_SLA_BREACHED_PARAMS = 5,

	/* keep last */
	QCA_WLAN_VENDOR_ATTR_SDWF_DEV_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_SDWF_DEV_MAX =
	QCA_WLAN_VENDOR_ATTR_SDWF_DEV_AFTER_LAST - 1,
};

enum qca_wlan_vendor_sdwf_dev_oper {
	QCA_WLAN_VENDOR_SDWF_DEV_OPER_DEF_Q_MAP = 0,
	QCA_WLAN_VENDOR_SDWF_DEV_OPER_DEF_Q_UNMAP = 1,
	QCA_WLAN_VENDOR_SDWF_DEV_OPER_DEF_Q_MAP_GET = 2,
	QCA_WLAN_VENDOR_SDWF_DEV_OPER_STREAMING_STATS = 3,
	QCA_WLAN_VENDOR_SDWF_DEV_OPER_RESET_STATS = 4,
	QCA_WLAN_VENDOR_SDWF_DEV_OPER_BREACH_DETECTED = 5,
};

enum qca_wlan_vendor_attr_sdwf_streaming_stats {
	QCA_WLAN_VENDOR_ATTR_SDWF_STREAMING_INVALID = 0,
	QCA_WLAN_VENDOR_ATTR_SDWF_STREAMING_BASIC_STATS = 1,
	QCA_WLAN_VENDOR_ATTR_SDWF_STREAMING_EXTND_STATS = 2,
	QCA_WLAN_VENDOR_ATTR_SDWF_MLO_LINK_ID = 3,

	/* keep last */
	QCA_WLAN_VENDOR_ATTR_SDWF_STREAMING_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_SDWF_STREAMING_MAX =
	QCA_WLAN_VENDOR_ATTR_SDWF_STREAMING_AFTER_LAST - 1,
};

enum qca_wlan_vendor_attr_sdwf_sla_breach_param {
	QCA_WLAN_VENDOR_ATTR_SDWF_SLA_BREACH_PARAM_INVALID = 0,
	QCA_WLAN_VENDOR_ATTR_SDWF_SLA_BREACH_PARAM_PEER_MAC = 1,
	QCA_WLAN_VENDOR_ATTR_SDWF_SLA_BREACH_PARAM_SVC_ID = 2,
	QCA_WLAN_VENDOR_ATTR_SDWF_SLA_BREACH_PARAM_TYPE = 3,
	QCA_WLAN_VENDOR_ATTR_SDWF_SLA_BREACH_PARAM_SET_CLEAR = 4,
	QCA_WLAN_VENDOR_ATTR_SDWF_SLA_BREACH_PARAM_PEER_MLD_MAC = 5,
	QCA_WLAN_VENDOR_ATTR_SDWF_SLA_BREACH_PARAM_AC = 6,

	/* Keep last */
	QCA_WLAN_VENDOR_ATTR_SDWF_SLA_BREACH_PARAM_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_SDWF_SLA_BREACH_PARAM_MAX =
	QCA_WLAN_VENDOR_ATTR_SDWF_SLA_BREACH_PARAM_AFTER_LAST - 1
};

enum qca_wlan_vendor_sdwf_sla_breach_type {
	QCA_WLAN_VENDOR_SDWF_SLA_BREACH_PARAM_TYPE_INVALID = 0,
	QCA_WLAN_VENDOR_SDWF_SLA_BREACH_PARAM_TYPE_MIN_THROUGHPUT,
	QCA_WLAN_VENDOR_SDWF_SLA_BREACH_PARAM_TYPE_MAX_THROUGHPUT,
	QCA_WLAN_VENDOR_SDWF_SLA_BREACH_PARAM_TYPE_BURST_SIZE,
	QCA_WLAN_VENDOR_SDWF_SLA_BREACH_PARAM_TYPE_SERVICE_INTERVAL,
	QCA_WLAN_VENDOR_SDWF_SLA_BREACH_PARAM_TYPE_DELAY_BOUND,
	QCA_WLAN_VENDOR_SDWF_SLA_BREACH_PARAM_TYPE_MSDU_TTL,
	QCA_WLAN_VENDOR_SDWF_SLA_BREACH_PARAM_TYPE_MSDU_LOSS,
	QCA_WLAN_VENDOR_SDWF_SLA_BREACH_PARAM_TYPE_MAX,
};

/**
 * enum qca_wlan_vendor_attr_pri_link_migrate: Attributes used by the vendor
 *     subcommand/event %QCA_NL80211_VENDOR_SUBCMD_PRI_LINK_MIGRATE.
 *
 * @QCA_WLAN_VENDOR_ATTR_PRI_LINK_MIGR_MLD_MAC_ADDR: 6 byte MAC address.
 *	(a) Used in subcommand to indicate that primary link migration
 * will occur only for the ML client with the given MLD MAC address.
 *	(b) Used in event to specify the MAC address of the peer for which
 * the primary link has been modified.
 * @QCA_WLAN_VENDOR_ATTR_PRI_LINK_MIGR_CURRENT_PRI_LINK_ID: Optional u8
 *     attribute. When specified, all ML clients having their current primary
 *     link as specified will be considered for migration.
 * @QCA_WLAN_VENDOR_ATTR_PRI_LINK_MIGR_NEW_PRI_LINK_ID: u8 attribute.
 *	(a) Optional attribute used in subcommand, to indicate the new
 * primary link to which the selected ML clients should be migrated to.
 * If not provided, the driver will select a suitable primary link
 * on its own.
 *	(b) Used in event, to indicate the new link ID which is set
 * as primary link.
 */
enum qca_wlan_vendor_attr_pri_link_migrate {
       QCA_WLAN_VENDOR_ATTR_PRI_LINK_MIGR_INVALID = 0,
       QCA_WLAN_VENDOR_ATTR_PRI_LINK_MIGR_MLD_MAC_ADDR,
       QCA_WLAN_VENDOR_ATTR_PRI_LINK_MIGR_CURRENT_PRI_LINK_ID,
       QCA_WLAN_VENDOR_ATTR_PRI_LINK_MIGR_NEW_PRI_LINK_ID,

       /* keep this last */
       QCA_WLAN_VENDOR_ATTR_PRI_LINK_MIGR_AFTER_LAST,
       QCA_WLAN_VENDOR_ATTR_PRI_LINK_MIGR_MAX =
       QCA_WLAN_VENDOR_ATTR_PRI_LINK_MIGR_AFTER_LAST - 1,
};

enum qca_wlan_vendor_attr_dcs {
	QCA_WLAN_VENDOR_ATTR_DCS_INVALID = 0,
	QCA_WLAN_VENDOR_ATTR_DCS_MLO_LINK_ID,
	QCA_WLAN_VENDOR_ATTR_DCS_WLAN_INTERFERENCE_CONFIGURE,

	/* Keep last */
	QCA_WLAN_VENDOR_ATTR_DCS_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_DCS_MAX =
		QCA_WLAN_VENDOR_ATTR_DCS_AFTER_LAST - 1
};

/**
 * @QCA_WLAN_VENDOR_ATTR_WLAN_INTERFERENCE_PARAM_TSF:
 * Required (u32).
 * Current running timestamp in the firmware; tsf - time synchronize function.
 *
 * @QCA_WLAN_VENDOR_ATTR_WLAN_LAST_ACK_RSSI:
 * Required (u32).
 * RSSI value of Last ACK frame.
 *
 * @QCA_WLAN_VENDOR_ATTR_WLAN_INTERFERENCE_PARAM_TX_WASTE_TIME:
 * Required (u32).
 * Sum of all the failed durations in the last one second interval.
 *
 * It's the amount of time that we spent in backoff or waiting for other
 * transmit/receive to complete; since these parameters are receieved
 * in the userspace every one second, hence the calculation for
 * wasted time would be _tx_waste_time/delta_tsf
 *
 * @QCA_WLAN_VENDOR_ATTR_WLAN_INTERFERENCE_PARAM_RX_TIME:
 * Required (u32)
 * Sum of all the Rx Frame Duration(microsecond), in a period of one second.
 *
 * @QCA_WLAN_VENDOR_ATTR_WLAN_INTERFERENCE_PARAM_PHY_ERR_COUNT:
 * Required (u32).
 * Current running phy error/CRC Error packet count.
 *
 * @QCA_WLAN_VENDOR_ATTR_WLAN_INTERFERENCE_PARAM_LISTEN_TIME:
 * Required (u32)
 * Listen time is the time spent by HW in Rx + noise reception in millisecond.
 *
 * @QCA_WLAN_VENDOR_ATTR_WLAN_INTERFERENCE_PARAM_TX_FRAME_COUNT:
 * Required (u32)
 * The current running tx frame count in micorsecond.
 *
 * @QCA_WLAN_VENDOR_ATTR_WLAN_INTERFERENCE_PARAM_RX_FRAME_COUNT:
 * Required (u32)
 * The current running rx frame count in microsecond.
 *
 * @QCA_WLAN_VENDOR_ATTR_WLAN_INTERFERENCE_PARAM_RX_CLR_COUNT:
 * Required (u32)
 * The current running rx clear count in microsecond.
 *
 * @QCA_WLAN_VENDOR_ATTR_WLAN_INTERFERENCE_PARAM_CYCLE_COUNT:
 * Required (u32)
 * Cycle count is just the fixed count Of a given period, here we
 * sample cycle counter at 1 sec interval , the delta between successive
 * counts will be ~ 1,000 ms
 *
 * QCA_WLAN_VENDOR_ATTR_WLAN_INTERFERENCE_PARAM_OFDM_PHYERR_COUNT:
 * Required (u32)
 * The current running OFDM phy Error Count in microsecond.
 *
 * @QCA_WLAN_VENDOR_ATTR_WLAN_INTERFERENCE_PARAM_CCK_PHYERR_COUNT:
 * Required (u32)
 * The current running CCK Phy Error Count in microsecond.
 *
 * @QCA_WLAN_VENDOR_ATTR_WLAN_INTERFERENCE_PARAM_CHANNEL_NF:
 * Required (s32)
 * Channel noise floor (units are dBr)
 *
 * @QCA_WLAN_VENDOR_ATTR_WLAN_INTERFERENCE_PARAM_MY_BSS_RX_CYCLE_COUNT:
 * Required (u32)
 * The Current running counter for th Rx Frames destined to my BSS in microsecond
 *
 */

enum qca_wlan_vendor_wlan_interference_attr {
	QCA_WLAN_VENDOR_ATTR_WLAN_INTERFERENCE_PARAM_TSF,
	QCA_WLAN_VENDOR_ATTR_WLAN_LAST_ACK_RSSI,
	QCA_WLAN_VENDOR_ATTR_WLAN_INTERFERENCE_PARAM_TX_WASTE_TIME,
	QCA_WLAN_VENDOR_ATTR_WLAN_INTERFERENCE_PARAM_RX_TIME,
	QCA_WLAN_VENDOR_ATTR_WLAN_INTERFERENCE_PARAM_PHY_ERR_COUNT,
	QCA_WLAN_VENDOR_ATTR_WLAN_INTERFERENCE_PARAM_LISTEN_TIME,
	QCA_WLAN_VENDOR_ATTR_WLAN_INTERFERENCE_PARAM_TX_FRAME_COUNT,
	QCA_WLAN_VENDOR_ATTR_WLAN_INTERFERENCE_PARAM_RX_FRAME_COUNT,
	QCA_WLAN_VENDOR_ATTR_WLAN_INTERFERENCE_PARAM_RX_CLR_COUNT,
	QCA_WLAN_VENDOR_ATTR_WLAN_INTERFERENCE_PARAM_CYCLE_COUNT,
	QCA_WLAN_VENDOR_ATTR_WLAN_INTERFERENCE_PARAM_RX_CLR_EXT_COUNT,
	QCA_WLAN_VENDOR_ATTR_WLAN_INTERFERENCE_PARAM_OFDM_PHYERR_COUNT,
	QCA_WLAN_VENDOR_ATTR_WLAN_INTERFERENCE_PARAM_CCK_PHYERR_COUNT,
	QCA_WLAN_VENDOR_ATTR_WLAN_INTERFERENCE_PARAM_CHANNEL_NF,
	QCA_WLAN_VENDOR_ATTR_WLAN_INTERFERENCE_PARAM_MY_BSS_RX_CYCLE_COUNT,
	QCA_WLAN_VENDOR_ATTR_WLAN_INTERFERENCE_PARAM_LAST,
	QCA_WLAN_VENDOR_ATTR_WLAN_INTERFERENCE_PARAM_MAX =
		QCA_WLAN_VENDOR_ATTR_WLAN_INTERFERENCE_PARAM_LAST - 1,
};

/**
 * enum qca_wlan_vendor_dynamic_init_conf - This enum defines the different
 * dynamic app init/deinit configurations.
 *
 * @QCA_WLAN_VENDOR_DYNAMIC_INIT_CONF_RM_APP_START: Indicates to driver this
 * is the initial app init and not an individual service dynamic init/de-init.
 *
 * @QCA_WLAN_VENDOR_DYNAMIC_INIT_CONF_SERVICE_START: Indicates to driver this
 * is dynamic service init/start.
 *
 * @QCA_WLAN_VENDOR_DYNAMIC_INIT_CONF_SERVICE_STOP: Indicates to driver this
 * is dynamic service de-init/stop.
 */
enum qca_wlan_vendor_dynamic_init_conf {
	QCA_WLAN_VENDOR_DYNAMIC_INIT_CONF_RM_APP_START = 0,
	QCA_WLAN_VENDOR_DYNAMIC_INIT_CONF_SERVICE_START = 1,
	QCA_WLAN_VENDOR_DYNAMIC_INIT_CONF_SERVICE_STOP = 2,
	QCA_WLAN_VENDOR_DYNAMIC_INIT_CONF_CONT_SERVICE_START = 3,
	QCA_WLAN_VENDOR_DYNAMIC_INIT_CONF_CONT_SERVICE_STOP = 4,
};
/**
 * enum qca_wlan_vendor_attr_soc_device_info - Represents the SOC device
 * information available in the driver. The driver will send this information
 * to the userspace as part of the registration event.
 *
 * @QCA_WLAN_VENDOR_ATTR_SOC_DEVICE_SOC_ID: u8, represents the SOC device ID.
 *
 * @QCA_WLAN_VENDOR_ATTR_SOC_DEVICE_NUM_LINKS: u8, represents the number of
 * links present in the SOC device.
 *
 * @QCA_WLAN_VENDOR_ATTR_SOC_DEVICE_LINK_INFO: represents the link level
 * information. Array of nested attributes are defined in enum
 * qca_wlan_vendor_attr_link_info
 */
enum qca_wlan_vendor_attr_soc_device_info {
	QCA_WLAN_VENDOR_ATTR_SOC_DEVICE_SOC_ID = 1,
	QCA_WLAN_VENDOR_ATTR_SOC_DEVICE_NUM_LINKS = 2,
	QCA_WLAN_VENDOR_ATTR_SOC_DEVICE_LINK_INFO = 3,

	/* keep last */
	QCA_WLAN_VENDOR_ATTR_SOC_DEVICE_INFO_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_SOC_DEVICE_INFO_MAX =
		QCA_WLAN_VENDOR_ATTR_SOC_DEVICE_INFO_AFTER_LAST - 1,
};

/**
 * enum qca_wlan_vendor_attr_link_info - Represents the link level information
 * available in the driver. The driver will send this information to the
 * userspace as part of the registration event.
 *
 * @QCA_WLAN_VENDOR_ATTR_LINK_INFO_HW_LINK_ID: u16, represents the hardware link
 * ID.
 *
 * @QCA_WLAN_VENDOR_ATTR_LINK_MAC: 6 byte MAC address represents the Link
 * MAC address.
 *
 * @QCA_WLAN_VENDOR_ATTR_LINK_CHAN_BW: u8, represents the channel bandwidth,
 * values are defined in enum qca_wlan_vendor_channel_width
 *
 * @QCA_WLAN_VENDOR_ATTR_LINK_CHAN_FREQ: u16, represents the channel frequency
 * in MHz.
 *
 * @QCA_WLAN_VENDOR_ATTR_LINK_BAND_CAP: Channel band capability, values are
 * defined in enum qca_wlan_vendor_link_band_caps
 *
 * @QCA_WLAN_VENDOR_ATTR_LINK_TX_CHAIN_MASK: u8, represents the max tx chainmask
 * value.
 *
 * @QCA_WLAN_VENDOR_ATTR_LINK_RX_CHAIN_MASK: u8, represents the max rx chainmask
 * value.
 */
enum qca_wlan_vendor_attr_link_info {
	QCA_WLAN_VENDOR_ATTR_LINK_INFO_HW_LINK_ID = 1,
	QCA_WLAN_VENDOR_ATTR_LINK_MAC = 2,
	QCA_WLAN_VENDOR_ATTR_LINK_CHAN_BW = 3,
	QCA_WLAN_VENDOR_ATTR_LINK_CHAN_FREQ = 4,
	QCA_WLAN_VENDOR_ATTR_LINK_BAND_CAP = 5,
	QCA_WLAN_VENDOR_ATTR_LINK_TX_CHAIN_MASK = 6,
	QCA_WLAN_VENDOR_ATTR_LINK_RX_CHAIN_MASK = 7,

	/* keep last */
	QCA_WLAN_VENDOR_ATTR_LINK_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_LINK_MAX =
		QCA_WLAN_VENDOR_ATTR_LINK_AFTER_LAST - 1,
};

/**
 * enum qca_wlan_vendor_attr_app_generic_category: Represents the Generic
 * mapping frame category value.
 *
 * @QCA_WLAN_VENDOR_ATTR_GENERIC_CATEGORY_INVALID: Generic mapping catefory
 * invalid.
 *
 * @QCA_WLAN_VENDOR_ATTR_GENERIC_CATEGORY_APP_INIT: The driver includes this
 * category in the event  sent to the userspace when it receives a APP INIT
 * request frame.
 *
 * @QCA_WLAN_VENDOR_ATTR_GENERIC_CATEGORY_LINK_BW_NSS_CHANGE: Represents the
 * notification message that will be sent to the RM APP for changes observed
 * in the BW and NSS values at AP side. Array of nested attributes are defined
 * in enum qca_wlan_vendor_attr_link_bw_nss_change_info
 *
 * @QCA_WLAN_VENDOR_ATTR_GENERIC_CATEGORY_ASSOC_NO_T2LM_INFO: The driver
 * includes this category in the event sent to the userspace when it receives a
 * Assoc request frame from the STA without T2LM IE.
 *
 * @QCA_WLAN_VENDOR_ATTR_GENERIC_CATEGORY_ASSOC_WITH_T2LM_INFO: The driver
 * includes this category in the event sent to the userspace when it receives a
 * Assoc request frame from the STA with T2LM IE.
 *
 * @QCA_WLAN_VENDOR_ATTR_GENERIC_CATEGORY_OMI_NO_T2LM_INFO: The driver includes
 * this category in the event sent to the userspace when it receives a Operatin
 * mode change notification from the STA.
 *
 * @QCA_WLAN_VENDOR_ATTR_GENERIC_CATEGORY_DISASSOC: The driver includes this
 * category in the event sent to the userspace when it receives a disassoc from
 * the connected STA.
 */
enum qca_wlan_vendor_attr_app_generic_category {
	QCA_WLAN_VENDOR_ATTR_GENERIC_CATEGORY_INVALID = 0,
	QCA_WLAN_VENDOR_ATTR_GENERIC_CATEGORY_APP_INIT = 1,
	QCA_WLAN_VENDOR_ATTR_GENERIC_CATEGORY_LINK_BW_NSS_CHANGE = 2,
	QCA_WLAN_VENDOR_ATTR_GENERIC_CATEGORY_ASSOC_NO_T2LM_INFO = 3,
	QCA_WLAN_VENDOR_ATTR_GENERIC_CATEGORY_ASSOC_WITH_T2LM_INFO = 4,
	QCA_WLAN_VENDOR_ATTR_GENERIC_CATEGORY_OMI_NO_T2LM_INFO = 5,
	QCA_WLAN_VENDOR_ATTR_GENERIC_CATEGORY_DISASSOC = 6,
};

/**
 * enum qca_wlan_vendor_attr_t2lm_mlo_peer_link_info - Represents the MLO peer
 * link inforamtion.
 *
 * @QCA_WLAN_VENDOR_ATTR_MLO_PEER_LINK_HW_LINK_ID: u16, represents the hardware
 * link id of the MLO peer link. This is included in the commands sent from the
 * userspace to the driver and used in the events sent from the driver to the
 * userspace.
 *
 * @QCA_WLAN_VENDOR_ATTR_MLO_PEER_LINK_PEER_MAC: 6 byte MAC address represents
 * the MLO peer mac address. This is included in the commands sent from the
 * userspace to the driver and used in the events sent from the driver to the
 * userspace.
 *
 * @QCA_WLAN_VENDOR_ATTR_MLO_PEER_LINK_MLO_LINK_ID: u8, represents the mlo peer
 * link index. This is included in the commands sent from the userspace to the
 * driver and used in the events sent from the driver to the userspace.
 *
 * @QCA_WLAN_VENDOR_ATTR_MLO_PEER_LINK_IS_ASSOC_LINK: u8, this is included in
 * the event sent from the driver to the userspace to identify the Assoc request
 * received link. Userspace includes this in all the commands sent to the driver
 * to identify the Assoc request received list.
 *
 * @QCA_WLAN_VENDOR_ATTR_MLO_PEER_LINK_CHAN_BW: u8, this is included in the
 * event sent from the driver to the userspace to indicate the STA's channel
 * bandwidth. The values are defined in enum qca_wlan_vendor_channel_width.
 * The driver includes this attribute in the event sent for
 * QCA_WLAN_VENDOR_T2LM_CATEGORY_REQUEST,
 * QCA_WLAN_VENDOR_ATTR_GENERIC_CATEGORY_ASSOC_NO_T2LM_INFO and
 * QCA_WLAN_VENDOR_ATTR_GENERIC_CATEGORY_OMI_NO_T2LM_INFO.
 *
 * @QCA_WLAN_VENDOR_ATTR_MLO_PEER_LINK_CHAN_FREQ: u16, this is included in the
 * event sent from the driver to the userspace to indicate the STA's channel
 * frequency in MHz. The driver includes this attribute in the event sent for
 * QCA_WLAN_VENDOR_T2LM_CATEGORY_REQUEST,
 * QCA_WLAN_VENDOR_ATTR_GENERIC_CATEGORY_ASSOC_NO_T2LM_INFO and
 * QCA_WLAN_VENDOR_ATTR_GENERIC_CATEGORY_OMI_NO_T2LM_INFO.
 *
 * @QCA_WLAN_VENDOR_ATTR_MLO_PEER_LINK_AVAILABLE_AIRTIME: u16, this is included
 * in the event sent from the driver to the userspace to indicate MLO peer
 * link's available airtime value (unit is percentage). The driver includes this
 * attribute in the event sent for QCA_WLAN_VENDOR_T2LM_CATEGORY_REQUEST,
 * QCA_WLAN_VENDOR_ATTR_GENERIC_CATEGORY_ASSOC_NO_T2LM_INFO and
 * QCA_WLAN_VENDOR_ATTR_GENERIC_CATEGORY_OMI_NO_T2LM_INFO.
 *
 * @QCA_WLAN_VENDOR_ATTR_MLO_PEER_LINK_RSSI: s8, this is included in the event
 * sent from the driver to the userspace to indicate MLO peer link's (Assoc
 * request received link) RSSI value in dBm. The driver includes this attribute
 * in the event sent for
 * QCA_WLAN_VENDOR_T2LM_CATEGORY_REQUEST,
 * QCA_WLAN_VENDOR_ATTR_GENERIC_CATEGORY_ASSOC_NO_T2LM_INFO and
 * QCA_WLAN_VENDOR_ATTR_GENERIC_CATEGORY_OMI_NO_T2LM_INFO.
 *
 * @QCA_WLAN_VENDOR_ATTR_MLO_PEER_LINK_EHT_CAPS: This is included in the event
 * sent from the driver to the userspace to indicate MLO peer link's EHT
 * capabilities. Values are defined in enum
 * qca_wlan_vendor_attr_eht_peer_capabilities.
 * The driver includes this attribute in the event sent for
 * QCA_WLAN_VENDOR_T2LM_CATEGORY_REQUEST,
 * QCA_WLAN_VENDOR_ATTR_GENERIC_CATEGORY_ASSOC_NO_T2LM_INFO and
 * QCA_WLAN_VENDOR_ATTR_GENERIC_CATEGORY_OMI_NO_T2LM_INFO.
 *
 * @QCA_WLAN_VENDOR_ATTR_MLO_PEER_LINK_BAND_CAP: This is included in the event
 * sent from the driver to the userspace to indicate MLO peer link's band
 * capabilities. Values are defined in enum qca_wlan_vendor_link_band_caps.
 * The driver includes this attribute in the event sent for
 * QCA_WLAN_VENDOR_T2LM_CATEGORY_REQUEST,
 * QCA_WLAN_VENDOR_ATTR_GENERIC_CATEGORY_ASSOC_NO_T2LM_INFO and
 * QCA_WLAN_VENDOR_ATTR_GENERIC_CATEGORY_OMI_NO_T2LM_INFO.
 *
 * @QCA_WLAN_VENDOR_ATTR_MLO_PEER_LINK_VDEV_ID: u8, represents the vdev id.
 * This is included in the commands sent from the userspace to the driver
 * and used in the events sent from the driver to the userspace.
 *
 * @QCA_WLAN_VENDOR_ATTR_MLO_PEER_LINK_AP_MLD_MAC: 6 byte MAC address represents
 * the vdev mld mac address.
 *
 * @QCA_WLAN_VENDOR_ATTR_MLO_PEER_LINK_CAPS: u16, represents peer capabilities.
 */
enum qca_wlan_vendor_attr_t2lm_mlo_peer_link_info {
	QCA_WLAN_VENDOR_ATTR_MLO_PEER_LINK_HW_LINK_ID = 1,
	QCA_WLAN_VENDOR_ATTR_MLO_PEER_LINK_PEER_MAC = 2,
	QCA_WLAN_VENDOR_ATTR_MLO_PEER_LINK_MLO_LINK_ID = 3,
	QCA_WLAN_VENDOR_ATTR_MLO_PEER_LINK_IS_ASSOC_LINK = 4,
	QCA_WLAN_VENDOR_ATTR_MLO_PEER_LINK_CHAN_BW = 5,
	QCA_WLAN_VENDOR_ATTR_MLO_PEER_LINK_CHAN_FREQ = 6,
	QCA_WLAN_VENDOR_ATTR_MLO_PEER_LINK_AVAILABLE_AIRTIME = 7,
	QCA_WLAN_VENDOR_ATTR_MLO_PEER_LINK_RSSI = 8,
	QCA_WLAN_VENDOR_ATTR_MLO_PEER_LINK_EHT_PEER_CAPS = 9,
	QCA_WLAN_VENDOR_ATTR_MLO_PEER_LINK_BAND_CAP = 10,
	QCA_WLAN_VENDOR_ATTR_MLO_PEER_LINK_EFF_CHAN_BW = 11,
	QCA_WLAN_VENDOR_ATTR_MLO_PEER_LINK_VDEV_ID = 12,
	QCA_WLAN_VENDOR_ATTR_MLO_PEER_LINK_AP_MLD_MAC = 13,
	QCA_WLAN_VENDOR_ATTR_MLO_PEER_LINK_CAPS = 14,
	/* keep last */
	QCA_WLAN_VENDOR_ATTR_MLO_PEER_LINK_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_MLO_PEER_LINK_MAX =
		QCA_WLAN_VENDOR_ATTR_MLO_PEER_LINK_AFTER_LAST - 1,
};

/**
 * enum qca_wlan_vendor_channel_width - Represents the channel bandwidth in MHz
 * available in the driver. The driver will send this information to the
 * userspace as part of the registration event.
 *
 * @QCA_WLAN_VENDOR_CHAN_WIDTH_INVALID: Invalid channel bandwidth
 *
 * @QCA_WLAN_VENDOR_CHAN_WIDTH_20MHZ: 20 MHz channel bandwidth
 *
 * @QCA_WLAN_VENDOR_CHAN_WIDTH_40MHZ: 40 MHz channel bandwidth
 *
 * @QCA_WLAN_VENDOR_CHAN_WIDTH_80MHZ: 80 MHz channel bandwidth
 *
 * @QCA_WLAN_VENDOR_CHAN_WIDTH_160MZ: 160 MHz channel bandwidth
 *
 * @QCA_WLAN_VENDOR_CHAN_WIDTH_80_80MHZ: 80+80 MHz channel bandwidth
 *
 * @QCA_WLAN_VENDOR_CHAN_WIDTH_320MHZ: 320 MHz channel bandwidth
 */
enum qca_wlan_vendor_channel_width {
	QCA_WLAN_VENDOR_CHAN_WIDTH_INVALID = 0,
	QCA_WLAN_VENDOR_CHAN_WIDTH_20MHZ = 1,
	QCA_WLAN_VENDOR_CHAN_WIDTH_40MHZ = 2,
	QCA_WLAN_VENDOR_CHAN_WIDTH_80MHZ = 3,
	QCA_WLAN_VENDOR_CHAN_WIDTH_160MZ = 4,
	QCA_WLAN_VENDOR_CHAN_WIDTH_80_80MHZ = 5,
	QCA_WLAN_VENDOR_CHAN_WIDTH_320MHZ = 6,
};

enum qca_wlan_vendor_attr_scs_rule_config {
		QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_INVALID = 0,
		QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_RULE_ID = 1,
		QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_REQUEST_TYPE = 2,
		QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_OUTPUT_TID = 3,
		QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_CLASSIFIER_TYPE = 4,
		QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_TCLAS4_VERSION = 5,
		QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_TCLAS4_SRC_IPV4_ADDR = 6,
		QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_TCLAS4_DST_IPV4_ADDR = 7,
		QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_TCLAS4_SRC_IPV6_ADDR = 8,
		QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_TCLAS4_DST_IPV6_ADDR = 9,
		QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_TCLAS4_SRC_PORT = 10,
		QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_TCLAS4_DST_PORT = 11,
		QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_TCLAS4_DSCP = 12,
		QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_TCLAS4_NEXT_HEADER = 13,
		QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_TCLAS4_FLOW_LABEL = 14,
		QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_TCLAS10_PROTOCOL_INSTANCE = 15,
		QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_TCLAS10_NEXT_HEADER = 16,
		QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_TCLAS10_FILTER_MASK = 17,
		QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_TCLAS10_FILTER_VALUE = 18,
		QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_SERVICE_CLASS_ID = 19,
		QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_DST_MAC_ADDR = 20,
		QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_NETDEV_IF_INDEX = 21,

		/* Keep last */
		QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_AFTER_LAST,
		QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_MAX =
		QCA_WLAN_VENDOR_ATTR_SCS_RULE_CONFIG_AFTER_LAST -1,
};

#define ATH12K_VENDOR_PUT(vendor_event, type, attr, param)             \
	do {                                                            \
		if (nla_put_##type(vendor_event, attr, param)) {        \
			ath12k_err(NULL, "Fails to put " #attr "\n");   \
			return -1;                                      \
		}                                                       \
	} while (0)

#define ATH_PARAM_MASK     0x1000
enum ath_cfg_param_radio {
	ACFG_PARAM_RADIO_TXCHAINMASK	      = 1  | ATH_PARAM_MASK,
	ACFG_PARAM_RADIO_RXCHAINMASK	      = 2  | ATH_PARAM_MASK,
	PARAM_RADIO_TXCHAINSOFT               = 361 | ATH_PARAM_MASK,
};

enum qca_wlan_vendor_channel_width
ath12k_nl_chan_bw_to_qca_vendor_chan_bw(enum nl80211_chan_width chan_bw);

int ath12k_vendor_put_ar_hw_link_id(struct sk_buff *vendor_event,
				    struct ath12k *ar);
int ath12k_vendor_put_ar_link_mac_addr(struct sk_buff *vendor_event,
				       struct ath12k *ar);
int ath12k_vendor_put_ar_chan_info(struct sk_buff *vendor_event,
				   struct ath12k *ar);
int ath12k_vendor_put_ar_nss_chains(struct sk_buff *vendor_event,
				    struct ath12k *ar);
int ath12k_vendor_put_ab_soc_id(struct sk_buff *vendor_event,
				struct ath12k_base *ab);
int ath12k_vendor_put_ab_num_links(struct sk_buff *vendor_event,
				   struct ath12k_base *ab,
				   const int num_active_links);
int ath12k_vendor_put_ag_num_socs(struct sk_buff *vendor_event,
				  struct ath12k_hw_group *ag);
void ath12k_vendor_telemetry_notify_breach(struct ieee80211_vif *vif, u8 *mac_addr,
					   u8 svc_id, u8 param, bool set_clear,
					   u8 tid, u8 *mld_addr);
int ath12k_vendor_register(struct ath12k_hw *ah);
int ath12k_vendor_put_umac_migration_notif(struct ieee80211_vif *vif,
					   u8 *mld_addr, u8 link_id);
#endif
