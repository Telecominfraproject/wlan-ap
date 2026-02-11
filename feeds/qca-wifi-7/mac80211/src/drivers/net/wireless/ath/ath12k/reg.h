/* SPDX-License-Identifier: BSD-3-Clause-Clear */
/*
 * Copyright (c) 2019-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) 2021-2025 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#ifndef ATH12K_REG_H
#define ATH12K_REG_H

#include <linux/kernel.h>
#include <net/regulatory.h>

struct ath12k_base;
struct ath12k;

#define ATH12K_2GHZ_MAX_FREQUENCY	2495
#define ATH12K_5GHZ_MAX_FREQUENCY	5920

#define AFC_AUTH_STATUS_OFFSET	1
#define AFC_AUTH_SUCCESS	1
#define AFC_AUTH_ERROR		0
#define REG_SP_CLIENT_TYPE	3

#define ATH12K_MAX_CHANNELS_PER_6GHZ_OPERATING_CLASS	70
#define DEFAULT_MIN_POWER                             (-10)
#define ATH12K_MIN_6GHZ_OPER_CLASS                     131
#define ATH12K_MAX_6GHZ_OPER_CLASS                     137
#define ATH12K_FREQ_TO_CHAN_SCALE                        5

extern bool ath12k_afc_disable_timer_check;
extern bool ath12k_afc_disable_req_id_check;
extern bool ath12k_afc_reg_no_action;
extern bool ath12k_6ghz_sp_pwrmode_supp_enabled;

/* DFS regdomains supported by Firmware */
enum ath12k_dfs_region {
	ATH12K_DFS_REG_UNSET,
	ATH12K_DFS_REG_FCC,
	ATH12K_DFS_REG_ETSI,
	ATH12K_DFS_REG_MKK,
	ATH12K_DFS_REG_CN,
	ATH12K_DFS_REG_KR,
	ATH12K_DFS_REG_MKK_N,
	ATH12K_DFS_REG_UNDEF,
};

enum ath12k_afc_power_update_status {
	ath12k_AFC_POWER_UPDATE_IGNORE = 0, /* Used for expiry event */
	ath12k_AFC_POWER_UPDATE_SUCCESS = 1,
	ath12k_AFC_POWER_UPDATE_FAIL = 3,
};

/**
 * enum ath12k_afc_event_state - AFC event state enumeration
 * @ATH12K_AFC_EVENT_POWER_INFO: Power information event
 * @ATH12K_AFC_EVENT_TIMER_EXPIRY: Timer expiry event
 *
 * Enumeration of different AFC event states.
 */
enum ath12k_afc_event_state {
	ATH12K_AFC_EVENT_POWER_INFO   = 1,
	ATH12K_AFC_EVENT_TIMER_EXPIRY = 2,
};

/**
 * enum ath12k_afc_expiry_event_subtype - AFC expiry event subtype enumeration
 * @REG_AFC_EXPIRY_EVENT_START: Start expiry event
 * @REG_AFC_EXPIRY_EVENT_RENEW: Renew expiry event
 * @REG_AFC_EXPIRY_EVENT_SWITCH_TO_LPI: Switch to LPI expiry event
 *
 * Enumeration of different AFC expiry event subtypes.
 */
enum ath12k_afc_expiry_event_subtype {
	REG_AFC_EXPIRY_EVENT_START = 1,
	REG_AFC_EXPIRY_EVENT_RENEW = 2,
	REG_AFC_EXPIRY_EVENT_SWITCH_TO_LPI = 3,
};

enum ath12k_afc_power_event_status_code {
	REG_FW_AFC_POWER_EVENT_SUCCESS = 0,
	REG_FW_AFC_POWER_EVENT_RESP_NOT_RECEIVED = 1,
	REG_FW_AFC_POWER_EVENT_RESP_PARSING_FAILURE = 2,
	REG_FW_AFC_POWER_EVENT_FAILURE = 3,
};

