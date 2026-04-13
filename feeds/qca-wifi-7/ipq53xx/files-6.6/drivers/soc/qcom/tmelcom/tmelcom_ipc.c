/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2024 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include <linux/delay.h>
#include <linux/dma-direction.h>
#include <linux/dma-mapping.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/tmelcom_ipc.h>

#include "tmelcom.h"
#include "tmelcom_message_uids.h"

int tmelcom_probed(void)
{
	struct device *dev = tmelcom_get_device();

	if (!dev)
		return -ENODEV;
	else
		return 0;
}

int tmelcom_fuse_list_read(struct tmel_fuse_payload *fuse, size_t size)
{
	int ret;
	struct tmel_fuse_read_multiple_msg msg = {0};
	struct device *dev = tmelcom_get_device();
	dma_addr_t dma_fuse;

	if (!dev || !fuse || !size)
		return -EINVAL;

	dma_fuse = dma_map_single(dev, fuse, size, DMA_BIDIRECTIONAL);
	ret = dma_mapping_error(dev, dma_fuse);
	if (ret != 0) {
		dev_err(dev, "DMA Mapping Error : %d\n", ret);
		return ret;
	}

	dev_dbg(dev, "dma_fuse: %pad size: %zu\n", &dma_fuse, size);

	msg.status = TMEL_ERROR_GENERIC;
	msg.fuse_read_data.buf = (u32)dma_fuse;
	msg.fuse_read_data.buf_len = size;

	/*Send Fuse read row IPC call to TME*/
	ret = tmelcom_process_request(TMEL_MSG_UID_FUSE_READ_MULTIPLE_ROW,
				      &msg, sizeof(msg));
	if (ret || msg.status)
		dev_err(dev, "%s : IPC Failed. ret: %d, msg.status = 0x%x\n",
			__func__, ret, msg.status);

	dma_unmap_single(dev, dma_fuse, size, DMA_BIDIRECTIONAL);

	return ret ? ret : msg.status;
}
EXPORT_SYMBOL_GPL(tmelcom_fuse_list_read);

int tmelcom_secboot_sec_auth(u32 sw_id, void *metadata, size_t size)
{
	struct device *dev = tmelcom_get_device();
	struct tmel_secboot_sec_auth msg = {0};
	dma_addr_t elf_buf_phys;
	void *elf_buf;
	int ret;

	if (!dev || !metadata)
		return -EINVAL;

	elf_buf = dma_alloc_coherent(dev, size, &elf_buf_phys, GFP_KERNEL);
	if (!elf_buf)
		return -ENOMEM;

	memcpy(elf_buf, metadata, size);

	msg.req.sw_id = sw_id;
	msg.req.elf_buf.buf = (u32)elf_buf_phys;
	msg.req.elf_buf.buf_len = (u32)size;

	ret = tmelcom_process_request(TMEL_MSG_UID_SECBOOT_SEC_AUTH, &msg,
				      sizeof(msg));
	if (ret || msg.resp.status)
		dev_err(dev, "%s : IPC Failed. ret: %d, msg.status = 0x%x\n",
			__func__, ret, msg.resp.status);

	dma_free_coherent(dev, size, elf_buf, elf_buf_phys);

	return ret ? ret : msg.resp.status;
}
EXPORT_SYMBOL_GPL(tmelcom_secboot_sec_auth);

int tmelcom_secboot_teardown(u32 sw_id, u32 secondary_sw_id)
{
	struct device *dev = tmelcom_get_device();
	struct tmel_secboot_teardown msg = {0};
	int ret;

	if (!dev)
		return -EINVAL;

	msg.req.sw_id = sw_id;
	msg.req.secondary_sw_id = secondary_sw_id;
	msg.resp.status = TMEL_ERROR_GENERIC;

	ret = tmelcom_process_request(TMEL_MSG_UID_SECBOOT_SS_TEAR_DOWN, &msg,
				      sizeof(msg));
	if (ret || msg.resp.status)
		dev_err(dev, "%s : IPC Failed. ret: %d, msg.status = 0x%x\n",
			__func__, ret, msg.resp.status);

	return ret ? ret : msg.resp.status;
}

int tmelcom_set_tmel_log_config(void *buf, u32 size)
{
	int ret;
	struct tmel_log_set_config_message msg = {0};
	struct device *dev = tmelcom_get_device();
	dma_addr_t dma_buf;

	if (!dev)
		return -ENODEV;

	dma_buf = dma_map_single(dev, buf,
				     size, DMA_BIDIRECTIONAL);
	ret = dma_mapping_error(dev, dma_buf);
	if (ret) {
		dev_err(dev, "DMA Mapping Error : %d\n", ret);
		return ret;
	}

	msg.status = TMEL_ERROR_GENERIC;
	msg.log.buf = (u32)dma_buf;
	msg.log.buf_len = size;

	ret = tmelcom_process_request(TMEL_MSG_UID_LOG_SET_CONFIG, &msg,
					sizeof(msg));
	if (ret || msg.status)
		dev_err(dev, "%s : IPC Failed. ret: %d, msg.status = 0x%x\n",
			__func__, ret, msg.status);

	dma_unmap_single(dev, dma_buf, size, DMA_BIDIRECTIONAL);
	return ret ? ret : msg.status;
}
EXPORT_SYMBOL_GPL(tmelcom_set_tmel_log_config);

