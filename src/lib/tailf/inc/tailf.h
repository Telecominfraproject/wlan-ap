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

#ifndef TAILF_H_INCLUDED
#define TAILF_H_INCLUDED

#include <sys/types.h>
#include <sys/stat.h>

/*
 * ===========================================================================
 *  tailf -- library for incrementally reading files a-la tail -f. The library
 *  follows file paths, hence it's suitable for tailing rotating logs.
 * ===========================================================================
 */

/**
 * tailf basic structure
 */
typedef struct
{
    const char  *tf_path;       /* File path                        */
    int          tf_fd;         /* Current tailing file descriptor  */
    off_t        tf_off;        /* Last known offset                */
    struct stat  tf_stat;       /* stat structure                   */
    int          tf_ret_trunc;  /* Return read if file is truncated */
}
tailf_t;

extern void tailf_open_seek(tailf_t *self, const char *path, int seek_to_end, int ret_trunc);
extern void tailf_open(tailf_t *self, const char *path);
extern void tailf_close(tailf_t *self);
extern ssize_t tailf_read(tailf_t *self, void *buf, ssize_t bufsz);
extern ino_t tailf_get_inode(tailf_t *self);
#endif /* TAILF_H_INCLUDED */
