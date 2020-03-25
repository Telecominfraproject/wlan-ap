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
#include <unistd.h>
#include <time.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdbool.h>
#include <syslog.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <jansson.h>

#include "log.h"
#include "os_time.h"
#include "target.h"
#include "assert.h"

#define LF '\n'
#define CR '\r'
#define NUL '\0'
#define LOGGER_BUFF_LEN (1024*8)
#define LOGGER_EXT_BUFF_LEN (3 * LOGGER_BUFF_LEN)
#define LIVE_LOGGING_PERIOD  5 /*seconds */

/**
 * Generate the module_table
 */

static log_module_entry_t log_module_table[LOG_MODULE_ID_LAST] =
{
    /** Expand the LOG_MODULE_TABLE by using the ENTRY macro above */
    #define LOG_ENTRY(mod)      { LOG_MODULE_ID_##mod, #mod, LOG_SEVERITY_DEFAULT },
    LOG_MODULE_TABLE(LOG_ENTRY)
};

log_module_entry_t log_module_remote[LOG_MODULE_ID_LAST] =
{
    /** Expand the LOG_MODULE_TABLE by using the ENTRY macro above */
    LOG_MODULE_TABLE(LOG_ENTRY)
};

/**
 * Elements must be in the same order as the log_severity_t enum
 */
static log_severity_entry_t log_severity_table[LOG_SEVERITY_LAST] =
{
    /** Expand the LOG_SEVERITY_TABLE by using the ENTRY macro above */
    #define COLOR_MAP(sev, color)   { LOG_SEVERITY_ ## sev,  #sev, LOG_COLOR_ ## color  },
    LOG_SEVERITY_TABLE(COLOR_MAP)
};


/* static global configuration */
static bool log_enabled              = false;
static bool traceback_enabled        = false;
bool log_remote_enabled = false;


typedef struct
{
    bool        enabled;
    ev_stat     stat;
    const char *state_file_path;
    char        severity_string[LOG_SEVERITY_STR_MAX];
    char        severity_remote[LOG_SEVERITY_STR_MAX];
    const char *trigger_directory;
    int         trigger_value;
    void        (*trigger_callback)(FILE *fp);
} log_dynamic_t;


ds_dlist_t log_logger_list = DS_DLIST_INIT(logger_t, logger_node);

static log_dynamic_t log_dynamic;

static const char *log_name = ""; // process name

const char* log_get_name()
{
    return log_name;
}

static void _log_sink_severity_set_default(log_sink_t sink);

/**
 * Severity per-module
 */
static void log_init(char *name)
{
    // set default TZ environment variable, because otherwise localtime() would
    // stat("/etc/localtime") on every call thus impacting cpu usage.
    // Note: using overwrite=0, so if TZ is already set externally it's not overwritten
    setenv("TZ", ":/etc/localtime", 0);

    _log_sink_severity_set_default(LOG_SINK_LOCAL);
    _log_sink_severity_set_default(LOG_SINK_REMOTE);

    /* enable it */
    log_enabled  = true;

    log_name = name;

    memset(&log_dynamic, 0, sizeof(log_dynamic_t));
}

/**
 * See LOG_OPEN_ macros for flags, 0 or LOG_OPEN_DEFAULT will use the defaults (log to syslog and to
 * stdout (only if filedescriptor 0 is a TTY)
 */
