/* QTI Secure Execution Environment Communicator (QSEECOM) driver
 *
 * Copyright (c) 2012, 2015, 2017-2018 The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef _qseecom_h
#define _qseecom_h

#include <linux/cdev.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/ioctl.h>
#include <linux/module.h>
#include <linux/syscalls.h>
#include <linux/fcntl.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/highuid.h>
#include <linux/sysfs.h>
#include <linux/sizes.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/kernel_read_file.h>
#include <linux/kobject.h>
#include <linux/firmware/qcom/qcom_scm.h>
#include <linux/sysfs.h>
#include <linux/dma-map-ops.h>
#include <linux/string.h>
#include <linux/gfp.h>
#include <linux/mm.h>
#include <linux/types.h>
#include <linux/bitops.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/random.h>
#include <linux/of_reserved_mem.h>
#include <linux/debugfs.h>
#include <linux/tmelcom_ipc.h>

#define QTI_CMD_AES_CLEAR_KEY		10
#define QTI_CMD_AES_DERIVE_KEY		9
#define QTI_CMD_AES_DERIVE_128_KEY	0xE
#define CLIENT_CMD_CRYPTO_AES_DECRYPT	8
#define CLIENT_CMD_CRYPTO_AES_ENCRYPT	7
#define CLIENT_CMD_CRYPTO_AES_64	6
#define CLIENT_CMD_CRYPTO_RSA_64	5
#define CLIENT_CMD_CRYPTO_RSA		3
#define CLIENT_CMD_CRYPTO_AES		2
#define CLIENT_CMD1_BASIC_DATA		1
#define CLIENT_CMD8_RUN_CRYPTO_TEST	3
#define CLIENT_CMD8_RUN_CRYPTO_ENCRYPT	8
#define CLIENT_CMD40_RUN_AES_ENCRYPT	40
#define CLIENT_CMD41_RUN_AES_DECRYPT	41
#define CLIENT_CMD42_RUN_RSA_CRYPT	42
#define CLIENT_CMD43_RUN_FUSE_BLOW	43
#define CLIENT_CMD9_RUN_CRYPTO_DECRYPT	9
#define CLIENT_CMD_AUTH			26
#define CLIENT_CMD53_RUN_LOG_BITMASK_TEST	53
#define CLIENT_CMD18_RUN_FUSE_TEST	18
#define CLIENT_CMD13_RUN_MISC_TEST	13
#define CLIENT_CMD121_RUN_KEY_DERIVE_TEST	121
#define CLIENT_CMD122_CLEAR_KEY		122

/* Command IDs for Crypto TA */
#define CLIENT_CMD0_GET_VERSION		0
#define CLIENT_CMD1_ECDSA_IMPORT_KEY	1
#define CLIENT_CMD2_ECDSA_SIGN		2
#define CLIENT_CMD3_ECDSA_VERIFY	3

#define MAX_INPUT_SIZE			4096
#define QSEE_64				64
#define QSEE_32				32
#define AES_BLOCK_SIZE			16
#define MAX_CONTEXT_BUFFER_LEN_V1	64
#define MAX_CONTEXT_BUFFER_LEN_V2	128
#define MAX_KEY_HANDLE_SIZE		8

#define MAX_ENCRYPTED_DATA_SIZE  (2072 * sizeof(uint8_t))
#define MAX_PLAIN_DATA_SIZE	 (2048 * sizeof(uint8_t))
#define MAX_RSA_PLAIN_DATA_SIZE  (8192 * sizeof(uint8_t))

#define ENCRYPTED_DATA_HEADER \
	(MAX_ENCRYPTED_DATA_SIZE - MAX_PLAIN_DATA_SIZE)

#define DEFAULT_POLICY_DESTINATION	0x0
#define DEFAULT_KEY_TYPE	0x2
#define KEY_BLOB_SIZE		(56 * sizeof(uint8_t))
#define KEY_SIZE		(32 * sizeof(uint8_t))
#define MAX_FUSE_WRITE_VALUE	0xffffffffffffff
#define RSA_KEY_SIZE_MAX	((528) * sizeof(uint8_t))
#define RSA_IV_LENGTH		(16 * sizeof(uint8_t))
#define RSA_HMAC_LENGTH		(32 * sizeof(uint8_t))
#define RSA_2K_MODULUS_LEN	(2048 * sizeof(uint8_t))
#define RSA_4K_MODULUS_LEN	(4096 * sizeof(uint8_t))
#define RSA_PUBLIC_EXPONENT	(0x10001)
#define RSA_PUB_EXP_SIZE_MAX	(5 * sizeof(uint8_t))
#define RSA_KEY_MATERIAL_SIZE	((528 + 2 + 5 + 1 + 528 + 2) * sizeof(uint8_t))
#define MAX_RSA_SIGN_DATA_SIZE	(2048 * sizeof(uint8_t))
#define RSA_PARAM_LEN		(5 * sizeof(uint8_t))
#define INVALID_AES_KEY_HANDLE_VAL	117
#define MESSAGE_LEN			32

#define KEY_HANDLE_OUT_OF_SLOT		0x12C

#define TME_KID_ALLOC			0xAAAAAAAA
#define TME_KID_INVALID			0xFFFFFFFF

#define TZAPP_LOG_VER1		0x1
#define TZAPP_LOG_VER2		0x2

static int app_state;
static int app_libs_state;
struct qseecom_props *props;

u8 *tzapp_log;
u32 qsee_log_buf_len = SZ_4K;
struct dentry *tzapp_log_dir;
u32 tzapp_log_len;

enum qti_crypto_service_aes_cmd_t {
	QTI_CRYPTO_SERVICE_AES_ENC_ID = 0x1,
	QTI_CRYPTO_SERVICE_AES_DEC_ID = 0x2,
};

enum qti_crypto_service_aes_type_t {
	QTI_CRYPTO_SERVICE_AES_TYPE_SHK = 1,
	QTI_CRYPTO_SERVICE_AES_TYPE_PHK,
	QTI_CRYPTO_SERVICE_AES_TYPE_MAX,
};

enum qti_crypto_service_aes_mode_t {
	QTI_CRYPTO_SERVICE_AES_MODE_ECB = 0,
	QTI_CRYPTO_SERVICE_AES_MODE_CBC,
	QTI_CRYPTO_SERVICE_AES_MODE_MAX,
};

enum qti_storage_service_cmd_t {
	QTI_STOR_SVC_GENERATE_KEY = 0x00000001,
	QTI_STOR_SVC_SEAL_DATA	 = 0x00000002,
	QTI_STOR_SVC_UNSEAL_DATA	 = 0x00000003,
	QTI_STOR_SVC_IMPORT_KEY	 = 0x00000004,
};

enum qti_storage_service_rsa_cmd_t {
	QTI_STOR_SVC_RSA_GENERATE_KEY      = 0x00000001,
	QTI_STOR_SVC_RSA_EXPORT_PUBLIC_KEY = 0x00000002,
	QTI_STOR_SVC_RSA_SIGN_DATA         = 0x00000003,
	QTI_STOR_SVC_RSA_VERIFY_SIGNATURE  = 0x00000004,
	QTI_STOR_SVC_RSA_IMPORT_KEY        = 0x00000005,
	CRYPTO_STORAGE_UPDATE_KEYBLOB	   = 0x00000006,
};

enum qti_storage_service_digest_pad_algo_t {
	QTI_STOR_SVC_RSA_DIGEST_PAD_NONE             = 0x00000000,
	QTI_STOR_SVC_RSA_DIGEST_PAD_PKCS115_SHA2_256 = 0x00000001,
	QTI_STOR_SVC_RSA_DIGEST_PAD_PSS_SHA2_256     = 0x00000002,
};

struct qti_storage_service_operation_policy {
	uint32_t operations;
	uint32_t algorithm;
};

