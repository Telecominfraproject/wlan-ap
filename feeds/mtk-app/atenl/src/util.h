/* Copyright (C) 2021-2022 Mediatek Inc. */
#ifndef __ATENL_UTIL_H
#define __ATENL_UTIL_H

#include <linux/const.h>
#include <linux/if_arp.h>
#include <linux/if_ether.h>
#include <linux/if_packet.h>
#include <stdint.h>
#include <string.h>

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int8_t s8;
typedef int16_t s16;
typedef int32_t s32;
typedef int64_t s64, ktime_t;

#ifndef __WORDSIZE
#define __WORDSIZE (__SIZEOF_LONG__ * 8)
#endif

#ifndef BITS_PER_LONG
#define BITS_PER_LONG __WORDSIZE
#endif

#define UL(x)		(_UL(x))
#define ULL(x)		(_ULL(x))

#define BIT(nr)		(1UL << (nr))

#define GENMASK_INPUT_CHECK(h, l) 0
#define __GENMASK(h, l) \
	(((~UL(0)) - (UL(1) << (l)) + 1) & \
	 (~UL(0) >> (BITS_PER_LONG - 1 - (h))))
#define GENMASK(h, l) \
	(GENMASK_INPUT_CHECK(h, l) + __GENMASK(h, l))

#define __bf_shf(x) (__builtin_ffsll(x) - 1)
#define FIELD_GET(_mask, _reg)						\
	({								\
		(typeof(_mask))(((_reg) & (_mask)) >> __bf_shf(_mask));	\
	})
#define FIELD_PREP(_mask, _val)						\
	({								\
		((typeof(_mask))(_val) << __bf_shf(_mask)) & (_mask);	\
	})

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(a) (sizeof(a) / sizeof(a[0]))
#endif

#ifndef DIV_ROUND_UP
#define DIV_ROUND_UP(n, d) (((n) + (d) - 1) / (d))
#endif

#define PIPE_READ 0
#define PIPE_WRITE 1

#define MAC2STR(a) (a)[0], (a)[1], (a)[2], (a)[3], (a)[4], (a)[5]
#define MACSTR "%02x:%02x:%02x:%02x:%02x:%02x"

static inline bool ether_addr_equal(const u8 *addr1, const u8 *addr2)
{
	const u16 *a = (const u16 *)addr1;
	const u16 *b = (const u16 *)addr2;

	return ((a[0] ^ b[0]) | (a[1] ^ b[1]) | (a[2] ^ b[2])) == 0;
}

static inline bool is_broadcast_ether_addr(const u8 *addr)
{
	return (*(const u16 *)(addr + 0) &
		*(const u16 *)(addr + 2) &
		*(const u16 *)(addr + 4)) == 0xffff;
}

static inline bool is_multicast_ether_addr(const u8 *addr)
{
	return 0x01 & addr[0];
}

static inline bool is_unicast_ether_addr(const u8 *addr)
{
	return !is_multicast_ether_addr(addr);
}

static inline bool is_zero_ether_addr(const u8 *addr)
{
	return (*(const u16 *)(addr + 0) |
		*(const u16 *)(addr + 2) |
		*(const u16 *)(addr + 4)) == 0;
}

static inline bool use_default_addr(const u8 *addr)
{
	return !is_unicast_ether_addr(addr) ||
	       is_zero_ether_addr(addr);
}

static inline void eth_broadcast_addr(u8 *addr)
{
	memset(addr, 0xff, ETH_ALEN);
}

static inline void ether_addr_copy(u8 *dst, const u8 *src)
{
	u16 *a = (u16 *)dst;
	const u16 *b = (const u16 *)src;

	a[0] = b[0];
	a[1] = b[1];
	a[2] = b[2];
}

static inline int snprintf_error(size_t size, int res)
{
	return res < 0 || (unsigned int) res >= size;
}

#endif