bool log_open(char *name, int flags)
{
    log_init(name);

    openlog(name, LOG_NDELAY|LOG_PID, LOG_USER);

    /* Install default loggers */
    static logger_t logger_syslog;
    static logger_t logger_stdout;
    static logger_t logger_remote;
    static logger_t logger_traceback;

    if ((flags == 0) || (flags & LOG_OPEN_DEFAULT))
    {
        flags |= LOG_OPEN_SYSLOG;
        if (isatty(0)) flags |= LOG_OPEN_STDOUT;
        flags |= LOG_OPEN_REMOTE;
    }

    if (flags & LOG_OPEN_SYSLOG)
    {
        logger_syslog_new(&logger_syslog);
        log_register_logger(&logger_syslog);
    }

    if (flags & LOG_OPEN_STDOUT)
    {
        logger_stdout_new(&logger_stdout, flags & LOG_OPEN_STDOUT_QUIET);
        log_register_logger(&logger_stdout);
    }

#ifdef BUILD_REMOTE_LOG
    if (flags & LOG_OPEN_REMOTE)
    {
        // disable remote for QM
        if (strcmp(name, "QM") != 0) {
            logger_remote_new(&logger_remote);
            log_register_logger(&logger_remote);
        }
    }
#endif

    traceback_enabled = logger_traceback_new(&logger_traceback);
    log_register_logger(&logger_traceback);

    return true;
}

void log_register_logger(logger_t *logger)
{
    ds_dlist_insert_tail(&log_logger_list, logger);
}

void log_unregister_logger(logger_t *logger)
{
    ds_dlist_remove(&log_logger_list, logger);
}

/**
 * Map the severity name to the severty entry structure
 */
log_severity_entry_t *log_severity_get_by_name(char *name)
{
    log_severity_entry_t    *se;
    log_severity_t          s;

    for (s = 0;s < LOG_SEVERITY_LAST;s++) {
        se = &log_severity_table[s];
        if (!strcasecmp(se->name, name)) {
            return se;
        }
    }

    return NULL;
}
/**
 * Map the severity id to the severty entry structure
 */
log_severity_entry_t *log_severity_get_by_id(log_severity_t id)
{
    if (id >= LOG_SEVERITY_LAST) return NULL;

    return &log_severity_table[id];
}


log_module_entry_t* _log_sink_get(log_sink_t sink, char **name)
{
    if (name) *name = "";
    switch (sink)
    {
        case LOG_SINK_LOCAL:
            return log_module_table;
        case LOG_SINK_REMOTE:
            if (name) *name = "remote ";
            return log_module_remote;
    }
    LOG(WARNING, "Unknown log sink: %d", sink);
    return NULL;
}


static void _log_sink_severity_set(log_sink_t sink, log_severity_t s)
{
    char *sink_name;
    log_module_t mod;
    log_module_entry_t *module_table = _log_sink_get(sink, &sink_name);
    if (!module_table) return;

    /* The default log severity cannot be set to DEFAULT; demote it to INFO */
    if (s == LOG_SEVERITY_DEFAULT) s = LOG_SEVERITY_INFO;

    for (mod = 0; mod < LOG_MODULE_ID_LAST; mod++)
    {
        /* set severity for all modules         */
        module_table[mod].severity = s;
    }
    if (sink == LOG_SINK_REMOTE && s > LOG_SEVERITY_DISABLED)
    {
        log_remote_enabled = true;
    }
}

static void _log_sink_severity_set_default(log_sink_t sink)
{
    log_severity_t severity = LOG_SEVERITY_DISABLED;
    switch (sink) {
        case LOG_SINK_LOCAL:
            severity = LOG_SEVERITY_DEFAULT;
            break;
        case LOG_SINK_REMOTE:
            severity = LOG_SEVERITY_DISABLED;
            log_remote_enabled = false;
            break;
    }
    _log_sink_severity_set(sink, severity);
}

void log_severity_set(log_severity_t s)
{
    LOGD("Log severity: %s", log_severity_str(s));
    _log_sink_severity_set(LOG_SINK_LOCAL, s);
}


log_severity_t log_severity_get(void)
{
    return LOG_SEVERITY_DEFAULT;
}

static void _log_sink_module_severity_set(log_sink_t sink, log_module_t mod, log_severity_t sev)
{
    char *sink_name;
    log_module_entry_t *module_table = _log_sink_get(sink, &sink_name);
    if (!module_table) return;

    if (mod >= LOG_MODULE_ID_LAST)
    {
        return;
    }

    module_table[mod].severity = sev;

    if (sink == LOG_SINK_REMOTE && sev > LOG_SEVERITY_DISABLED)
    {
        log_remote_enabled = true;
    }
}