enum ath12k_serv_resp_code {
	REG_AFC_SERV_RESP_GENERAL_FAILURE = -1,
	REG_AFC_SERV_RESP_SUCCESS = 0,
	REG_AFC_SERV_RESP_VERSION_NOT_SUPPORTED = 100,
	REG_AFC_SERV_RESP_DEVICE_UNALLOWED = 101,
	REG_AFC_SERV_RESP_MISSING_PARAM = 102,
	REG_AFC_SERV_RESP_INVALID_VALUE = 103,
	REG_AFC_SERV_RESP_UNEXPECTED_PARAM = 106,
	REG_AFC_SERV_RESP_UNSUPPORTED_SPECTRUM = 300,
};

struct ath12k_bw_10log10_pair {
	u16 bw;
	s16 ten_l_ten;
};

static const struct ath12k_bw_10log10_pair ath12k_bw_to_10log10_map[] = {
	{ 20, 13}, /* 10* 1.30102 = 13.0102 */
	{ 40, 16}, /* 10* 1.60205 = 16.0205 */
	{ 80, 19}, /* 10* 1.90308 = 19.0308 */
	{160, 22}, /* 10* 2.20411 = 22.0411 */
	{320, 25}, /* 10* 2.50514 = 25.0514 */
	{ 60, 18}, /* 10* 1.77815 = 17.7815 */
	{140, 21}, /* 10* 2.14612 = 21.4612 */
	{120, 21}, /* 10* 2.07918 = 20.7918 */
	{200, 23}, /* 10* 2.30102 = 23.0102 */
	{240, 24}, /* 10* 2.38021 = 23.8021 */
	{280, 24}, /* 10* 2.44715 = 24.4715 */
};

struct ath12k_opclass_bw_pair {
	u8 opclass;
	u16 bw;
};

static const struct ath12k_opclass_bw_pair ath12k_opclass_bw_map[] = {
	{131, ATH12K_CHWIDTH_20},
	{132, ATH12K_CHWIDTH_40},
	{133, ATH12K_CHWIDTH_80},
	{134, ATH12K_CHWIDTH_160},
	{137, ATH12K_CHWIDTH_320},
	/* TODO: Enhance to include 2.4 GHz and 5 GHz opclasses */
};

struct ath12k_opclass_nchans_pair {
	u8 opclass;
	u8 nchans;
};

static const struct ath12k_opclass_nchans_pair ath12k_opclass_nchans_map[] = {
	{131, 1},
	{136, 1},
	{132, 2},
	{133, 4},
	{134, 8},
	{137, 16},
};

struct ath12k_afc_freq_obj {
	u32 low_freq;
	u32 high_freq;
	s16 max_psd;
};

struct ath12k_chan_eirp_obj {
	u8 cfi;
	u16 eirp_power;
};

struct ath12k_afc_chan_obj {
	u8 global_opclass;
	u8 num_chans;
	struct ath12k_chan_eirp_obj *chan_eirp_info;
};

struct ath12k_afc_sp_reg_info {
	u32 resp_id;
	enum ath12k_afc_power_event_status_code fw_status_code;
	enum ath12k_serv_resp_code serv_resp_code;
	u32 afc_wfa_version;
	u32 avail_exp_time_d;
	u32 avail_exp_time_t;
	u8 num_freq_objs;
	u8 num_chan_objs;
	struct ath12k_afc_freq_obj *afc_freq_info;
	struct ath12k_afc_chan_obj *afc_chan_info;
};

struct ath12k_afc_expiry_info {
	bool is_afc_exp_valid;
	enum ath12k_afc_expiry_event_subtype event_subtype;
	u32 req_id;
};

/**
 * struct ath12k_afc_freq_range_obj - Frequency range object
 * @lowfreq: Lower frequency
 * @highfreq: Higher frequency
 *
 * Structure representing a frequency range object.
 */
struct ath12k_afc_freq_range_obj {
	u16 lowfreq;
	u16 highfreq;
};