int tmelcom_get_tmel_log(void *buf, uint32_t max_buf_size, uint32_t *size)
{
	int ret;
	struct tmel_log_get_message msg = {0};
	struct device *dev = tmelcom_get_device();
	dma_addr_t dma_log_buf;

	if (!dev)
		return -ENODEV;

	dma_log_buf = dma_map_single(dev, buf,
				     max_buf_size, DMA_BIDIRECTIONAL);
	ret = dma_mapping_error(dev, dma_log_buf);
	if (ret) {
		dev_err(dev, "DMA Mapping Error : %d\n", ret);
		return ret;
	}

	msg.status = TMEL_ERROR_GENERIC;
	msg.log_buf.buf = (u32)dma_log_buf;
	msg.log_buf.buf_len = max_buf_size;

	ret = tmelcom_process_request(TMEL_MSG_UID_LOG_GET, &msg,
					sizeof(msg));
	if (ret || msg.status)
		dev_err(dev, "%s : IPC Failed. ret: %d, msg.status = 0x%x\n",
			__func__, ret, msg.status);
	else
		*size = msg.log_buf.out_buf_len;

	dma_unmap_single(dev, dma_log_buf, max_buf_size, DMA_BIDIRECTIONAL);
	return ret ? ret : msg.status;
}
EXPORT_SYMBOL_GPL(tmelcom_get_tmel_log);

int tmelcom_init_attestation(u32 *key_buf, u32 key_buf_len, u32 *key_buf_size)
{
	int ret;
	struct tmel_qwes_init_att_msg msg = {0};
	struct device *dev = tmelcom_get_device();
	dma_addr_t dma_key_buf;

	if (!dev || !key_buf_len)
		return -EINVAL;

	dma_key_buf = dma_map_single(dev, key_buf,
				     key_buf_len, DMA_FROM_DEVICE);
	ret = dma_mapping_error(dev, dma_key_buf);
	if (ret != 0) {
		dev_err(dev, "DMA Mapping Error : %d\n", ret);
		return ret;
	}

	msg.status = TMEL_ERROR_GENERIC;
	msg.rsp.buf = (u32)dma_key_buf;
	msg.rsp.buf_len = key_buf_len;

	ret = tmelcom_process_request(TMEL_MSG_UID_QWES_INIT_ATTESTATION,
				      &msg, sizeof(msg));
	if (ret || msg.status)
		dev_err(dev, "%s : IPC Failed. ret: %d, msg.status = 0x%x\n",
			__func__, ret, msg.status);
	else
		*key_buf_size = msg.rsp.out_buf_len;

	dma_unmap_single(dev, dma_key_buf, key_buf_len, DMA_FROM_DEVICE);
	return ret ? ret : msg.status;
}
EXPORT_SYMBOL_GPL(tmelcom_init_attestation);

int tmelcom_qwes_getattestation_report(u32 *req_buf, u32 req_buf_len,
		u32 *extclaim_buf, u32 extclaim_buf_len, u32 *resp_buf,
		u32 resp_buf_len, u32 *resp_buf_size)
{
	int ret;
	struct tmel_qwes_device_att_msg msg = {0};
	struct device *dev = tmelcom_get_device();
	dma_addr_t dma_att_req_buf;
	dma_addr_t dma_ext_claim_buf = 0;
	dma_addr_t dma_att_rsp_buf;

	if (!dev || !req_buf_len || !resp_buf_len)
		return -EINVAL;

	dma_att_req_buf = dma_map_single(dev, req_buf,
					 req_buf_len, DMA_TO_DEVICE);
	ret = dma_mapping_error(dev, dma_att_req_buf);
	if (ret != 0) {
		dev_err(dev, "DMA Mapping Error : %d\n", ret);
		return ret;
	}
	if (extclaim_buf) {
		dma_ext_claim_buf = dma_map_single(dev, extclaim_buf,
					extclaim_buf_len, DMA_TO_DEVICE);
		ret = dma_mapping_error(dev, dma_ext_claim_buf);
		if (ret != 0) {
			dev_err(dev, "DMA Mapping Error : %d\n", ret);
			goto dma_unmap_req_buf;
		}
	}
	dma_att_rsp_buf = dma_map_single(dev, resp_buf, resp_buf_len,
					 DMA_FROM_DEVICE);
	ret = dma_mapping_error(dev, dma_att_rsp_buf);
	if (ret != 0) {
		dev_err(dev, "DMA Mapping Error : %d\n", ret);
		goto dma_unmap_extclaim_buf;
	}

	msg.status = TMEL_ERROR_GENERIC;
	msg.req.buf = (u32)dma_att_req_buf;
	msg.req.buf_len = req_buf_len;
	msg.ext_claim.buf = (u32)dma_ext_claim_buf;
	msg.ext_claim.buf_len = extclaim_buf_len;
	msg.rsp.buf = (u32)dma_att_rsp_buf;
	msg.rsp.buf_len = resp_buf_len;

	ret = tmelcom_process_request(TMEL_MSG_UID_QWES_DEVICE_ATTESTATION,
				      &msg, sizeof(msg));
	if (ret || msg.status)
		dev_err(dev, "%s : IPC Failed. ret: %d, msg.status = 0x%x\n",
			__func__, ret, msg.status);
	else
		*resp_buf_size = msg.rsp.out_buf_len;

	dma_unmap_single(dev, dma_att_rsp_buf,
			 resp_buf_len, DMA_FROM_DEVICE);
dma_unmap_extclaim_buf:
	if (extclaim_buf) {
		dma_unmap_single(dev, dma_ext_claim_buf,
					extclaim_buf_len, DMA_TO_DEVICE);
	}
dma_unmap_req_buf:
	dma_unmap_single(dev, dma_att_req_buf, req_buf_len, DMA_TO_DEVICE);

	return ret ? ret : msg.status;
}
EXPORT_SYMBOL_GPL(tmelcom_qwes_getattestation_report);


