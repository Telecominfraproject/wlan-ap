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

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>

#include <unistd.h>
#include <dirent.h>
#include <inttypes.h>
#include <sys/vfs.h>

#include "target.h"
#include "target_common.h"  // interface

#include "log.h"
#include "dpp_device.h"



#define MODULE_ID LOG_MODULE_ID_TARGET

#define LINUX_PROC_LOADAVG_FILE  "/proc/loadavg"
#define LINUX_PROC_UPTIME_FILE   "/proc/uptime"
#define LINUX_PROC_MEMINFO_FILE  "/proc/meminfo"
#define LINUX_PROC_STAT_FILE     "/proc/stat"

#define STR_BEGINS_WITH(buf, token)  \
            (strncmp(buf, token, strlen(token)) == 0)


#define PID_BUF_NUM      128


typedef struct
{
    uint32_t   pid;
    char       cmd[18];

    uint32_t   utime;       // [clock ticks]
    uint32_t   stime;       // [clock ticks]
    uint64_t   starttime;   // [clock ticks]

    uint32_t   rss;         // [kB]
    uint32_t   pss;         // [kB]

    uint32_t   mem_util;    // Memory usage estimation [kB]
    uint32_t   cpu_util;    // CPU utilization [%] [0..100]
} pid_util_t;


typedef struct
{
    uint64_t     timestamp;   // [clock ticks]

    pid_util_t  *pid_util;
    unsigned     n_pid_util;

    uint32_t     mem_total;   // System memory size [kB]
    uint32_t     mem_used;    // System memory used [kB]
    uint32_t     swap_total;  // Swap file size [kB]
    uint32_t     swap_used;   // Swap file used [kB]
} system_util_t;


typedef struct
{
    uint64_t hz_user;
    uint64_t hz_nice;
    uint64_t hz_system;
    uint64_t hz_idle;
} cpu_stats_hz_t;


/* Defaults. Values will be acquired at runtime, although they will likely
 * match the defaults set here. */
static uint32_t PAGE_KB   =    4;
static uint32_t CLOCK_TCK =  100;

/* TODO: Do this properly, previous device stats should be remembered at a
 * higher api level, and passed over to here, possibly by a context struct
 * (and hence api made re-entrant). */
static cpu_stats_hz_t  g_cpu_stats_prev;
static system_util_t   g_sysutil_prev;



static bool linux_device_load_get(dpp_device_record_t *record);
static bool linux_device_uptime_get(dpp_device_record_t *record);
static bool linux_device_cpuutil_get(dpp_device_cpuutil_t *cpuutil);
static bool linux_device_memutil_get(dpp_device_memutil_t *memutil);
static bool linux_device_fsutil_get(dpp_device_fsutil_t *fsutil);
static bool linux_device_top(dpp_device_record_t *device_record);

static int proc_parse_uptime(uint64_t *uptime);
static int proc_parse_meminfo(system_util_t *system_util);
static int proc_parse_pid_stat(uint32_t pid, pid_util_t *pid_util);
static int proc_parse_pid_smaps(uint32_t pid, pid_util_t *pid_util);

static const char* str_ntok(const char *str, char delim, unsigned n);
static int get_all_pids(uint32_t **pid_list, unsigned *pid_num);
static int get_pid_util_list(const uint32_t *pid_list, unsigned pid_num,
                             pid_util_t **pid_util_list);
static int compare_pid_util_t_mem(const void *a, const void *b);
static int compare_pid_util_t_cpu(const void *a, const void *b);
static const pid_util_t* sysutil_prev_find(uint32_t pid);


