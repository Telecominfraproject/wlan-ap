/* SPDX-License-Identifier: BSD-3-Clause */

#ifndef COMMON_H
#define COMMON_H

/********** List of all the Headers *******************/
#include <endian.h>
#include <byteswap.h>
#include <arpa/inet.h>

/***********************************************************************/
/* ******************** ENUM TYPEDEFS *********************************  */
typedef unsigned long long u64;
typedef unsigned int u32;
typedef signed int s32;
typedef unsigned short u16;
typedef short s16;
typedef unsigned char u8;
typedef signed char s8;

/***********************************************************************/
/* ********************** DEFINES ********************************** */
#if __BYTE_ORDER == __LITTLE_ENDIAN
#define le_to_host16(n) (n)
#define host_to_le16(n) (n)
#define be_to_host16(n) bswap_16(n)
#define host_to_be16(n) bswap_16(n)
#define le_to_host64(n) (n)
#define host_to_le64(n) (n)
#define be_to_host64(n) bswap_64(n)
#define host_to_be64(n) bswap_64(n)
#else
#define le_to_host16(n) bswap_16(n)
#define host_to_le16(n) bswap_16(n)
#define be_to_host16(n) (n)
#define host_to_be16(n) (n)
#define le_to_host64(n) bswap_64(n)
#define host_to_le64(n) bswap_64(n)
#define be_to_host64(n) (n)
#define host_to_be64(n) (n)
#endif

/***********************************************************************/
/* *************************  Structures ************************** */

struct hostapd_ip_addr 
{
  union 
  {
  	struct in_addr v4;
  	struct in6_addr v6;
  } u;
  int af; /* AF_INET / AF_INET6 */
};

/***********************************************************************/
/* ************************ Prototype Definitions **************************/

int hostapd_get_rand(u8 *buf, size_t len);
void hostapd_hexdump(const char *title, u8 *buf, size_t len);
int hostapd_hex2num8(const char *hex);
u8 * hostapd_hex_str(const char *hex, size_t *len);
int hwaddr_aton(char *txt, u8 *addr);
void hostapd_counter_inc(u8 *counter, size_t len);

static inline void print_char(char c)
{
  if (c >= 32 && c < 127)
  	printf("%c", c);
  else
  	printf("<%02x>", c);
}


static inline void fprint_char(FILE *f, char c)
{
  if (c >= 32 && c < 127)
  	fprintf(f, "%c", c);
  else
  	fprintf(f, "<%02x>", c);
}

/***********************************************************************/
#endif /* COMMON_H */