void log_module_severity_set(log_module_t mod, log_severity_t sev)
{
    LOGD("Log severity %s=%s", log_module_str(mod), log_severity_str(sev));

    _log_sink_module_severity_set(LOG_SINK_LOCAL, mod, sev);
}

log_severity_t log_module_severity_get(log_module_t mod)
{
    if (mod >= LOG_MODULE_ID_LAST)
    {
        return LOG_SEVERITY_DEFAULT;
    }

    return log_module_table[mod].severity;
}

/**
 * Parse a severity string definition and set the severity levels accordingly.
 *
 * @p sevstr must be in the following formats
 *
 * [MODULE:]SEVERITY,[MODULE:]SEVERITY,...
 *
 * If MODULE is not specified, the default severity is modified.
 */
bool log_severity_parse_sink(log_sink_t sink, char *sevstr)
{
    char    psevstr[LOG_SEVERITY_STR_MAX];
    char   *tsevstr;
    char   *token;
    char   *sink_name;

    // start with default then apply parsed settings
    _log_sink_severity_set_default(sink);
    _log_sink_get(sink, &sink_name);

    if (sevstr == NULL) return false;

    if (strlen(sevstr) >= LOG_SEVERITY_STR_MAX)
    {
        LOG(ERR, "Severity string is too long.");
        return false;
    }

    STRSCPY(psevstr, sevstr);

    token = strtok_r(psevstr, ", ", &tsevstr);

    while (token != NULL)
    {
        log_severity_t sev;
        log_module_t       mod;

        char *arg1 = NULL;
        char *arg2 = NULL;


        arg1 = token;
        arg2 = strchr(token, ':');

        if (arg2 == NULL)
        {
            /* If arg2 is NULL, it means we don't have a ':' string; this is the case where only the severity is specified in arg 1*/
            sev = log_severity_fromstr(arg1);

            if (sev != LOG_SEVERITY_LAST)
            {
                LOG(NOTICE, "%sDefault severity set: %s", sink_name, log_severity_str(sev));
                _log_sink_severity_set(sink, sev);
            }
            else
            {
                LOG(ERR, "Invalid default severity: %s", arg1);
                return false;
            }
        }
        else
        {
            /* String contains a colon. arg2 points to the ':' after the module name. Split the string,
             * so that arg1 points to the module string and arg2 points to the severity string */
            *arg2++ = '\0';

            mod = log_module_fromstr(arg1);
            sev = log_severity_fromstr(arg2);

            if (sev != LOG_SEVERITY_LAST && mod != LOG_MODULE_ID_LAST)
            {
                LOG(NOTICE, "%sModule log severity changed. module=%s|severity=%s",
                        sink_name, log_module_str(mod), log_severity_str(sev));

                _log_sink_module_severity_set(sink, mod, sev);
            }
            else
            {
                LOG(ERR, "Invalid module severity. module=%s|severity=%s", arg1, arg2);
                return false;
            }
        }

        token = strtok_r(NULL, ", ", &tsevstr);
    }

    return true;
}

bool log_severity_parse(char *sevstr)
{
    return log_severity_parse_sink(LOG_SINK_LOCAL, sevstr);
}

void log_close()
{
    LOG_MODULE_MESSAGE(NOTICE, LOG_MODULE_ID_COMMON, "log functionality closed");
    log_enabled = false;
}

static bool log_any_sink_match(log_severity_t sev, log_module_t module)
{
    bool match = false;
    // local
    if (sev <= log_module_table[module].severity) {
        match = true;
    }
    // remote
    if (sev <= log_module_remote[module].severity) {
        match = true;
    }
    if (traceback_enabled) {
        match = true;
    }
    return match;
}

#ifdef BUILD_LOG_MEMINFO
#if __GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 6)
#pragma message("LOG MEMINFO ENABLED")
#endif
/* format a short mem info string, store in "str" at offset "pos"
 * {VmSize VmData delta-VmSize delta-VmData}
 */
