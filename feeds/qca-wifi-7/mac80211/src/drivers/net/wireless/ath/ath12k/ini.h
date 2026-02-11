/* SPDX-License-Identifier: BSD-3-Clause-Clear */
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#ifndef ATH12K_INI_H
#define ATH12K_INI_H

#include <linux/types.h>
#include <net/netlink.h>
#include <linux/bug.h>
#include "core.h"
#include "debug.h"

/* Key components of the configuration generation using Macros
 * 1. Configuration Macros: e.g ATH12K_CFG_ALL, ATH12K_CFG_DP, ATH12K_CFG_CP etc
 * 2. Configuration Definitions: e.g #define ATH12K_CFG_DP_RXDMA_BUF_RING
 * 3. Fallback Behavior which determines what happens if a config value
 *    is out of range
 * 4. Metadata Table : The macro expansion builds a table of config metadata.
 *    Each entry includes name, offset, type, handler, min/max, and fallback.
 * 5. Default Value Initialization: The function ath12k_cfg_store_set_defaults()
 *    initializes the config store with default values using macro expansion.
 * 6. CFG Value Structure: Generate the cfg_value structure based on type, id.
 *    initializes the config store with default values using macro expansion.
 *
 * Macro Expansion Flow for Meta table population:
 * 1. ATH12K_CFG_ALL expands to all config macros CP/DP etc.
 * 2. Each config macro (like ATH12K_CFG_DP_RXDMA_BUF_RING) expands to an
 *    ATH12K_CFG_INI_* macro.
 * 3. These are then processed by __ATH12K_CFG_INI, which builds the metadata,
 *    cfg_value structure and populates the default values.
 * Eg flow :
 * 1. ATH12K_CFG(ATH12K_CFG_DP_RXDMA_BUF_RING) is replaced with >>
 * 2. _ATH12K_CFG(__ATH12K_CFG_DP_RXDMA_BUF_RING,
 *    rm_parens ATH12K_CFG_DP_RXDMA_BUF_RING)
 * 3. Since we have the internal macro as
 * #define ATH12K_CFG_DP_RXDMA_BUF_RING \
 *    ATH12K_CFG_INI_UINT("dp_rxdma_buf_ring", 1024, 8192, 1024,
 *    ATH12K_CFG_VALUE_OR_DEFAULT, "DP RXDMA buffer ring size")
 * 4. Our code with rm_parens ATH12K_CFG_DP_RXDMA_BUF_RING removes the
 *    parentheses from ATH12K_CFG_DP_RXDMA_BUF_RING macro and we get
 * 5. ATH12K_CFG_INI_UINT("dp_rxdma_buf_ring", 1024, 8192, 1024,
 *    ATH12K_CFG_VALUE_OR_DEFAULT, "DP RXDMA buffer ring size")
 * 6. This is passed to __ATH12K_CFG(...) which using token pasting (##)
 *    to build the final macro name, like:
 *    __ATH12K_CFG_INI_UINT(...) which finally becomes __ATH12K_CFG_INI(args)
 * 7. Which finally expands as below for the meta population
 *{
 *    .name = "dp_rxdma_buf_ring",
 *    .field_offset = offsetof(struct ath12k_cfg_values,
 *                             __ATH12K_CFG_DP_RXDMA_BUF_RING_internal),
 *    .cfg_data_type = ATH12K_CFG_UINT_ITEM,
 *    .ath12k_cfg_item_handler = ath12k_cfg_uint_item_handler,
 *    .min = 1024,
 *    .max = 8192,
 *    .fallback = ATH12K_CFG_VALUE_OR_DEFAULT,
 *},
 *
 * Flow For Default value Initialization :
 * 1. ath12k_cfg_store_set_defaults declares temporary variables with default
 * value (e.g., uint32_t dp_rxdma_buf_ring = 1024;) using macro.
 * 2. Copies default strings into the config store incase for string values.
 * 3. Assigns the temporary default values to the actual config store for non
 *    string based values e.g bool ,int etc.
 *
 * Flow for Populating the cfg_value structure
 * 1. We use the similar macro expansion to initialize the structure with id.
 * the following structure
 * struct ath12k_cfg_values {
 * 		const int32_t __ATH12K_CFG_DP_RXDMA_BUF_RING_internal;
 */

#define ATH12K_CFG_META_NAME_LENGTH_MAX 256
#define ATH12K_CFG_INI_LENGTH_MAX 128
#define ATH12K_CFG_FILE_NAME_MAX 128
#define ATH12K_MAX_CFG 0xffffffff

