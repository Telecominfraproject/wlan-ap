/*
Copyright (c) 2015, Plume Design Inc. All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
   1. Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
   2. Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
   3. Neither the name of the Plume Design Inc. nor the
      names of its contributors may be used to endorse or promote products
      derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL Plume Design Inc. BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include <stdint.h>

#ifndef __DNS_STR_UTILS__
#define __DNS_STR_UTILS__

/*
 * Encodes the data into plaintext (minus newlines and delimiters).  Escaped
 * characters are in the format \x33 (an ! in this case).  The escaped
 * characters are:
 * All characters < \x20
 *  Backslash (\x5c)
 *  All characters >= \x7f
 * Arguments (packet, start, end):
 *  packet - The uint8_t array of the whole packet.
 *  start - the position of the first character in the data.
 *  end - the position + 1 of the last character in the data.
 */
char *
escape_data(const uint8_t *, uint32_t, uint32_t);

/*
 * Read a reservation record style name, dealing with any compression.
 * A newly allocated string of the read name with length bytes
 * converted to periods is placed in the char * argument.
 * If there was an error reading the name, NULL is returned and the
 * position argument is left with it's passed value.
 * Args (packet, pos, id_pos, len, name)
 * packet - The uint8_t array of the whole packet.
 * pos - the start of the rr name.
 * id_pos - the start of the dns packet (id field)
 * len - the length of the whole packet
 * name - We will return read name via this pointer.
 */
char *
read_rr_name(const uint8_t *, uint32_t *, uint32_t, uint32_t);

char *
fail_name(const uint8_t *, uint32_t, uint32_t, const char *);

char *
b64encode(const uint8_t *, uint32_t, uint16_t);
#endif /* __DNS_STR_UTILS__ */
