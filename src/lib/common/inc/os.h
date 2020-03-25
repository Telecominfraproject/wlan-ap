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

#ifndef __OS__H__
#define __OS__H__

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>

#include "log.h"

/* Macro to make it a bit harder to access private members of structures */
#define PRIV(x) __priv__ ## x

/* See the prctl(2) - PR_GET_NAME */
#define OS_TASK_NAME_SZ     16

/**
 * Linux specific type definitions
 */
typedef FILE *  file_ptr_t;
typedef fd_set  fd_set_type_t;
typedef struct  timeval timeval_t;

typedef struct
{
    pthread_mutex_t mtx;
    bool            status;
}
task_once_t;

#define ETH_ALEN 6

/* pid_t 0 is reserved, no process can have pid=0 */
#define OS_ERROR_PID   (-1)

#define TASK_ONCE_INIT                      \
{                                           \
    .mtx    = PTHREAD_MUTEX_INITIALIZER,    \
    .status = true,                         \
}

#define strlcpy(dest, src, size)            \
do                                          \
{                                           \
    if (size > 0)                           \
    {                                       \
        strncpy(dest, src, size - 1);       \
        dest[size - 1] = '\0';              \
    }                                       \
}                                           \
while(0)

#define strlcat(dest, src, size)            \
do                                          \
{                                           \
    size_t __dsz;                           \
    size_t __ssz;                           \
                                            \
    __dsz = strlen(dest) + 1;               \
    __ssz = strlen(src);                    \
                                            \
    if (__dsz + __ssz > size)               \
    __ssz = size - __dsz;                   \
                                            \
    strncat(dest, src, __ssz);              \
}                                           \
while (0)


#define MEM_SET(ptr, val, size) memset(ptr, val, size)
#define MEM_CPY(dest_ptr, src_ptr, size) memcpy(dest_ptr, src_ptr, size)

// auto sizeof(), be careful not to use on pointers
#define STRLCPY(DEST, SRC) strlcpy(DEST, SRC, sizeof(DEST))
#define STRLCAT(DEST, SRC) strlcat(DEST, SRC, sizeof(DEST))
#define MEMZERO(DEST) memset(&DEST, 0, sizeof(DEST))


/* Replace char 'c' with char 'r' for every occurrence in str */
#define STR_REPLACE_CHAR(str, c, r)                         \
{                                                           \
    uint32_t loop_cnt;                                      \
    for (loop_cnt = 0; str[loop_cnt] != '\0'; loop_cnt++)   \
        if (str[loop_cnt] == c)                             \
            str[loop_cnt] = r;                              \
}

int32_t hwaddr_aton(const char *txt, uint8_t *addr);


/* os_open popen interface
    returns process id of child which executes shell cmd
    stores opened read pipe descriptor in pipe_desc
*/
extern pid_t os_popen(const char *shell_cmd, int *pipe_desc);

/**
 * Execute the command @p shell_cmd and redirect it's standard output to the
 * log file.
 *
 * @param[in]       shell_cmd       Command to execute
 *
 * @return
 * This function returns the shell exit code using wait() semantics or a
 * negative number in case of error.
 *
 * This function returns the shell exit code or -1 on error.
 */
extern int cmd_log(const char *shell_cmd);

/**
 * Parse command-line arguments and sets the log severity
 *
 * @param[in]       argc            Number of arguments
 * @param[in]       argv            List of arguments
 * @param[out]      log_severity    Log severity
 *
 * @return
 * This function returns success of parsing (0) or error (-1)
 */
extern int os_get_opt(int argc, char ** argv, log_severity_t* log_severity);

/**
 * Task related methods
 */
typedef pthread_t task_id_t;

typedef bool (task_entry_point_t)(void *args);

bool task_name_get(char *name, size_t);
bool task_name_set(char *name);

bool task_create(task_id_t          *id,
                 char               *name,
                 task_entry_point_t *ep,
                 void               *args);

bool task_once(task_once_t *once);

extern bool os_pid_wait(pid_t pid, int timeout_ms);
extern bool os_pid_terminate(pid_t, int timeout_ms);
extern bool os_pid_exists(pid_t pid);
extern pid_t os_pid_from_file(char *pid_file);

#define OS_CMD_FLAG_ANY_EXIT_CODE   1 // do not assume exit_code=0 for success
#define OS_CMD_FLAG_NO_LOG_OUTPUT   2 // do not log each line of command output
#define OS_CMD_FLAG_LOG_ERROR_DEBUG 4 // use debug log level on error
bool os_cmd_exec_xv(char **buffer, int *len, int *exit_code, int flags, char *fmt, va_list args);
bool os_cmd_exec_x(char **buffer, int *len, int *exit_code, int flags, char *fmt, ...);
bool os_cmd_exec(char **buffer, char *fmt, ...);

#endif /* __OS__H__ */