int tmelcom_qwes_device_provision(u32 *req_buf, u32 req_buf_len, u32 *resp_buf,
				  u32 resp_buf_len, u32 *resp_buf_size)
{
	int ret;
	struct tmel_qwes_device_prov_msg msg = {0};
	struct device *dev = tmelcom_get_device();
	dma_addr_t dma_prov_req_buf;
	dma_addr_t dma_prov_rsp_buf;

	if (!dev || !req_buf_len || !resp_buf_len)
		return -EINVAL;

	dma_prov_req_buf = dma_map_single(dev, req_buf, req_buf_len,
					  DMA_TO_DEVICE);
	ret = dma_mapping_error(dev, dma_prov_req_buf);
	if (ret != 0) {
		dev_err(dev, "DMA Mapping Error : %d\n", ret);
		return ret;
	}

	dma_prov_rsp_buf = dma_map_single(dev, resp_buf, resp_buf_len,
					  DMA_FROM_DEVICE);
	ret = dma_mapping_error(dev, dma_prov_rsp_buf);
	if (ret != 0) {
		dev_err(dev, "DMA Mapping Error : %d\n", ret);
		goto dma_unmap_prov_req_buf;
	}

	msg.req.buf = (u32)dma_prov_req_buf;
	msg.req.buf_len = req_buf_len;
	msg.rsp.buf = (u32)dma_prov_rsp_buf;
	msg.rsp.buf_len = resp_buf_len;

	ret = tmelcom_process_request(TMEL_MSG_UID_QWES_DEVICE_PROVISIONING,
				      &msg, sizeof(msg));
	if (ret || msg.status)
		dev_err(dev, "%s : IPC Failed. ret: %d, msg.status = 0x%x\n",
			__func__, ret, msg.status);
	else
		*resp_buf_size = msg.rsp.out_buf_len;

	dma_unmap_single(dev, dma_prov_rsp_buf,
			 resp_buf_len, DMA_FROM_DEVICE);

dma_unmap_prov_req_buf:
	dma_unmap_single(dev, dma_prov_req_buf, req_buf_len, DMA_TO_DEVICE);

	return ret ? ret : msg.status;
}
EXPORT_SYMBOL_GPL(tmelcom_qwes_device_provision);

int tmelcom_licensing_check(void *cbor_req, u32 req_len, void *cbor_resp,
			    u32 resp_len, u32 *used_resp_len)
{
	int ret;
	struct tmel_licensing_check_msg msg = {0};
	struct device *dev = tmelcom_get_device();
	dma_addr_t dma_cbor_req, dma_cbor_resp;

	if (!dev || !cbor_req || !req_len || !cbor_resp || !resp_len)
		return -EINVAL;

	dma_cbor_req = dma_map_single(dev, cbor_req, req_len, DMA_TO_DEVICE);
	ret = dma_mapping_error(dev, dma_cbor_req);
	if (ret != 0) {
		dev_err(dev, "DMA Mapping Error, cbor_req : %d\n", ret);
		return ret;
	}

	dma_cbor_resp = dma_map_single(dev, cbor_resp, resp_len, DMA_FROM_DEVICE);
	ret = dma_mapping_error(dev, dma_cbor_resp);
	if (ret != 0) {
		dev_err(dev, "DMA Mapping Error, cbor_resp : %d\n", ret);
		dma_unmap_single(dev, dma_cbor_req, req_len, DMA_TO_DEVICE);
		return ret;
	}

	msg.status = TMEL_ERROR_GENERIC;
	msg.request.buf = dma_cbor_req;
	msg.request.buf_len = req_len;
	msg.response.buf = dma_cbor_resp;
	msg.response.buf_len = resp_len;
	ret = tmelcom_process_request(TMEL_MSG_UID_QWES_LICENSING_CHECK,
				      &msg, sizeof(msg));

	dma_unmap_single(dev, dma_cbor_req, req_len, DMA_TO_DEVICE);
	dma_unmap_single(dev, dma_cbor_resp, resp_len, DMA_FROM_DEVICE);

	if (ret || msg.status)
		dev_err(dev, "%s : IPC Failed. ret: %d, msg.status = 0x%x\n",
			__func__, ret, msg.status);
	else
		*used_resp_len = msg.response.out_buf_len;

	return ret ? ret : msg.status;
}
EXPORT_SYMBOL_GPL(tmelcom_licensing_check);

int tmelcom_ttime_get_req_params(void *params_buf, u32 buf_len, u32 *used_buf_len)
{
	struct tmel_ttime_get_req_params msg = {0};
	struct device *dev = tmelcom_get_device();
	dma_addr_t dma_params_buf;
	int ret;

	if (!dev || !params_buf || !buf_len || !used_buf_len)
		return -EINVAL;

	dma_params_buf = dma_map_single(dev, params_buf, buf_len, DMA_BIDIRECTIONAL);
	ret = dma_mapping_error(dev, dma_params_buf);
	if (ret) {
		dev_err(dev, "DMA Mapping Error, ttime_get_req : %d\n", ret);
		return ret;
	}

	msg.status = TMEL_ERROR_GENERIC;
	msg.params.buf = dma_params_buf;
	msg.params.buf_len = buf_len;

	ret = tmelcom_process_request(TMEL_MSG_UID_QWES_TTIME_CLOUD_REQUEST,
				      &msg, sizeof(msg));

	dma_unmap_single(dev, dma_params_buf, buf_len, DMA_BIDIRECTIONAL);

	if (ret || msg.status)
		dev_err(dev, "%s : IPC Failed. ret: %d, msg.status = 0x%x\n",
			__func__, ret, msg.status);
	else
		*used_buf_len = msg.params.out_buf_len;

	return ret ? ret : msg.status;
}
EXPORT_SYMBOL_GPL(tmelcom_ttime_get_req_params);

