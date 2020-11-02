/* SPDX-License-Identifier: BSD-3-Clause */
#ifndef _APC_IP_H_
#define _APC_IP_H_

#include "lib/endian.h"
#include "lib/bitops.h"


#define IP4_NONE		_MI4(0)


typedef u32 ip4_addr;

#define _MI4(x) (x)
#define _I(x) (x)


/* Provisionary ip_addr definition same as ip4_addr */
typedef ip4_addr ip_addr;
#define IPA_NONE IP4_NONE

#define ipa_to_ip4(x) x
#define ipa_to_u32(x) ip4_to_u32(ipa_to_ip4(x))

/*
 *	Public constructors
 */

#define ip4_to_u32(x) _I(x)

/*
 *	Basic algebraic functions
 */

static inline int ip4_equal(ip4_addr a, ip4_addr b)
{ return _I(a) == _I(b); }

static inline int ip4_nonzero(ip4_addr a)
{ return _I(a) != 0; }

#define ipa_equal(x,y) ip4_equal(x,y)
#define ipa_nonzero(x) ip4_nonzero(x)

/*
 *	Miscellaneous
 */

struct prefix {
  ip_addr addr;
  unsigned int len;
};


#endif