/**
 * struct ath12k_afc_frange_list - Frequency range list
 * @num_ranges: Number of ranges
 * @range_objs: Pointer to array of frequency range objects
 *
 * Structure representing a list of frequency ranges.
 */
struct ath12k_afc_frange_list {
	u32 num_ranges;
	struct ath12k_afc_freq_range_obj *range_objs;
};

/**
 * struct ath12k_afc_opclass_obj - Operating class object
 * @opclass_num_cfis: Number of CFIs in the operating class
 * @opclass: Operating class
 * @cfis: Pointer to array of CFIs
 *
 * Structure representing an operating class object.
 */
struct ath12k_afc_opclass_obj {
	u8 opclass_num_cfis;
	u8 opclass;
	u8 *cfis;
};

/**
 * struct ath12k_afc_opclass_obj_list - List of operating class objects
 * @num_opclass_objs: Number of operating class objects
 * @opclass_objs: Pointer to array of operating class objects
 *
 * Structure representing a list of operating class objects.
 */
struct ath12k_afc_opclass_obj_list {
	u8 num_opclass_objs;
	struct ath12k_afc_opclass_obj *opclass_objs;
};

/**
 * enum ath12k_afc_dev_deploy_type - AFC device deployment type enumeration
 * @ATH12K_AFC_DEPLOYMENT_UNKNOWN: Unknown deployment type
 * @ATH12K_AFC_DEPLOYMENT_INDOOR: Indoor deployment type
 * @ATH12K_AFC_DEPLOYMENT_OUTDOOR: Outdoor deployment type
 *
 * Enumeration of different AFC device deployment types.
 */
enum ath12k_afc_dev_deploy_type {
	ATH12K_AFC_DEPLOYMENT_UNKNOWN = 0,
	ATH12K_AFC_DEPLOYMENT_INDOOR  = 1,
	ATH12K_AFC_DEPLOYMENT_OUTDOOR = 2,
};

/**
 * struct ath12k_afc_location - AFC location object
 * @deployment_type: Deployment type
 *
 * Structure representing an AFC location object.
 */
struct ath12k_afc_location {
	enum ath12k_afc_dev_deploy_type deployment_type;
};

/**
 * struct ath12k_afc_host_request - AFC host request
 * @req_id: Request ID
 * @version_minor: Minor version
 * @version_major: Major version
 * @min_des_power: Minimum desired power
 * @freq_lst: Pointer to frequency range list
 * @opclass_obj_lst: Pointer to operating class object list
 * @afc_location: Pointer to AFC location object
 *
 * Structure representing an AFC host request.
 */
struct ath12k_afc_host_request {
	u64 req_id;
	u16 version_minor;
	u16 version_major;
	s16 min_des_power;
	struct ath12k_afc_frange_list *freq_lst;
	struct ath12k_afc_opclass_obj_list *opclass_obj_lst;
	struct ath12k_afc_location *afc_location;
} __packed;

/**
 * struct ath12k_afc_info - AFC-related information maintained per device
 * @request_id: Request ID associated with the current AFC request.
 *              This ID is typically received from the firmware during AFC expiry
 *              events and is used to track and correlate AFC requests.
 *
 * @expiry_event_subtype: Subtype of the AFC expiry event as defined in
 *                        enum ath12k_afc_expiry_event_subtype. This helps determine
 *                        the appropriate action (e.g., renew, switch to LPI).
 *
 * @afc_wfa_version: Version of the AFC WFA (Wi-Fi Alliance) specification used
 *                   in the current request.
 *
 * @is_6g_afc_expiry_event_received: Boolean flag indicating whether an AFC expiry
 *                                   event has occurred. Set to true when the firmware
 *                                   notifies the host of an expiry condition.
 *
 * @afc_req: Pointer to the current AFC host request structure.
 *           This holds the most recent AFC request data including frequency ranges,
 *           operating classes, and location (if available).
 */

