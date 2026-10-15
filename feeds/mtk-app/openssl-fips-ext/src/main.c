/*
 * Copyright 2012-2019 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the OpenSSL license (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <ctype.h>
#include <errno.h>
#include <unistd.h>

#include "common.h"
#include "aesgcm.h"
#include "aesccm.h"

int main(int argc, char **argv)
{
	ENGINE *e;
	int opt;

	e = setup_engine();
	if (e == NULL) {
		printf("engine set error\n");
		return 0;
	}

	while ((opt = getopt(argc, argv, "a:c:def:i:k:l:n:p:t:g:")) > 0) {
		switch (opt) {
		case 'a':
			input_data.add = optarg;
			input_data.add_len = strlen(optarg);
			break;
		case 'd':
			input_data.oper = DECRYPT;
			break;
		case 'e':
			input_data.oper = ENCRYPT;
			break;
		case 'i':
			input_data.iv = optarg;
			input_data.iv_len = strlen(optarg);
			break;
		case 'k':
			input_data.key = optarg;
			input_data.key_len = strlen(optarg);
			if (!(input_data.key_len == 32 || input_data.key_len == 48
					|| input_data.key_len == 64)) {
				printf("Key size must be 128, 192 or 256 .\n");
				return 0;
			}
		case 'n':
			input_data.nonce = optarg;
			input_data.nonce_len = strlen(optarg);
			break;
		case 'c':
			input_data.ct = optarg;
			input_data.ct_len = strlen(optarg);
			break;
		case 't':
			input_data.tag = optarg;
			input_data.tag_len = strlen(optarg);
			break;
		case 'f':
			input_data.adata = optarg;
			input_data.adata_len = strlen(optarg);
			break;
		case 'l':
			input_data.payload = optarg;
			input_data.payload_len = strlen(optarg);
			break;
		case 'p':
			input_data.pt = optarg;
			input_data.pt_len = strlen(optarg);
			break;
		case 'g':
			input_data.tag_output_size = atoi(optarg);
			break;
		case '?':
			usage();
		default:
			break;
		}

	}
	if (argc > optind) {
		if (strncmp("gcm", argv[optind], 3) == 0) {
			input_data.algo = GCM;
		} else if (strncmp("ccm", argv[optind], 3) == 0) {
			input_data.algo = CCM;
		} else {
			printf("parameter error !!\n");
			usage();
			return 0;
		}
	} else {
		printf("select algorithm gcm or ccm\n");
		usage();
		return 0;
	}
	init_algo_data();
	do_operation();

	return 0;
}