static bool linux_device_load_get(dpp_device_record_t *record)
{
    int32_t     rc;
    const char  *filename = LINUX_PROC_LOADAVG_FILE;
    FILE        *proc_file = NULL;

    proc_file = fopen(filename, "r");
    if (proc_file == NULL)
    {
        LOG(ERR, "Parsing device stats (Failed to open %s)", filename);
        return false;
    }

    rc = fscanf(proc_file,
            "%lf %lf %lf",
            &record->load[DPP_DEVICE_LOAD_AVG_ONE],
            &record->load[DPP_DEVICE_LOAD_AVG_FIVE],
            &record->load[DPP_DEVICE_LOAD_AVG_FIFTEEN]);

    fclose(proc_file);

    if (rc != DPP_DEVICE_LOAD_AVG_QTY)
    {
        LOG(ERR, "Parsing device stats (Failed to read %s)", filename);
        return false;
    }

    LOG(TRACE, "Parsed device load %0.2f %0.2f %0.2f",
            record->load[DPP_DEVICE_LOAD_AVG_ONE],
            record->load[DPP_DEVICE_LOAD_AVG_FIVE],
            record->load[DPP_DEVICE_LOAD_AVG_FIFTEEN]);

    return true;
}


static bool linux_device_uptime_get(dpp_device_record_t *record)
{
    int32_t     rc;
    const char  *filename = LINUX_PROC_UPTIME_FILE;
    FILE        *proc_file = NULL;

    proc_file = fopen(filename, "r");
    if (proc_file == NULL)
    {
        LOG(ERR, "Parsing device stats (Failed to open %s)", filename);
        return false;
    }

    rc = fscanf(proc_file, "%u", &record->uptime);

    fclose(proc_file);

    if (rc != 1)
    {
        LOG(ERR, "Parsing device stats (Failed to read %s)", filename);
        return false;
    }

    LOG(TRACE, "Parsed device uptime %u", record->uptime);

    return true;
}


static bool linux_device_cpuutil_get(dpp_device_cpuutil_t *cpuutil)
{
    const char *filename = LINUX_PROC_STAT_FILE;
    FILE *proc_file = NULL;
    char buf[256] = { 0 };


    memset(cpuutil, 0, sizeof(*cpuutil));

    proc_file = fopen(filename, "r");
    if (proc_file == NULL)
    {
        LOG(ERROR, "Failed opening file: %s", filename);
        return false;
    }

    while (fgets(buf, sizeof(buf), proc_file) != NULL)
    {
        cpu_stats_hz_t now;
        cpu_stats_hz_t diff;
        uint64_t hz_total_diff;
        double busy;

        /* Check for 'cpu', but not 'cpu0', 'cpu1', and such */
        if (!STR_BEGINS_WITH(buf, "cpu ")) continue;  // not the right line

        if (sscanf(buf, "cpu %"PRIu64" %"PRIu64" %"PRIu64" %"PRIu64"",
                    &(now.hz_user), &(now.hz_nice), &(now.hz_system), &(now.hz_idle)) != 4)
        {
            goto parse_error;
        }

        diff.hz_user   = now.hz_user   - g_cpu_stats_prev.hz_user;
        diff.hz_nice   = now.hz_nice   - g_cpu_stats_prev.hz_nice;
        diff.hz_system = now.hz_system - g_cpu_stats_prev.hz_system;
        diff.hz_idle   = now.hz_idle   - g_cpu_stats_prev.hz_idle;

        g_cpu_stats_prev = now;  // store current values

        hz_total_diff = diff.hz_user
                        + diff.hz_nice
                        + diff.hz_system
                        + diff.hz_idle;

        if (hz_total_diff == 0)
        {
            LOG(ERROR, "%s: Unexpected hz_total value: %"PRIu64"",
                        __func__, hz_total_diff);
            return false;
        }

        /* Calculate percentage and round */
        busy = (1.0 - ((double)diff.hz_idle / (double)hz_total_diff)) * 100.0;

        cpuutil->cpu_util = (uint32_t) (busy + 0.5);

        break;  // found the aggregate 'cpu' line, exit loop
    }

    fclose(proc_file);
    return true;

parse_error:
    fclose(proc_file);
    LOG(ERROR, "Error parsing %s.", filename);
    return false;
}


