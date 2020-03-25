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

#include "os_common.h"

#define LOG_MODULE_ID  LOG_MODULE_ID_OSA

#include "os.h"
#include "log.h"
#include "os_proc.h"
#include "os_file_ops.h"

void os_time_stamp(char *bfr, int32_t len)
{
    time_t now;
    struct tm ts;

    time(&now);
    /* Format time, "yyyy-mm-dd hh:mm:ss zzz" */
    ts = *localtime(&now);
    memset(bfr, 0x00, len);
    strftime(bfr, len, "%Y%m%d_%H:%M:%S", &ts);
}

/* Open the text file "<prefix>_process_name_<time_stamp>.pid>" at specified location. */
FILE *os_file_open(char *location, char *prefix)
{
    char file[64], time_stamp[32];
    FILE *fp;
    int32_t pid = getpid();


    memset(file, 0x00, sizeof(file));
    snprintf(file, sizeof(file), "%s/%s_", location, prefix);

    if (os_pid_to_name(pid, file + strlen(file), sizeof(file) - strlen(file)) != 0) {
        LOG(ERR, "Error! os_pid_to_name() failed");
        return NULL;
    }

    memset(time_stamp, 0x00, sizeof(time_stamp));
    os_time_stamp(time_stamp, sizeof(time_stamp));

    snprintf(file + strlen(file),
             sizeof(file) - strlen(file), "_%s.%d", time_stamp, pid);

    fp = fopen(file, "w+");
    if (NULL == fp)
        LOG(ERR, "Error opening the file: %s", file);

    return fp;
}

void os_file_close(FILE *fp)
{
    if (NULL != fp)
        fclose(fp);
}