#define ATH12K_CFG_ALL \
	ATH12K_CFG_DP \
	ATH12K_CFG_CP \

#define ATH12K_CFG_CP \
	ATH12K_CFG(ATH12K_CFG_MAX_DESC) \
	ATH12K_CFG(ATH12K_CFG_LOW_MEM_SYSTEM) \
	ATH12K_CFG(ATH12K_CFG_TWT_INTERFERENCE_THRESH_SETUP) \
	ATH12K_CFG(ATH12K_CFG_PRI20_CFG_BLOCKCHANLIST) \
	ATH12K_CFG(ATH12K_CFG_CARRIER_PROFILE_CFG) \
	ATH12K_CFG(ATH12K_CFG_REP_UL_RESP) \

#define ATH12K_CFG_DP \
	ATH12K_CFG(ATH12K_CFG_DP_RXDMA_BUF_RING) \

#define ATH12K_CFG_MAX_DESC \
	ATH12K_CFG_INI_UINT("max_descs", \
	0, 2198, 0, \
	ATH12K_CFG_VALUE_OR_DEFAULT, "Override default max descriptors")

#define ATH12K_CFG_CARRIER_PROFILE_CFG \
	ATH12K_CFG_INI_UINT("carrier_profile_config", \
	0, 0xFFFFFFFF, 0,  \
	ATH12K_CFG_VALUE_OR_DEFAULT, "config for carrier profile ")

#define ATH12K_CFG_LOW_MEM_SYSTEM \
	ATH12K_CFG_INI_BOOL("low_mem_system", false, \
	"Low Memory System")

#define ATH12K_CFG_TWT_INTERFERENCE_THRESH_SETUP \
	ATH12K_CFG_INI_UINT("twt_interference_thresh_setup", \
	0, 100, 50, \
	ATH12K_CFG_VALUE_OR_DEFAULT, "TWT Setup Interference threshold in percentage")

#define ATH12K_CFG_PRI20_CFG_BLOCKCHANLIST \
	ATH12K_CFG_INI_STRING("pri20_cfg_blockchanlist", \
	0, 512, "", \
	"Primary 20MHz CFG block channel list")

#define WLAN_ATH12K_CFG_RXDMA_BUF_RING_SIZE_MIN 1024
#define WLAN_ATH12K_CFG_RXDMA_BUF_RING_SIZE_MAX 8192
#define WLAN_ATH12K_CFG_RXDMA_BUF_RING_SIZE 1024

#define ATH12K_CFG_DP_RXDMA_BUF_RING \
	ATH12K_CFG_INI_UINT("dp_rxdma_buf_ring", \
	WLAN_ATH12K_CFG_RXDMA_BUF_RING_SIZE_MIN, \
	WLAN_ATH12K_CFG_RXDMA_BUF_RING_SIZE_MAX, \
	WLAN_ATH12K_CFG_RXDMA_BUF_RING_SIZE, \
	ATH12K_CFG_VALUE_OR_DEFAULT, "DP RXDMA buffer ring size")

#define ATH12K_CFG_REP_UL_RESP \
	ATH12K_CFG_INI_UINT("rep_ul_resp", \
	0, 0xFFFFFFFF, 0, \
	ATH12K_CFG_VALUE_OR_DEFAULT, "Enable UL MU-OFDMA/MIMO for Repeater")

/**
 * enum ath12k_cfg_data_type - Enum for CFG/INI types
 * @ATH12K_CFG_VALUE_OR_CLAMP: If values is not within the range the max value
 *  will be set for that variable.
 * @ATH12K_CFG_VALUE_OR_DEFAULT: If a value is not within range the default
 * value will be set for that variable.
 */
enum ath12k_cfg_fallback_behavior {
	ATH12K_CFG_VALUE_OR_CLAMP,
	ATH12K_CFG_VALUE_OR_DEFAULT,
};

#define rm_parens(...) __VA_ARGS__
#define ATH12K_CFG(id) _ATH12K_CFG(__##id, rm_parens id)
#define _ATH12K_CFG(id, args) __ATH12K_CFG(id, args)
#define __ATH12K_CFG(id, is_ini, mtype, args...) \
	__ATH12K_CFG_##is_ini##_##mtype(id, mtype, args)

#define __ATH12K_CFG_INI_INT(args...) __ATH12K_CFG_INI(args)
#define __ATH12K_CFG_INI_UINT(args...) __ATH12K_CFG_INI(args)
#define __ATH12K_CFG_INI_BOOL(args...) __ATH12K_CFG_INI(args)
#define __ATH12K_CFG_INI_STRING(args...) __ATH12K_CFG_INI(args)
#define __ATH12K_CFG_INI_MAC(args...) __ATH12K_CFG_INI(args)
#define __ATH12K_CFG_INI(args...) (args)