/* Get system uptime [clock ticks]. */
static int proc_parse_uptime(uint64_t *uptime)
{
    const char *filename = LINUX_PROC_UPTIME_FILE;
    FILE *proc_file = NULL;
    char buf[256];
    double sys_time;


    proc_file = fopen(filename, "r");
    if (proc_file == NULL)
    {
        LOG(ERROR, "Failed opening file: %s", filename);
        return -1;
    }

    if (fgets(buf, sizeof(buf), proc_file) == NULL)
    {
        goto read_error;
    }

    if (sscanf(buf, "%lf", &sys_time) != 1)
    {
        goto parse_error;
    }

    *uptime = (uint64_t) (sys_time * CLOCK_TCK);

    fclose(proc_file);
    return 0;

parse_error:
    LOG(ERROR, "Error parsing %s", filename);
    fclose(proc_file);
    return -1;

read_error:
    LOG(ERROR, "Error reading %s (%d)", filename, errno);
    fclose(proc_file);
    return -1;
}


static int proc_parse_meminfo(system_util_t *system_util)
{
    const char *filename = LINUX_PROC_MEMINFO_FILE;
    FILE *proc_file = NULL;
    char buf[256];
    uint32_t mem_total;
    uint32_t mem_avail;
    uint32_t mem_free;
    uint32_t swap_total;
    uint32_t swap_free;


    proc_file = fopen(filename, "r");
    if (proc_file == NULL)
    {
        LOG(ERROR, "Failed opening file: %s", filename);
        return -1;
    }

    while (fgets(buf, sizeof(buf), proc_file) != NULL)
    {
        if (STR_BEGINS_WITH(buf, "MemTotal:"))
        {
            if (sscanf(buf, "MemTotal: %u", &mem_total) != 1)
                goto parse_error;
        }
        else if (STR_BEGINS_WITH(buf, "MemFree:"))
        {
            if (sscanf(buf, "MemFree: %u", &mem_free) != 1)
                goto parse_error;
        }
        else if (STR_BEGINS_WITH(buf, "MemAvailable:"))
        {
            if (sscanf(buf, "MemAvailable: %u", &mem_avail) != 1)
                goto parse_error;
        }
        else if (STR_BEGINS_WITH(buf, "SwapTotal:"))
        {
            if (sscanf(buf, "SwapTotal: %u", &swap_total) != 1)
                goto parse_error;
        }
        else if (STR_BEGINS_WITH(buf, "SwapFree:"))
        {
            if (sscanf(buf, "SwapFree: %u", &swap_free) != 1)
                goto parse_error;
        }
    }

    system_util->mem_total = mem_total;
    if (mem_avail > 0) {
        system_util->mem_used = mem_total - mem_avail;
    } else {
        system_util->mem_used = mem_total - mem_free;   /* older kernels */
    }

    system_util->swap_total = swap_total;
    system_util->swap_used  = swap_total - swap_free;

    fclose(proc_file);
    return 0;

parse_error:
    fclose(proc_file);
    LOG(ERROR, "Error parsing %s.", filename);
    return -1;
}


static bool linux_device_memutil_get(dpp_device_memutil_t *memutil)
{
    system_util_t system_util;

    if (proc_parse_meminfo(&system_util) != 0)
    {
        return false;
    }

    memset(memutil, 0, sizeof(*memutil));

    memutil->mem_total  = system_util.mem_total;
    memutil->mem_used   = system_util.mem_used;
    memutil->swap_total = system_util.swap_total;
    memutil->swap_used  = system_util.swap_used;

    return true;
}