struct qti_storage_service_hwkey_policy {
	struct qti_storage_service_operation_policy op_policy;
	uint32_t kdf_depth;
	uint32_t permissions;
	uint32_t key_type;
	uint32_t destination;
};

struct qti_storage_service_hwkey_bindings_v1 {
	uint32_t bindings;
	uint32_t context_len;
	uint8_t context[MAX_CONTEXT_BUFFER_LEN_V1];
};

struct qti_storage_service_derive_key_cmd_t_v1 {
	struct qti_storage_service_hwkey_policy policy;
	struct qti_storage_service_hwkey_bindings_v1 hw_key_bindings;
	uint32_t source;
	uint64_t mixing_key;
	uint64_t key;
};

struct qti_storage_service_hwkey_bindings_v2 {
	uint32_t bindings;
	uint32_t context_len;
	uint8_t context[MAX_CONTEXT_BUFFER_LEN_V2];
};

struct qti_storage_service_derive_key_cmd_t_v2 {
	struct qti_storage_service_hwkey_policy policy;
	struct qti_storage_service_hwkey_bindings_v2 hw_key_bindings;
	uint32_t source;
	uint64_t mixing_key;
	uint64_t key;
};

struct qti_storage_service_key_blob_t {
	uint64_t key_material;
	uint32_t key_material_len;
};

struct qti_storage_service_import_key_cmd_t {
	enum qti_storage_service_cmd_t cmd_id;
	struct qti_storage_service_key_blob_t key_blob;
	uint64_t input_key;
	uint32_t input_key_len;
};

struct qti_storage_service_gen_key_cmd_t {
	enum qti_storage_service_cmd_t cmd_id;
	struct qti_storage_service_key_blob_t key_blob;
};

struct qti_storage_service_gen_key_resp_t {
	enum qti_storage_service_cmd_t cmd_id;
	int32_t status;
	uint32_t key_blob_size;
};

struct qti_storage_service_seal_data_cmd_t {
	enum qti_storage_service_cmd_t cmd_id;
	struct qti_storage_service_key_blob_t key_blob;
	uint64_t plain_data;
	uint32_t plain_data_len;
	uint64_t output_buffer;
	uint32_t output_len;
};

struct qti_storage_service_seal_data_resp_t {
	enum qti_storage_service_cmd_t cmd_id;
	int32_t status;
	uint32_t sealed_data_len;
};

struct qti_storage_service_unseal_data_cmd_t {
	enum qti_storage_service_cmd_t cmd_id;
	struct qti_storage_service_key_blob_t key_blob;
	uint64_t sealed_data;
	uint32_t sealed_dlen;
	uint64_t output_buffer;
	uint32_t output_len;
};

struct qti_storage_service_unseal_data_resp_t {
	enum qti_storage_service_cmd_t cmd_id;
	int32_t status;
	uint32_t unsealed_data_len;
};

struct qti_crypto_service_encrypt_data_cmd_t {
	uint64_t type;
	uint64_t mode;
	uint64_t plain_data;
	uint64_t plain_data_len;
	uint64_t iv;
	uint64_t iv_len;
	uint64_t output_buffer;
	uint64_t output_len;
};

struct qti_crypto_service_encrypt_data_cmd_t_v2 {
	uint64_t key_handle;
	struct qti_crypto_service_encrypt_data_cmd_t v1;
};

struct qti_crypto_service_decrypt_data_cmd_t {
	uint64_t type;
	uint64_t mode;
	uint64_t encrypted_data;
	uint64_t encrypted_dlen;
	uint64_t iv;
	uint64_t iv_len;
	uint64_t output_buffer;
	uint64_t output_len;
};

struct qti_crypto_service_decrypt_data_cmd_t_v2 {
	uint64_t key_handle;
	struct qti_crypto_service_decrypt_data_cmd_t v1;
};

struct qti_storage_service_rsa_key_t {
	uint32_t magic_num;
	uint32_t version;
	enum qti_storage_service_digest_pad_algo_t pad_algo;
	uint8_t modulus[RSA_KEY_SIZE_MAX];
	uint32_t modulus_len;
	uint8_t public_exponent[RSA_KEY_SIZE_MAX];
	uint32_t public_exponent_len;
	uint8_t iv[RSA_IV_LENGTH];
	uint8_t pvt_exponent[RSA_KEY_SIZE_MAX];
	uint32_t pvt_exponent_len;
	uint8_t hmac[RSA_HMAC_LENGTH];
};

struct qti_storage_service_rsa_key_blob_t {
	uint64_t key_material;
	uint32_t key_material_len;
};

#define RSA_KEY_BLOB_SIZE sizeof(struct qti_storage_service_rsa_key_t)

struct qti_storage_service_rsa_keygen_params_t {
	uint32_t modulus_size;
	uint64_t public_exponent;
	enum qti_storage_service_digest_pad_algo_t pad_algo;
};

struct qti_storage_service_rsa_gen_key_cmd_t {
	enum qti_storage_service_rsa_cmd_t cmd_id;
	struct qti_storage_service_rsa_key_blob_t key_blob;
	struct qti_storage_service_rsa_keygen_params_t rsa_params;
};

struct qti_storage_service_rsa_gen_key_resp_t {
	enum qti_storage_service_rsa_cmd_t cmd_id;
	int32_t status;
	uint32_t key_blob_size;
};

struct qti_storage_service_rsa_import_key_cmd_t {
	enum qti_storage_service_rsa_cmd_t cmd_id;
	uint8_t modulus[RSA_KEY_SIZE_MAX];
	uint32_t modulus_len;
	uint8_t public_exponent[RSA_PUB_EXP_SIZE_MAX];
	uint32_t public_exponent_len;
	uint8_t pvt_exponent[RSA_KEY_SIZE_MAX];
	uint32_t pvt_exponent_len;
	uint32_t digest_pad_type;
	struct qti_storage_service_rsa_key_blob_t key_blob;
};

struct qti_storage_service_rsa_import_key_resp_t {
	enum qti_storage_service_rsa_cmd_t cmd_id;
	int32_t status;
};

struct qti_storage_service_rsa_sign_data_cmd_t {
	enum qti_storage_service_rsa_cmd_t cmd_id;
	struct qti_storage_service_rsa_key_blob_t key_blob;
	uint64_t plain_data;
	uint32_t plain_data_len;
	uint64_t output_buffer;
	uint32_t output_len;
};

struct qti_storage_service_rsa_sign_data_resp_t {
	enum qti_storage_service_rsa_cmd_t cmd_id;
	uint32_t sealed_data_len;
	int32_t status;
};

struct qti_storage_service_rsa_verify_data_cmd_t {
	enum qti_storage_service_rsa_cmd_t cmd_id;
	struct qti_storage_service_rsa_key_blob_t key_blob;
	uint64_t data;
	uint32_t data_len;
	uint64_t signed_data;
	uint32_t signed_dlen;
};

struct qti_storage_service_rsa_verify_data_resp_t {
	enum qti_storage_service_rsa_cmd_t cmd_id;
	int32_t status;
};

struct qti_storage_service_rsa_update_keyblob_cmd_t {
	enum qti_storage_service_rsa_cmd_t cmd_id;
	struct qti_storage_service_rsa_key_blob_t key_blob;
	enum qti_storage_service_digest_pad_algo_t pad_algo;
};

struct qti_storage_service_rsa_update_keyblob_data_resp_t {
	enum qti_storage_service_rsa_cmd_t cmd_id;
	int32_t status;
	uint32_t key_blob_size;
};

struct qsee_64_send_cmd {
	uint32_t cmd_id;
	uint64_t data;
	uint64_t data2;
	uint32_t len;
	uint32_t start_pkt;
	uint32_t end_pkt;
	uint32_t test_buf_size;
};

struct qsee_send_cmd_rsp {
	uint32_t data;
	int32_t status;
};