/* configuration available in ini */
#define ATH12K_CFG_INI_INT(name, min, max, def, fallback, desc) \
	(INI, INT, int32_t, name, min, max, fallback, desc, def)
#define ATH12K_CFG_INI_UINT(name, min, max, def, fallback, desc) \
	(INI, UINT, uint32_t, name, min, max, fallback, desc, def)
#define ATH12K_CFG_INI_BOOL(name, def, desc) \
	(INI, BOOL, bool, name, false, true, -1, desc, def)
#define ATH12K_CFG_INI_STRING(name, min_len, max_len, def, desc) \
	(INI, STRING, char *, name, min_len, max_len, -1, desc, def)
#define ATH12K_CFG_INI_MAC(name, def, desc) \
	(INI, MAC_ADDR, struct ini_mac_addr, name, -1, -1, -1, desc, def)

#undef __ATH12K_CFG_INI_STRING
#define __ATH12K_CFG_INI_STRING(id, mtype, ctype, name, min, max, fallback, desc, \
			 def...) \
	const char id##_internal[(max) + 1];
#undef __ATH12K_CFG_INI
#define __ATH12K_CFG_INI(id, mtype, ctype, name, min, max, fallback, desc, def...) \
	const ctype id##_internal;

struct ath12k_cfg_values {
	/* e.g. const int32_t __ATH12K_CFG_SCAN_DWELL_TIME_internal; */
	ATH12K_CFG_ALL
};

#undef __ATH12K_CFG_INI_STRING
#define __ATH12K_CFG_INI_STRING(args...) __ATH12K_CFG_INI(args)
#undef __ATH12K_CFG_INI
#define __ATH12K_CFG_INI(args...) (args)

/* Section Parsing - section names to be parsed */
#define ATH12K_CFG_512M_SECTION "512M"
#define ATH12K_CFG_2GHZ_SECTION "2G"
#define ATH12K_CFG_5GHZ_SECTION "5G"
#define ATH12K_CFG_5GHZ_LOW_SECTION "5GL"
#define ATH12K_CFG_5GHZ_HIGH_SECTION "5GH"
#define ATH12K_CFG_6GHZ_HIGH_SECTION "6GH"
#define ATH12K_CFG_6GHZ_LOW_SECTION "6GL"
#define ATH12K_CFG_6GHZ_SECTION "6G"

#define MAC_ADDR_FMT "%pM"
#define MAC_ADDR_REF(a) (a)

#define ath12k_cfg_get(ab, id) __ath12k_cfg_get(ab, __##id)
#define __ath12k_cfg_get(ab, id) (ath12k_cfg_get_values( \
			(struct ath12k_base *)ab)->id##_internal)

/* Value-or-Clamped APIs */
#define ath12k_cfg_clamp(val, min, max) \
		    ((val) < (min) ? (min) : ((val) > (max) ? (max) : (val)))
#define ath12k_cfg_clamp_id(id, value) ath12k_cfg_clamp(value, cfg_min(id), cfg_max(id))

/**
 * ath12k_cfg_init - Initialize the INI
 * @ab: reference to ath12_base
 * Return: 0 on success, Non-zero on error
 */
int ath12k_cfg_init(struct ath12k_base *ab);

/**
 * ath12k_cfg_deinit - De-initialize the INI
 * @ab: reference to ath12_base
 * Return: void
 */
void ath12k_cfg_deinit(struct ath12k_base *ab);

/**
 * ath12k_cfg_global_init - Initialize the INI global ctx
 * Return: void
 */
void ath12k_cfg_global_init(void);

/**
 * ath12k_cfg_global_deinit - Deinitialize the INI global ctx
 * Return: void
 */
void ath12k_cfg_global_deinit(void);

/**
 * ath12k_cfg_parse_pdev_section - Parse pdev section
 * @ab: reference to ath12_base
 * Return: void
 */
void ath12k_cfg_parse_pdev_section(struct ath12k_base *ab);

/**
 * ath12k_cfg_store_print - Print the contents of cfg store
 * @ab: reference to ath12_base
 * Return: 0 on success, Non-zero on error
 */
int ath12k_cfg_store_print(struct ath12k_base *ab);