static bool linux_device_fsutil_get(dpp_device_fsutil_t *fsutil)
{
    const char *path;
    struct statfs fs_info;
    int rc;


    switch (fsutil->fs_type)
    {
        case DPP_DEVICE_FS_TYPE_ROOTFS:
            path = "/";
            break;
        case DPP_DEVICE_FS_TYPE_TMPFS:
            path = "/tmp";
            break;
        default:
            LOG(ERROR, "Invalid fs type: %d", fsutil->fs_type);
            return false;
    }


    rc = statfs(path, &fs_info);
    if (rc != 0)
    {
        LOG(ERROR, "Error getting filesystem status info: %s: %s", path, strerror(errno));
        return false;
    }

    fsutil->fs_total = (fs_info.f_blocks * fs_info.f_bsize) / 1024;
    fsutil->fs_used = ((fs_info.f_blocks - fs_info.f_bfree) * fs_info.f_bsize) / 1024;

    return true;
}


/* Get an array of pids of all current processes.
 * Returned 'pid_list' is malloc-ed and must be free-d by the caller. */
static int get_all_pids(uint32_t **pid_list, unsigned *pid_num)
{
    const char *dirname = "/proc";
    DIR *proc_dir = NULL;
    struct dirent *dire;
    uint32_t *pids = NULL;
    size_t num_allocated;
    size_t num;


    proc_dir = opendir(dirname);
    if (proc_dir == NULL)
    {
        LOG(ERROR, "Error opening directory: %s: %s", dirname, strerror(errno));
        return -1;
    }

    num_allocated = PID_BUF_NUM;
    pids = malloc(num_allocated * sizeof(*pids));
    num = 0;
    while ((dire = readdir(proc_dir)) != NULL)
    {
        char c = dire->d_name[0];
        if ( !(c >= '0' && c <= '9') ) continue;

        pids[num++] = (uint32_t)atoi(dire->d_name);

        if (num == num_allocated)
        {
            num_allocated += PID_BUF_NUM;
            pids = realloc(pids, num_allocated * sizeof(*pids));
        }
    }
    closedir(proc_dir);

    *pid_list = pids;
    *pid_num = num;
    return 0;
}


/* Find and return a pointer to the 'n'-th [0..] token in a string 'str'
 * where tokens are delimited by 'delim' characters. */
static const char* str_ntok(const char *str, char delim, unsigned n)
{
    unsigned i = 0;
    const char *s = str;

    if (i == n) return str;

    while (i < n)
    {
        s = strchr(s, delim);
        if (s == NULL) return NULL;

        while (*s == delim) s++;

        if (*s == '\0') return NULL;

        i++;
        if (i == n) return s;
    }
    return NULL;
}


/* For pid get its utilization stats from proc. */
static int proc_parse_pid_stat(uint32_t pid, pid_util_t *pid_util)
{
    char filename[32];
    FILE *proc_file = NULL;
    char line[512];
    const char *tok;
    const char *end;
    int rc = 0;


    snprintf(filename, sizeof(filename), "/proc/%u/stat", pid);
    proc_file = fopen(filename, "r");
    if (proc_file == NULL)
    {
        LOG(DEBUG, "Error opening %s: %s", filename, strerror(errno));
        /* Process with this pid probably already exited */
        return -ESRCH;
    }

    if (fgets(line, sizeof(line), proc_file) == NULL)
    {
        goto read_error;
    }

    if (!(tok = strchr(line, '(')))
        goto parse_error;
    tok++;
    if (!(end = strchr(tok, ')')))
        goto parse_error;

    strncpy(pid_util->cmd, tok, end-tok);

    tok = end+2;

    if (!(tok = str_ntok(tok, ' ', 11)))
        goto parse_error;
    rc = sscanf(tok, "%u", &pid_util->utime);
    if (rc != 1) goto parse_error;

    if (!(tok = str_ntok(tok, ' ', 1)))
        goto parse_error;
    rc = sscanf(tok, "%u", &pid_util->stime);
    if (rc != 1) goto parse_error;

    if (!(tok = str_ntok(tok, ' ', 7)))
        goto parse_error;
    rc = sscanf(tok, "%"PRIu64, &pid_util->starttime);
    if (rc != 1) goto parse_error;

    if (!(tok = str_ntok(tok, ' ', 2)))
        goto parse_error;
    rc = sscanf(tok, "%u", &pid_util->rss);
    if (rc != 1) goto parse_error;
    pid_util->rss = pid_util->rss * PAGE_KB;

    fclose(proc_file);
    return 0;

parse_error:
    LOG(ERROR, "Failed parsing %s: rc=%d, contents=<%s>", filename, rc, line);
    fclose(proc_file);
    return -1;

read_error:
    LOG(ERROR, "Error reading %s (%d)", filename, errno);
    fclose(proc_file);
    return -1;
}