struct ath12k_afc_info {
	enum ath12k_afc_event_state event_type;
	bool is_6ghz_afc_power_event_received;
	struct ath12k_afc_sp_reg_info *afc_reg_info;
	bool afc_regdom_configured;
	u32 request_id;
	enum ath12k_afc_expiry_event_subtype event_subtype;
	u32 afc_wfa_version;
	bool is_6g_afc_expiry_event_received;
	struct ath12k_afc_host_request *afc_req;
};

/**
 * struct ath12k_afc_host_resp - Structure for AFC Host response to FW
 * @header:Header for compatibility.
 *         Valid value: 0
 * @status:Flag to indicate validity of data. To be updated by TZ
 *         1:  Success
 *         -1: Failure
 * @time_to_live: Period(in seconds) the data is valid for
 * @length:       Length of the response message
 * @resp_format:AFC response format.
 *              0 - JSON format
 *              1 - Binary data format
 * @afc_resp:Response message from the AFC server for queried parameters
 *
 * The following is the layout of the AFC response.
 *
 * struct ath12k_afc_host_resp {
 *     header;
 *     status;
 *     time_to_live;
 *     length;
 *     resp_format;
 *     afc_resp {
 *          struct ath12k_afc_bin_resp_data fixed_params;
 *          struct ath12k_afc_resp_freq_psd_info obj[0];
 *          ....
 *          struct ath12k_afc_resp_freq_psd_info obj[num_frequency_obj - 1];
 *          struct ath12k_afc_resp_opclass_info opclass[0];
 *          {
 *              struct ath12k_afc_resp_eirp_info eirp[0];
 *              ....
 *              struct ath12k_afc_resp_eirp_info eirp[num_channels - 1];
 *          }
 *          .
 *          .
 *          struct ath12k_afc_resp_opclass_info opclass[num_channel_obj - 1];
 *          {
 *              struct ath12k_afc_resp_eirp_info eirp[0];
 *              ....
 *              struct ath12k_afc_resp_eirp_info eirp[num_channels - 1];
 *          }
 *     }
 * }
 *
 */
struct ath12k_afc_host_resp {
	u32 header;
	s32 status;
	u32 time_to_live;
	u32 length;
	u32 resp_format;
	s8  afc_resp[];
} __packed;

/**
 * struct at12k_afc_resp_opclass_info - Structure to populate operating class
 * and channel information from AFC response.
 * @opclass: Operating class
 * @num_channels: Number of channels received in AFC response
 */
struct ath12k_afc_resp_opclass_info {
	u32 opclass;
	u32 num_channels;
} __packed;

/**
 * struct ath12k_afc_resp_eirp_info - Structure to update EIRP values for channels
 * @channel_cfi: Channel center frequency index
 * @max_eirp_pwr: Maximum permissible EIRP(in dBm) for the Channel
 */
struct ath12k_afc_resp_eirp_info {
	u32 channel_cfi;
	s32 max_eirp_pwr;
} __packed;

/**
 * struct ath12k_afc_resp_freq_psd_info - Structure to update PSD values for
 * queried frequency ranges
 * @freq_info: Frequency range in MHz :- bits 15:0  = u16 start_freq,
 *                                       bits 31:16 = u16 end_freq
 * @max_psd: Maximum PSD in dbm/MHz
 */
struct ath12k_afc_resp_freq_psd_info {
	u32 freq_info;
	u32 max_psd;
} __packed;

