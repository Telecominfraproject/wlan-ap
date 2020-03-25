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

/*
 * ===========================================================================
 *  Rudimentary procfs interface
 * ===========================================================================
 */

#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "const.h"
#include "log.h"
#include "util.h"

#include "procfs.h"

#define PROC_DIR "/proc"
#define PROC_CMDLINE "cmdline"
#define PROC_STATUS "status"

#define PROC_SELF "self"
#define PROC_SELF_THREAD "self-thread"

#define PROC_STATUS_NAME "Name:\t"
#define PROC_STATUS_STATE "State:\t"
#define PROC_STATUS_PID "Pid:\t"
#define PROC_STATUS_PPID "PPid:\t"

static bool __procfs_entry_getpid(procfs_entry_t *self, pid_t pid);
static bool __procfs_entry_read_status(procfs_entry_t *self, const char *path);
static bool __procfs_entry_read_cmdline(procfs_entry_t *self, const char *path);
static void __procfs_entry_free_cmdline(procfs_entry_t *self);

bool procfs_open(procfs_t *self)
{
    memset(self, 0, sizeof(*self));

    self->pf_dir = opendir(PROC_DIR);
    if (self->pf_dir == NULL)
        return false;

    if (!procfs_entry_init(&self->pf_entry))
    {
        return false;
    }

    return true;
}

procfs_entry_t *procfs_read(procfs_t *self)
{
    struct dirent *de;

    while ((de = readdir(self->pf_dir)) != NULL)
    {
        pid_t pid;

        if (strcmp(de->d_name, PROC_SELF) == 0) continue;
        if (strcmp(de->d_name, PROC_SELF_THREAD) == 0) continue;

        errno = 0;
        pid = strtoul(de->d_name, NULL, 10);
        if (errno != 0 || pid <= 0) continue;

        if (!__procfs_entry_getpid(&self->pf_entry, pid))
        {
            /* XXX: The error below may happen if a PID disappers as we're reading it, silently ignore such cases */
            //LOG(ERR, "procfs: Error retrieving entry for pid %jd.", (intmax_t)pid);
            continue;
        }

        break;
    }

    if (de == NULL) return NULL;

    return &self->pf_entry;
}

bool procfs_close(procfs_t *self)
{
    if (self->pf_dir != NULL) closedir(self->pf_dir);

    procfs_entry_fini(&self->pf_entry);

    return true;
}

bool procfs_entry_init(procfs_entry_t *self)
{
    memset(self, 0, sizeof(*self));

    return true;
}

void procfs_entry_fini(procfs_entry_t *self)
{
    __procfs_entry_free_cmdline(self);
}

void procfs_entry_del(procfs_entry_t *self)
{
    procfs_entry_fini(self);
    free(self);
}

void __procfs_entry_free_cmdline(procfs_entry_t *self)
{
    if (self->pe_cmdline != NULL) free(self->pe_cmdline);
    self->pe_cmdline = NULL;

    if (self->pe_cmdbuf != NULL) free(self->pe_cmdbuf);
    self->pe_cmdbuf = NULL;
}

procfs_entry_t *procfs_entry_getpid(pid_t pid)
{
    procfs_entry_t *pe;

    pe = malloc(sizeof(*pe));
    if (pe == NULL) return NULL;

    if (!procfs_entry_init(pe))
    {
        free(pe);
        return NULL;
    }

    if (!__procfs_entry_getpid(pe, pid))
    {
        procfs_entry_fini(pe);
        return false;
    }

    return pe;
}

bool __procfs_entry_getpid(procfs_entry_t *self, pid_t pid)
{
    char p_path[C_MAXPATH_LEN];
    size_t len;

    /* Parse stat */
    len = snprintf(p_path, sizeof(p_path), "%s/%jd/%s", PROC_DIR, (intmax_t)pid, PROC_STATUS);
    if (len >= sizeof(p_path))
    {
        LOG(WARN, "procfs: Name too long %s/%jd/%s.", PROC_DIR, (intmax_t)pid, PROC_STATUS);
        return false;
    }

    if (access(p_path, R_OK) != 0)
    {
        /* Probably not a PID entry */
        return false;
    }

    if (!__procfs_entry_read_status(self, p_path))
    {
        LOG(ERR, "procs: Error parsing status: %s", p_path);
        return false;
    }

    /* Parse cmdline */
    len = snprintf(p_path, sizeof(p_path), "%s/%jd/%s", PROC_DIR, (intmax_t)pid, PROC_CMDLINE);
    if (len >= sizeof(p_path))
    {
        LOG(WARN, "procfs: Name too long %s/%jd/%s.", PROC_DIR, (intmax_t)pid, PROC_CMDLINE);
        return false;
    }

    if (!__procfs_entry_read_cmdline(self, p_path))
    {
        LOG(WARN, "procfs: Error parsing cmdline: %s", p_path);
        return false;
    }

    return true;
}