/* For pid get/calculate its total PSS from per-mapping PSS values from proc. */
static int proc_parse_pid_smaps(uint32_t pid, pid_util_t *pid_util)
{
    char filename[32];
    FILE *proc_file = NULL;
    char buf[512];
    uint32_t pss_total;


    snprintf(filename, sizeof(filename), "/proc/%u/smaps", pid);  /* 2.6.14 + */
    proc_file = fopen(filename, "r");
    if (proc_file == NULL)
    {
        LOG(DEBUG, "Error opening %s: %s", filename, strerror(errno));

        /* Try opening the stat file instead */
        snprintf(filename, sizeof(filename), "/proc/%u/stat", pid);
        proc_file = fopen(filename, "r");
        if (proc_file != NULL)
        {
            LOG(DEBUG, "/proc/[pid]/smaps not supported on this kernel.");
            fclose(proc_file);
            return -ENOENT;
        }

        /* Process probably already exited */
        return -ESRCH;
    }

    pss_total = 0;
    while (fgets(buf, sizeof(buf), proc_file) != NULL)
    {
        uint32_t pss;
        if (STR_BEGINS_WITH(buf, "Pss:"))
        {
            if (sscanf(buf, "Pss: %u", &pss) != 1)
            {
                LOG(ERROR, "Error parsing %s: %s.", filename, buf);
                fclose(proc_file);
                return -1;
            }
            pss_total += pss;
        }
    }

    pid_util->pss = pss_total;
    fclose(proc_file);
    return 0;
}


/* For pids in the array 'pid_list' get an array of pid utilization info.
 * Returned 'pid_util_list' is malloc-ed and must be free-d by the caller. */
static int get_pid_util_list(const uint32_t *pid_list, unsigned pid_num,
                             pid_util_t **pid_util_list)
{
    pid_util_t *pid_util = NULL;
    unsigned i;
    int rc;


    pid_util = calloc(pid_num, sizeof(*pid_util));
    for (i = 0; i < pid_num; i++)
    {
        pid_util[i].pid = pid_list[i];

        rc = proc_parse_pid_stat(pid_util[i].pid, &pid_util[i]);
        if (rc == -ESRCH)
        {
            /* Process probably exited in the meantime - ignore this pid */
            pid_util[i].pid = 0;
            continue;
        }
        else if (rc != 0)
        {
            goto error;
        }

        rc = proc_parse_pid_smaps(pid_util[i].pid, &pid_util[i]);
        if (rc == -ESRCH)
        {
            pid_util[i].pid = 0;
            continue;
        }
        else if (rc == -ENOENT)
        {
            /* '/proc/[pid]/smaps' not available, default to rss for mem_util */
            pid_util[i].mem_util = pid_util[i].rss;
            continue;
        }
        else if (rc != 0)
        {
            goto error;
        }

        /* For process memory usage estimation PSS should be available on most
         * kernels, but fallback to RSS if this is not the case */
        if (pid_util[i].pss > 0) {
            pid_util[i].mem_util = pid_util[i].pss;
        } else {
            pid_util[i].mem_util = pid_util[i].rss;
        }
    }

    *pid_util_list = pid_util;
    return 0;

error:
    // should be already logged, just free allocated memory
    if (pid_util != NULL) free(pid_util);
    return -1;
}


