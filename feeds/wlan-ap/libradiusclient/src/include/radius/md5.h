/* SPDX-License-Identifier: BSD-3-Clause */
#ifndef MD5_H
#define MD5_H

/********** List of all the Headers *******************/

/***********************************************************************/
/* ******************** ENUM TYPEDEFS *********************************  */
typedef struct MD5Context MD5_CTX;

/***********************************************************************/
/* ********************** DEFINES ********************************** */
#define MD5_MAC_LEN 16

/***********************************************************************/
/* *************************  Structures ************************** */
struct MD5Context 
{
  u32 buf[4];
  u32 bits[2];
  u8 in[64];
};

/***********************************************************************/
/* ************************ Prototype Definitions **************************/

void MD5Init(struct MD5Context *context);
void MD5Update(struct MD5Context *context, unsigned char const *buf,
	       unsigned len);
void MD5Final(unsigned char digest[16], struct MD5Context *context);
void MD5Transform(u32 buf[4], u32 const in[16]);
void md5_mac(u8 *key, size_t key_len, u8 *data, size_t data_len, u8 *mac);
void hmac_md5(u8 *key, size_t key_len, u8 *data, size_t data_len, u8 *mac);

void md5_vector(size_t num_elem, const u8 *addr[], const size_t *len, u8 *mac);

/***********************************************************************/
#endif /* MD5_H */
