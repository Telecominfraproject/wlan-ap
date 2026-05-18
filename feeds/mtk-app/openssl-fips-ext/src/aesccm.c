#include "aesccm.h"


struct operator ccm_oper = {
	.init = ccm_init,
	.uninit = ccm_uninit,
	.encrypt = ccm_encrypt,
	.decrypt = ccm_decrypt,
	.check = ccm_check,
};

static struct ccm_data data;

int ccm_init(void)
{
	hex2bin(&data.key, input_data.key, &(data.key_size));
	hex2bin(&data.nonce, input_data.nonce, &(data.nonce_size));
	hex2bin(&data.adata, input_data.adata, &(data.adata_size));
	hex2bin(&data.payload, input_data.payload, &(data.payload_size));
	hex2bin(&data.ct, input_data.ct, &(data.ct_size));
	if (input_data.oper == DECRYPT) {
		hex2bin(&data.tag, input_data.tag, &(data.tag_size));
	} else if (input_data.oper == ENCRYPT) {
		if (input_data.tag != NULL)
			data.tag_size = atoi(input_data.tag);
		else
			data.tag_size = 16;
	}
	return 0;
}

void init_ccm_evp_encrypt(EVP_CIPHER_CTX **ctx)
{
	if (data.key_size == 16)
		EVP_EncryptInit_ex(*ctx, EVP_aes_128_ccm(), NULL, NULL, NULL);
	else if (data.key_size == 24)
		EVP_EncryptInit_ex(*ctx, EVP_aes_192_ccm(), NULL, NULL, NULL);
	else if (data.key_size == 32)
		EVP_EncryptInit_ex(*ctx, EVP_aes_256_ccm(), NULL, NULL, NULL);
}

void init_ccm_evp_decrypt(EVP_CIPHER_CTX **ctx)
{
	if (data.key_size == 16)
		EVP_DecryptInit_ex(*ctx, EVP_aes_128_ccm(), NULL, NULL, NULL);
	else if (data.key_size == 24)
		EVP_DecryptInit_ex(*ctx, EVP_aes_192_ccm(), NULL, NULL, NULL);
	else if (data.key_size == 32)
		EVP_DecryptInit_ex(*ctx, EVP_aes_256_ccm(), NULL, NULL, NULL);
}
int ccm_encrypt(void)
{
	EVP_CIPHER_CTX *ctx;
	int outlen;
	unsigned char outbuf[1024];

	ctx = EVP_CIPHER_CTX_new();
	init_ccm_evp_encrypt(&ctx);

	EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_IVLEN, data.nonce_size, NULL);
	EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_TAG, data.tag_size, NULL);
	EVP_EncryptInit_ex(ctx, NULL, NULL, data.key, data.nonce);

	EVP_EncryptUpdate(ctx, NULL, &outlen, NULL, data.payload_size);
	if (data.adata != NULL)
		EVP_EncryptUpdate(ctx, NULL, &outlen, data.adata, data.adata_size);
	if (data.payload != NULL) {
		EVP_EncryptUpdate(ctx, outbuf, &outlen, data.payload, data.payload_size);
		printf("Ciphertext: ");
		print_hex(outbuf, outlen);
	} else {
		EVP_EncryptUpdate(ctx, outbuf, &outlen, "", data.payload_size);
	}

	EVP_EncryptFinal_ex(ctx, outbuf, &outlen);
	EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_GET_TAG, data.tag_size, outbuf);
	printf("Tags: ");
	print_hex(outbuf, data.tag_size);
	EVP_CIPHER_CTX_free(ctx);
	return 0;
}

int ccm_decrypt(void)
{
	EVP_CIPHER_CTX *ctx;
	int outlen, rv;
	unsigned char outbuf[1024];

	ctx = EVP_CIPHER_CTX_new();

	init_ccm_evp_decrypt(&ctx);

	EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_IVLEN, data.nonce_size, NULL);
	EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_TAG, data.tag_size, data.tag);
	EVP_DecryptInit_ex(ctx, NULL, NULL, data.key, data.nonce);
	EVP_DecryptUpdate(ctx, NULL, &outlen, NULL, data.ct_size);

	if (data.adata != NULL)
		EVP_DecryptUpdate(ctx, NULL, &outlen, data.adata, data.adata_size);

	if (data.ct == NULL)
		rv = EVP_DecryptUpdate(ctx, outbuf, &outlen, "", data.ct_size);
	else
		rv = EVP_DecryptUpdate(ctx, outbuf, &outlen, data.ct, data.ct_size);

	if (rv > 0) {
		printf("Tag Verify: Pass\n");
		if (data.ct != NULL) {
			printf("Plaintext: ");
			print_hex(outbuf, outlen);
		}
	} else {
		printf("Tag Verify: Fail\n");
	}

	EVP_CIPHER_CTX_free(ctx);
	return 0;
}
int ccm_uninit(void)
{
	free_openssl_data(data.key);
	free_openssl_data(data.nonce);
	free_openssl_data(data.adata);
	free_openssl_data(data.payload);
	free_openssl_data(data.ct);
	free_openssl_data(data.tag);
	return 0;
}

int ccm_check(void)
{
	if (data.key == NULL || data.nonce == NULL) {
		printf("ccm must have Key and IV\n");
		return -1;
	}

	if (input_data.oper == DECRYPT && data.tag == NULL) {
		printf("ccm decrypt must have Tag\n");
		return -1;
	}

	return 0;
}