/**
 * struct ath12k_afc_bin_resp_data - Structure to populate AFC binary response
 * @local_err_code: Internal error code between AFC app and FW
 *                  0 - Success
 *                  1 - General failure
 * @version: Internal version between AFC app and FW
 *           Current version - 1
 * @afc_wfa_version: AFC spec version info. Bits 15:0  - Minor version
 *                                          Bits 31:16 - Major version
 * @request_id: AFC unique request ID
 * @avail_exp_time_d: Availability expiry date in UTC.
 *                    Date format- bits 7:0   - DD (Day 1-31)
 *                                 bits 15:8  - MM (Month 1-12)
 *                                 bits 31:16 - YYYY (Year)
 * @avail_exp_time_t: Availability expiry time in UTC.
 *                    Time format- bits 7:0   - SS (Seconds 0-59)
 *                                 bits 15:8  - MM (Minutes 0-59)
 *                                 bits 23:16 - HH (Hours 0-23)
 *                                 bits 31:24 - Reserved
 * @afc_serv_resp_code: AFC server response code. The AFC server response codes
 *                      are defined in the WiFi Spec doc for AFC as follows:
 *                      0: Success.
 *                      100 - 199 - General errors related to protocol.
 *                      300 - 399 - Error events specific to message exchange
 *                                  for the available Spectrum Inquiry.
 * @num_frequency_obj: Number of frequency objects
 * @num_channel_obj: Number of channel objects
 * @shortdesc: Short description corresponding to resp_code field
 * @reserved: Reserved for future use
 */
struct ath12k_afc_bin_resp_data {
	u32 local_err_code;
	u32 version;
	u32 afc_wfa_version;
	u32 request_id;
	u32 avail_exp_time_d;
	u32 avail_exp_time_t;
	u32 afc_serv_resp_code;
	u32 num_frequency_obj;
	u32 num_channel_obj;
	u8  shortdesc[64];
	u32 reserved[2];
} __packed;

/**
 * enum offset_t - Offset type enumeration
 * @BW20: 20 MHz bandwidth
 * @BW40_LOW_PRIMARY: 40 MHz bandwidth with low primary channel
 * @BW40_HIGH_PRIMARY: 40 MHz bandwidth with high primary channel
 * @BW80: 80 MHz bandwidth
 * @BWALL: All bandwidths
 * @BW_INVALID: Invalid bandwidth
 *
 * Enumeration of different bandwidth offsets.
 */
enum offset_t {
	BW20 = 0,
	BW40_LOW_PRIMARY = 1,
	BW40_HIGH_PRIMARY = 3,
	BW80,
	BWALL,
	BW_INVALID = 0xFF
};

/**
 * enum behav_limit - Behavior limit enumeration
 * @BEHAV_NONE: No behavior limit
 * @BEHAV_BW40_LOW_PRIMARY: Behavior limit for 40 MHz bandwidth with low primary channel
 * @BEHAV_BW40_HIGH_PRIMARY: Behavior limit for 40 MHz bandwidth with high primary channel
 * @BEHAV_BW80_PLUS: Behavior limit for 80 MHz bandwidth plus
 * @BEHAV_INVALID: Invalid behavior limit
 *
 * Enumeration of different behavior limits.
 */
enum behav_limit {
	BEHAV_NONE,
	BEHAV_BW40_LOW_PRIMARY,
	BEHAV_BW40_HIGH_PRIMARY,
	BEHAV_BW80_PLUS,
	BEHAV_INVALID = 0xFF
};

/**
 * struct ath12k_c_freq_lst - Frequency list
 * @num_cfis: Number of CFIs
 * @p_cfis_arr: Pointer to array of CFIs
 *
 * Structure representing a frequency list.
 */
struct ath12k_c_freq_lst {
	u8 num_cfis;
	const u8 *p_cfis_arr;
};

/**
 * struct ath12k_op_class_map_t - Operating class map
 * @op_class: Operating class
 * @chan_spacing: Channel spacing
 * @offset: Offset type
 * @behav_limit: Behavior limit
 * @start_freq: Starting frequency
 * @channels: Array of channels
 * @p_cfi_lst_obj: Pointer to frequency list object
 *
 * Structure representing an operating class map.
 */
struct ath12k_op_class_map_t {
	u8 op_class;
	u16 chan_spacing;
	enum offset_t offset;
	u16 behav_limit;
	u16 start_freq;
	u8 channels[ATH12K_MAX_CHANNELS_PER_6GHZ_OPERATING_CLASS];
	const struct ath12k_c_freq_lst *p_cfi_lst_obj;
};