enum qseecom_qceos_cmd_status {
	QSEOS_RESULT_SUCCESS	= 0,
	QSEOS_RESULT_INCOMPLETE,
	QSEOS_RESULT_FAILURE	= 0xFFFFFFFF
};

struct qti_storage_service_rsa_message_type {
	uint64_t input;
	uint64_t input_len;
	uint64_t label;
	uint64_t label_len;
	uint64_t output;
	uint64_t output_len;
	uint64_t padding_type;
	uint64_t hashidx;
};

struct qti_storage_service_rsa_key_type {
	uint64_t nbits;   //Number of bits in modulus
	uint64_t n;  // Modulus
	uint64_t e;  // Public Exponent
	uint64_t d;  // Private Exponent
};

struct qti_storage_service_rsa_message_req {
	uint64_t key_req;
	uint64_t msg_req;
	uint64_t operation;
};

struct qti_storage_service_fuse_blow_req {
	uint64_t addr;
	uint64_t value;
	uint64_t is_fec_enable;
};

enum qti_storage_service_rsa_operation_id {
	QTI_APP_RSA_ENCRYPTION_ID = 0,
	QTI_APP_RSA_DECRYPTION_ID
};

enum qti_storage_service_rsa_padding_type {
	QSEE_RSA_PADDING_TYPE_OAEP = 0,
	QSEE_RSA_PADDING_TYPE_PKCS,
	QSEE_RSA_NO_PADDING,
	QSEE_RSA_PADDING_TYPE_MAX
};

enum qti_storage_service_qsee_hash_id {
	QSEE_HASH_IDX_NULL = 1,
	QSEE_HASH_IDX_SHA1,
	QSEE_HASH_IDX_SHA256,
	QSEE_HASH_IDX_SHA224,
	QSEE_HASH_IDX_SHA384,
	QSEE_HASH_IDX_SHA512,
	QSEE_HASH_IDX_SHA256_SHA1,
	QSEE_HASH_IDX_MAX,
	QSEE_HASH_IDX_INVALID = 0x7FFFFFFF,
};

#define FILE_PATH_MAX		100

static uint32_t ecdsa_key_handle;

struct ta_info {
	char mdt_file[FILE_PATH_MAX];
	char seg_file[FILE_PATH_MAX];
};

struct ecdsa_import_key {
	void *ecdsa_blob;
	u32 ecdsa_blob_len;
	u32 key_handle;
	u32 result;
};

struct qsee_ecdsa_import_key {
	u32 ecdsa_blob;
	u32 ecdsa_blob_len;
	u32 key_handle;
	u32 result;
};

#define MAX_ECC_PUB_KEY_LEN		133
#define MAX_ECC_ENC_PVT_KEY_LEN		80
#define MAX_CONTEXT_LEN			128
#define MAX_IV_SIZE			16
#define MAX_CMAC_AES_SIZE		16

struct qsee_ecdsa_import_blob {
	u32 curve_mode;
	u8 public_key[MAX_ECC_PUB_KEY_LEN];
	u32 public_key_len;
	u8 encrypted_private_key[MAX_ECC_ENC_PVT_KEY_LEN];
	u32 encrypted_private_key_len;
	u8 context[MAX_CONTEXT_LEN];
	u32 context_len;
	u8 iv_data[MAX_IV_SIZE];
	u32 iv_data_len;
	u8 cmac[MAX_CMAC_AES_SIZE];
};

struct ecdsa_points {
	unsigned char public_key[MAX_ECC_PUB_KEY_LEN];
	unsigned int public_key_len;
};

struct ecdsa_points ec_points;

#define ECDSA_SIGNATURE_MAX_LEN	132

struct ecdsa_sign {
	u32 key_handle;
	void *data;
	u32 data_len;
	void *signature;
	u32 signature_out_len;
	u32 result;
};

struct qsee_ecdsa_sign {
	u32 key_handle;
	u32 data;
	u32 data_len;
	u32 signature;
	u32 signature_len;
	u32 signature_out_len;
	u32 result;
};

struct ecdsa_verify {
	u32 key_handle;
	void *data;
	u32 data_len;
	void *signature;
	u32 signature_len;
	u32 result;
};

struct qsee_ecdsa_verify {
	u32 key_handle;
	u32 data;
	u32 data_len;
	u32 signature;
	u32 signature_len;
	u32 result;
};

#define QSEECOM_LOAD_TA_LIB             _IOWR('Q', 1, struct ta_info)
#define QSEECOM_LOAD_TA_APP             _IOWR('Q', 2, struct ta_info)
#define QSEECOM_UNLOAD_TA_LIB		_IO('Q', 3)
#define QSEECOM_UNLOAD_TA_APP           _IO('Q', 4)
#define QSEECOM_ECDSA_IMPORT_KEY	_IOWR('Q', 5, struct ecdsa_import_key)
#define QSEECOM_SET_KEY_HANDLE		_IOWR('Q', 6, uint32_t)
#define QSEECOM_ECDSA_SIGN		_IOWR('Q', 7, struct ecdsa_sign)
#define QSEECOM_ECDSA_VERIFY		_IOWR('Q', 8, struct ecdsa_verify)
#define QSEECOM_GET_EC_POINTS		_IOWR('Q', 9, struct ecdsa_points)

char message[MESSAGE_LEN];

static uint32_t qsee_app_id;
static void *qsee_sbuffer;
static unsigned long basic_output;
static size_t enc_len;
static size_t dec_len;
static int basic_data_len;
static int context_data_len;
static int aes_context_data_len;
static size_t mdt_size;
static size_t seg_size;
static int auth_size;
static uint8_t *mdt_file;
static uint8_t *seg_file;
static uint8_t *auth_file;
static uint8_t *aes_sealed_buf;
static uint64_t aes_encrypted_len;
static uint8_t *aes_unsealed_buf;
static uint64_t aes_decrypted_len;
static uint8_t *aes_ivdata;
static uint8_t aes_context_data[MAX_CONTEXT_BUFFER_LEN_V2];
static dma_addr_t __aligned(sizeof(dma_addr_t) * 8) aes_source_data;
static dma_addr_t __aligned(sizeof(dma_addr_t) * 8) aes_bindings_data;
static uint64_t aes_ivdata_len;
static uint64_t aes_type;
static uint64_t aes_mode;
static uint32_t rsa_keysize = RSA_2K_MODULUS_LEN;
static uint8_t *rsa_unsealed_buf;
static uint8_t *rsa_sealed_buf;
static uint64_t rsa_decrypted_len;
static uint64_t rsa_encrypted_len;
static uint8_t *rsa_label;
static uint64_t rsa_label_len;
static uint8_t *rsa_n_key;
static uint64_t rsa_n_key_len;
static uint8_t *rsa_e_key;
static uint64_t rsa_e_key_len;
static uint8_t *rsa_d_key;
static uint64_t rsa_d_key_len;
static uint64_t rsa_nbits_key;
static uint64_t rsa_hashidx;
static uint64_t rsa_padding_type;
static uint64_t fuse_addr;
static uint64_t fuse_value;
static uint64_t is_fec_enable;
static uint32_t cur_rsa_pad_scheme = QTI_STOR_SVC_RSA_DIGEST_PAD_PKCS115_SHA2_256;

static uint8_t *key_handle;
dma_addr_t dma_key_handle;
static uint8_t *aes_key_handle;
dma_addr_t dma_aes_key_handle;

static struct kobject *sec_kobj;
static uint8_t *key;
static size_t key_len;
static uint8_t *key_blob;
static size_t key_blob_len;
static uint8_t *sealed_buf;
static size_t seal_len;
static uint8_t *unsealed_buf;
static size_t unseal_len;
static uint64_t encrypted_len;
static uint64_t decrypted_len;
static uint8_t *ivdata;
static uint8_t context_data[MAX_CONTEXT_BUFFER_LEN_V2];
static dma_addr_t __aligned(sizeof(dma_addr_t) * 8) source_data;
static dma_addr_t __aligned(sizeof(dma_addr_t) * 8) bindings_data;
static uint64_t type;
static uint64_t mode;
static uint64_t ivdata_len;

