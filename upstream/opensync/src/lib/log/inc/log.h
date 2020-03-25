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

#ifndef __LOG__H__
#define __LOG__H__

#include <ev.h>
#include <stdint.h>
#include <syslog.h>
#include <stdbool.h>
#include <stdarg.h>
#include <stdio.h>
#include <time.h>
#include <string.h>
#include <stdbool.h>

#include "ds_dlist.h"

#ifdef LOG_TARGET_H
#include LOG_TARGET_H
#endif


#define LOG_COLOR_NONE           NULL
#define LOG_COLOR_NORMAL         "\x1b[m"
#define LOG_COLOR_GREEN          "\x1b[32;1m"
#define LOG_COLOR_YELLOW         "\x1b[33;1m"
#define LOG_COLOR_RED            "\x1b[31;1m"
#define LOG_COLOR_PURPLE         "\x1b[35;1m"
#define LOG_COLOR_DK_GREEN       "\x1b[32m"
#define LOG_COLOR_DK_PURPLE      "\x1b[35m"
#define LOG_COLOR_GRAY           "\x1b[37;1m"
#define LOG_COLOR_CYAN           "\x1b[36m"
#define LOG_COLOR_BLUE           "\x1b[34m"
#define LOG_NAME_LEN             80
#define LOG_BIT_MASK_DISPLAY_LEN 8
/**< Maximum size of the string that can be passed to log_severity_parse() */
#define LOG_SEVERITY_STR_MAX     256

/**
 * Dynamic state related to logging
 */
#define LOG_TRIGGER_DIR_MAX                 128

#define LOG_DEFAULT_ENTRY "DEFAULT"
#define LOG_DEFAULT_REMOTE "REMOTE"

typedef enum log_sink_e
{
    LOG_SINK_LOCAL = 0,
    LOG_SINK_REMOTE = 1,
} log_sink_t;

/**
 * Defines all available logging severities
 *
 *  DISABLED    - Disabled
 *  DEFAULT     - Default system severity
 *  EMERG       - System is unusable
 *  ALERT       - Action must be taken immediately
 *  CRIT        - Critical conditions
 *  ERR         - Error conditions
 *  WARNING     - Warning conditions
 *  NOTICE      - Normal, but significant, condition
 *  INFO        - Informational message
 *  DEBUG       - Debug-level message
 */
#define LOG_SEVERITY_TABLE(ENTRY)   \
    ENTRY(DISABLED, RED)            \
    ENTRY(EMERG,    PURPLE)         \
    ENTRY(ALERT,    PURPLE)         \
    ENTRY(CRIT,     RED)            \
    ENTRY(ERR,      RED)            \
    ENTRY(WARNING,  YELLOW)         \
    ENTRY(NOTICE,   GREEN)          \
    ENTRY(INFO,     CYAN)           \
    ENTRY(DEBUG,    NONE)           \
    ENTRY(TRACE,    BLUE)

#define LOG_SEVERITY_WARN   LOG_SEVERITY_WARNING
/* This always maps to a legal severity level */
#define LOG_SEVERITY_STDOUT LOG_SEVERITY_NOTICE
// ERROR -> ERR alias
#define LOG_SEVERITY_ERROR LOG_SEVERITY_ERR
// default
#define LOG_SEVERITY_DEFAULT    LOG_SEVERITY_INFO


/**
 * Definition of all available modules
 *
 *  MISC        - Default logging module
 *  COMMON      - COMMON lib layer
 *  DS          - Data structures library
 *  LOG         - Logging facilities
 */
