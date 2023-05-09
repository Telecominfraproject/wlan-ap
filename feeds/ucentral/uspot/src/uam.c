/* SPDX-License-Identifier: ISC */

/* Copyright (C) 2022 John Crispin <john@phrozen.org> */

#include <ucode/module.h>

#include <stdio.h>
#include <string.h>

#include <libubox/md5.h>

#define MIN(a, b)	((a > b) ? b : a)

/**
 * Convert a string of hex bytes into the equivalent null-terminated character string.
 * @param in null-terminated input hex string buffer
 * @param out output buffer
 * @param osize output buffer size
 * @return number of characters decoded
 * @note if osize is <= strlen(in)/2, the output will be truncated (null-terminated).
 * @warning no input sanitization is performed: in must be null-terminated;
 * the resulting output string may contain non-representable characters.
 */
static int
str_to_hex(const char *in, char *out, int osize)
{
	int ilen = strlen(in);
	int i;

	for (i = 0; (i < ilen/2) && (i < osize - 1); i++) {
		if (sscanf(&in[i * 2], "%2hhx", &out[i]) != 1)
			break;	// truncate output on scan errors
	}

	out[i] = '\0';
	return i;
}

/**
 * Convert a byte input buffer to null-terminated hex string representation.
 * @param in input bytes buffer
 * @param out output buffer
 * @param len number of input bytes to convert
 * @warning output buffer size must be 2*len+1 or there will be a buffer overflow.
 */
static void
hex_to_str(const void *in, char *out, int len)
{
	const unsigned char *hex = in;
	int i;

	for (i = 0; i < len; i++)
		snprintf(&out[i * 2], 3, "%02hhX", hex[i]);
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

	plen = str_to_hex(ucv_to_string(vm, _pass), pass, sizeof(pass));
	clen = str_to_hex(ucv_to_string(vm, _chal), chal, sizeof(chal));
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
uc_challenge(uc_vm_t *vm, size_t nargs)
{
	uc_value_t *_chal = uc_fn_arg(0);
	uc_value_t *_secret = uc_fn_arg(1);
	char chal[32];
	char uamchal[32];
	char ret[33] = {};
	int clen;
	char *secret;
	md5_ctx_t md5 = {};

	if (!_chal || !_secret)
		return ucv_boolean_new(false);

	clen = str_to_hex(ucv_to_string(vm, _chal), chal, sizeof(chal));
	secret = ucv_to_string(vm, _secret);

	md5_begin(&md5);
	md5_hash(chal, clen, &md5);
	md5_hash(secret, strlen(secret), &md5);
	md5_end(uamchal, &md5);

	hex_to_str(uamchal, ret, sizeof(uamchal)/2);
	return ucv_string_new(ret);

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

	if (!_str || !_secret)
		return ucv_boolean_new(false);

	str = ucv_to_string(vm, _str);
	secret = ucv_to_string(vm, _secret);

	md5_begin(&md5);
	md5_hash(str, strlen(str), &md5);
	md5_hash(secret, strlen(secret), &md5);
	md5_end(_md, &md5);

	hex_to_str(_md, md, sizeof(_md)/2);
	return ucv_string_new(md);
}

static const uc_function_list_t global_fns[] = {
        { "password",	uc_password },
        { "chap_challenge",	uc_challenge },
        { "md5",	uc_md5 },
};

void
uc_module_init(uc_vm_t *vm, uc_value_t *scope)
{
        uc_function_list_register(scope, global_fns);
}