static struct kobject *rsa_sec_kobj;
static uint8_t *rsa_import_modulus;
static size_t rsa_import_modulus_len;
static uint8_t *rsa_import_public_exponent;
static size_t rsa_import_public_exponent_len;
static uint8_t *rsa_import_pvt_exponent;
static size_t rsa_import_pvt_exponent_len;
static uint8_t *rsa_key_blob;
static size_t rsa_key_blob_len;
static uint8_t *rsa_sign_data_buf;
static size_t rsa_sign_data_len;
static uint8_t *rsa_plain_data_buf;
static size_t rsa_plain_data_len;
static int rsa_key_blob_buf_valid;


void *buf_rsa_key_blob = NULL;
void *buf_rsa_import_modulus = NULL;
void *buf_rsa_import_public_exponent = NULL;
void *buf_rsa_import_pvt_exponent = NULL;
void *buf_rsa_sign_data_buf = NULL;
void *buf_rsa_plain_data_buf = NULL;

dma_addr_t dma_rsa_key_blob = 0;
dma_addr_t dma_rsa_import_modulus = 0;
dma_addr_t dma_rsa_import_public_exponent = 0;
dma_addr_t dma_rsa_import_pvt_exponent = 0;
dma_addr_t dma_rsa_sign_data_buf = 0;
dma_addr_t dma_rsa_plain_data_buf = 0;

void *buf_key = NULL;
void *buf_key_blob = NULL;
void *buf_sealed_buf = NULL;
void *buf_unsealed_buf = NULL;
void *buf_iv = NULL;

dma_addr_t dma_key = 0;
dma_addr_t dma_key_blob = 0;
dma_addr_t dma_sealed_data = 0;
dma_addr_t dma_unsealed_data = 0;
dma_addr_t dma_iv_data = 0;

void *buf_aes_sealed_buf = NULL;
void *buf_aes_unsealed_buf = NULL;
void *buf_aes_iv = NULL;

dma_addr_t dma_aes_sealed_buf = 0;
dma_addr_t dma_aes_unsealed_buf = 0;
dma_addr_t dma_aes_ivdata = 0;

/* TMEL AES_v2 relevant variables start */
#define TME_KAL_KDF_NIST		0x80000
#define TME_KAL_KDF_HKDF		0x84000
#define TME_KID_CHIP_RAND_BASE		0x9
#define TME_KID_OEM_PRODUCT_SEED	0xC
#define TME_KID_L2_KEYWRAPSVC		0x6
#define TME_KID_L2_SECURESTRGSVC	0x7
#define TME_KAL_ECC_ALGO_ECDSA		0x0
#define TME_KAL_ECC_ALGO_ECDH		0x4000
#define TME_KAL_SHA256_HMAC		0x48000
#define TME_KAL_AES256_GCM		0x2
#define TME_KAL_AES256_CBC		0x8
#define TME_KAL_AES256_ECB		0xC
#define TME_KAL_AES256_SIV		0x30000
#define TME_KSC_SWContext		0x00000020
#define TME_KSC_MixingKey		0x00000080
#define TME_KAL_SHA512_HMAC		0x58000
#define TME_WK_CONTEXT_BYTES_MAX        0x64
#define TME_MAX_KEY_LEN			48

#define TME_KDF_SW_CONTEXT_BYTES_MAX	128
#define TME_KDF_SALT_LABEL_BYTES_MAX	64

#define TME_MAX_AAD_LEN			256
#define TME_MAX_TAG_LEN			16

static struct kobject *tmel_sec_kobj;
static uint8_t *tmel_key_handle;
dma_addr_t tmel_dma_key_handle;

static u32 tmel_wrap_key_id; /* ID of the key to be wrapped */
static u32 tmel_wrapping_key_id; /* The ID of the key wrapping key to be used to wrap the target key */

static uint32_t tmel_aes_encrypted_len;
static uint32_t tmel_aes_decrypted_len;
static uint32_t tmel_aes_aad_len;
static uint32_t tmel_aes_iv_len;
static uint32_t tmel_aes_tag_len;
static uint32_t tmel_salt_label_len;
static uint32_t tmel_sw_context_len;
static uint32_t tmel_sec_ctx = TME_KSC_SWContext;
static uint32_t tmel_algo;
static uint32_t tmel_input_key;
static uint32_t tmel_pt_key_len;
static uint32_t tmel_curve_id;
static uint32_t tmel_prv_key_id;
static uint32_t tmel_prv_key2_id;
static uint32_t tmel_pub_key_len;
static uint32_t tmel_pub_key2_len;
static uint32_t tmel_ecc_sign_msg_len;
static uint32_t tmel_ecc_plain_txt_len;
static uint32_t tmel_hash_algo_id;
static uint32_t tmel_ecc_algo;
static uint32_t tmel_shared_key_id;
static uint32_t tmel_mix_key_id;
static uint32_t tmel_hmac_sha_key_id;
static uint32_t tmel_hmac_sha_input_len;
static uint32_t tmel_hmac_sha_digest_len;

void *buf_aad;
void *buf_plain_txt;
void *buf_ivd;
void *buf_tag;
void *buf_cipher_txt;
void *buf_salt_label;
void *buf_sw_context;
void *buf_pt_key;
void *buf_wrapped_key;
void *buf_pub_key;
void *buf_pub_key2;
void *buf_ecc_msg;
void *buf_ecc_signature;
void *buf_hmac_sha_input;
void *buf_hmac_sha_digest;

static uint8_t *aad;
static uint8_t *plain_txt;
static uint8_t *iv;
static uint8_t *tag;
static uint8_t *cipher_txt;
static uint8_t *salt_label;
static uint8_t *sw_context;
static uint8_t *pt_key;
static uint8_t *wrapped_key;
static uint8_t *pub_key;
static uint8_t *pub_key2;
static uint8_t *ecc_msg;
static uint8_t *ecc_signature;
static uint8_t *hmac_sha_input;
static uint8_t *hmac_sha_digest;

dma_addr_t dma_aad;
dma_addr_t dma_plain_txt;
dma_addr_t dma_ivd;
dma_addr_t dma_tag;
dma_addr_t dma_cipher_txt;
dma_addr_t dma_salt_label;
dma_addr_t dma_sw_context;
dma_addr_t dma_pt_key;
dma_addr_t dma_wrapped_key;
dma_addr_t dma_pub_key;
dma_addr_t dma_pub_key2;
dma_addr_t dma_ecc_msg;
dma_addr_t dma_ecc_signature;
dma_addr_t dma_hmac_sha_input;
dma_addr_t dma_hmac_sha_digest;
/* TMEL AES_v2 relevant variables end */

void *buf_rsa_unsealed_buf = NULL;
void *buf_rsa_sealed_buf = NULL;
void *buf_rsa_label = NULL;
void *buf_rsa_n_key = NULL;
void *buf_rsa_e_key = NULL;
void *buf_rsa_d_key = NULL;

dma_addr_t dma_rsa_unsealed_buf = 0;
dma_addr_t dma_rsa_sealed_buf = 0;
dma_addr_t dma_rsa_label = 0;
dma_addr_t dma_rsa_n_key = 0;
dma_addr_t dma_rsa_e_key = 0;
dma_addr_t dma_rsa_d_key = 0;

struct kobject *qtiapp_kobj;
struct attribute_group qtiapp_attr_grp;
struct kobject *qtiapp_aes_kobj;
struct kobject *qtiapp_rsa_kobj;
struct kobject *qtiapp_fuse_write_kobj;

static void *q_qsee_log;
static dma_addr_t dma_qsee_log_buf;

