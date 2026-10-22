#ifndef COMMON_H
#define COMMON_H
#include <stddef.h>
#define DECRYPT 0
#define ENCRYPT 1
#define GCM 0
#define CCM 1

#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/engine.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>
#include <openssl/pem.h>
#include <openssl/pkcs12.h>
#include <openssl/safestack.h>
#include <openssl/err.h>
#include <openssl/bn.h>
#include <openssl/ssl.h>

ENGINE *setup_engine(void);
void init_alog_data(void);
void usage(void);
void init_algo_data(void);
void do_operation(void);
void hex2bin(unsigned char **des, char *src, long *len);
void print_hex(unsigned char *str, int len);
void free_openssl_data(unsigned char *data);

struct common_data {
	char *key;
	char *iv;
	char *pt;
	char *add;
	char *ct;
	char *tag;
	char *nonce;
	char *adata;
	char *payload;
	int key_len;
	int iv_len;
	int pt_len;
	int add_len;
	int ct_len;
	int tag_len;
	int nonce_len;
	int adata_len;
	int payload_len;
	int algo;
	int oper;
	int tag_output_size;
};

struct operator {
	int (*init)(void);
	int (*uninit)(void);
	int (*encrypt)(void);
	int (*decrypt)(void);
	int (*check)(void);
};

struct algorithm_data {
	struct operator oper;
	void *data;
} cur;

extern struct common_data input_data;

#endif