void log_meminfo(char *str, int size, int pos, const char *str_prefix, const char *str_append)
{
    static int last_size, last_data, last_rss;

    int fd;
    int sz;
    char buf[1024];
    char *s;
    int vmsize = 0, vmdata = 0, vmrss = 0;
    int d_size = 0, d_data = 0, d_rss = 0;

    if (pos >= size) return;
    str += pos;
    size -= pos;

    *str = 0;
    fd = open("/proc/self/status", O_RDONLY);
    if (!fd) return;

    sz = read(fd, buf, sizeof(buf) - 1);
    if (sz > 0) {
        buf[sz] = 0;
        s = strstr(buf, "VmSize:");
        if (s) s = strchr(s, ':') + 1;
        vmsize = atoi(s);
        s = strstr(buf, "VmData:");
        if (s) s = strchr(s, ':') + 1;
        vmdata = atoi(s);
        s = strstr(buf, "VmRSS:");
        if (s) s = strchr(s, ':') + 1;
        vmrss = atoi(s);
        d_size = vmsize - last_size;
        d_data = vmdata - last_data;
        d_rss = vmrss - last_rss;
        last_size = vmsize;
        last_data = vmdata;
        last_rss = vmrss;
    }
    close(fd);

    // %+d = print a + sign for positive numbers
    snprintf(str, size, "%s{%d %d %d %+d %+d %+d}%s",
            str_prefix ? str_prefix : "",
            vmsize, vmdata, vmrss,
            d_size, d_data, d_rss,
            str_append ? str_append : "");
}
#endif

void mlog(log_severity_t sev,
          log_module_t module,
          const char  *fmt, ...)
{
    char            buff[LOGGER_BUFF_LEN];
    char            timestr[80];
    struct tm             *lt;
    time_t                 t;
    va_list                args;
    char           *strip;
    log_severity_entry_t *se;
    char           *tag;

    // Save errno, so that log does not overwrite it
    int save_errno = errno;

    if (false == log_enabled) {
        return;
    }

    if (sev == LOG_SEVERITY_DISABLED) {
        return;
    }

    if (module > LOG_MODULE_ID_LAST) module = LOG_MODULE_ID_MISC;

    if (!log_any_sink_match(sev, module)) {
        return;
    }

    se = &log_severity_table[sev];
    tag = log_module_table[module].module_name;
    t = time_real();
    lt = localtime(&t);

    strftime(timestr, sizeof(timestr), "%d %b %H:%M:%S %Z", lt);

    // format
    va_start(args, fmt);
    vsnprintf(buff, sizeof(buff), fmt, args);
    va_end(args);

    // chop \r\n
    strip = &buff[strlen(buff) - 1];
    while ((strip > buff) && ((*strip == LF) || (*strip == CR)))
        *strip = NUL;

    // pretty print
    char se_tag[64];
    char *spaces = "          ";
    size_t se_len = 16;
    int scnt = 1;

    if ((strlen(se->name) + strlen(tag) + 2) < se_len) {
        scnt = se_len - (strlen(se->name) + strlen(tag) + 2);
    }

    snprintf(se_tag, sizeof(se_tag), "<%s>%.*s%s", se->name, scnt, spaces, tag);

#ifdef BUILD_LOG_MEMINFO
    // append debug mem info
    log_meminfo(se_tag, sizeof(se_tag), strlen(se_tag), " ", "");
#endif

    /* Craft the message */
    logger_msg_t msg;

    msg.lm_severity = sev;
    msg.lm_module = module;
    msg.lm_module_name = log_module_table[module].module_name;
    msg.lm_tag = se_tag;
    msg.lm_timestamp = timestr;
    msg.lm_text = buff;

    /* Feed messages to the registered loggers */
    logger_t *plog;
    ds_dlist_foreach(&log_logger_list, plog)
    {
        if (plog->match_fn) {
            if (!plog->match_fn(sev, module)) continue;
        } else {
            if (sev > log_module_table[module].severity) continue;
        }
        plog->logger_fn(plog, &msg);
    }

    // restore saved errno value
    errno = save_errno;

    return;
}

