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

#ifndef READ_UNTIL_H_INCLUDED
#define READ_UNTIL_H_INCLUDED

#include <stdlib.h>
#include <string.h>

/**
 * This is quite similar to fgets() except that it works with non-blocking I/O
 */
typedef struct __read_until read_until_t;

struct __read_until
{
    char        *buf;
    ssize_t     bufsz;
    char        *head;
    char        *tail;
};

#define READ_UNTIL_INIT(b, bsz)  \
{                                       \
    .buf = (b),                         \
    .bufsz = (bsz),                     \
    .head = (b),                        \
    .tail = (b)                         \
}

static inline void read_until_init(read_until_t *self, char *buf, ssize_t bufsz)
{
    read_until_t tmp = READ_UNTIL_INIT(buf, bufsz);

    memcpy(self, &tmp, sizeof(*self));
}

extern ssize_t read_until(read_until_t *self, char **out, int fd, char *dm);

#endif /* READ_UNTIL_H_INCLUDED */