int tmelcom_ttime_set(void *ttime_buf, u32 buf_len)
{
	struct device *dev = tmelcom_get_device();
	struct tmel_ttime_set msg = {0};
	dma_addr_t dma_ttime_buf;
	int ret;

	if (!dev || !ttime_buf || !buf_len)
		return -EINVAL;

	dma_ttime_buf = dma_map_single(dev, ttime_buf, buf_len, DMA_BIDIRECTIONAL);
	ret = dma_mapping_error(dev, dma_ttime_buf);
	if (ret) {
		dev_err(dev, "DMA Mapping Error, ttime_set: %d\n", ret);
		return ret;
	}

	msg.status = TMEL_ERROR_GENERIC;
	msg.ttime.buf = dma_ttime_buf;
	msg.ttime.buf_len = buf_len;

	ret = tmelcom_process_request(TMEL_MSG_UID_QWES_TTIME_SET, &msg, sizeof(msg));

	dma_unmap_single(dev, dma_ttime_buf, buf_len, DMA_BIDIRECTIONAL);

	if (ret || msg.status)
		dev_err(dev, "%s : IPC Failed. ret: %d, msg.status = 0x%x\n",
			__func__, ret, msg.status);

	return ret ? ret : msg.status;
}
EXPORT_SYMBOL_GPL(tmelcom_ttime_set);

int tmelcom_licensing_install(void *license_buf, u32 license_len, void *ident_buf,
			      u32 ident_len, u32 *ident_used_len, u32 *flags)
{
	dma_addr_t dma_license_buf, dma_ident_buf;
	struct device *dev = tmelcom_get_device();
	struct tmel_licensing_install msg = {0};
	int ret;

	if (!dev || !license_buf || !license_len || !ident_buf ||
	    !ident_len || !ident_used_len)
		return -EINVAL;

	dma_license_buf = dma_map_single(dev, license_buf, license_len,
					 DMA_TO_DEVICE);
	ret = dma_mapping_error(dev, dma_license_buf);
	if (ret != 0) {
		dev_err(dev, "DMA Mapping Error, license_buf: %d\n", ret);
		return ret;
	}

	dma_ident_buf = dma_map_single(dev, ident_buf, ident_len,
				       DMA_FROM_DEVICE);
	ret = dma_mapping_error(dev, dma_ident_buf);
	if (ret != 0) {
		dev_err(dev, "DMA Mapping Error, identifier_buf: %d\n", ret);
		dma_unmap_single(dev, dma_license_buf, license_len,
				 DMA_TO_DEVICE);
		return ret;
	}

	msg.status = TMEL_ERROR_GENERIC;
	msg.license.buf = dma_license_buf;
	msg.license.buf_len = license_len;
	msg.identifier.buf = dma_ident_buf;
	msg.identifier.buf_len = ident_len;

	ret = tmelcom_process_request(TMEL_MSG_UID_QWES_LICENSING_INSTALL,
				      &msg, sizeof(msg));

	dma_unmap_single(dev, dma_license_buf, license_len, DMA_TO_DEVICE);
	dma_unmap_single(dev, dma_ident_buf, ident_len, DMA_FROM_DEVICE);

	if (ret || msg.status) {
		dev_err(dev, "%s : IPC Failed. ret: %d, msg.status = 0x%x\n",
			__func__, ret, msg.status);
	} else {
		*ident_used_len = msg.identifier.out_buf_len;
		*flags = msg.flags;
	}

	return ret ? ret : msg.status;

}
EXPORT_SYMBOL_GPL(tmelcom_licensing_install);

int tmelcom_licensing_get_toBeDel_licenses(void *toBeDelLic_buf, u32 toBeDelLic_len,
					   u32 *used_toBeDelLic_len)

{
	struct tmel_licensing_ToBeDel_licenses msg = {0};
	struct device *dev = tmelcom_get_device();
	dma_addr_t dma_toBeDelLic_buf;
	int ret;

	if (!dev || !toBeDelLic_buf || !toBeDelLic_len || !used_toBeDelLic_len)
		return -EINVAL;

	dma_toBeDelLic_buf = dma_map_single(dev, toBeDelLic_buf, toBeDelLic_len,
					    DMA_BIDIRECTIONAL);
	ret = dma_mapping_error(dev, dma_toBeDelLic_buf);
	if (ret) {
		dev_err(dev, "DMA Mapping Error, toBeDeletedLicense buffer : %d\n", ret);
		return ret;
	}

	msg.status = TMEL_ERROR_GENERIC;
	msg.toBeDelLicenses.buf = dma_toBeDelLic_buf;
	msg.toBeDelLicenses.buf_len = toBeDelLic_len;

	ret = tmelcom_process_request(TMEL_MSG_UID_QWES_LICENSING_TBDLICENSES,
				      &msg, sizeof(msg));

	dma_unmap_single(dev, dma_toBeDelLic_buf, toBeDelLic_len, DMA_BIDIRECTIONAL);

	if (ret || msg.status)
		dev_err(dev, "%s : IPC Failed. ret: %d, msg.status = 0x%x\n",
			__func__, ret, msg.status);
	else
		*used_toBeDelLic_len = msg.toBeDelLicenses.out_buf_len;

	return ret ? ret : msg.status;
}
EXPORT_SYMBOL_GPL(tmelcom_licensing_get_toBeDel_licenses);