#define LOG_MODULE_TABLE_COMMON(ENTRY)      \
    ENTRY(TRACEBACK)                        \
    ENTRY(MISC)                             \
    ENTRY(COMMON)                           \
    ENTRY(EXEC)                             \
    ENTRY(CMD)                              \
    ENTRY(OVSDB)                            \
    ENTRY(CEV)                              \
    ENTRY(OSA)                              \
    ENTRY(MQTT)                             \
    ENTRY(DPP)                              \
    ENTRY(MAIN)                             \
    ENTRY(CLI)                              \
    ENTRY(EVENT)                            \
    ENTRY(MEMPOOL)                          \
    ENTRY(NOTIFY)                           \
    ENTRY(RADIO)                            \
    ENTRY(HAL)                              \
    ENTRY(VIF)                              \
    ENTRY(IOCTL)                            \
    ENTRY(LINK)                             \
    ENTRY(BLE)                              \
    ENTRY(UPG)                              \
    ENTRY(OVS)                              \
    ENTRY(DHCPS)                            \
    ENTRY(SCHED)                            \
    ENTRY(PASYNC)                           \
    ENTRY(TARGET)                           \
    ENTRY(BSAL)                             \
    ENTRY(CLIENT)                           \
    ENTRY(KICK)                             \
    ENTRY(STATS)                            \
    ENTRY(WL)                               \
    ENTRY(NEIGHBORS)

#ifndef LOG_MODULE_TABLE_TARGET
#define LOG_MODULE_TABLE(ENTRY) \
        LOG_MODULE_TABLE_COMMON(ENTRY)
#else
#define LOG_MODULE_TABLE(ENTRY) \
        LOG_MODULE_TABLE_COMMON(ENTRY) \
        LOG_MODULE_TABLE_TARGET(ENTRY)
#endif