static dev_t chr_dev;
static struct class *dev_class;
static struct cdev qseecom_cdev;

static struct device *qdev;

/*
 * Array Length is 4096 bytes, since 4MB is the max input size
 * that can be passed to SCM call
 */
static uint8_t encrypt_text[MAX_INPUT_SIZE];
static uint8_t decrypt_text[MAX_INPUT_SIZE];

#define MUL		0x1
#define ENC		0x2
#define DEC		0x4
#define CRYPTO		0x8
#define AUTH_OTP	0x10
#define AES_SEC_KEY	0x20
#define RSA_SEC_KEY	0x40
#define LOG_BITMASK	0x80
#define FUSE		0x100
#define MISC		0x200
#define AES_TZAPP	0x400
#define RSA_TZAPP	0x800
#define FUSE_WRITE	0x1000

#define RSA_KEY_ALIGN	8
enum qti_app_cmd_ids {
	QTI_APP_BASIC_DATA_TEST_ID = 1,
	QTI_APP_ENC_TEST_ID,
	QTI_APP_DEC_TEST_ID,
	QTI_APP_CRYPTO_TEST_ID,
	QTI_APP_AUTH_OTP_TEST_ID,
	QTI_APP_LOG_BITMASK_TEST_ID,
	QTI_APP_FUSE_TEST_ID,
	QTI_APP_MISC_TEST_ID,
	QTI_APP_AES_ENCRYPT_ID,
	QTI_APP_AES_DECRYPT_ID,
	QTI_APP_RSA_ENC_DEC_ID,
	QTI_APP_FUSE_BLOW_ID,
	QTI_APP_KEY_DERIVE_TEST,
	QTI_APP_CLEAR_KEY,
	QTI_APP_GET_VERSION,
	QTI_APP_ECDSA_IMPORT_KEY,
	QTI_APP_ECDSA_SIGN,
	QTI_APP_ECDSA_VERIFY
};

static ssize_t generate_key_blob(struct device *dev,
				struct device_attribute *attr, char *buf);

static ssize_t show_aes_derive_key(struct device *dev,
				struct device_attribute *attr, char *buf);

static ssize_t show_aes_derive_128_byte_key(struct device *dev,
				struct device_attribute *attr, char *buf);

static ssize_t store_aes_derive_key(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count);

static ssize_t store_aes_clear_key(struct device *dev,
				struct device_attribute *attr,
				const char* buf, size_t count);

static ssize_t store_key(struct device *dev, struct device_attribute *attr,
			const char *buf, size_t count);

static ssize_t import_key_blob(struct device *dev,
			      struct device_attribute *attr, char *buf);

static ssize_t store_key_blob(struct device *dev,
			     struct device_attribute *attr,
			     const char *buf, size_t count);

static ssize_t store_unsealed_data(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf, size_t count);

static ssize_t show_sealed_data(struct device *dev,
			       struct device_attribute *attr, char *buf);

static ssize_t store_sealed_data(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count);

static ssize_t show_unsealed_data(struct device *dev,
				 struct device_attribute *attr, char *buf);

static ssize_t store_aes_type(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count);

static ssize_t store_aes_mode(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count);

static ssize_t store_source_data(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count);

static ssize_t store_context_data(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count);

static ssize_t store_bindings_data(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count);

static ssize_t store_iv_data(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count);

static ssize_t store_encrypted_data(struct device *dev,
                                struct device_attribute *attr,
                                const char *buf, size_t count);

static ssize_t show_encrypted_data(struct device *dev,
				struct device_attribute *attr, char *buf);

static ssize_t store_decrypted_data(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count);

static ssize_t show_decrypted_data(struct device *dev,
				struct device_attribute *attr, char *buf);

static ssize_t store_rsa_keysize(struct device *dev,
				 struct device_attribute *attr,
				 const char *buf, size_t count);

static ssize_t generate_rsa_key_blob(struct device *dev,
				    struct device_attribute *attr,
				    char *buf);

static ssize_t store_rsa_key(struct device *dev, struct device_attribute *attr,
			    const char *buf, size_t count);

static ssize_t import_rsa_key_blob(struct device *dev,
				  struct device_attribute *attr, char *buf);

static ssize_t store_rsa_key_blob(struct device *dev,
				 struct device_attribute *attr,
				 const char *buf, size_t count);

static ssize_t store_rsa_plain_data(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t count);

static ssize_t show_rsa_signed_data(struct device *dev,
				   struct device_attribute *attr, char *buf);

static ssize_t store_rsa_signed_data(struct device *dev,
				    struct device_attribute *attr,
				    const char *buf, size_t count);

static ssize_t verify_rsa_signed_data(struct device *dev,
				     struct device_attribute *attr, char *buf);

static ssize_t store_rsa_pad_scheme(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t count);

static ssize_t show_rsa_pad_scheme(struct device *dev,
				  struct device_attribute *attr, char *buf);

static ssize_t show_rsa_update_keyblob(struct device *dev,
				       struct device_attribute *attr,
				       char *buf);

static ssize_t mdt_write(struct file *filp, struct kobject *kobj,
			struct bin_attribute *bin_attr,
			char *buf, loff_t pos, size_t count);

static ssize_t seg_write(struct file *filp, struct kobject *kobj,
			struct bin_attribute *bin_attr,
			char *buf, loff_t pos, size_t count);

static ssize_t auth_write(struct file *filp, struct kobject *kobj,
			 struct bin_attribute *bin_attr,
			 char *buf, loff_t pos, size_t count);

static ssize_t store_log_size(struct device *dev, struct device_attribute *attr,
			      const char *buf, size_t count);

static ssize_t store_load_start(struct device *dev,
			       struct device_attribute *attr,
			       const char *buf, size_t count);

static ssize_t show_basic_output(struct device *dev,
				struct device_attribute *attr, char *buf);

static ssize_t store_basic_input(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count);

static ssize_t show_encrypt_output(struct device *dev,
				  struct device_attribute *attr, char *buf);

static ssize_t store_encrypt_input(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf, size_t count);

static ssize_t show_decrypt_output(struct device *dev,
				  struct device_attribute *attr, char *buf);

static ssize_t store_decrypt_input(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf, size_t count);

static ssize_t store_crypto_input(struct device *dev,
				 struct device_attribute *attr,
				 const char *buf, size_t count);

static ssize_t store_fuse_otp_input(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t count);

static ssize_t store_log_bitmask_input(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t count);

static ssize_t store_fuse_input(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t count);

static ssize_t store_misc_input(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t count);

static ssize_t show_qsee_app_id(struct device *dev,
				   struct device_attribute *attr, char *buf);

static ssize_t show_aes_derive_key_qtiapp(struct device *dev,
				struct device_attribute *attr, char *buf);

static ssize_t store_aes_derive_key_qtiapp(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count);

static ssize_t store_aes_clear_key_qtiapp(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count);

static ssize_t store_aes_type_qtiapp(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count);

static ssize_t store_aes_mode_qtiapp(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count);

static ssize_t store_source_data_qtiapp(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count);

static ssize_t store_context_data_qtiapp(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count);

static ssize_t store_bindings_data_qtiapp(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count);

static ssize_t store_iv_data_qtiapp(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count);

static ssize_t show_aes_encrypted_data_qtiapp(struct device *dev,
				struct device_attribute *attr, char *buf);

static ssize_t store_aes_decrypted_data_qtiapp(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count);

static ssize_t show_aes_decrypted_data_qtiapp(struct device *dev,
				struct device_attribute *attr, char *buf);

static ssize_t store_aes_encrypted_data_qtiapp(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count);


static ssize_t store_decrypted_rsa_data_qtiapp(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count);
static ssize_t show_encrypted_rsa_data_qtiapp(struct device *dev,
				struct device_attribute *attr, char *buf);