int tmelcom_secure_io_read(struct tmel_secure_io *buf, size_t size)
{
	int ret;
	struct tmel_secure_io_read msg = {0};
	struct device *dev = tmelcom_get_device();
	dma_addr_t dma_secure_io;

	if (!dev || !buf || !size)
		return -EINVAL;

	dma_secure_io = dma_map_single(dev, buf, size, DMA_BIDIRECTIONAL);
	ret = dma_mapping_error(dev, dma_secure_io);
	if (ret) {
		dev_err(dev, "DMA Mapping Error : %d\n", ret);
		return ret;
	}

	msg.status = TMEL_ERROR_GENERIC;
	msg.read_buf.buf = (u32)dma_secure_io;
	msg.read_buf.buf_len = size;
	msg.read_buf.out_buf_len = 0;

	/*Send Secure IO read IPC call to TME*/
	ret = tmelcom_process_request(TMEL_MSG_UID_ACCESS_CONTROL_SECURE_IO_READ,
				      &msg, sizeof(msg));

	dma_unmap_single(dev, dma_secure_io, size, DMA_BIDIRECTIONAL);

	if (ret || msg.status)
		dev_err(dev, "%s : IPC Failed. ret: %d, msg.status = 0x%x\n",
			__func__, ret, msg.status);

	return ret ? ret : msg.status;
}
EXPORT_SYMBOL_GPL(tmelcom_secure_io_read);

int tmelcom_secure_io_write(struct tmel_secure_io *buf, size_t size)
{
	int ret;
	struct tmel_secure_io_write msg = {0};
	struct device *dev = tmelcom_get_device();
	dma_addr_t dma_secure_io;

	if (!dev || !buf || !size)
		return -EINVAL;

	dma_secure_io = dma_map_single(dev, buf, size, DMA_BIDIRECTIONAL);
	ret = dma_mapping_error(dev, dma_secure_io);
	if (ret) {
		dev_err(dev, "DMA Mapping Error : %d\n", ret);
		return ret;
	}

	msg.status = TMEL_ERROR_GENERIC;
	msg.write_buf.buf = (u32)dma_secure_io;
	msg.write_buf.buf_len = size;

	/*Send Secure IO write IPC call to TME*/
	ret = tmelcom_process_request(TMEL_MSG_UID_ACCESS_CONTROL_SECURE_IO_WRITE,
				      &msg, sizeof(msg));

	dma_unmap_single(dev, dma_secure_io, size, DMA_BIDIRECTIONAL);

	if (ret || msg.status)
		dev_err(dev, "%s : IPC Failed. ret: %d, msg.status = 0x%x\n",
			__func__, ret, msg.status);

	return ret ? ret : msg.status;
}
EXPORT_SYMBOL_GPL(tmelcom_secure_io_write);

int tmelcomm_secboot_get_arb_version(u32 type, u32 *version)
{
	struct device *dev = tmelcom_get_device();
	struct tmel_get_arb_version msg = {0};
	int ret;

	msg.req.sw_id = type;
	ret = tmelcom_process_request(TMEL_MSG_UID_SECBOOT_GET_ARB_VERSION,
				      &msg, sizeof(msg));
	if (ret || msg.rsp.status)
		dev_err(dev, "%s : IPC Failed. ret: %d, msg.status = %x\n",
			__func__, ret, msg.rsp.status);
	else
		*version = msg.rsp.oem_version;

	return ret ? ret : msg.rsp.status;
}
EXPORT_SYMBOL_GPL(tmelcomm_secboot_get_arb_version);

int tmelcomm_secboot_update_arb_version_list(u32 *sw_id_list, size_t size)
{
	int ret;
	struct device *dev = tmelcom_get_device();
	struct tmel_update_arb_version_sw_id_list msg = {0};
	dma_addr_t dma_addr;

	if (!dev || !sw_id_list || !size)
		return -EINVAL;

	dma_addr = dma_map_single(dev, sw_id_list, size,
				  DMA_TO_DEVICE);
	ret = dma_mapping_error(dev, dma_addr);
	if (ret) {
		dev_err(dev, "DMA Mapping Error : %d\n", ret);
		return -EINVAL;
	}

	msg.req.cbuffer.buf = dma_addr;
	msg.req.cbuffer.buf_len = size;

	ret = tmelcom_process_request(TMEL_MSG_UID_SECBOOT_UPDATE_ARB_VER_SWID_LIST,
				      &msg, sizeof(msg));
	if (ret || msg.rsp.status)
		dev_err(dev, "%s : IPC Failed. ret: %d, msg.status = %x\n",
			__func__, ret, msg.rsp.status);

	dma_unmap_single(dev, dma_addr, size, DMA_TO_DEVICE);
	return ret ? ret : msg.rsp.status;
}
EXPORT_SYMBOL_GPL(tmelcomm_secboot_update_arb_version_list);