enum ath12k_reg_cc_code {
	REG_SET_CC_STATUS_PASS = 0,
	REG_CURRENT_ALPHA2_NOT_FOUND = 1,
	REG_INIT_ALPHA2_NOT_FOUND = 2,
	REG_SET_CC_CHANGE_NOT_ALLOWED = 3,
	REG_SET_CC_STATUS_NO_MEMORY = 4,
	REG_SET_CC_STATUS_FAIL = 5,
};

struct ath12k_reg_rule {
	u16 start_freq;
	u16 end_freq;
	u16 max_bw;
	u8 reg_power;
	u8 ant_gain;
	u16 flags;
	bool psd_flag;
	s8 psd_eirp;
};

struct ath12k_reg_info {
	enum ath12k_reg_cc_code status_code;
	u8 num_phy;
	u8 phy_id;
	u16 reg_dmn_pair;
	u16 ctry_code;
	u8 alpha2[REG_ALPHA2_LEN + 1];
	u32 dfs_region;
	u32 phybitmap;
	bool is_ext_reg_event;
	u32 min_bw_2g;
	u32 max_bw_2g;
	u32 min_bw_5g;
	u32 max_bw_5g;
	u32 num_2g_reg_rules;
	u32 num_5g_reg_rules;
	struct ath12k_reg_rule *reg_rules_2g_ptr;
	struct ath12k_reg_rule *reg_rules_5g_ptr;
	enum wmi_reg_6g_client_type client_type;
	bool rnr_tpe_usable;
	bool unspecified_ap_usable;
	/* TODO: All 6G related info can be stored only for required
	 * combination instead of all types, to optimize memory usage.
	 */
	u8 domain_code_6g_ap[WMI_REG_CURRENT_MAX_AP_TYPE];
	u8 domain_code_6g_client[WMI_REG_CURRENT_MAX_AP_TYPE][WMI_REG_MAX_CLIENT_TYPE];
	u32 domain_code_6g_super_id;
	u32 min_bw_6g_ap[WMI_REG_CURRENT_MAX_AP_TYPE];
	u32 max_bw_6g_ap[WMI_REG_CURRENT_MAX_AP_TYPE];
	u32 min_bw_6g_client[WMI_REG_CURRENT_MAX_AP_TYPE][WMI_REG_MAX_CLIENT_TYPE];
	u32 max_bw_6g_client[WMI_REG_CURRENT_MAX_AP_TYPE][WMI_REG_MAX_CLIENT_TYPE];
	u32 num_6g_reg_rules_ap[WMI_REG_CURRENT_MAX_AP_TYPE];
	u32 num_6g_reg_rules_cl[WMI_REG_CURRENT_MAX_AP_TYPE][WMI_REG_MAX_CLIENT_TYPE];
	struct ath12k_reg_rule *reg_rules_6g_ap_ptr[WMI_REG_CURRENT_MAX_AP_TYPE];
	struct ath12k_reg_rule *reg_rules_6g_client_ptr
		[WMI_REG_CURRENT_MAX_AP_TYPE][WMI_REG_MAX_CLIENT_TYPE];
};

/* Phy bitmaps */
enum ath12k_reg_phy_bitmap {
	ATH12K_REG_PHY_BITMAP_NO11AX	= BIT(5),
	ATH12K_REG_PHY_BITMAP_NO11BE	= BIT(6),
};

void ath12k_reg_init(struct ieee80211_hw *hw);
void ath12k_reg_free(struct ath12k_base *ab);
void ath12k_regd_update_work(struct work_struct *work);

/**
 * ath12k_set_previous_country_work - Reset to previous country code
 * @work: Pointer to work_struct
 *
 * This function is queued when the firmware returns zero regulatory rules.
 * It resets the regulatory domain to the previous country code.
 */
void ath12k_set_previous_country_work(struct work_struct *work);
struct ieee80211_regdomain *ath12k_reg_build_regd(struct ath12k_base *ab,
						  struct ath12k_reg_info *reg_info);