#define LOG(level, ...) \
    mlog(LOG_SEVERITY_##level, MODULE_ID, __VA_ARGS__)

#define LOG_SEVERITY(level, ...) \
    mlog(level, MODULE_ID,  __VA_ARGS__)

#define LOG_MODULE_MESSAGE(level, module_id, ...)    \
    mlog(LOG_SEVERITY_##level, module_id, __VA_ARGS__)

/* Shortcut log macros */
#define LOGEM(fmt, ...)       LOG(EMERG,   fmt, ## __VA_ARGS__)
#define LOGA(fmt, ...)        LOG(ALERT,   fmt, ## __VA_ARGS__)
#define LOGC(fmt, ...)        LOG(CRIT,    fmt, ## __VA_ARGS__)
#define LOGE(fmt, ...)        LOG(ERR,     fmt, ## __VA_ARGS__)
#define LOGW(fmt, ...)        LOG(WARNING, fmt, ## __VA_ARGS__)
#define LOGN(fmt, ...)        LOG(NOTICE,  fmt, ## __VA_ARGS__)
#define LOGI(fmt, ...)        LOG(INFO,    fmt, ## __VA_ARGS__)
#define LOGD(fmt, ...)        LOG(DEBUG,   fmt, ## __VA_ARGS__)
#define LOGT(fmt, ...)        LOG(TRACE,   fmt, ## __VA_ARGS__)

#define TRACEF(FMT, ...)      LOGT("%s:%d " FMT, __FUNCTION__, __LINE__, ## __VA_ARGS__)
#define TRACE(...)            TRACEF(""__VA_ARGS__)

#define LOG_SEVERITY_ENABLED(LEVEL) (LEVEL <= log_module_severity_get(MODULE_ID))

#define WARN_ON(cond) \
({ \
    typeof(cond) __c = (cond); \
    if (__c) LOGW("%sL%d@%s: [%s] failed", __FILE__, __LINE__, __func__, #cond); \
    __c; \
})


/** Generate the log_severity_t table */
typedef enum
{
    #define LOG_SEVERITY_T(sev, color)   LOG_SEVERITY_ ## sev,
    /* Expand the LOG_SEVERITY_TABLE macro */
    LOG_SEVERITY_TABLE(LOG_SEVERITY_T)

    LOG_SEVERITY_LAST
}
log_severity_t;


/** Generate the log_module_t table */
typedef enum
{
    #define LOG_MODULE_M(mod)   LOG_MODULE_ID_ ## mod,
    LOG_MODULE_TABLE(LOG_MODULE_M)

    LOG_MODULE_ID_LAST
}
log_module_t;

typedef struct
{
    log_severity_t  s;
    char            *name;
    char            *color;
} log_severity_entry_t;

typedef struct
{
    log_module_t    module;
    char            module_name[LOG_NAME_LEN];
    log_severity_t  severity;
} log_module_entry_t;

/**
 * We use these macros to get a default module id inside the LOG_* macros.
 * In case there's no MODULE_ID defined, MODULE_ID_MISC will be used as
 * the default module ID.
 */
#ifndef MODULE_ID
static log_module_t MODULE_ID = LOG_MODULE_ID_MISC;

static inline void __unused_MODULE_ID(void)
{
    (void)MODULE_ID;
}
#endif

/**
 * Flags for log_open()
 */
#define LOG_OPEN_DEFAULT        (1 << 0)        /* Use defaults: log to syslog and to stdout, if stdin is a TTY */
#define LOG_OPEN_SYSLOG         (1 << 1)        /* Log to syslog */
#define LOG_OPEN_STDOUT         (1 << 2)        /* Log to stdout */
#define LOG_OPEN_STDOUT_QUIET   (1 << 3)        /* Log to stdout is quiet, shows only STDOUT severity messages */
#define LOG_OPEN_REMOTE         (1 << 4)        /* Log to mqtt */

/*
 * ===========================================================================
 *  Logger support functions
 * ===========================================================================
 */
typedef struct logger logger_t;
typedef struct logger_msg logger_msg_t;

typedef void logger_fn_t(logger_t *self, logger_msg_t *);
typedef bool logger_match_fn_t(log_severity_t sev, log_module_t module);

struct logger
{
    logger_fn_t        *logger_fn;                  /* Logger callback */
    logger_match_fn_t  *match_fn;                   /* severity match callback */
    ds_dlist_node_t     logger_node;                /* List structure */

    union
    {
        /* stdout logger private data */
        struct
        {
            bool        quiet;                      /* Quiet mode -- do not show any log messages except LOG_INFO */
        }
        log_stdout;
    };
};

struct logger_msg
{
    log_severity_t      lm_severity;                /* Message severity */
    log_module_t        lm_module;                  /* Message module */
    char               *lm_module_name;             /* Message module name */
    char               *lm_timestamp;               /* Timestamp string */
    char               *lm_tag;                     /* Message tag */
    char               *lm_text;                    /* Message text */
};

/*
 * ===========================================================================
 *  Log API
 * ===========================================================================
 */
void                  mlog(log_severity_t s,
                           log_module_t module,
                           const char *fmt, ...)
                           __attribute__ ((format(printf, 3, 4)));

bool                  log_open(char *name, int flags);
void                  log_close();
const char*           log_get_name();

log_severity_t        log_severity_get();
void                  log_severity_set(log_severity_t s);
void                  log_module_severity_set(log_module_t mod, log_severity_t sev);
log_severity_t        log_module_severity_get(log_module_t mod);
bool                  log_severity_parse(char *sevstr);

bool                  log_isenabled();

void                  log_register_logger(logger_t *logger);
void                  log_unregister_logger(logger_t *logger);

char                 *log_module_str(log_module_t mod);
char                 *log_severity_str(log_severity_t dev);
log_module_t          log_module_fromstr(char *str);
log_severity_t        log_severity_fromstr(char *str);
log_severity_entry_t *log_severity_get_by_name(char *name);
log_severity_entry_t *log_severity_get_by_id(log_severity_t id);

bool                  log_register_dynamic_severity(struct ev_loop *loop);
bool                  log_register_dynamic_trigger(struct ev_loop *loop,
                                                   void (*callback)(FILE *fp));
bool                  log_severity_dynamic_set();

/*
 * ===========================================================================
 *  Loggers (backends)
 * ===========================================================================
 */
bool logger_syslog_new(logger_t *self);
bool logger_stdout_new(logger_t *self, bool quiet_mode);
bool logger_remote_new(logger_t *self);
bool logger_traceback_new(logger_t *);
#endif /* __LOG__H__ */