/* Memory utilization comparison function (for a descending sort order) */
static int compare_pid_util_t_mem(const void *_a, const void *_b)
{
    const pid_util_t *a = _a;
    const pid_util_t *b = _b;

    /* reverse sort - larger memory consumers first */
    if (a->mem_util < b->mem_util) return 1;
    if (a->mem_util > b->mem_util) return -1;

    return 0;
}


/* CPU utilization comparison function (for a descending sort order) */
static int compare_pid_util_t_cpu(const void *_a, const void *_b)
{
    const pid_util_t *a = _a;
    const pid_util_t *b = _b;

    /* reverse sort - larger CPU consumers first */
    if (a->cpu_util < b->cpu_util) return 1;
    if (a->cpu_util > b->cpu_util) return -1;

    return 0;
}


/* Find previous-measurements system-utilization-stats for 'pid'. */
static const pid_util_t* sysutil_prev_find(uint32_t pid)
{
    unsigned i;

    if (g_sysutil_prev.pid_util == NULL) return NULL;

    for (i = 0; i < g_sysutil_prev.n_pid_util; i++)
    {
        const pid_util_t *iter = &g_sysutil_prev.pid_util[i];

        if (iter->pid == 0) continue;

        if (iter->pid == pid) return iter;  /* found */
    }
    return NULL;
}


/* Find top cpu consuming and top memory consuming processes. */
static bool linux_device_top(dpp_device_record_t *device_record)
{
    system_util_t sysutil;
    uint32_t *pid_list = NULL;
    bool retval = false;
    unsigned i;


    memset(&sysutil, 0, sizeof(sysutil));

    if (proc_parse_uptime(&sysutil.timestamp) != 0)
    {
        LOG(ERROR, "Error parsing system uptime information.");
        goto err_out;
    }
    if (proc_parse_meminfo(&sysutil) != 0)
    {
        LOG(ERROR, "Error parsing system memory utilization info.");
        goto err_out;
    }
    if (get_all_pids(&pid_list, &sysutil.n_pid_util) != 0)
    {
        LOG(ERROR, "Error getting list of running processes.");
        goto err_out;
    }
    if (get_pid_util_list(pid_list, sysutil.n_pid_util, &sysutil.pid_util) != 0)
    {
        LOG(ERROR, "Error getting processes utilization stats.");
        goto err_out;
    }

    /* Alright, first let's find out who are top memory-consuming pids... */
    qsort(sysutil.pid_util, sysutil.n_pid_util, sizeof(*sysutil.pid_util),
          compare_pid_util_t_mem);

    for (i = 0; (i < sysutil.n_pid_util) && (i < DPP_DEVICE_TOP_MAX); i++)
    {
        device_record->top_mem[i].pid = sysutil.pid_util[i].pid;
        device_record->top_mem[i].util = sysutil.pid_util[i].mem_util;

        sysutil.pid_util[i].cmd[sizeof(sysutil.pid_util[i].cmd) - 1] = '\0';
        strncpy(device_record->top_mem[i].cmd, sysutil.pid_util[i].cmd,
                sizeof(device_record->top_mem[i].cmd));
    }
    device_record->n_top_mem = i;

    /* Now, let's find the top cpu-consuming pids... */

    /* First, calculate cpu utilizations for the observed processes: */
    for (i = 0; i < sysutil.n_pid_util; i++)
    {
        pid_util_t       *util_curr;
        const pid_util_t *util_prev;
        uint32_t cpu_time_curr;
        uint32_t cpu_time_prev;
        uint64_t timestamp_curr;
        uint64_t timestamp_prev;
        double util;

        util_curr = &sysutil.pid_util[i];
        if (util_curr->pid == 0) continue;

        util_prev = sysutil_prev_find(util_curr->pid);
        if (util_prev == NULL) continue;

        if (util_prev->starttime != util_curr->starttime
            || strcmp(util_prev->cmd, util_curr->cmd))
        {
            continue;
        }

        cpu_time_curr = util_curr->utime + util_curr->stime;
        cpu_time_prev = util_prev->utime + util_prev->stime;
        timestamp_curr = sysutil.timestamp;
        timestamp_prev = g_sysutil_prev.timestamp;
        if ((timestamp_curr - timestamp_prev) == 0)
        {
            LOG(ERROR, "%s: Unexpected timestamp_curr==timestamp_prev==%"PRIu64"",
                       __func__, timestamp_curr);
            goto err_out;
        }

        /* Calculate percentage and round */
        util = 100.0 * (double)(cpu_time_curr - cpu_time_prev)
                     / (double)(timestamp_curr - timestamp_prev);

        util_curr->cpu_util = (uint32_t) (util + 0.5);
    }

    /* Now, finally find the top cpu consumers: */
    qsort(sysutil.pid_util, sysutil.n_pid_util, sizeof(*sysutil.pid_util),
          compare_pid_util_t_cpu);

    for (i = 0; (i < sysutil.n_pid_util) && (i < DPP_DEVICE_TOP_MAX); i++)
    {
        device_record->top_cpu[i].pid = sysutil.pid_util[i].pid;
        device_record->top_cpu[i].util = sysutil.pid_util[i].cpu_util;

        sysutil.pid_util[i].cmd[sizeof(sysutil.pid_util[i].cmd) - 1] = '\0';
        strncpy(device_record->top_cpu[i].cmd, sysutil.pid_util[i].cmd,
                sizeof(device_record->top_cpu[i].cmd));
    }
    device_record->n_top_cpu = i;

    retval = true;

err_out:
    /* Discard previous sysutil measurements */
    if (g_sysutil_prev.pid_util != NULL)
    {
        free(g_sysutil_prev.pid_util);
        g_sysutil_prev.pid_util = NULL;
    }
    if (pid_list != NULL)
    {
        free(pid_list);
    }

    if (sysutil.pid_util == NULL) sysutil.n_pid_util = 0;

    /* Remember this measurement for the next time */
    g_sysutil_prev = sysutil;

    /* All done */
    return retval;
}