log_module_t log_module_fromstr(char *str)
{
    int ii;

    for (ii = 0; ii < LOG_MODULE_ID_LAST; ii++)
    {
        if (strcasecmp(str, log_module_table[ii].module_name) == 0)
        {
            return log_module_table[ii].module;
        }
    }

    return LOG_MODULE_ID_LAST;
}

log_severity_t log_severity_fromstr(char *str)
{
    int ii;

    for (ii = 0; ii < LOG_SEVERITY_LAST; ii++)
    {
        if (strcasecmp(str, log_severity_table[ii].name) == 0)
        {
            return log_severity_table[ii].s;
        }
    }

    return LOG_SEVERITY_LAST;
}

char *log_module_str(log_module_t mod)
{
    if (mod >= LOG_MODULE_ID_LAST) mod = LOG_MODULE_ID_MISC;

    return log_module_table[mod].module_name;
}

char *log_severity_str(log_severity_t sev)
{
    if (sev >= LOG_SEVERITY_LAST) sev = LOG_SEVERITY_DEFAULT;

    return log_severity_table[sev].name;
}

bool log_isenabled()
{
    return log_enabled;
}

/**
 * Dynamic logging state handling
 */

static bool log_dynamic_state_parse_severity(json_t *loggers_json,
        const char *name, const char *default_name,
        int *new_trigger, char *new_severity, int new_severity_len)
{
    // Get settings for our logger if there is any.
    json_t *logger = json_object_get(loggers_json, name);
    if (!logger) {
        // If process settings don't exist, use "DEFAULT" entry
        logger = json_object_get(loggers_json, default_name);
        if (!logger) {
            // No setting found
            return false;
        }
    }

    // Get new severity
    json_t *severity = json_object_get(logger, "log_severity");
    if (severity && json_is_string(severity))
    {
        strscpy(new_severity,
                json_string_value(severity),
                new_severity_len);
    }

    // Get new trigger
    json_t *trigger = json_object_get(logger, "log_trigger");
    if (trigger && json_is_integer(trigger))
    {
        if (new_trigger) {
            *new_trigger = json_integer_value(trigger);
        }
    }

    return true;
}


static void log_set_severity_if_changed(log_sink_t sink, char *new_severity)
{
    char *severity_str = NULL;
    int severity_size;
    char *sink_name;
    _log_sink_get(sink, &sink_name);
    switch (sink) {
        case LOG_SINK_LOCAL:
            severity_str = log_dynamic.severity_string;
            severity_size = sizeof(log_dynamic.severity_string);
            break;
        case LOG_SINK_REMOTE:
            severity_str = log_dynamic.severity_remote;
            severity_size = sizeof(log_dynamic.severity_remote);
            break;
    }
    if (!severity_str) {
        LOGW("Unknown sink %d", sink);
        return;
    }
    // set severity if changed
    if (strcmp(new_severity, severity_str))
    {
        LOGN("%sLog severity changed from \"%s\" to \"%s\".",
                sink_name, severity_str, new_severity);
        if (!log_severity_parse_sink(sink, new_severity))
        {
            LOGW("%sFailed to parse log severity '%s'", sink_name, new_severity);
        }
        strscpy(severity_str, new_severity, severity_size);
    }
}