static ssize_t store_encrypted_rsa_data_qtiapp(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count);
static ssize_t show_decrypted_rsa_data_qtiapp(struct device *dev,
				struct device_attribute *attr, char *buf);

static ssize_t store_label_rsa_data_qtiapp(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count);
static ssize_t store_n_key_rsa_data_qtiapp(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count);
static ssize_t store_e_key_rsa_data_qtiapp(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count);
static ssize_t store_d_key_rsa_data_qtiapp(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count);
static ssize_t store_nbits_key_rsa_data_qtiapp(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count);
static ssize_t store_hashidx_rsa_data_qtiapp(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count);
static ssize_t store_padding_type_rsa_data_qtiapp(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count);
static ssize_t store_addr_fuse_write_qtiapp(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count);
static ssize_t store_value_fuse_write_qtiapp(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count);
static ssize_t store_fec_enable_fuse_write_qtiapp(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count);
static ssize_t store_blow_fuse_write_qtiapp(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count);

/* TMEL AES v2 */
static ssize_t tmecomm_show_derive_key(struct device *dev,
				       struct device_attribute *attr,
				       char *buf);
static ssize_t tmecomm_show_key(struct device *dev,
				struct device_attribute *attr,
				char *buf);
static ssize_t tmecomm_store_key(struct device *dev,
				 struct device_attribute *attr,
				 const char *buf, size_t count);
static ssize_t tmecomm_store_clear_key(struct device *dev,
				       struct device_attribute *attr,
				       const char *buf, size_t count);
static ssize_t tmecomm_store_aes_decrypted_data(struct device *dev,
						struct device_attribute *attr,
						const char *buf, size_t count);
static ssize_t tmecomm_show_aes_encrypted_data(struct device *dev,
					       struct device_attribute *attr,
					       char *buf);
static ssize_t tmecomm_store_aes_encrypted_data(struct device *dev,
						struct device_attribute *attr,
						const char *buf, size_t count);
static ssize_t tmecomm_show_aes_decrypted_data(struct device *dev,
					       struct device_attribute *attr,
					       char *buf);
static ssize_t tmecomm_store_context_data(struct device *dev,
					  struct device_attribute *attr,
					  const char *buf, size_t count);
static ssize_t tmecomm_store_salt_label_data(struct device *dev,
					     struct device_attribute *attr,
					     const char *buf, size_t count);
static ssize_t tmecomm_store_security_context(struct device *dev,
					      struct device_attribute *attr,
					      const char *buf, size_t count);
static ssize_t tmecomm_aes_show_iv_data(struct device *dev,
					struct device_attribute *attr,
					char *buf);
static ssize_t tmecomm_aes_store_iv_data(struct device *dev,
					 struct device_attribute *attr,
					 const char *buf, size_t count);
static ssize_t tmecomm_aes_show_aad_data(struct device *dev,
					 struct device_attribute *attr,
					 char *buf);
static ssize_t tmecomm_aes_store_aad_data(struct device *dev,
					  struct device_attribute *attr,
					  const char *buf, size_t count);
static ssize_t tmecomm_aes_show_tag_data(struct device *dev,
					 struct device_attribute *attr,
					 char *buf);
static ssize_t tmecomm_aes_store_tag_data(struct device *dev,
					  struct device_attribute *attr,
					  const char *buf, size_t count);
static ssize_t tmecomm_store_algo(struct device *dev,
				      struct device_attribute *attr,
				      const char *buf, size_t count);
static ssize_t tmecomm_store_input_key(struct device *dev,
				       struct device_attribute *attr,
				       const char *buf, size_t count);
static ssize_t tmecomm_show_generate_key(struct device *dev,
					 struct device_attribute *attr,
					 char *buf);
static ssize_t tmecomm_show_import_key(struct device *dev,
				       struct device_attribute *attr,
				       char *buf);
static ssize_t tmecomm_store_pt_key(struct device *dev,
				    struct device_attribute *attr,
				    const char *buf, size_t count);
static ssize_t tmecomm_store_wrap_key_id(struct device *dev,
					 struct device_attribute *attr,
					 const char *buf, size_t count);
static ssize_t tmecomm_store_wrapping_key_id(struct device *dev,
					     struct device_attribute *attr,
					     const char *buf, size_t count);
static ssize_t tmecomm_show_wrap_key(struct device *dev,
				     struct device_attribute *attr,
				     char *buf);
static ssize_t tmecomm_store_unwrap_key(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count);
static ssize_t tmecomm_show_unwrap_key(struct device *dev,
				       struct device_attribute *attr,
				       char *buf);
static ssize_t tmecomm_store_ecc_curve_id(struct device *dev,
					  struct device_attribute *attr,
					  const char *buf, size_t count);
static ssize_t tmecomm_store_ecc_prv_key_id(struct device *dev,
					    struct device_attribute *attr,
					    const char *buf, size_t count);
static ssize_t tmecomm_store_hash_algo(struct device *dev,
				       struct device_attribute *attr,
				       const char *buf, size_t count);
static ssize_t tmecomm_show_ecc_pub_key(struct device *dev,
					struct device_attribute *attr,
					char *buf);
static ssize_t tmecomm_store_ecc_pub_key(struct device *dev,
					 struct device_attribute *attr,
					 const char *buf, size_t count);
static ssize_t tmecomm_store_ecc_input_buf(struct device *dev,
					   struct device_attribute *attr,
					   const char *buf, size_t count);
static ssize_t tmecomm_show_ecc_sign_msg(struct device *dev,
					 struct device_attribute *attr,
					 char *buf);
static ssize_t tmecomm_store_ecc_signature(struct device *dev,
					   struct device_attribute *attr,
					   const char *buf, size_t count);
static ssize_t tmecomm_show_ecc_verify_msg(struct device *dev,
					   struct device_attribute *attr,
					   char *buf);
static ssize_t tmecomm_show_ecc_pub_key2(struct device *dev,
					struct device_attribute *attr,
					char *buf);
static ssize_t tmecomm_store_ecc_pub_key2(struct device *dev,
					  struct device_attribute *attr,
					  const char *buf, size_t count);
static ssize_t tmecomm_store_ecc_prv_key_id2(struct device *dev,
					     struct device_attribute *attr,
					     const char *buf, size_t count);
static ssize_t tmecomm_show_ecdh_shared_secret(struct device *dev,
					       struct device_attribute *attr,
					       char *buf);
static ssize_t tmecomm_store_mix_key_id(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count);
static ssize_t tmecomm_store_hmac_sha_key_id(struct device *dev,
					     struct device_attribute *attr,
					     const char *buf, size_t count);
static ssize_t tmecomm_show_hmac_sha(struct device *dev,
				     struct device_attribute *attr,
				     char *buf);
static ssize_t tmecomm_store_hmac_sha(struct device *dev,
				      struct device_attribute *attr,
				      const char *buf, size_t count);

/* Qti app device attrs starts here....*/

static DEVICE_ATTR(log_size, 0644, NULL, store_log_size);
static DEVICE_ATTR(load_start, S_IWUSR, NULL, store_load_start);
static DEVICE_ATTR(basic_data, 0644, show_basic_output, store_basic_input);
static DEVICE_ATTR(encrypt, 0644, show_encrypt_output, store_encrypt_input);
static DEVICE_ATTR(decrypt, 0644, show_decrypt_output, store_decrypt_input);
static DEVICE_ATTR(crypto, 0644, NULL, store_crypto_input);
static DEVICE_ATTR(fuse_otp, 0644, NULL, store_fuse_otp_input);
static DEVICE_ATTR(log_bitmask, 0644, NULL, store_log_bitmask_input);
static DEVICE_ATTR(fuse, 0644, NULL, store_fuse_input);
static DEVICE_ATTR(misc, 0644, NULL, store_misc_input);
static DEVICE_ATTR(qsee_app_id, 0644, show_qsee_app_id, NULL);

