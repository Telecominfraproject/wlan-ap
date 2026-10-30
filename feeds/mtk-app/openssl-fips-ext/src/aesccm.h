#ifndef AESCCM_H
#define AESCCM_H
#include "common.h"
int ccm_init(void);
int ccm_uninit(void);
int ccm_encrypt(void);
int ccm_decrypt(void);
int ccm_check(void);

struct ccm_data {
	unsigned char *key;
	unsigned char *nonce;
	unsigned char *adata;
	unsigned char *payload;
	unsigned char *ct;
	unsigned char *tag;
	long key_size;
	long nonce_size;
	long adata_size;
	long payload_size;
	long ct_size;
	int tag_output_size;
	long tag_size;
};

extern struct operator ccm_oper;

#endif