int tmelcomm_get_ecc_public_key(u32 type, void *buf, u32 size, u32 *rsp_len)
{
	int ret;
	struct tmel_km_ecdh_ipkey_msg msg = {0};
	struct device *dev = tmelcom_get_device();
	dma_addr_t dma_addr;

	if (!dev)
		return -EINVAL;

	dma_addr = dma_map_single(dev, buf, size,
				  DMA_FROM_DEVICE);
	ret = dma_mapping_error(dev, dma_addr);
	if (ret != 0) {
		pr_err("DMA Mapping Error : %d\n", ret);
		return -EINVAL;
	}

	msg.rsp.status = TMEL_ERROR_GENERIC;
	msg.req.key_id = type;
	msg.rsp.rsp_buf.data = (u32)dma_addr;
	msg.rsp.rsp_buf.len = size;

	ret = tmelcom_process_request(TMEL_MSG_UID_KM_EXPORT_ECDH_IP,
				      &msg, sizeof(msg));
	if (ret || msg.rsp.status || msg.rsp.seq_status.tmel_err_status)
		dev_err(dev, "%s : IPC Failed. ret: %d, msg.status = %x tme response status = %x\n",
			__func__, ret, msg.rsp.status,
			msg.rsp.seq_status.tmel_err_status);
	else
		*rsp_len = msg.rsp.rsp_buf.len_used;

	dma_unmap_single(dev, dma_addr, size, DMA_FROM_DEVICE);
	return ret;
}
EXPORT_SYMBOL_GPL(tmelcomm_get_ecc_public_key);

int tmelcom_aes_derive_key(u32 key_id, dma_addr_t *dma_kdf_spec, u32 kdf_len,
			   u8 *key_handle)
{
	struct tmel_aes_derive_key_msg msg = {0};
	struct device *dev = tmelcom_get_device();
	int ret;

	msg.req.key_id = key_id;
	msg.req.kdf_info.buf = *dma_kdf_spec;
	msg.req.kdf_info.buf_len = kdf_len;
	msg.req.cred_slot = 0;

	ret = tmelcom_process_request(TMEL_MSG_UID_KM_DERIVE, &msg, sizeof(msg));
	if (ret || msg.resp.status)
		dev_err(dev, "%s : IPC Failed. ret: %d msg->resp.status = %x\n",
			__func__, ret, msg.resp.status);
	else
		*key_handle = msg.resp.key_id;

	return ret ? ret : msg.resp.status;
}
EXPORT_SYMBOL_GPL(tmelcom_aes_derive_key);

int tmelcom_aes_clear_key(u32 handle)
{
	struct tmel_aes_clear_key_msg msg = {0};
	struct device *dev = tmelcom_get_device();
	int ret;

	msg.req.key_id = handle;

	ret = tmelcom_process_request(TMEL_MSG_UID_KM_CLEAR, &msg, sizeof(msg));
	if (ret || msg.resp.status)
		dev_err(dev, "%s : IPC Failed. ret: %d msg->resp.status = %x\n",
			__func__, ret, msg.resp.status);

	return ret ? ret : msg.resp.status;
}
EXPORT_SYMBOL_GPL(tmelcom_aes_clear_key);

int tmelcom_aes_encrypt(struct tmel_aes_encrypt_msg *msg, u32 size)
{
	struct device *dev = tmelcom_get_device();
	int ret;

	ret = tmelcom_process_request(TMEL_MSG_UID_HCS_AES_ENCRYPT, msg, size);
	if (ret || msg->resp.status)
		dev_err(dev, "%s : IPC Failed. ret: %d msg->resp.status = %x\n",
			__func__, ret, msg->resp.status);

	return ret ? ret : msg->resp.status;
}
EXPORT_SYMBOL_GPL(tmelcom_aes_encrypt);

int tmelcom_aes_decrypt(struct tmel_aes_decrypt_msg *msg, u32 size)
{
	struct device *dev = tmelcom_get_device();
	int ret;

	ret = tmelcom_process_request(TMEL_MSG_UID_HCS_AES_DECRYPT, msg, size);
	if (ret || msg->resp.status)
		dev_err(dev, "%s : IPC Failed. ret: %d msg->resp.status = %x\n",
			__func__, ret, msg->resp.status);

	return ret ? ret : msg->resp.status;
}
EXPORT_SYMBOL_GPL(tmelcom_aes_decrypt);

int tmelcom_aes_generate_key(u32 key_id, struct tme_key_policy *policy,
			     u8 *key_handle)
{
	struct tmel_aes_generate_key_msg msg = {0};
	struct device *dev = tmelcom_get_device();
	int ret;

	msg.req.key_id = key_id;
	msg.req.policy.low = policy->low;
	msg.req.policy.high = policy->high;
	msg.req.cred_slot = 0;

	ret = tmelcom_process_request(TMEL_MSG_UID_KM_GENERATE, &msg, sizeof(msg));
	if (ret || msg.resp.status)
		dev_err(dev, "%s : IPC Failed. ret: %d msg.resp.status = %x\n",
			__func__, ret, msg.resp.status);
	else
		*key_handle = msg.resp.key_id;

	return ret ? ret : msg.resp.status;
}
EXPORT_SYMBOL_GPL(tmelcom_aes_generate_key);

