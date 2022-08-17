/* SPDX-License-Identifier: ISC */

/* Copyright (C) 2022 John Crispin <john@phrozen.org> */

#include <ucode/module.h>

#include <stdio.h>
#include <string.h>

#include <libubox/md5.h>

#define MIN(a, b)	((a > b) ? b : a)

static int
hex_to_str(char *in, char *out, int olen)
{
	int ilen = strlen(in);
	int len = 0;

	while (ilen >= 2 && olen > 1) {
		int c;
		sscanf(in, "%2x", &c);
		*out++ = (char) c;

		in += 2;
		ilen -= 2;
		len++;
	}
	*out = '\0';

	return len;
}

static uc_value_t *
uc_password(uc_vm_t *vm, size_t nargs)
{
	uc_value_t *_chal = uc_fn_arg(0);
	uc_value_t *_pass = uc_fn_arg(1);
	uc_value_t *_secret = uc_fn_arg(2);
	char pass[32];
	char chal[32];
	char uamchal[32];
	char cleartext[32] = {};
	int plen;
	int clen;
	char *secret;
	int i;
	md5_ctx_t md5 = {};

	if (!_chal || !_pass || !_secret)
		return ucv_boolean_new(false);

	plen = hex_to_str(ucv_to_string(vm, _pass), pass, sizeof(pass));
	clen = hex_to_str(ucv_to_string(vm, _chal), chal, sizeof(chal));
	secret = ucv_to_string(vm, _secret);

	md5_begin(&md5);
	md5_hash(chal, clen, &md5);
	md5_hash(secret, strlen(secret), &md5);
	md5_end(uamchal, &md5);

	for (i = 0; i < MIN(plen, 16); i++)
		cleartext[i] = pass[i] ^ uamchal[i];

	return ucv_string_new(cleartext);

}

static uc_value_t *
uc_md5(uc_vm_t *vm, size_t nargs)
{
	uc_value_t *_str = uc_fn_arg(0);
	uc_value_t *_secret = uc_fn_arg(1);
	char _md[32] = {};
	char md[33] = {};
	char *secret;
	char *str;
	md5_ctx_t md5 = {};
	int i = 0;

	if (!_str || !_secret)
		return ucv_boolean_new(false);

	str = ucv_to_string(vm, _str);
	secret = ucv_to_string(vm, _secret);

	md5_begin(&md5);
	md5_hash(str, strlen(str), &md5);
	md5_hash(secret, strlen(secret), &md5);
	md5_end(_md, &md5);

	for (i = 0; i < 16; i++)
		sprintf(&md[2 * i], "%02X", _md[i]);

	return ucv_string_new(md);
}

static const uc_function_list_t global_fns[] = {
        { "password",	uc_password },
        { "md5",	uc_md5 },
};

void
uc_module_init(uc_vm_t *vm, uc_value_t *scope)
{
        uc_function_list_register(scope, global_fns);
}