static bool log_dynamic_update(int *new_trigger)
{
    *new_trigger = 0;
    char new_severity[LOG_SEVERITY_STR_MAX] = "";
    char remote_severity[LOG_SEVERITY_STR_MAX] = "";

    if (!log_dynamic.state_file_path) {
        return false;
    }
    LOGT("Re-reading logging state file %s.", log_dynamic.state_file_path);

    json_t *loggers_json = json_load_file(log_dynamic.state_file_path, 0, NULL);
    if (!loggers_json) {
        LOGD("Unable to read dynamic log state!");
        goto out;
    }

    // local
    log_dynamic_state_parse_severity(loggers_json, log_name, LOG_DEFAULT_ENTRY,
            new_trigger, new_severity, sizeof(new_severity));

    // remote
    char remote_name[64];
    snprintf(remote_name, sizeof(remote_name), "%s_%s", LOG_DEFAULT_REMOTE, log_name);
    log_dynamic_state_parse_severity(loggers_json, remote_name, LOG_DEFAULT_REMOTE,
            NULL, remote_severity, sizeof(remote_severity));

    json_decref(loggers_json);

out:
    // if reading is not successful because file or record was deleted
    // default values need to be applied

    log_set_severity_if_changed(LOG_SINK_LOCAL, new_severity);
    log_set_severity_if_changed(LOG_SINK_REMOTE, remote_severity);

    return true;
}


static void log_dynamic_full_path_get(char *buf, int len)
{
    time_t now;
    struct tm ts;
    time(&now);
    ts = *localtime(&now);

    char timestamp[64];
    strftime(timestamp, sizeof(timestamp), "%Y%m%d-%H%M%S", &ts);

    // Create full path
    //   <directory>/<name>-<timestamp>.dump
    snprintf(buf, len, "%s/%s-%s.dump",
             log_dynamic.trigger_directory,
             log_name,
             timestamp);
}


static void log_dynamic_state_handler(struct ev_loop *loop,
                                      ev_stat *watcher,
                                      int revents)
{
    int     new_trigger;

    if (!log_dynamic_update(&new_trigger)) {
        return;
    }

    // Trigger logging callback in case it was requested. Logging
    // callback expects file pointer and should dump information
    // in this file.
    if (log_dynamic.trigger_callback &&
        log_dynamic.trigger_directory &&
        new_trigger &&
        new_trigger > log_dynamic.trigger_value)
    {
        char full_path[LOG_TRIGGER_DIR_MAX * 2];
        log_dynamic_full_path_get(full_path, sizeof(full_path));

        LOGN("Module requested to dump information into %s", full_path);

        FILE *fp = fopen(full_path, "w");
        if (fp) {
            log_dynamic.trigger_callback(fp);
            fclose(fp);
        } else {
            LOGE("Unable to open file %s for dumping information! [%s]",
                 full_path, strerror(errno));
        }
    }

    // Update global values
    log_dynamic.trigger_value = new_trigger;

}

static bool log_dynamic_init()
{
    log_dynamic.state_file_path = target_log_state_file();
    if (!log_dynamic.state_file_path)
    {
        // On this target we don't enable dynamic log handler
        return false;
    }
    return true;
}

static void log_dynamic_handler_init(struct ev_loop *loop)
{
    if (!log_dynamic.enabled)
    {
        if (!log_dynamic_init()) return;

        // Init local state
        int   new_trigger;
        log_dynamic_update(&new_trigger);

        log_dynamic.enabled           = true;
        log_dynamic.trigger_value     = new_trigger;
        log_dynamic.trigger_directory = target_log_trigger_dir();

        // Init timer
        ev_stat_init(&log_dynamic.stat,
                     log_dynamic_state_handler,
                     log_dynamic.state_file_path,
                     1.0);
        ev_stat_start(loop, &log_dynamic.stat);
    }
}

/**
 * Register your logger to dynamic log severity updates.
 */
bool log_register_dynamic_severity(struct ev_loop *loop)
{
    log_dynamic_handler_init(loop);
    return true;
}

/**
 * Register your logger to dynamic data dump trigger callbacks.
 */
bool log_register_dynamic_trigger(struct ev_loop *loop, void (*callback)(FILE *fp))
{
    log_dynamic.trigger_callback = callback;

    log_dynamic_handler_init(loop);
    return true;
}

/**
 * Sets dynamic log severity. Note that this function can be used by
 * short running programs to set dynamic log severity during runtime.
 */
bool log_severity_dynamic_set()
{
    int  new_trigger; // Not used
    if (!log_dynamic_init()) return false;
    return log_dynamic_update(&new_trigger);
}
