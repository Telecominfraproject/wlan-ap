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

#ifndef PROCFS_H_INCLUDED
#define PROCFS_H_INCLUDED

#include <stdbool.h>
#include <dirent.h>
#include <unistd.h>

typedef struct __procfs procfs_t;
typedef struct __procfs_entry procfs_entry_t;

struct __procfs_entry
{
    char            pe_name[32];                /* Process name */
    pid_t           pe_pid;                     /* PID of the process */
    pid_t           pe_ppid;                    /* Parent PID */
    char            pe_state[64];               /* Process state */
    char            **pe_cmdline;               /* NULL terminated command line */
    char            *pe_cmdbuf;                 /* Buffer holding command line arguments */
};

struct __procfs
{
    DIR             *pf_dir;
    procfs_entry_t  pf_entry;

};

extern bool procfs_open(procfs_t *self);
extern procfs_entry_t *procfs_read(procfs_t *self);
extern bool procfs_close(procfs_t *self);

extern bool procfs_entry_init(procfs_entry_t *self);
extern void procfs_entry_fini(procfs_entry_t *self);
extern void procfs_entry_del(procfs_entry_t *self);

extern procfs_entry_t* procfs_entry_getpid(pid_t pid);

#endif /* PROCFS_H_INCLUDED */
