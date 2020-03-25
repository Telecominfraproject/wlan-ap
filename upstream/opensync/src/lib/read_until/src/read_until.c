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

#include <stdlib.h>
#include <unistd.h>

#include "read_until.h"

/**
 * Read data from file descriptor @p fd and split the stream
 * into strings delimited by @dm
 */
ssize_t read_until(read_until_t *self, char **out, int fd, char *dm)
{
    ssize_t nrd;
    size_t ns;

    if (self->head == NULL) self->head = self->buf;
    if (self->tail == NULL) self->tail = self->buf;

    do
    {
        self->tail[0] = '\0';

        /* Find first occurence of a delimiter character */
        ns = strcspn(self->head, dm);
        if (self->head[ns] != '\0')
        {
            /* Delimiter found -- Pad it with 0 and return the number of bytes in the strnig buffer, including '\0' */
            self->head[ns] = '\0';
            *out = self->head;
            self->head += ns + 1;
            return ns + 1;
        }

        /* Is the buffer full? */
        if ((self->tail - self->head) >= (self->bufsz - 1)) break;

        /* Shift data to the beginning of the buffer */
        memmove(self->buf, self->head, self->tail - self->head);
        self->tail -= self->head - self->buf;
        self->head = self->buf;

        /* Top the input buffer, make sure to leave room for a terminator */
        nrd = read(fd, self->tail, self->bufsz - (self->tail - self->buf) - 1);
        if (nrd <= 0)
        {
            /* Delay EOF until the buffer is fully exhausted */
            if (nrd == 0 && (self->tail - self->head) > 0) break;
            return nrd;
        }

        self->tail += nrd;
    }
    while (nrd > 0);

    /*
     * Return the remainder of the buffer, this may happen for two reasons:
     *  1) the buffer is full and a delimiter was not found
     *  2) EOF was received from the file descriptor and there's still data in the buffer
     */
    *out = self->head;

    nrd = self->tail - self->head;
    self->head = self->tail;

    return nrd + 1;     /* Include the terminator in the size */
}

