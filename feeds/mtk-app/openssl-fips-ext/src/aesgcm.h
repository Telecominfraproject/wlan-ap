#ifndef AESGCM_H
#define AESGCM_H
#include "common.h"
int gcm_init(void);
int gcm_encrypt(void);
int gcm_decrypt(void);
int gcm_uninit(void);
int gcm_check(void);

struct gcm_data {
	unsigned char *key;
	unsigned char *iv;
	unsigned char *pt;
	unsigned char *add;
	unsigned char *ct;
	unsigned char *tag;
	long key_size;
	long iv_size;
	long pt_size;
	long add_size;
	long ct_size;
	long tag_size;
	int tag_output_size;
};

extern struct operator gcm_oper;

#endif