static DEVICE_ATTR(derive_key_aes, 0644, show_aes_derive_key_qtiapp, store_aes_derive_key_qtiapp);
static DEVICE_ATTR(clear_key_qtiapp, 0644, NULL, store_aes_clear_key_qtiapp);
static DEVICE_ATTR(encrypt_aes, 0644, show_aes_encrypted_data_qtiapp, store_aes_decrypted_data_qtiapp);
static DEVICE_ATTR(decrypt_aes, 0644, show_aes_decrypted_data_qtiapp, store_aes_encrypted_data_qtiapp);
static DEVICE_ATTR(ivdata_aes, 0644, NULL, store_iv_data_qtiapp);
static DEVICE_ATTR(type_aes, 0644, NULL, store_aes_type_qtiapp);
static DEVICE_ATTR(mode_aes, 0644, NULL, store_aes_mode_qtiapp);
static DEVICE_ATTR(context_data_aes, 0644, NULL, store_context_data_qtiapp);
static DEVICE_ATTR(source_data_aes, 0644, NULL, store_source_data_qtiapp);
static DEVICE_ATTR(bindings_data_aes, 0644, NULL, store_bindings_data_qtiapp);

static DEVICE_ATTR(encrypt_rsa, 0644, show_encrypted_rsa_data_qtiapp, store_decrypted_rsa_data_qtiapp);
static DEVICE_ATTR(decrypt_rsa, 0644, show_decrypted_rsa_data_qtiapp, store_encrypted_rsa_data_qtiapp);
static DEVICE_ATTR(label_rsa, 0644, NULL, store_label_rsa_data_qtiapp);
static DEVICE_ATTR(modulus_key, 0644, NULL, store_n_key_rsa_data_qtiapp);
static DEVICE_ATTR(public_exponent_key, 0644, NULL, store_e_key_rsa_data_qtiapp);
static DEVICE_ATTR(private_exponent_key, 0644, NULL, store_d_key_rsa_data_qtiapp);
static DEVICE_ATTR(nbits_key, 0644, NULL, store_nbits_key_rsa_data_qtiapp);
static DEVICE_ATTR(hashidx, 0644, NULL, store_hashidx_rsa_data_qtiapp);
static DEVICE_ATTR(padding_type, 0644, NULL, store_padding_type_rsa_data_qtiapp);

static DEVICE_ATTR(addr, 0644, NULL, store_addr_fuse_write_qtiapp);
static DEVICE_ATTR(value, 0644, NULL, store_value_fuse_write_qtiapp);
static DEVICE_ATTR(fec_enable, 0644, NULL, store_fec_enable_fuse_write_qtiapp);
static DEVICE_ATTR(blow, 0644, NULL, store_blow_fuse_write_qtiapp);

/* Tz app device attrs ends here....*/

static DEVICE_ATTR(generate, 0644, generate_key_blob, NULL);
static DEVICE_ATTR(derive_aes_key, 0644, show_aes_derive_key, store_aes_derive_key);
static DEVICE_ATTR(derive_aes_max_ctxt_key, 0644,
		show_aes_derive_128_byte_key, store_aes_derive_key);
static DEVICE_ATTR(clear_key, 0644, NULL, store_aes_clear_key);
static DEVICE_ATTR(import, 0644, import_key_blob, store_key);
static DEVICE_ATTR(key_blob, 0644, NULL, store_key_blob);
static DEVICE_ATTR(seal, 0644, show_sealed_data, store_unsealed_data);
static DEVICE_ATTR(unseal, 0644, show_unsealed_data, store_sealed_data);
static DEVICE_ATTR(aes_encrypt, 0644, show_encrypted_data, store_decrypted_data);
static DEVICE_ATTR(aes_decrypt, 0644, show_decrypted_data, store_encrypted_data);
static DEVICE_ATTR(aes_ivdata, 0644, NULL, store_iv_data);
static DEVICE_ATTR(context_data, 0644, NULL, store_context_data);
static DEVICE_ATTR(source_data, 0644, NULL, store_source_data);
static DEVICE_ATTR(bindings_data, 0644, NULL, store_bindings_data);
static DEVICE_ATTR(aes_type, 0644, NULL, store_aes_type);
static DEVICE_ATTR(aes_mode, 0644, NULL, store_aes_mode);

static DEVICE_ATTR(rsa_keysize, 0644, NULL, store_rsa_keysize);
static DEVICE_ATTR(rsa_generate, 0644, generate_rsa_key_blob, NULL);
static DEVICE_ATTR(rsa_key_blob, 0644, NULL, store_rsa_key_blob);
static DEVICE_ATTR(rsa_import, 0644, import_rsa_key_blob, store_rsa_key);
static DEVICE_ATTR(rsa_sign, 0644, show_rsa_signed_data,
		   store_rsa_plain_data);
static DEVICE_ATTR(rsa_verify, 0644, verify_rsa_signed_data,
		   store_rsa_signed_data);
static DEVICE_ATTR(rsa_pad_scheme, 0644, show_rsa_pad_scheme, store_rsa_pad_scheme);
static DEVICE_ATTR(rsa_update_keyblob, 0644, show_rsa_update_keyblob, NULL);

/* TMEL AES device attrs */
static DEVICE_ATTR(tmel_key, 0644, tmecomm_show_key, tmecomm_store_key);
static DEVICE_ATTR(tmel_derive_key, 0644, tmecomm_show_derive_key, NULL);
static DEVICE_ATTR(tmel_clear_key, 0644, NULL, tmecomm_store_clear_key);
static DEVICE_ATTR(tmel_aes_encrypt, 0644, tmecomm_show_aes_encrypted_data,
		   tmecomm_store_aes_decrypted_data);
static DEVICE_ATTR(tmel_aes_decrypt, 0644, tmecomm_show_aes_decrypted_data,
		   tmecomm_store_aes_encrypted_data);
static DEVICE_ATTR(tmel_algo, 0644, NULL, tmecomm_store_algo);
static DEVICE_ATTR(tmel_context_data, 0644, NULL, tmecomm_store_context_data);
static DEVICE_ATTR(tmel_salt_label_data, 0644, NULL, tmecomm_store_salt_label_data);
static DEVICE_ATTR(tmel_aes_iv_data, 0644, tmecomm_aes_show_iv_data,
		   tmecomm_aes_store_iv_data);
static DEVICE_ATTR(tmel_aes_aad_data, 0644, tmecomm_aes_show_aad_data,
		   tmecomm_aes_store_aad_data);
static DEVICE_ATTR(tmel_aes_tag_data, 0644, tmecomm_aes_show_tag_data,
		   tmecomm_aes_store_tag_data);
