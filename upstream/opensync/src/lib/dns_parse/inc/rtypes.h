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

#ifndef __RTYPES_H__
#define __RTYPES_H__

#include <pcap.h>
#include <stdint.h>
#include <stdbool.h>

typedef char * rr_data_parser(const uint8_t*, uint32_t, uint32_t,
                              uint16_t, uint32_t);

typedef struct
{
    uint16_t cls;
    uint16_t rtype;
    rr_data_parser * parser;
    const char * name;
    const char * doc;
    unsigned long long count;
} rr_parser_container;

rr_parser_container *
find_parser(uint16_t, uint16_t);

char *
read_dns_name(uint8_t *, uint32_t, uint32_t);

rr_data_parser opts;
rr_data_parser escape;

extern rr_parser_container rr_parsers[];

void
print_parsers(void);

void
print_parser_usage(void);

bool
is_default_rr_parser(rr_parser_container *parser);

#endif
