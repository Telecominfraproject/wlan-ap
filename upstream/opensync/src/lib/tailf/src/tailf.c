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

#include <stdbool.h>

#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include "os.h"
#include "log.h"
#include "tailf.h"

static bool PRIV(tailf_init)(tailf_t *self);
static void PRIV(tailf_end)(tailf_t *self);

/**
 * Initialize tailf structure
 */
void tailf_open_seek(tailf_t * self, const char * path, int seek_to_end, int ret_trunc)
{
    memset(self, 0, sizeof(*self));

    self->tf_path = path;
    self->tf_fd = -1;
    self->tf_ret_trunc = ret_trunc;
    if (PRIV(tailf_init)(self))
    {
        /* Seek to end of file */
        if(seek_to_end)
        {
            lseek(self->tf_fd, 0, SEEK_END);
        }
    }
}

void tailf_open(tailf_t *self, const char *path)
{
    tailf_open_seek(self, path, 1, 0);
}

void tailf_close(tailf_t *self)
{
    PRIV(tailf_end)(self);
}

/**
 * Private function -- open the destination tail path; if it is already open
 * then do nothing.
 */
bool PRIV(tailf_init)(tailf_t *self)
{
    /* Is path already open? */
    if (self->tf_fd >= 0) return true;

    self->tf_off = 0;

    /* Stat the file */
    if (stat(self->tf_path, &self->tf_stat) != 0)
    {
        /* File might not exist -- this is a temporary error */
        return false;
    }

    self->tf_fd = open(self->tf_path, O_RDONLY);
    if (self->tf_fd < 0)
    {
        return false;
    }

    return true;
}

void PRIV(tailf_end)(tailf_t *self)
{
    if (self->tf_fd > 0) close(self->tf_fd);
    self->tf_fd = -1;
}

/* Get current file inode. It can be used to track whether file was truncated */
ino_t tailf_get_inode(tailf_t *self)
{
    if(self->tf_fd < 0)
        return 0;
    else
        return self->tf_stat.st_ino;
}

/**
 * This function returns the number of bytes read or <0 on error. It can produce shorter than bufsz reads, however it tries hard to
 * keep the buffer as full as possible even when the data crosses files (log rotation).
 *
 * If 0 is returned, it means there's no more data or to retry later.
 */
ssize_t tailf_read(tailf_t *self, void *buf, ssize_t bufsz)
{
    struct stat st;
    ssize_t     nr;

    uint8_t    *pbuf = (uint8_t *)buf;
    ssize_t     total_nr = 0;

    while (total_nr < bufsz)
    {
        if (!PRIV(tailf_init)(self))
        {
            LOG(DEBUG, "TAILF: File does not exist: %s", self->tf_path);
            /* If we're unable to open the target path, treat is a temporary error */
            return 0;
        }

        /* Stat the file and check if it was truncated */
        if (fstat(self->tf_fd, &st) != 0)
        {
            return -1;
        }

        /*
         * Check for file truncation -- the size of the file has shrunk
         * file was probably truncated.
         */
        if (st.st_size  < self->tf_off)
        {
            LOG(DEBUG, "TILF: File truncated: %d < %d: %s\n",
                    (int)st.st_size, (int)self->tf_off, self->tf_path);

            self->tf_off = 0;
            /* Seek to beginning of the file */
            if (lseek(self->tf_fd, 0, SEEK_SET) < 0)
            {
                return -1;
            }
            if(!self->tf_ret_trunc)
            {
                continue;
            }

        }

        /* Read as much as possible */
        nr = read(self->tf_fd, pbuf + total_nr, bufsz - total_nr);
        if (nr < 0)
        {
            /* Error reading file -- what to do? Return hard error? */
            return -1;
        }

        /* Update the variables tracking the amount of data read */
        self->tf_off += nr;
        total_nr += nr;

        if (total_nr >= bufsz)
        {
            /* Buffer is full, return it */
            break;
        }

        /*
         * Short read, we're at the current EOF -- check what happened with the target path.
         */
        if (stat(self->tf_path, &st) != 0)
        {
            /* Target path was deleted -- this is all the data we have, close the file and return */
            LOG(DEBUG, "TAILF: Deleted: %s", self->tf_path);
            PRIV(tailf_end)(self);
            break;
        }

        /* Compare inode numbers */
        if (st.st_ino != self->tf_stat.st_ino ||
            st.st_dev != self->tf_stat.st_dev)
        {
            /* File was rotated, close current file and restart the loop so we get new loop */
            LOG(DEBUG, "TAILF: Moved: %s", self->tf_path);
            /*   File was rotated, close current file and restart the loop */
            PRIV(tailf_end)(self);

            /*If return on truncate is set, return currenly read bytes from previous file*/
            if(!self->tf_ret_trunc)
            {
                continue;
            }
        }

        /* We're at the EOF of the current file and there's no new file to read -- break out */
        break;
    }

    return total_nr;
}