int tmelcom_aes_import_key(u32 key_id, struct tme_key_policy *policy,
			   struct tmel_plain_text_key *key_material,
			   u8 *key_handle)
{
	struct tmel_aes_import_key_msg msg = {0};
	struct device *dev = tmelcom_get_device();
	int ret;

	msg.req.key_id = key_id;
	msg.req.key_policy.low = policy->low;
	msg.req.key_policy.high = policy->high;
	msg.req.key_material.buf = key_material->buf;
	msg.req.key_material.buf_len = key_material->buf_len;
	msg.req.cred_slot = 0;

	ret = tmelcom_process_request(TMEL_MSG_UID_KM_IMPORT, &msg, sizeof(msg));
	if (ret || msg.resp.status)
		dev_err(dev, "%s : IPC Failed. ret: %d msg->resp.status = %x\n",
			__func__, ret, msg.resp.status);
	else
		*key_handle = msg.resp.key_id;

	return ret ? ret : msg.resp.status;
}
EXPORT_SYMBOL_GPL(tmelcom_aes_import_key);

int tmelcom_wrap_key(u32 key_id, u32 kw_key_id, dma_addr_t *key, u32 len)
{
	struct device *dev = tmelcom_get_device();
	struct tmel_wrap_key_msg msg = {0};
	int ret;

	msg.req.key_id = key_id;
	msg.req.kw_key_id = kw_key_id;
	msg.req.cred_slot = 0;
	msg.resp.wrapped_key.key = *key;
	msg.resp.wrapped_key.length = len;

	ret = tmelcom_process_request(TMEL_MSG_UID_KM_WRAP, &msg, sizeof(msg));
	if (ret || msg.resp.status)
		dev_err(dev, "%s : IPC Failed. ret: %d msg->resp.status = %x\n",
			__func__, ret, msg.resp.status);

	return ret ? ret : msg.resp.status;

}
EXPORT_SYMBOL_GPL(tmelcom_wrap_key);

int tmelcom_unwrap_key(u32 kw_key_id, dma_addr_t *key, u32 len, u32 *key_id)
{
	struct device *dev = tmelcom_get_device();
	struct tmel_unwrap_key_msg msg = {0};
	int ret;

	msg.req.key_id = *key_id;
	msg.req.kw_key_id = kw_key_id;
	msg.req.wrapped_key.key = *key;
	msg.req.wrapped_key.length = len;

	ret = tmelcom_process_request(TMEL_MSG_UID_KM_UNWRAP, &msg, sizeof(msg));
	if (ret || msg.resp.status)
		dev_err(dev, "%s : IPC Failed. ret: %d msg->resp.status = %x\n",
				__func__, ret, msg.resp.status);
	else
		*key_id = msg.resp.key_id;

	return ret ? ret : msg.resp.status;
}
EXPORT_SYMBOL_GPL(tmelcom_unwrap_key);

int tmelcomm_ecc_get_pubkey(u32 curve_id, u32 prv_key_id, dma_addr_t *dma_pub_key,
			    u32 buf_len, u32 *used_len, u32 ecc_algo)
{
	int ret;
	struct tme_ecc_get_pubkey_msg msg = {0};
	struct device *dev = tmelcom_get_device();

	msg.req.curve_id = curve_id;
	msg.req.prv_key_id = prv_key_id;
	msg.resp.pub_key.buf = (u32)*dma_pub_key;
	msg.resp.pub_key.length = buf_len;

	if (ecc_algo == 0)
		ret = tmelcom_process_request(TMEL_MSG_UID_HCS_ECC_GET_PUBKEY, &msg,
					      sizeof(msg));
	else
		ret = tmelcom_process_request(TMEL_MSG_UID_HCS_ECDH_GET_PUBKEY, &msg,
					      sizeof(msg));

	if (ret || msg.resp.status)
		dev_err(dev, "%s : IPC Failed. ret: %d msg->resp.status = %x\n",
			__func__, ret, msg.resp.status);
	else
		*used_len = msg.resp.pub_key.length_used;

	return ret ? ret : msg.resp.status;
}
EXPORT_SYMBOL_GPL(tmelcomm_ecc_get_pubkey);

int tmelcomm_ecc_sign_msg(u32 curve_id, u32 prv_key_id, u32 hash_algo, dma_addr_t *buf,
			  u32 buf_len, dma_addr_t *sig, u32 sig_len, u32 *used_len)
{
	int ret;
	struct tme_ecc_sign_msg_msg msg = {0};
	struct device *dev = tmelcom_get_device();

	msg.req.curve_id = curve_id;
	msg.req.prv_key_id = prv_key_id;
	msg.req.hash_algo = hash_algo;
	msg.req.msg.buf = (u32)*buf;
	msg.req.msg.buf_len = buf_len;
	msg.resp.sig.buf = (u32)*sig;
	msg.resp.sig.length = sig_len;
	msg.resp.sig.length_used = 0;

	if (!hash_algo)
		ret = tmelcom_process_request(TMEL_MSG_UID_HCS_ECC_SIGN_DIGEST, &msg,
					      sizeof(msg));
	else
		ret = tmelcom_process_request(TMEL_MSG_UID_HCS_ECC_SIGN_MSG, &msg,
					      sizeof(msg));

	if (ret || msg.resp.status)
		dev_err(dev, "%s : IPC Failed. ret: %d msg->resp.status = %x\n",
			__func__, ret, msg.resp.status);
	else
		*used_len = msg.resp.sig.length_used;

	return ret ? ret : msg.resp.status;
}
EXPORT_SYMBOL_GPL(tmelcomm_ecc_sign_msg);