bool __procfs_entry_read_status(procfs_entry_t *self, const char *path)
{
    char buf[getpagesize()];

    FILE *f = NULL;
    bool retval = false;

    f = fopen(path, "r");
    if (f == NULL)
    {
        goto exit;
    }

    char spid[C_PID_LEN] = { 0 };
    char sppid[C_PID_LEN] = { 0 };

    while (fgets(buf, sizeof(buf), f) != NULL)
    {
        size_t psz;

        char *parg = NULL;
        char *pbuf = NULL;

        if (strncmp(buf, PROC_STATUS_NAME, strlen(PROC_STATUS_NAME)) == 0)
        {
            pbuf = buf + strlen(PROC_STATUS_NAME);
            parg = self->pe_name;
            psz = sizeof(self->pe_name);
        }
        else if (strncmp(buf, PROC_STATUS_STATE, strlen(PROC_STATUS_STATE)) == 0)
        {
            pbuf = buf + strlen(PROC_STATUS_STATE);
            parg = self->pe_state;
            psz = sizeof(self->pe_state);
        }
        else if (strncmp(buf, PROC_STATUS_PID, strlen(PROC_STATUS_PID)) == 0)
        {
            pbuf = buf + strlen(PROC_STATUS_PID);
            parg = spid;
            psz = sizeof(spid);
        }
        else if (strncmp(buf, PROC_STATUS_PPID, strlen(PROC_STATUS_PPID)) == 0)
        {
            pbuf = buf + strlen(PROC_STATUS_PPID);
            parg = sppid;
            psz = sizeof(sppid);
        }

        if (parg == NULL) continue;

        /* Chop off ending \n */
        char *ebuf = buf;
        ebuf += strlen(ebuf) - 1;
        while (*ebuf == '\n') *ebuf-- = '\0';

        if (strscpy(parg, pbuf, psz) < 0)
        {
            *parg = '\0';
            continue;
        }
    }

    /* Check if all fields were parsed properly */
    if (self->pe_state[0] == '\0' || self->pe_name[0] == '\0' || spid[0] == '\0' || sppid[0] == '\0')
    {
        goto exit;
    }

    errno = 0;
    self->pe_pid = strtoul(spid, NULL, 10);
    if (errno != 0)
    {
        self->pe_pid = 0;
        goto exit;
    }

    errno = 0;
    self->pe_ppid = strtoul(sppid, NULL, 10);
    if (errno != 0)
    {
        self->pe_ppid = 0;
        goto exit;
    }

    retval = true;

exit:
    if (f != NULL) fclose(f);

    return retval;
}

bool __procfs_entry_read_cmdline(procfs_entry_t *self, const char *path)
{
    FILE *f;
    size_t nalloc;
    size_t nrd;
    size_t argc;
    char *parg;

    bool retval = false;

    __procfs_entry_free_cmdline(self);

    f = fopen(path, "r");
    if (f == NULL)
    {
        LOG(ERR, "procfs: Error opening: %s.", path);
        goto exit;
    }

    nalloc = 256;
    self->pe_cmdbuf = malloc(nalloc);
    nrd = 0;
    do
    {
        if (nalloc <= nrd)
        {
            nalloc <<= 1;
            self->pe_cmdbuf = realloc(self->pe_cmdbuf, nalloc);
        }

        nrd += fread(self->pe_cmdbuf + nrd, 1, nalloc - nrd, f);
        if (ferror(f))
        {
            LOG(WARN, "procfs: Error reading cmdline file. %p %zd   %zd %s", self->pe_cmdbuf, nrd, nalloc, strerror(errno));
            goto exit;
        }
    }
    while (!feof(f));

    /* Parse cmdlines */
    nalloc = 16;
    self->pe_cmdline = malloc(nalloc * sizeof(char *));
    argc = 0;
    parg = self->pe_cmdbuf;

    while (parg < self->pe_cmdbuf + nrd)
    {
        if (nalloc <= (argc + 1))
        {
            nalloc <<= 1;
            /* Make sure to allocate room for the terminating NULL */
            self->pe_cmdline = realloc(self->pe_cmdline, nalloc * sizeof(char *));
        }

        self->pe_cmdline[argc++] = parg;
        /* Move to the next string */
        parg += strlen(parg) + 1;
    }

    self->pe_cmdline[argc++] = NULL;

    retval = true;

exit:
    if (f != NULL) fclose(f);
    if (!retval) __procfs_entry_free_cmdline(self);

    return retval;
}

