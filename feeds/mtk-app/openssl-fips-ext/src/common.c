#include "common.h"
#include "aesgcm.h"
#include "aesccm.h"


struct common_data input_data = {
	.key = NULL,
	.iv  = NULL,
	.pt  = NULL,
	.add = NULL,
	.ct  = NULL,
	.tag = NULL,
	.nonce = NULL,
	.adata = NULL,
	.payload = NULL,
	.key_len = 0,
	.iv_len = 0,
	.pt_len = 0,
	.add_len = 0,
	.ct_len = 0,
	.tag_len = 0,
	.nonce_len = 0,
	.adata_len = 0,
	.payload_len = 0,
	.tag_output_size = 0,
	.algo = -1,
	.oper = -1,
};

ENGINE *setup_engine(void)
{
	ENGINE *e;

	OpenSSL_add_all_algorithms();
	ENGINE_load_builtin_engines();
	e = ENGINE_by_id("devcrypto");

	if (e == NULL) {
		printf("engine error\n");
		return NULL;
	}
	if (!ENGINE_init(e)) {
		printf("error2\n");
		ENGINE_free(e);
		return NULL;
	}
	return e;
}

void usage(void)
{
	printf(
		"gcm and gcm tool:\n"
		"gcm Operations:\n"
		"-e              - encrypt\n"
		"-d              - decrypt\n"
		"Common requirement parameters:\n"
		"-k key(hex)     - key in hex (must)\n"
		"-i iv(hex)      - initial vector in hex (must)\n"
		"-p plain (hex)  - plain text in hex\n"
		"-c cipher(hex)  - cipher text in hex\n"
		"-a aad(hex)     - additional authentication data in hex\n"
		"-t tag(hex)     - tag in hex (decrypt must)\n"
		"-g tag size(dec)- tag output size (default 16)\n"
		"ccm Operation:\n"
		"-k key(hex)     - key in hex (must)\n"
		"-n nonce(hex)   - nonce in hex (must)\n"
		"-f adata(hex)   - adata in hex\n"
		"-l payload(hex) - payload in hex\n"
		"-t encrypt size - tag size (must)\n"
		"   decrypt tag  - tag (must)\n"
		"tools: gcm or ccm\n"
		"example:\n"
		"gcm encrypt ./openssl-fips-ext -e -k data -i data ... gcm\n"
		"gcm decrypt ./openssl-fips-ext -d -k data -i data -t data ... gcm\n"
		"gcm encrypt ./openssl-fips-ext -e -k data -n data -t size ... ccm\n"
		"gcm encrypt ./openssl-fips-ext -d -k data -n data -t data ... ccm\n");
}

void init_algo_data(void)
{
	if (input_data.algo == GCM)
		cur.oper = gcm_oper;
	else if (input_data.algo == CCM)
		cur.oper = ccm_oper;
}

void do_operation(void)
{
	cur.oper.init();

	if (cur.oper.check()) {
		printf("Input error\n");
		return;
	}

	if (input_data.oper == ENCRYPT)
		cur.oper.encrypt();
	else if (input_data.oper == DECRYPT)
		cur.oper.decrypt();

	cur.oper.uninit();
}

void hex2bin(unsigned char **des, char *src, long *len)
{
	if (src != NULL) {
		*des = OPENSSL_hexstr2buf(src, len);
		if (*des == NULL)
			printf("openssl str to buf error\n");
	} else {
		*des = NULL;
		*len = 0;
	}
}

void print_hex(unsigned char *str, int len)
{
	for (int i = 0; i < len; i++)
		printf("%02x", str[i]);
	printf("\n");
}

void free_openssl_data(unsigned char *data)
{
	if (data != NULL)
		OPENSSL_free(data);
}