static DEVICE_ATTR(tmel_sec_ctx, 0644, NULL, tmecomm_store_security_context);
static DEVICE_ATTR(tmel_input_key, 0644, NULL, tmecomm_store_input_key);
static DEVICE_ATTR(tmel_generate_key, 0644, tmecomm_show_generate_key, NULL);
static DEVICE_ATTR(tmel_import_key, 0644, tmecomm_show_import_key, NULL);
static DEVICE_ATTR(tmel_pt_key, 0644, NULL, tmecomm_store_pt_key);
static DEVICE_ATTR(tmel_wrap_key_id, 0644, NULL, tmecomm_store_wrap_key_id);
static DEVICE_ATTR(tmel_wrapping_key_id, 0644, NULL, tmecomm_store_wrapping_key_id);
static DEVICE_ATTR(tmel_wrap_key, 0644, tmecomm_show_wrap_key, NULL);
static DEVICE_ATTR(tmel_unwrap_key, 0644, tmecomm_show_unwrap_key, tmecomm_store_unwrap_key);
static DEVICE_ATTR(tmel_hash_algo, 0644, NULL, tmecomm_store_hash_algo);
static DEVICE_ATTR(tmel_ecc_curve_id, 0644, NULL, tmecomm_store_ecc_curve_id);
static DEVICE_ATTR(tmel_ecc_prv_key_id, 0644, NULL, tmecomm_store_ecc_prv_key_id);
static DEVICE_ATTR(tmel_ecc_prv_key_id2, 0644, NULL, tmecomm_store_ecc_prv_key_id2);
static DEVICE_ATTR(tmel_ecc_pub_key, 0644, tmecomm_show_ecc_pub_key, tmecomm_store_ecc_pub_key);
static DEVICE_ATTR(tmel_ecc_pub_key2, 0644, tmecomm_show_ecc_pub_key2, tmecomm_store_ecc_pub_key2);
static DEVICE_ATTR(tmel_ecc_input_buf, 0644, NULL, tmecomm_store_ecc_input_buf);
static DEVICE_ATTR(tmel_ecc_sign_msg, 0644, tmecomm_show_ecc_sign_msg, NULL);
static DEVICE_ATTR(tmel_ecc_signature, 0644, NULL, tmecomm_store_ecc_signature);
static DEVICE_ATTR(tmel_ecc_verify_msg, 0644, tmecomm_show_ecc_verify_msg, NULL);
static DEVICE_ATTR(tmel_ecdh_shared_secret, 0644, tmecomm_show_ecdh_shared_secret, NULL);
static DEVICE_ATTR(tmel_mix_key_id, 0644, NULL, tmecomm_store_mix_key_id);
static DEVICE_ATTR(tmel_hmac_sha_key_id, 0644, NULL, tmecomm_store_hmac_sha_key_id);
static DEVICE_ATTR(tmel_hmac_sha, 0644, tmecomm_show_hmac_sha, tmecomm_store_hmac_sha);

static struct attribute *sec_key_attrs[] = {
	&dev_attr_generate.attr,
	&dev_attr_import.attr,
	&dev_attr_key_blob.attr,
	&dev_attr_seal.attr,
	&dev_attr_unseal.attr,
	NULL,
};

static struct attribute *sec_key_aesv2_attrs[] = {
	&dev_attr_derive_aes_key.attr,
	&dev_attr_derive_aes_max_ctxt_key.attr,
	&dev_attr_clear_key.attr,
	&dev_attr_context_data.attr,
	&dev_attr_source_data.attr,
	&dev_attr_bindings_data.attr,
	&dev_attr_aes_encrypt.attr,
	&dev_attr_aes_decrypt.attr,
	&dev_attr_aes_ivdata.attr,
	&dev_attr_aes_type.attr,
	&dev_attr_aes_mode.attr,
	NULL,
};

static struct attribute *rsa_sec_key_attrs[] = {
	&dev_attr_rsa_keysize.attr,
	&dev_attr_rsa_generate.attr,
	&dev_attr_rsa_key_blob.attr,
	&dev_attr_rsa_import.attr,
	&dev_attr_rsa_sign.attr,
	&dev_attr_rsa_verify.attr,
	&dev_attr_rsa_pad_scheme.attr,
	&dev_attr_rsa_update_keyblob.attr,
	NULL,
};

static struct attribute *qtiapp_aes_attrs[] = {
	&dev_attr_encrypt_aes.attr,
	&dev_attr_decrypt_aes.attr,
	&dev_attr_ivdata_aes.attr,
	&dev_attr_type_aes.attr,
	&dev_attr_mode_aes.attr,
	NULL,
};

static struct attribute *qtiapp_aesv2_attrs[] = {
	&dev_attr_derive_key_aes.attr,
	&dev_attr_clear_key_qtiapp.attr,
	&dev_attr_context_data_aes.attr,
	&dev_attr_source_data_aes.attr,
	&dev_attr_bindings_data_aes.attr,
	NULL,
};

static struct attribute *qtiapp_fuse_write_attrs[] = {
	&dev_attr_addr.attr,
	&dev_attr_value.attr,
	&dev_attr_fec_enable.attr,
	&dev_attr_blow.attr,
	NULL,
};

static struct attribute *qtiapp_rsa_attrs[] = {
	&dev_attr_encrypt_rsa.attr,
	&dev_attr_decrypt_rsa.attr,
	&dev_attr_label_rsa.attr,
	&dev_attr_modulus_key.attr,
	&dev_attr_public_exponent_key.attr,
	&dev_attr_private_exponent_key.attr,
	&dev_attr_nbits_key.attr,
	&dev_attr_hashidx.attr,
	&dev_attr_padding_type.attr,
	NULL,
};

static struct attribute_group sec_key_attr_grp = {
	.attrs = sec_key_attrs,
};

static struct attribute_group rsa_sec_key_attr_grp = {
	.attrs = rsa_sec_key_attrs,
};

static struct attribute_group sec_key_aesv2_attr_grp = {
	.attrs = sec_key_aesv2_attrs,
};

static struct attribute *sec_key_tmel_aes_attrs[] = {
	&dev_attr_tmel_key.attr,
	&dev_attr_tmel_derive_key.attr,
	&dev_attr_tmel_clear_key.attr,
	&dev_attr_tmel_aes_encrypt.attr,
	&dev_attr_tmel_aes_decrypt.attr,
	&dev_attr_tmel_context_data.attr,
	&dev_attr_tmel_salt_label_data.attr,
	&dev_attr_tmel_aes_iv_data.attr,
	&dev_attr_tmel_aes_aad_data.attr,
	&dev_attr_tmel_aes_tag_data.attr,
	&dev_attr_tmel_sec_ctx.attr,
	&dev_attr_tmel_algo.attr,
	&dev_attr_tmel_input_key.attr,
	&dev_attr_tmel_generate_key.attr,
	&dev_attr_tmel_import_key.attr,
	&dev_attr_tmel_pt_key.attr,
	&dev_attr_tmel_wrap_key_id.attr,
	&dev_attr_tmel_wrapping_key_id.attr,
	&dev_attr_tmel_wrap_key.attr,
	&dev_attr_tmel_unwrap_key.attr,
	&dev_attr_tmel_ecc_curve_id.attr,
	&dev_attr_tmel_ecc_prv_key_id.attr,
	&dev_attr_tmel_ecc_prv_key_id2.attr,
	&dev_attr_tmel_hash_algo.attr,
	&dev_attr_tmel_ecc_pub_key.attr,
	&dev_attr_tmel_ecc_pub_key2.attr,
	&dev_attr_tmel_ecc_input_buf.attr,
	&dev_attr_tmel_ecc_sign_msg.attr,
	&dev_attr_tmel_ecc_signature.attr,
	&dev_attr_tmel_ecc_verify_msg.attr,
	&dev_attr_tmel_ecdh_shared_secret.attr,
	&dev_attr_tmel_mix_key_id.attr,
	&dev_attr_tmel_hmac_sha_key_id.attr,
	&dev_attr_tmel_hmac_sha.attr,
	NULL,
};

static struct attribute_group qtiapp_aes_attr_grp = {
	.attrs = qtiapp_aes_attrs,
};

static struct attribute_group qtiapp_rsa_attr_grp = {
	.attrs = qtiapp_rsa_attrs,
};
static struct attribute_group qtiapp_fuse_write_attr_grp = {
	.attrs = qtiapp_fuse_write_attrs,
};
static struct attribute_group qtiapp_aesv2_attr_grp = {
	.attrs = qtiapp_aesv2_attrs,
};

static struct attribute_group sec_key_tmel_attr_grp = {
	.attrs = sec_key_tmel_aes_attrs,
};

struct bin_attribute mdt_attr = {
	.attr = {.name = "mdt_file", .mode = 0666},
	.write = mdt_write,
};

struct bin_attribute seg_attr = {
	.attr = {.name = "seg_file", .mode = 0666},
	.write = seg_write,
};

struct bin_attribute auth_attr = {
	.attr = {.name = "auth_file", .mode = 0666},
	.write = auth_write,
};

#endif
