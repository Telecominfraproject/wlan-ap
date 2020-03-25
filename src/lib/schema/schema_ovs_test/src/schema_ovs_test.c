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
#include <stdbool.h>
#include <stdint.h>
#include <stdarg.h>
#include <assert.h>
#include <errno.h>
#include <string.h>
#include <getopt.h>
#include <ctype.h>

#include "log.h"
#include "os_time.h"
#include "schema.h"
#include "ovsdb.h"
#include "ovsdb_table.h"
#include "pjs_undef.h"

#define PROG_NAME "schema_tt"

typedef enum
{
    CMD_NONE,
    CMD_PRINT,
    CMD_MONITOR,
    CMD_LIST,
    CMD_X,
} cmd_t;

cmd_t opt_cmd         = CMD_NONE;
int opt_verbose_level = LOG_SEVERITY_NOTICE;
char *opt_table       = NULL; // NULL = all tables
bool opt_new          = true;
bool opt_old          = true;
bool opt_partial      = false;
bool opt_present      = false; // only print if present
bool opt_changed      = false; // only print if changed
bool opt_required     = false; // print required even if not changed
bool print_present    = false;
bool print_changed    = false;
time_t time_start;
time_t time_new = 3;

char **opt_ignore;
int opt_ignore_num;

void print_usage(char *err)
{
    if (err) {
        printf("ERROR: %s\n\n", err);
    }
    printf("%s [COMMAND] [OPTIONS] [TABLE]\n", PROG_NAME);
    printf("\n");
    printf("COMMAND:\n");
    printf("  -h|--help     help\n");
    printf("  -p|--print    print table\n");
    printf("  -m|--monitor  monitor table\n");
    printf("  -l|--list     list all tables\n");
    // todo: -i insert -u update -U upsert -d delete
    printf("\n");
    printf("OPTIONS:\n");
    printf("  -v|--verbose   increase log level\n");
    printf("  -n|--no-new    don't report NEW events for first %d seconds\n", (int)time_new);
    printf("  -o|--no-old    don't print old record values\n");
    printf("  -c|--changed   only print changed fields\n");
    printf("  -r|--present   only print present fields\n");
    printf("  -q|--required  print required even if not changed when -c enabled\n");
    printf("  --partial      allow partial update\n");
    printf("  -I TABLE       Ignore monitor a table when all are selected\n");
    printf("  -I .FIELD      Ignore monitor a field for all tables\n");
    printf("  -I TABLE.FIELD Ignore monitor a field for specified table\n");
    printf("  -M             Monitor compact output\n");
    printf("                 same as: -m -nocrq -I ._version -I Manager.status\n");
    printf("\n");
    printf("TABLE specifies an OVS table; If not specified use all\n");
    printf("\n");
    printf("FIELD FLAGS:\n");
    printf("  <req>: required\n");
    printf("  p:     _present\n");
    printf("  c:     _changed\n");
    printf("  e:     _exists\n");
    printf("  len:   _len - set or map length\n");
    printf("\n");
    exit(1);
}

void parse_opt(int argc, char *argv[])
{
    int     opt;

    struct option ovsh_long_opts[] =
    {
        { .name = "help",           .has_arg = 0, .val = 'h', },
        { .name = "verbose",        .has_arg = 0, .val = 'v', },
        { .name = "print",          .has_arg = 0, .val = 'p', },
        { .name = "monitor",        .has_arg = 0, .val = 'm', },
        { .name = "no-new",         .has_arg = 0, .val = 'n', },
        { .name = "partial",        .has_arg = 0, .val = 1001, },
        { .name = "present",        .has_arg = 0, .val = 'r', },
        { .name = "changed",        .has_arg = 0, .val = 'c', },
        { .name = "required",       .has_arg = 0, .val = 'q', },
        { NULL, 0, 0, 0 },
    };
    do {
        opt = getopt_long(argc, argv, "hvpmnrqlocI:Mx", ovsh_long_opts, NULL);
        switch (opt)
        {
            case 'h':
                print_usage(NULL);
                break;

            case 'v':
                opt_verbose_level++;
                break;

            case 'p':
                opt_cmd = CMD_PRINT;
                break;

            case 'm':
                opt_cmd = CMD_MONITOR;
                break;

            case 'l':
                opt_cmd = CMD_LIST;
                break;

            case 'n':
                opt_new = false;
                break;

            case 'o':
                opt_old = false;
                break;

            case 'c':
                opt_changed = true;
                break;

            case 'I':
                opt_ignore = realloc(opt_ignore, (opt_ignore_num+1)*sizeof(char*));
                opt_ignore[opt_ignore_num] = optarg;
                opt_ignore_num++;
                break;

            case 'M':
                opt_cmd = CMD_MONITOR;
                opt_new = false;
                opt_old = false;
                opt_changed = true;
                opt_present = true;
                opt_required = true;
                opt_ignore = realloc(opt_ignore, (opt_ignore_num+2)*sizeof(char*));
                opt_ignore[opt_ignore_num++] = "._version";
                opt_ignore[opt_ignore_num++] = "Manager.status";
                break;

            case 1001:
                opt_partial = true;
                break;

            case 'r':
                opt_present = true;
                break;

            case 'q':
                opt_required = true;
                break;

            case 'x':
                opt_cmd = CMD_X;
                break;

            case -1:
                break;

            default:
                print_usage("Invalid option.");
                break;
        }
    } while (opt >= 0);

    if (optind < argc) {
        opt_table = argv[optind];
    }
}

