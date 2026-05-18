#include "aesgcm.h"
#include "openssl/crypto.h"

struct operator gcm_oper = {
	.init = gcm_init,
	.uninit = gcm_uninit,
	.encrypt = gcm_encrypt,
	.decrypt = gcm_decrypt,
	.check = gcm_check,
};

static struct gcm_data data;

int gcm_init(void)
{
	if (input_data.key == NULL || input_data.iv == NULL)
		return -1;
	hex2bin(&data.key, input_data.key, &(data.key_size));
	hex2bin(&data.iv, input_data.iv, &(data.iv_size));
	hex2bin(&data.pt, input_data.pt, &(data.pt_size));
	hex2bin(&data.add, input_data.add, &(data.add_size));
	hex2bin(&data.ct, input_data.ct, &(data.ct_size));
	hex2bin(&data.tag, input_data.tag, &(data.tag_size));

	if (input_data.tag_output_size != 0)
		data.tag_output_size = input_data.tag_output_size;
	else
		data.tag_output_size = 16;

	return 0;
}

void init_gcm_evp_encrypt(EVP_CIPHER_CTX **ctx)
{
	if (data.key_size == 16)
		EVP_EncryptInit_ex(*ctx, EVP_aes_128_gcm(), NULL, NULL, NULL);
	else if (data.key_size == 24)
		EVP_EncryptInit_ex(*ctx, EVP_aes_192_gcm(), NULL, NULL, NULL);
	else if (data.key_size == 32)
		EVP_EncryptInit_ex(*ctx, EVP_aes_256_gcm(), NULL, NULL, NULL);
}

void init_gcm_evp_decrypt(EVP_CIPHER_CTX **ctx)
{
	if (data.key_size == 16)
		EVP_DecryptInit_ex(*ctx, EVP_aes_128_gcm(), NULL, NULL, NULL);
	else if (data.key_size == 24)
		EVP_DecryptInit_ex(*ctx, EVP_aes_192_gcm(), NULL, NULL, NULL);
	else if (data.key_size == 32)
		EVP_DecryptInit_ex(*ctx, EVP_aes_256_gcm(), NULL, NULL, NULL);
}

int gcm_encrypt(void)
{
	EVP_CIPHER_CTX *ctx;
	int outlen;
	unsigned char outbuf[1024];

	ctx = EVP_CIPHER_CTX_new();

	init_gcm_evp_encrypt(&ctx);

	EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_IVLEN, data.iv_size, NULL);
	EVP_EncryptInit_ex(ctx, NULL, NULL, data.key, data.iv);

	if (data.add != NULL)
		EVP_EncryptUpdate(ctx, NULL, &outlen, data.add, data.add_size);
	if (data.pt != NULL) {
		EVP_EncryptUpdate(ctx, outbuf, &outlen, data.pt, data.pt_size);
		printf("Ciphertext: ");
		print_hex(outbuf, outlen);
	} else {
		printf("Ciphertext: none\n");
	}
	EVP_EncryptFinal_ex(ctx, outbuf, &outlen);
	EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_GET_TAG, data.tag_output_size, outbuf);

	printf("Tag: ");
	print_hex(outbuf, data.tag_output_size);

	EVP_CIPHER_CTX_free(ctx);
	return 0;
}

int gcm_decrypt(void)
{
	EVP_CIPHER_CTX *ctx;
	int outlen, tmplen, rv;
	unsigned char outbuf[1024];

	ctx = EVP_CIPHER_CTX_new();

	init_gcm_evp_decrypt(&ctx);

	EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_IVLEN, data.iv_size, NULL);
	EVP_DecryptInit_ex(ctx, NULL, NULL, data.key, data.iv);

	if (data.add != NULL)
		EVP_DecryptUpdate(ctx, NULL, &outlen, data.add, data.add_size);
	if (data.ct != NULL) {
		EVP_DecryptUpdate(ctx, outbuf, &outlen, data.ct, data.ct_size);
		printf("Plaintext: ");
		print_hex(outbuf, outlen);
	}

	EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_TAG, data.tag_size,
			data.tag);
	rv = EVP_DecryptFinal_ex(ctx, outbuf, &outlen);
	printf("Tag Verify %s\n", rv > 0 ? "Pass" : "Fail");
	EVP_CIPHER_CTX_free(ctx);
	return 0;
}

int gcm_uninit(void)
{
	free_openssl_data(data.key);
	free_openssl_data(data.iv);
	free_openssl_data(data.pt);
	free_openssl_data(data.add);
	free_openssl_data(data.ct);
	free_openssl_data(data.tag);
	return 0;
}

int gcm_check(void)
{
	if (data.key == NULL || data.iv == NULL) {
		printf("gcm must have Key and IV\n");
		return -1;
	}

	if (input_data.oper == DECRYPT && data.tag == NULL) {
		printf("gcm decrypt must have Tag\n");
		return -1;
	}

	return 0;
}
