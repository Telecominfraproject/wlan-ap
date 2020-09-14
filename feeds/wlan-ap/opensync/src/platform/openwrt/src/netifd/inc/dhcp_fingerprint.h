/* SPDX-License-Identifier: BSD-3-Clause */

#ifndef _DHCP_FINGERPRINT_H_
#define _DHCP_FINGERPRINT_H_

#include "netifd.h"

#define MAX_DHCP_FINGERPRINT_OPTION_SEQ		256
#define MAX_FINGERPRINT_DEVICE_DESCRIPTION	256
#define MAX_DHCP_FINGERPRINT_LOCAL_DB	64
#define DHCP_FINGERPRINT_DB_FILE 	"/etc/dhcp_fingerprints.db"

typedef enum {
	DHCP_FP_DB_SUCCESS = 0,
	DHCP_FP_DB_FAILURE
} dhcp_fp_dbstatus_t;

typedef enum {
	DHCP_FP_DEV_TYPE_MISC = 0,
	DHCP_FP_DEV_TYPE_MOBILE = 1,
	DHCP_FP_DEV_TYPE_PC = 2,
	DHCP_FP_DEV_TYPE_PRINTER = 3,
	DHCP_FP_DEV_TYPE_VIDEO = 4,
	DHCP_FP_DEV_TYPE_GAME = 5,
	DHCP_FP_DEV_TYPE_VOIP = 6,
	DHCP_FP_DEV_TYPE_MONITORING = 7,
	DHCP_FP_DEV_TYPE_MAX	= 8
} dhcp_fp_devicetype_t;

typedef enum {
	DHCP_FP_DEV_MANUF_MISC = 0,
	DHCP_FP_DEV_MANUF_SAMSUNG = 1,
	DHCP_FP_DEV_MANUF_APPLE = 2,
	DHCP_FP_DEV_MANUF_GOOGLE = 3,
	DHCP_FP_DEV_MANUF_HP = 4,
	DHCP_FP_DEV_MANUF_INTEL = 5,
	DHCP_FP_DEV_MANUF_MICROSOFT = 6,
	DHCP_FP_DEV_MANUF_LG = 7,
	DHCP_FP_DEV_MANUF_CANON = 8,
	DHCP_FP_DEV_MANUF_BROTHER = 9,
	DHCP_FP_DEV_MANUF_DELL = 10,
	DHCP_FP_DEV_MANUF_LENOVO = 11,
	DHCP_FP_DEV_MANUF_VIVO = 12,
	DHCP_FP_DEV_MANUF_ALCATEL = 13,
	DHCP_FP_DEV_MANUF_ZTE = 14,
	DHCP_FP_DEV_MANUF_SONY = 15,
	DHCP_FP_DEV_MANU_MAX = 16
} dhcp_fp_manufid_t;


typedef struct {
	dhcp_fp_dbstatus_t db_status;
	char option_seq[MAX_DHCP_FINGERPRINT_OPTION_SEQ];
	char device_name[MAX_FINGERPRINT_DEVICE_DESCRIPTION];
	dhcp_fp_devicetype_t device_type;
	dhcp_fp_manufid_t manuf_id;
} dhcp_fp_data_t;

typedef struct {
	int db_num;
	dhcp_fp_data_t devices[MAX_DHCP_FINGERPRINT_LOCAL_DB];
	int index;
}dhcp_fp_localdb_t;

bool dhcp_fp_db_lookup (dhcp_fp_data_t *fp_data, char *option_seq);

#endif