#define DECL_TABLE(TABLE) \
ovsdb_table_t table_##TABLE;

#define INIT_TABLE(TABLE) \
OVSDB_TABLE_INIT_NO_KEY(TABLE); \
if (opt_partial) table_##TABLE.partial_update = true;

// Print

#define PRINT_COMMON(FIELD, q) printf(" %s p:%d c:%d e:%d    %-24s: ", \
        q?"     ":"<req>", \
        rec->FIELD##_present, \
        rec->FIELD##_changed, \
        rec->FIELD##_exists, \
        #FIELD);

#define PRINT_COMMON_LEN(FIELD) printf(" %s p:%d c:%d len:%-2d %-24s: ", \
        "     ", \
        rec->FIELD##_present, \
        rec->FIELD##_changed, \
        rec->FIELD##_len, \
        #FIELD);

#define PRINT_INT(name)         printf("%d", rec->name);
#define PRINT_BOOL(name)        printf("%s", rec->name ? "true" : "false");
#define PRINT_REAL(name)        printf("%f", rec->name);
#define PRINT_STRING(name)      printf("%s", rec->name);
#define PRINT_UUID(name)        printf("%s", rec->name.uuid);

#define PRINT_TYPE(TYPE, name)  PRINT_##TYPE(name)

bool print_condition(const char *name, bool present, bool changed, bool optional)
{
    if (!strcmp(name,"_uuid")) return true;
    if (print_present && !present) return false;
    if (print_changed && !changed) {
       if (opt_required) {
           if (optional) return false;
       } else {
           return false;
       }
    }
    return true;
}

