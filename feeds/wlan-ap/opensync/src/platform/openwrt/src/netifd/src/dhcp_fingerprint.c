/* SPDX-License-Identifier: BSD-3-Clause */

#include "netifd.h"
#include "dhcp_fingerprint.h"

dhcp_fp_localdb_t local_db = {0};

bool dhcp_fp_db_lookup (dhcp_fp_data_t *fp_data, char *option_seq)
{
	FILE *fp = NULL;
	char *line = NULL;
	int i, rd, rt, found = 0;
	size_t len;
	int dev_type, dev_manufid = 0;

	memset(fp_data, 0, sizeof(dhcp_fp_data_t));

	/* first, try to find it from local db */
	for (i=0; i < local_db.db_num; i++) {
		if (!strncmp(local_db.devices[i].option_seq, option_seq, MAX_DHCP_FINGERPRINT_OPTION_SEQ)) {
			LOG(DEBUG, "found a match from db cache");

			strcpy(fp_data->device_name, local_db.devices[i].device_name);
			fp_data->device_type = local_db.devices[i].device_type;
			fp_data->manuf_id = local_db.devices[i].manuf_id;
			fp_data->db_status = local_db.devices[i].db_status;;

			return true;
		}
	}

	fp = fopen(DHCP_FINGERPRINT_DB_FILE, "r");
	if (fp == NULL){
		LOG(ERR, "fingerprints.conf file does not exist");
		return false;
	}

	while ((rd = getline(&line, &len, fp)) != -1) {
		if (rd < 0)
			continue;

		if (line[0] == '#')
			continue;

		if (rd -1 >= 0)
			line[rd - 1] = '\0';
		else continue;

		if ((rt = strncmp(option_seq, &(line[0]), rd)) == 0) {
			if ((rd = getline(&line, &len, fp)) < 0) {
				LOG(ERR, "db file is in wrong format, device name");
				goto db_lookup_exit;
			}

			if (rd >= MAX_FINGERPRINT_DEVICE_DESCRIPTION) {
				LOG(ERR, "device name length from db:%d", rd);
				goto db_lookup_exit;
			}
			line[rd - 1] = '\0';
			strncpy(fp_data->device_name, &(line[0]), MAX_FINGERPRINT_DEVICE_DESCRIPTION);
			fp_data->db_status = DHCP_FP_DB_SUCCESS;


			if ((rd = getline(&line, &len, fp)) < 0) {
				LOG(ERR, "db file is in wrong format, device type");
				goto db_lookup_exit;
			}
			line[rd - 1] = '\0';
			dev_type = atoi(&(line[0]));
			if (dev_type >= DHCP_FP_DEV_TYPE_MAX) {
				LOG(ERR, "db file is in wrong format, device type:%d", dev_type);
				goto db_lookup_exit;
			}


			if ((rd = getline(&line, &len, fp)) < 0) {
				LOG(ERR, "db file is in wrong format, manufacturer id");
				goto db_lookup_exit;
			}

			line[rd - 1] = '\0';
			dev_manufid = atoi(&(line[0]));
			if (dev_manufid >= DHCP_FP_DEV_MANU_MAX) {
				LOG(ERR, "db file is in wrong format, manufacturer id:%d", dev_manufid);
				goto db_lookup_exit;
			}

			fp_data->device_type = dev_type;
			fp_data->manuf_id = dev_manufid;
			fp_data->db_status = DHCP_FP_DB_SUCCESS;
			found = 1;

			LOG(DEBUG, "match found, localDbEntries:%d, device_name:%s added to index:%d", local_db.db_num, fp_data->device_name, local_db.index-1);
			break;
		}
	}

	if (!found) {
		LOGN("dhcp option seq not found in the db, seq=%s", option_seq);
		fp_data->db_status = DHCP_FP_DB_FAILURE;
		fp_data->manuf_id = DHCP_FP_DEV_MANUF_MISC;
		fp_data->device_type = DHCP_FP_DEV_TYPE_MISC;
		fp_data->device_name[0] = '\0';
	}

	/* store in the local database */
	memcpy(local_db.devices[local_db.index].option_seq, option_seq, MAX_DHCP_FINGERPRINT_OPTION_SEQ);
	strcpy(local_db.devices[local_db.index].device_name, fp_data->device_name);
	local_db.devices[local_db.index].device_type = fp_data->device_type;
	local_db.devices[local_db.index].manuf_id = fp_data->manuf_id;
	local_db.devices[local_db.index].db_status = fp_data->db_status;

	LOG(DEBUG, "db added, option_seq:%s, name:%s, numofDbEntry:%d , index=%d", option_seq, fp_data->device_name, local_db.db_num, local_db.index);

	if (local_db.db_num < MAX_DHCP_FINGERPRINT_LOCAL_DB)
		local_db.db_num++;

	local_db.index++;
	if (local_db.index >= MAX_DHCP_FINGERPRINT_LOCAL_DB)
		local_db.index = 0;

	return true;

db_lookup_exit:
	fclose(fp);
	free(line);
	return false;

}