enum wmi_reg_6g_ap_type
ath12k_ieee80211_ap_pwr_type_convert(enum ieee80211_ap_reg_power power_type);
int ath12k_regd_update(struct ath12k *ar, bool init);
int ath12k_reg_update_chan_list(struct ath12k *ar, bool wait);
int ath12k_reg_get_num_chans_in_band(struct ath12k *ar,
				     struct ieee80211_supported_band *band);

/**
 * ath12k_reg_process_afc_power_event() - Process the AFC power event
 * @ar: pointer to ath12k
 * @afc: pointer to the AFC payload information
 *
 * This API processes the AFC power event and updates the regulatory domain
 * accordingly.
 *
 * Return: 0 on success, negative error code on failure
 */
int ath12k_reg_process_afc_power_event(struct ath12k *ar,
				       const struct ath12k_afc_info *afc);

/**
 * ath12k_get_afc_req_info - Get AFC request information
 * @ar: Pointer to ath12k structure
 * @afc_req: Pointer to pointer of AFC host request
 * @request_id: Request ID
 *
 * This function gets the AFC request information. It allocates memory for the
 * AFC host request structure and fills it with the necessary information,
 * including frequency list, operating class object list, and location object.
 *
 * Memory Allocation:
 * - The function allocates memory for the `ath12k_afc_host_request` structure
 *   and its substructures (`freq_lst`, `opclass_obj_lst`, and `afc_location`).
 * - If any of the allocations fail, the function frees the previously
 *   allocated memory and returns an error code.
 *
 * Memory Deallocation:
 * - The allocated memory should be freed using the `ath12k_free_afc_req`
 *   function once the AFC request is no longer needed. This function frees
 *   the `afc_req` structure and its substructures.
 * - It is recommended to free the memory in the calling function or after the
 *   AFC request has been processed and the information is no longer required.
 *
 * Return: 0 on success, negative error code on failure
 */
int ath12k_get_afc_req_info(struct ath12k *ar,
			    struct ath12k_afc_host_request **afc_req,
			    u64 request_id);

/**
 * ath12k_process_expiry_event - Process AFC expiry event received from the FW
 * @ar: Pointer to ath12k structure
 *
 * Return: 0 on success, negative error code on failure
 */
int ath12k_process_expiry_event(struct ath12k *ar);

int ath12k_copy_afc_response(struct ath12k *ar, char *afc_resp, u32 len);
u8 ath12k_reg_get_nsubchannels_for_opclass(u8 opclass);
void ath12k_reg_fill_subchan_centers(u8 nchans, u8 cfi, u8 *subchannels);

/**
 * ath12k_reg_get_opclass_from_bw() - Get operating class from bandwidth
 * @bw: Bandwidth in MHz
 *
 * This API returns the operating class corresponding to the given bandwidth.
 *
 * Return: Operating class on success, 0 on failure
 */
u8 ath12k_reg_get_opclass_from_bw(u16 bw);
s16 ath12k_reg_psd_2_eirp(s16 psd, uint16_t ch_bw);
void ath12k_reg_get_regulatory_pwrs(struct ath12k *ar, u32 freq,
				    u8 reg_6g_power_mode, s8 *max_reg_eirp,
				    s8 *reg_psd);
void ath12k_reg_get_reg_sp_regulatory_pwrs(struct ath12k_base *ab,
					   u32 freq,
					   s8 *max_reg_eirp,
					   s8 *reg_psd);
s8 ath12k_reg_get_afc_eirp_power(struct ath12k *ar, enum nl80211_chan_width bw,
				 int cfi);
void ath12k_reg_get_afc_eirp_power_for_bw(struct ath12k *ar, u16 *start_freq,
					  u16 *center_freq, int pwr_level,
					  struct cfg80211_chan_def *chan_def,
					  s8 *tx_power);
/**
 * ath12k_free_afc_power_event_info() - Free AFC power event info
 * @afc: Pointer to ath12k_afc_info
 *
 * Return: None
 */
void ath12k_free_afc_power_event_info(struct ath12k_afc_info *afc);
#endif