/**
 * ath12k_cfg_ini_config_print - Print ini configuration
 * @ab: reference to ath12_base
 * @buf: buffer reference
 * @plen: printable length from the supplied buffer
 * @buf_len: length of supplied buffer
 * Return: 0 on success, Non-zero on error
 */
int ath12k_cfg_ini_config_print(struct ath12k_base *ab, uint8_t *buf,
				ssize_t *plen, ssize_t buflen);

/**
 * ath12k_cfg_get_values - get the values from the cfg context
 * @ab: ath12k_base reference
 * Return: reference to cfg value
 */
struct ath12k_cfg_values *ath12k_cfg_get_values(struct ath12k_base *ab);

/**
 * typedef ath12k_ini_section_cb - ath12k_ini section callback functions
 * @context: context passed to the function
 * @name: name of the section
 * Return: 0 on success. Non-zero on error
 */
typedef int (*ath12k_ini_section_cb)(void *context, const char *name);

/**
 * typedef ath12k_ini_item_cb - ath12k_ini callback function
 * @context: context passed to the function
 * @key: Key passed to the function to search the data
 * @value: Value to be validated against the store cfg store metadata
 * Return: 0 on success. Non-zero on error
 */
typedef int (*ath12k_ini_item_cb)(void *context,
				  const char *key,
				  const char *value);
/**
 * struct ath12k_ini_mac_addr - A MAC address
 * @bytes: the raw address bytes array
 */
struct ath12k_ini_mac_addr {
	u8 bytes[ETH_ALEN];
};

/**
 *struct ath12k_ini_file structure to point to ath12k_ini files
 *@external: file name of external ini for particular target
 *@internal: file name of internal ini for particular target
 */
struct ath12k_ini_file {
	char *external;
	char *internal;
};

/**
 *@brief Get corresponding ini files for target
 *@details
 *   Get the corresponding ini files for a given target mainly to
 *   parse particular section based on requirement and use it
 *   accordingly
 *
 *@target_type: Target type to get the ini file
 *Return: 0 on success. Non-zero on error
 */
int ath12k_cfg_get_ini_file_name(u32 target_type, struct ath12k_ini_file *ini);

/**
 * ath12k_ini_parse() - parse an ini file
 * @ath12k_ini_path: the full file path of the ini file to parse
 * @context: the caller supplied context to pass into callbacks
 * @ath12k_item_cb: ini item (key/value pair) handler callback function
 *	return status_success to continue parsing, else to abort
 *
 * the *.ini file format is a simple format consisting of a list of key/value
 * pairs (items), separated by an '=' character. comments are initiated with
 * a '#' character while other form of comments eg '//' are not supported
 * Sections are also supported, using '[' and ']' around the
 * section name. e.g.
 *
 *	# comments are started with a '#' character
 *	# items are key/value string pairs, separated by the '=' character
 *	somekey1=somevalue1
 *	somekey2=somevalue2 # this is also a comment
 *
 *	# section headers are enclosed in square brackets
 *	[some section header] # new section begins
 *	somekey3=somevalue3
 *
 * return: status
 */
int
ath12k_ini_parse(const char *ath12k_ini_path, void *context,
		 ath12k_ini_item_cb item_cb);

/**
 * ath12k_ini_section_parse() - parse a section from ini file
 * @ath12k_ini_path: the full file path of the ini file to parse
 * @context: the caller supplied context to pass into callbacks
 * @ath12k_item_cb: ini item (key/value pair) handler callback function
 *	return status_success to continue parsing, else to abort
 * @ath12k_section_name: ini section name to be parsed from file
 *	return status_success to continue parsing, else to abort
 *
 * the *.ini file format is a simple format consisting of a list of key/value
 * pairs (items), separated by an '=' character. comments are initiated with
 * a '#' character while other form of comments eg '//' are not supported
 * Sections are also supported, using '[' and ']' around the
 * section name. e.g.
 *
 *	# comments are started with a '#' character
 *	# items are key/value string pairs, separated by the '=' character
 *	somekey1=somevalue1
 *	somekey2=somevalue2 # this is also a comment
 *
 *	# section headers are enclosed in square brackets
 *	[some section header] # new section begins
 *	somekey3=somevalue3
 *
 * return: status
 */
int ath12k_ini_section_parse(const char *ath12k_ini_path, void *context,
			     ath12k_ini_item_cb item_cb,
			     const char *ath12k_section_name);

/**
 * valid_ini_check() - check ini file for invalid characters
 * @path: path to ini file
 *
 * return: true if no invalid character found, false otherwise
 */
bool ath12k_valid_ini_check(const char *path);

#endif /* ATH12K_INI_H */