int tmelcomm_ecc_verify_msg(u32 curve_id, dma_addr_t *pub_key, u32 pub_key_len,
			    u32 hash_algo, dma_addr_t *buf, u32 buf_len,
			    dma_addr_t *sig, u32 sig_len)
{
	int ret;
	struct tme_ecc_verify_msg_msg msg = {0};
	struct device *dev = tmelcom_get_device();

	msg.req.curve_id = curve_id;
	msg.req.pub_key.buf = (u32)*pub_key;
	msg.req.pub_key.buf_len = pub_key_len;
	msg.req.hash_algo = hash_algo;
	msg.req.msg.buf = (u32)*buf;
	msg.req.msg.buf_len = buf_len;
	msg.req.sig.buf = (u32)*sig;
	msg.req.sig.buf_len = sig_len;

	if (!hash_algo)
		ret = tmelcom_process_request(TMEL_MSG_UID_HCS_ECC_VERIFY_DIGEST, &msg,
					      sizeof(msg));
	else
		ret = tmelcom_process_request(TMEL_MSG_UID_HCS_ECC_VERIFY_MSG, &msg,
					      sizeof(msg));
	if (ret || msg.resp.status)
		dev_err(dev, "%s : IPC Failed. ret: %d msg->resp.status = %x\n",
			__func__, ret, msg.resp.status);

	return ret ? ret : msg.resp.status;
}
EXPORT_SYMBOL_GPL(tmelcomm_ecc_verify_msg);

int tmelcomm_ecdh_shared_secret_msg(u32 curve_id, u32 prv_key_id, u32 *shared_key_id,
				    dma_addr_t *pub_key2, u32 pub_key_len2,
				    struct tme_key_policy *policy)
{
	int ret;
	struct tme_ecdh_shared_secret_msg msg = {0};
	struct device *dev = tmelcom_get_device();

	msg.req.curve_id = curve_id;
	msg.req.prv_key_id = prv_key_id;
	msg.req.shared_key_id = *shared_key_id;
	msg.req.pub_key2.buf = (u32)*pub_key2;
	msg.req.pub_key2.buf_len = pub_key_len2;
	msg.req.policy.low = policy->low;
	msg.req.policy.high = policy->high;

	ret = tmelcom_process_request(TMEL_MSG_UID_HCS_ECDH_SHARED_SECRET, &msg,
				      sizeof(msg));
	if (ret || msg.resp.status)
		dev_err(dev, "%s : IPC Failed. ret: %d msg->resp.status = %x\n",
			__func__, ret, msg.resp.status);
	else
		*shared_key_id = msg.resp.shared_key_id;

	return ret ? ret : msg.resp.status;
}
EXPORT_SYMBOL_GPL(tmelcomm_ecdh_shared_secret_msg);

int tmelcomm_hmac_sha_digest(u32 hash_algo, dma_addr_t *buf_in, u32 buf_in_len,
			     u32 key_id, dma_addr_t *buf_out, u32 buf_out_len,
			     u32 *used_len)
{
	int ret;
	struct tme_sha_msg msg = {0};
	struct device *dev = tmelcom_get_device();

	msg.req.hash_algo = hash_algo;
	msg.req.input.buf = (u32)*buf_in;
	msg.req.input.buf_len = buf_in_len;
	msg.req.hmac_key_id = key_id;
	msg.resp.digest.buf = (u32)*buf_out;
	msg.resp.digest.length = buf_out_len;
	msg.resp.digest.length_used = 0;

	if (key_id)
		ret = tmelcom_process_request(TMEL_MSG_UID_HCS_HMAC_DIGEST, &msg,
					      sizeof(msg));
	else
		ret = tmelcom_process_request(TMEL_MSG_UID_HCS_SHA_DIGEST, &msg,
					      sizeof(msg));

	if (ret || msg.resp.status)
		dev_err(dev, "%s : IPC Failed. ret: %d msg->resp.status = %x\n",
			__func__, ret, msg.resp.status);
	else
		*used_len = msg.resp.digest.length_used;

	return ret ? ret : msg.resp.status;
}
EXPORT_SYMBOL_GPL(tmelcomm_hmac_sha_digest);

int tmelcomm_qwes_enforce_hw_features(void *buf, u32 size)
{
	int ret;
	struct tmel_qwes_enf_hw_feat_msg msg = {0};
	struct device *dev = tmelcom_get_device();
	dma_addr_t dma_addr;

	if (!dev)
		return -EINVAL;

	dma_addr = dma_map_single(dev, buf, size, DMA_BIDIRECTIONAL);
	ret = dma_mapping_error(dev, dma_addr);
	if (ret) {
		pr_err("DMA Mapping Error : %d\n", ret);
		return -EINVAL;
	}

	msg.status = TMEL_ERROR_GENERIC;
	msg.featid_buf.buf = (u32)dma_addr;
	msg.featid_buf.buf_len = size;
	msg.featid_buf.out_buf_len = 0;
	msg.hw_reg_inf_ver = 0;

	ret = tmelcom_process_request(TMEL_MSG_UID_QWES_LICENSING_ENFORCEHWFEATURES,
				      &msg, sizeof(msg));

	dma_sync_single_for_cpu(dev, dma_addr, size, DMA_BIDIRECTIONAL);
	dma_unmap_single(dev, dma_addr, size, DMA_BIDIRECTIONAL);

	if (ret || msg.status)
		dev_err(dev, "%s : IPC Failed. ret: %d, msg.status = %x\n",
			__func__, ret, msg.status);

	return ret ? ret : msg.status;
}
EXPORT_SYMBOL_GPL(tmelcomm_qwes_enforce_hw_features);
