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

#include <pcap.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

char *
escape_data(const uint8_t * packet, uint32_t start, uint32_t end)
{
    uint32_t i;
    int o;
    uint8_t c;
    unsigned int length=1;
    char * outstr;

    for (i = start; i < end; i++)
    {
        c = packet[i];
        if (c < 0x20 || c == 0x5c || c >= 0x7f) length += 4;
        else length += 1;
    }

    outstr = (char *)malloc(sizeof(char)*length);
    /* If the malloc failed then fail. */
    if (outstr == 0) return (char *)0;

    o = 0;
    for (i = start; i < end; i++)
    {
        c = packet[i];
        if (c < 0x20 || c == 0x5c || c >= 0x7f)
        {
            outstr[o] = '\\';
            outstr[o+1] = 'x';
            outstr[o+2] = c/16 + 0x30;
            outstr[o+3] = c%16 + 0x30;
            if (outstr[o+2] > 0x39) outstr[o+2] += 0x27;
            if (outstr[o+3] > 0x39) outstr[o+3] += 0x27;
            o += 4;
        }
        else
        {
            outstr[o] = c;
            o++;
        }
    }
    outstr[o] = 0;
    return outstr;
}

char *
read_rr_name(const uint8_t * packet, uint32_t * packet_p,
             uint32_t id_pos, uint32_t len)
{
    uint32_t i, next, pos=*packet_p;
    uint32_t end_pos = 0;
    uint32_t name_len=0;
    uint32_t steps = 0;
    char * name;

    /*
     * Scan through the name, one character at a time. We need to look at
     * each character to look for values we can't print in order to allocate
     * extra space for escaping them.  'next' is the next position to look
     * for a compression jump or name end.
     * It's possible that there are endless loops in the name. Our protection
     * against this is to make sure we don't read more bytes in this process
     * than twice the length of the data.  Names that take that many steps to
     * read in should be impossible.
     */
    next = pos;
    while (pos < len && !(next == pos && packet[pos] == 0)
           && steps < len*2)
    {
        uint8_t c = packet[pos];
        steps++;
        if (next == pos)
        {
            /*
             * Handle message compression.
             * If the length byte starts with the bits 11, then the rest of
             * this byte and the next form the offset from the dns proto start
             * to the start of the remainder of the name.
             */
            if ((c & 0xc0) == 0xc0)
            {
                if (pos + 1 >= len) return 0;
                if (end_pos == 0) end_pos = pos + 1;
                pos = id_pos + ((c & 0x3f) << 8) + packet[pos+1];
                next = pos;
            }
            else
            {
                name_len++;
                pos++;
                next = next + c + 1;
            }
        }
        else
        {
            if (c >= '!' && c <= 'z' && c != '\\') name_len++;
            else name_len += 4;
            pos++;
        }
    }
    if (end_pos == 0) end_pos = pos;

    /*
     * Due to the nature of DNS name compression, it's possible to get a
     * name that is infinitely long. Return an error in that case.
     * We use the len of the packet as the limit, because it shouldn't
     * be possible for the name to be that long.
     */
    if (steps >= 2*len || pos >= len) return NULL;

    name_len++;

    name = (char *)malloc(sizeof(char) * name_len);
    pos = *packet_p;

    /*
     * Now actually assemble the name.
     * We've already made sure that we don't exceed the packet length, so
     * we don't need to make those checks anymore.
     * Non-printable and whitespace characters are replaced with a question
     * mark. They shouldn't be allowed under any circumstances anyway.
     * Other non-allowed characters are kept as is, as they appear sometimes
     * regardless.
     * This shouldn't interfere with IDNA (international
     * domain names), as those are ascii encoded.
     */
    next = pos;
    i = 0;
    while (next != pos || packet[pos] != 0)
    {
        if (pos == next)
        {
            if ((packet[pos] & 0xc0) == 0xc0)
            {
                pos = id_pos + ((packet[pos] & 0x3f) << 8) + packet[pos+1];
                next = pos;
            }
            else
            {
                /* Add a period except for the first time. */
                if (i != 0) name[i++] = '.';
                next = pos + packet[pos] + 1;
                pos++;
            }
        }
        else
        {
            uint8_t c = packet[pos];
            if (c >= '!' && c <= '~' && c != '\\')
            {
                name[i] = packet[pos];
                i++; pos++;
            }
            else
            {
                name[i] = '\\';
                name[i+1] = 'x';
                name[i+2] = c/16 + 0x30;
                name[i+3] = c%16 + 0x30;
                if (name[i+2] > 0x39) name[i+2] += 0x27;
                if (name[i+3] > 0x39) name[i+3] += 0x27;
                i+=4;
                pos++;
            }
        }
    }
    name[i] = 0;

    *packet_p = end_pos + 1;

    return name;
}


static const char cb64[]="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

char * b64encode(const uint8_t * data, uint32_t pos, uint16_t length) {
    char * out;
    uint32_t end_pos = pos + length;
    uint32_t op = 0;

    /*
     * We allocate a little extra here sometimes, but in this application
     * these strings are almost immediately de-allocated anyway.
     */
    out = malloc(sizeof(char) * ((length/3 + 1)*4 + 1));

    while (pos + 2 < end_pos)
    {
        out[op] = cb64[ data[pos] >> 2 ];
        out[op+1] = cb64[ ((data[pos] & 0x3) << 4) |
                          ((data[pos+1] & 0xf0) >> 4) ];
        out[op+2] = cb64[ ((data[pos+1] & 0xf) << 2) |
                          ((data[pos+2] & 0xc0) >> 6) ];
        out[op+3] = cb64[ data[pos+2] & 0x3f ];

        op = op + 4;
        pos = pos + 3;
    }

    if ((end_pos - pos) == 2)
    {
        out[op] = cb64[ data[pos] >> 2 ];
        out[op+1] = cb64[ ((data[pos] & 0x3) << 4) |
                          ((data[pos+1] & 0xf0) >> 4) ];
        out[op+2] = cb64[ ((data[pos+1] & 0xf) << 2) ];
        out[op+3] = '=';
        op = op + 4;
    }
    else if ((end_pos - pos) == 1)
    {
        out[op] = cb64[ data[pos] >> 2 ];
        out[op+1] = cb64[ ((data[pos] & 0x3) << 4) ];
        out[op+2] = out[op+3] = '=';
        op = op + 4;
    }
    out[op] = 0;

    return out;
}