bool target_stats_device_get(dpp_device_record_t  *device_entry)
{
    int i;
    long rc;


    /* Get actual values at runtime for page size and USER_HZ,
     * although they probably do not differ from default */
    if ((rc = sysconf(_SC_PAGESIZE)) != -1)
    {
        PAGE_KB = (uint32_t)(rc/1024);
    }
    if ((rc = sysconf(_SC_CLK_TCK)) != -1)
    {
        CLOCK_TCK = (uint32_t)rc;
    }

    memset(device_entry, 0, sizeof(*device_entry));

    if (!linux_device_load_get(device_entry))
    {
        LOG(ERR, "Failed to retrieve device load.");
        return false;
    }
    if (!linux_device_uptime_get(device_entry))
    {
        LOG(ERR, "Failed to retrieve device uptime.");
        return false;
    }
    if (!linux_device_memutil_get(&device_entry->mem_util))
    {
        LOG(ERR, "Failed to retrieve device memory utilization.");
        return false;
    }
    if (!linux_device_cpuutil_get(&device_entry->cpu_util))
    {
        LOG(ERR, "Failed to retrieve device cpu utilization.");
        return false;
    }

    device_entry->fs_util[DPP_DEVICE_FS_TYPE_ROOTFS].fs_type = DPP_DEVICE_FS_TYPE_ROOTFS;
    device_entry->fs_util[DPP_DEVICE_FS_TYPE_TMPFS].fs_type = DPP_DEVICE_FS_TYPE_TMPFS;
    for (i = 0; i < DPP_DEVICE_FS_TYPE_QTY; i++)
    {
        if (!linux_device_fsutil_get(&device_entry->fs_util[i]))
        {
            LOG(ERR, "Failed to retrieve device filesystem utilization.");
            return false;
        }
    }

    if (!linux_device_top(device_entry))
    {
        LOG(ERR, "Failed to retrieve device top cpu/mem utilization.");
        return false;
    }

    return true;
}