#define PRINT_CONDITION(name, OPT) \
        if (print_condition(#name, rec->name##_present, rec->name##_changed, OPT))

#define PRINT_BASIC(TYPE, name, OPT) \
            PRINT_CONDITION(name, OPT) { \
                PRINT_COMMON(name, OPT) \
                if (rec->name##_exists) { PRINT_TYPE(TYPE, name) } \
                printf("\n"); }

#define PRINT_SET(TYPE, name) \
            PRINT_CONDITION(name,1) { \
                PRINT_COMMON_LEN(name) \
                do{ \
                    int _i; \
                    printf("[ "); \
                    for (_i=0;_i<rec->name##_len;_i++) {\
                        PRINT_##TYPE(name[_i])\
                        if (_i < rec->name##_len-1) printf(", "); \
                        else printf(" "); \
                    } \
                    printf("]\n"); \
                } while (0); }

#define PRINT_XMAP(TYPE, KEY_FMT, name) \
            PRINT_CONDITION(name,1) { \
                PRINT_COMMON_LEN(name) \
                do{ \
                    int _i; \
                    char *_delim = (rec->name##_len <= 1) ? " " : "\n"; \
                    char *_pad = (rec->name##_len <= 1) ? "" : "                      "; \
                    printf("[%s", _delim); \
                    for (_i=0; _i < rec->name##_len; _i++) {\
                        printf("%s["KEY_FMT"]=", _pad, rec->name##_keys[_i]);\
                        PRINT_TYPE(TYPE, name[_i])\
                        if (_i < rec->name##_len-1) printf(",%s", _delim); \
                        else printf(" "); \
                    } \
                    printf("]\n"); \
                } while (0); }

#define PRINT_SMAP(TYPE, name) PRINT_XMAP(TYPE, "%s", name)

#define PRINT_DMAP(TYPE, name) PRINT_XMAP(TYPE, "%d", name)

#define PJS(TABLE,...) \
    __VA_ARGS__ \

#define PJS_OVS_INT(name)                   PRINT_BASIC(INT,name,0)
#define PJS_OVS_BOOL(name)                  PRINT_BASIC(BOOL,name,0)
#define PJS_OVS_REAL(name)                  PRINT_BASIC(REAL,name,0)
#define PJS_OVS_STRING(name, len)           PRINT_BASIC(STRING,name,0)
#define PJS_OVS_UUID(name)                  PRINT_BASIC(UUID,name,0)

#define PJS_OVS_INT_Q(name)                 PRINT_BASIC(INT,name,1)
#define PJS_OVS_BOOL_Q(name)                PRINT_BASIC(BOOL,name,1)
#define PJS_OVS_REAL_Q(name)                PRINT_BASIC(REAL,name,1)
#define PJS_OVS_STRING_Q(name, len)         PRINT_BASIC(STRING,name,1)
#define PJS_OVS_UUID_Q(name)                PRINT_BASIC(UUID,name,1)

#define PJS_OVS_SET_INT(name, sz)           PRINT_SET(INT, name)
#define PJS_OVS_SET_BOOL(name, sz)          PRINT_SET(BOOL, name)
#define PJS_OVS_SET_REAL(name, sz)          PRINT_SET(REAL, name)
#define PJS_OVS_SET_STRING(name, len, sz)   PRINT_SET(STRING, name)
#define PJS_OVS_SET_UUID(name, sz)          PRINT_SET(UUID, name)

#define PJS_OVS_SMAP_INT(name, sz)          PRINT_SMAP(INT, name)
#define PJS_OVS_SMAP_BOOL(name, sz)         PRINT_SMAP(BOOL, name)
#define PJS_OVS_SMAP_REAL(name, sz)         PRINT_SMAP(REAL, name)
#define PJS_OVS_SMAP_STRING(name, len, sz)  PRINT_SMAP(STRING, name)
#define PJS_OVS_SMAP_UUID(name, sz)         PRINT_SMAP(UUID, name)

#define PJS_OVS_DMAP_INT(name, sz)          PRINT_DMAP(INT, name)
#define PJS_OVS_DMAP_BOOL(name, sz)         PRINT_DMAP(BOOL, name)
#define PJS_OVS_DMAP_REAL(name, sz)         PRINT_DMAP(REAL, name)
#define PJS_OVS_DMAP_STRING(name, len, sz)  PRINT_DMAP(STRING, name)
#define PJS_OVS_DMAP_UUID(name, sz)         PRINT_DMAP(UUID, name)

void print_table_hdr(char *table, int i, int count)
{
    printf("TABLE: %s", table);
    if (count > 1) {
        printf(" [%d/%d]", i+1, count);
    }
    printf("\n");
}

#define IMPL_PRINT_TABLE(TABLE) \
void print_##TABLE(struct schema_##TABLE *rec, bool hdr, int i, int count) \
{ \
    if (hdr) print_table_hdr(#TABLE, i, count); \
    PJS_SCHEMA_##TABLE \
}

// END Print

#define GET_AND_PRINT(TABLE) \
    if (opt_table == NULL || !strcmp(#TABLE, opt_table)) { \
        list = ovsdb_table_select_where(&table_##TABLE, NULL, &count); \
        if (!list) { \
            printf("TABLE: %s [0]\n\n", #TABLE); \
        } else { \
            struct schema_##TABLE *p_##TABLE = list; \
            for (i=0; i<count; i++) { \
                print_##TABLE(&p_##TABLE[i], true, i, count); \
            } \
            printf("\n"); \
            free(list); \
        } \
    }

// Monitor

#define IMPL_MONITOR_CALLBACK(TABLE)                \
void callback_##TABLE(ovsdb_update_monitor_t *mon,  \
        struct schema_##TABLE *old_rec,             \
        struct schema_##TABLE *rec)                 \
{                                                   \
    bool old_printed = false;                       \
    if (!opt_new && mon->mon_type == OVSDB_UPDATE_NEW && (time_monotonic() - time_start <= time_new)) return; \
    printf("UPDATE: %s %s\n", ovsdb_update_type_to_str(mon->mon_type), mon->mon_table); \
    if (strcmp(mon->mon_table, #TABLE)) LOGW("TABLE %s != %s", mon->mon_table, #TABLE); \
    print_changed = false;                          \
    if (mon->mon_type == OVSDB_UPDATE_MODIFY) {     \
        print_present = opt_changed;                \
        if (opt_old) {                              \
            printf("OLD:\n");                       \
            print_##TABLE(old_rec, false, 0, 0);    \
            old_printed = true;                     \
        }                                           \
        print_changed = opt_changed;                \
    }                                               \
    if (old_printed) printf("NEW:\n");              \
    printf("%21s %-24s: %s\n", "", "_update_type", ovsdb_update_type_to_str(rec->_update_type)); \
    print_present = opt_present;                    \
    print_##TABLE(rec, false, 0, 0);                \
    printf("\n");                                   \
}

bool is_table_monitored(char *table)
{
    if (opt_table && strcmp(table, opt_table)) {
        // not selected
        return false;
    }
    // check ignore list
    int i;
    for (i=0; i<opt_ignore_num; i++) {
        if (!strcmp(table, opt_ignore[i])) {
            return false;
        }
    }
    return true;
}

char **get_table_filter(char *table)
{
    static char *f[SCHEMA_FILTER_LEN];
    int i, n;
    int len = strlen(table);
    n = 0;
    f[0] = "-";
    for (i=0; i < opt_ignore_num; i++) {
        if (n >= SCHEMA_FILTER_LEN - 2) break;
        if (opt_ignore[i][0] == '.') {
            n++;
            f[n] = &opt_ignore[i][1];
            continue;
        }
        if (!strncmp(table, opt_ignore[i], len) && opt_ignore[i][len] == '.') {
            n++;
            f[n] = &opt_ignore[i][len + 1];
        }
    }
    f[n+1] = NULL;
    if (!n) return NULL;
    return f;
}


#define DO_MONITOR_TABLE(TABLE) \
    if (is_table_monitored(#TABLE)) { \
        char **f = get_table_filter(#TABLE); \
        if (f) { \
            OVSDB_TABLE_MONITOR_F(TABLE, f); \
        } else { \
            OVSDB_TABLE_MONITOR(TABLE, false); \
        } \
    }


SCHEMA_LISTX(DECL_TABLE)
SCHEMA_LISTX(IMPL_PRINT_TABLE)
SCHEMA_LISTX(IMPL_MONITOR_CALLBACK)

void table_init()
{
    SCHEMA_LISTX(INIT_TABLE)
}

void cmd_init()
{
    table_init();
}

void cmd_print()
{
    void *list;
    int count;
    int i;
    SCHEMA_LISTX(GET_AND_PRINT)
}

void cmd_monitor()
{
    ovsdb_init(PROG_NAME);
    SCHEMA_LISTX(DO_MONITOR_TABLE)
    time_start = time_monotonic();
    ev_run(EV_DEFAULT, 0);
}

#define PRINT_TABLE_NAME(TABLE) printf("%s\n", #TABLE);
void cmd_list()
{
    SCHEMA_LISTX(PRINT_TABLE_NAME)
}

// test
void do_x()
{
    struct schema_Wifi_Master_State pstate;
    MEMZERO(pstate);
    schema_Wifi_Master_State_mark_all_present(&pstate);
    pstate._partial_update = true;
    pstate.port_state_present = false;
    pstate.if_uuid_present = false;
    STRSCPY(pstate.port_state, "active");
    STRSCPY(pstate.if_name, "eth0");
    STRSCPY(pstate.if_type, "vif");
    ovsdb_table_upsert(&table_Wifi_Master_State, &pstate, false);
}

int main(int argc, char *argv[])
{
    parse_opt(argc, argv);
    log_open(PROG_NAME, 0);
    log_severity_set(opt_verbose_level);

    cmd_init();

    switch (opt_cmd)
    {
        case CMD_NONE:
            print_usage("Specify command.");
            break;
        case CMD_PRINT:
            cmd_print();
            break;
        case CMD_MONITOR:
            cmd_monitor();
            break;
        case CMD_LIST:
            cmd_list();
            break;
        case CMD_X:
            do_x();
            break;
    }

    return 0;
}

