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

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/wait.h>

#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdarg.h>
#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <getopt.h>
#include <ctype.h>

#include <jansson.h>

#include "os.h"
#include "os_socket.h"

#define OVSH_COL_NUM    256
#define OVSH_COL_STR   1024
#define UUID_STR_LEN     36

#define DEBUG(...) if (ovsh_opt_verbose) { fprintf(stderr, "[DEBUG] "); fprintf(stderr, __VA_ARGS__); }

typedef enum
{
    OVSH_FORMAT_JSON,
    OVSH_FORMAT_RAW,
    OVSH_FORMAT_CUSTOM,
    OVSH_FORMAT_TABLE,
    OVSH_FORMAT_COLUMN,
    OVSH_FORMAT_MULTI_LINE,
    OVSH_FORMAT_MULTI,
} ovsh_format_t;

static char     ovsh_opt_db[512] = "Open_vSwitch";
static bool     ovsh_opt_verbose = false;
static char    *ovsh_opt_format_output = NULL;
static ovsh_format_t ovsh_opt_format = OVSH_FORMAT_MULTI_LINE;
static bool     ovsh_opt_uuid_compact = true;
static bool     ovsh_opt_uuid_abbrev = true;
static long     ovsh_opt_timeout = 60*1000;
static bool     ovsh_opt_quiet = false;
static bool     ovsh_opt_wait_equals = true;
static int      ovsh_where_num = 0;
static char   **ovsh_where_expr = NULL;

static void     ovsh_usage(const char *fmt, ...);
static bool     ovsh_select(char *table, json_t *where, int coln, char *colv[]);
static bool     ovsh_insert(char *table, json_t *where, int coln, char *colv[], json_t *parents);
static bool     ovsh_update(char *table, json_t *where, int coln, char *colv[]);
static bool     ovsh_upsert(char *table, json_t *where, int coln, char *colv[], json_t *parents);
static bool     ovsh_wait(char *table, json_t *where, bool equals, int coln, char *colv[]);
static bool     ovsh_delete(char *table, json_t *where, int coln, char *colv[]);
static json_t  *json_value(char *str);
static bool     ovsh_parse_where_statement(json_t *where, char *_str, bool is_parent_where);
static bool     ovsh_parse_where(json_t *where, char *str, bool is_parent_where);
static bool     ovsh_parse_columns(json_t *columns, int argc, char *argv[]);
bool            ovsh_parse_mutations(json_t *mutations, int *colc, char *colv[]);
static bool     ovsh_parse_parent(json_t *parent_where,json_t *parent_table,
                                  json_t *parent_col, const char *_str);
static json_t  *ovsdb_json_exec(char *method, json_t *params);
static bool     ovsdb_json_error(json_t *jres);
static bool     ovsdb_json_show_count(json_t *jres);
static bool     ovsdb_json_show_uuid(json_t *jres, json_t **a_juuid);
static bool     ovsdb_json_show_result(json_t *jobj, json_t *columns);
static bool     ovsdb_json_show_result_json(json_t *jres);
static bool     ovsdb_json_show_result_raw(json_t *jobj, json_t *columns);
static bool     ovsdb_json_show_result_table(json_t *jobj, json_t *columns);
static bool     ovsdb_json_show_result_column(json_t *jrows, json_t *columns);
static bool     ovsdb_json_show_result_multi(json_t *jrows, json_t *columns, bool multi_line);
static bool     ovsdb_json_show_result_output(json_t *jrows, json_t *columns);
static int      systemvp(const char *file, char *argv[]);
bool ovsdb_json_get_result_rows(json_t *jobj, json_t **jrows);

/*
 * ===========================================================================
 *  OVSH entry points
 * ===========================================================================
 */
int main(int argc, char *argv[])
{
    int     opt;
    char   *pend;

    json_t *where = json_array();
    json_t *parents = json_array(); // relations to parent(s), if any

    assert(where != NULL);

    do
    {
        /*
         * Long options structure for parsing options
         */
        struct option ovsh_long_opts[] =
        {
            { .name = "where",          .has_arg = 1, .val = 'w', },
            { .name = "verbose",        .has_arg = 0, .val = 'v', },
            { .name = "db",             .has_arg = 1, .val = 'd', },
            { .name = "json",           .has_arg = 0, .val = 'j', },
            { .name = "raw",            .has_arg = 0, .val = 'r', },
            { .name = "column",         .has_arg = 0, .val = 'c', },
            { .name = "multi",          .has_arg = 0, .val = 'M', },
            { .name = "multi-line",     .has_arg = 0, .val = 'm', },
            { .name = "table",          .has_arg = 0, .val = 'T', },
            { .name = "uuid-compact",   .has_arg = 0, .val = 'u', },
            { .name = "no-uuid-compact",.has_arg = 0, .val = 'U', },
            { .name = "abbrev",         .has_arg = 0, .val = 'a', },
            { .name = "no-abbrev",      .has_arg = 0, .val = 'A', },
            { .name = "output",         .has_arg = 1, .val = 'o', },
            { .name = "timeout",        .has_arg = 1, .val = 't', },
            { .name = "quiet",          .has_arg = 0, .val = 'q', },
            { .name = "notequal",       .has_arg = 0, .val = 'n', },
            { .name = "parent",         .has_arg = 1, .val = 'p', },
            { NULL, 0, 0, 0 },
        };

        /*
         * Parse options
         */
        opt = getopt_long(argc, argv, "w:vd:t:jrcmMTuUo:qnaA:p", ovsh_long_opts, NULL);
        switch (opt)
        {
            case 'w':
                if (!ovsh_parse_where(where, optarg, false))
                {
                    ovsh_usage("Error parsing WHERE statement: %s", optarg);
                }

                break;

            case 'v':
                ovsh_opt_verbose = true;
                break;

            case 'd':
                if (strlen(optarg) + 1 > sizeof(ovsh_opt_db))
                {
                    ovsh_usage("Database string must not exceed %d bytes.",
                            sizeof(ovsh_opt_db));
                }

                DEBUG("Setting database to %s\n", optarg);

                strcpy(ovsh_opt_db, optarg);
                break;

            case 'j':
                ovsh_opt_format = OVSH_FORMAT_JSON;
                break;

            case 'r':
                ovsh_opt_format = OVSH_FORMAT_RAW;
                break;

            case 'o':
                ovsh_opt_format = OVSH_FORMAT_CUSTOM;
                if (ovsh_opt_format_output != NULL) free(ovsh_opt_format_output);
                ovsh_opt_format_output = strdup(optarg);
                break;

            case 'c':
                ovsh_opt_format = OVSH_FORMAT_COLUMN;
                break;

            case 'm':
                ovsh_opt_format = OVSH_FORMAT_MULTI_LINE;
                break;

            case 'M':
                ovsh_opt_format = OVSH_FORMAT_MULTI;
                break;

            case 'T':
                ovsh_opt_format = OVSH_FORMAT_TABLE;
                break;

            case 'u':
                ovsh_opt_uuid_compact = true;
                break;

            case 'U':
                ovsh_opt_uuid_compact = false;
                break;

            case 'a':
                ovsh_opt_uuid_abbrev = true;
                break;

            case 'A':
                ovsh_opt_uuid_abbrev = false;
                break;

            case 't':
                ovsh_opt_timeout = strtoul(optarg, &pend, 0);
                if (*pend != '\0')
                {
                    ovsh_usage("Invalid timeout: %s", optarg);
                }

                break;

            case 'q':
                /* Quiet operation -- report only errors */
                ovsh_opt_quiet = true;
                break;

            case 'n':
                ovsh_opt_wait_equals = false;
                break;

            case 'p':
                {
                    json_t *parent_where = json_array();
                    json_t *parent_table = json_string("");
                    json_t *parent_col = json_string("");

                    if (!ovsh_parse_parent(parent_where, parent_table, parent_col, optarg))
                    {
                        ovsh_usage("Error parsing --parent statement: %s", optarg);
                        break;
                    }

                    json_t *a_parent = json_object();
                    json_object_set_new(a_parent, "parent_table", parent_table);
                    json_object_set_new(a_parent, "parent_col", parent_col);
                    json_object_set_new(a_parent, "parent_where", parent_where);

                    json_array_append_new(parents, a_parent);
                }

                break;

            case -1:
                break;

            default:
                ovsh_usage("Invalid option.");
                break;
        }
    }
    while (opt >= 0);

    if (optind + 2 > argc)
    {
        ovsh_usage("Invalid number of arguments.");
    }

    // don't compact or abbrev uuid in display modes: raw, json, custom
    switch (ovsh_opt_format)
    {
        case OVSH_FORMAT_JSON:
        case OVSH_FORMAT_RAW:
        case OVSH_FORMAT_CUSTOM:
            ovsh_opt_uuid_compact = false;
            ovsh_opt_uuid_abbrev = false;
            break;
        default:
            break;
    }

    /*
     * From the reminder of the arguments, figure out the command build the column list
     */
    char   *cmd = argv[optind];
    char   *table = argv[optind + 1];
    int     colc = argc - optind - 2;
    char  **colv = argv + optind + 2;

    if (strcmp("select", cmd) == 0 || strcmp("s", cmd) == 0)
    {
        if (!ovsh_select(table, where, colc, colv))
        {
            return 1;
        }

    }
    else if (strcmp("insert", cmd) == 0 || strcmp("i", cmd) == 0)
    {
        if (!ovsh_insert(table, where, colc, colv, parents))
        {
            return 1;
        }
    }
    else if (strcmp("update", cmd) == 0 || strcmp("u", cmd) == 0)
    {
        if (!ovsh_update(table, where, colc, colv))
        {
            return 1;
        }
    }
    else if (strcmp("upsert", cmd) == 0 || strcmp("U", cmd) == 0)
    {
        if (!ovsh_upsert(table, where, colc, colv, parents))
        {
            return 1;
        }
    }
    else if (strcmp("wait", cmd) == 0 || strcmp("w", cmd) == 0)
    {
        if (!ovsh_wait(table, where, ovsh_opt_wait_equals, colc, colv))
        {
            return 1;
        }
    }
    else if (strcmp("delete", cmd) == 0 || strcmp("d", cmd) == 0)
    {
        if (!ovsh_delete(table, where, colc, colv))
        {
            return 1;
        }
    }
    else
    {
        ovsh_usage("Invalid command: %s", cmd);
    }

    return 0;
}

/*
 * Print usage screen and die
 */
void ovsh_usage(const char *fmt, ...)
{
    fprintf(stderr, "\n");
    fprintf(stderr, "ovsh COMMAND TABLE [-w|--where [COLUMN==VAL|COLUMN!=VAL]] [COLUMN[:=VAL]] [COLUMN[::OBJ]] [COLUMN[~=STRING]]...\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "COMMAND can be one of:\n");
    fprintf(stderr, "   - s/select: Read one or multiple rows\n");
    fprintf(stderr, "   - i/insert: Insert a new row\n");
    fprintf(stderr, "   - u/update: Update one or multiple rows\n");
    fprintf(stderr, "   - U/upsert: Update or insert one row\n");
    fprintf(stderr, "   - d/delete: Delete one or multiple rows\n");
    fprintf(stderr, "   - w/wait:   Perform a WAIT operation on one or multiple rows\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "TABLE specifies an OVS table; this is a required option.\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "Additional options are:\n");
    fprintf(stderr, "   -w EXPR | --where=EXPR\n");
    fprintf(stderr, "                   - Use a WHERE statement to filter rows (select, update, wait)\n");
    fprintf(stderr, "   -j|--json       - print the result in JSON format (select, insert, update)\n");
    fprintf(stderr, "   -T|--table      - print the result in table format (select)\n");
    fprintf(stderr, "   -r|--raw        - print the result in RAW format (select)\n");
    fprintf(stderr, "   -c|--column     - print the result in single COLUMN (select)\n");
    fprintf(stderr, "   -m|--multi-line - print the result in multi COLUMNS, split long lines [default] (select)\n");
    fprintf(stderr, "   -M|--multi      - print the result in multi COLUMNS, do not split long lines (select)\n");
    fprintf(stderr, "   -u|--uuid-compact    - print compact uuid [default] (select)\n");
    fprintf(stderr, "   -U|--no-uuid-compact - do not print compact uuid (select)\n");
    fprintf(stderr, "   -a|--abbrev     - abbreviate uuid to 1234~6789 [default] (select)\n");
    fprintf(stderr, "   -A|--no-abbrev  - do not abbreviate uuid (select)\n");
    fprintf(stderr, "   -o fmt | -output=fmt\n");
    fprintf(stderr, "                   - print the result in a custom printf-like format (select)\n");
    fprintf(stderr, "   -t MSEC | --timeout=MSEC\n");
    fprintf(stderr, "                   - The timeout in milliseconds for a wait command\n");
    fprintf(stderr, "   -n|--notequal   - Use the \"!=\" (not-equal) operator instead of \"==\" (wait)\n");
    fprintf(stderr, "   -v|--verbose    - Increase debugging level\n");
    fprintf(stderr, "   -q|--quiet      - Report only errors\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "Note: print modes: json, raw and custom do not compact and abbreviate uuid\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "Operators:\n");
    fprintf(stderr, "   ==  compare equal\n");
    fprintf(stderr, "   !=  compare not equal\n");
    fprintf(stderr, "   :=  assign value: bool, int or string\n");
    fprintf(stderr, "   ~=  assign value: string\n");
    fprintf(stderr, "   ::  assign value: json object\n");
    fprintf(stderr, "   :ins:  mutate map or set: insert json object (update,upsert,insert)\n");
    fprintf(stderr, "   :del:  mutate map or set: delete json object (update,upsert,insert)\n");
    fprintf(stderr, "   The := operator autodetects the value type (bool,int,string)\n");
    fprintf(stderr, "   To force the value type to a string either use :='\"5\"' or ~=5\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "Examples:\n");
    fprintf(stderr, "  Upsert:\n");
    fprintf(stderr, "    ovsh U AW_Debug -w name==SM log_severity:=INFO\n");
    fprintf(stderr, "  Mutate:\n");
    fprintf(stderr, "    ovsh u AWLAN_Node mqtt_settings:ins:'[\"map\",[[\"remote_log\",\"1\"]]]'\n");
    fprintf(stderr, "    ovsh u AWLAN_Node mqtt_settings:del:'[\"set\",[\"remote_log\"]]'\n");
    fprintf(stderr, "\n\n");

    if (*fmt != '\0')
    {
        va_list varg;

        va_start(varg, fmt);
        fprintf(stderr, "ERROR: ");
        vfprintf(stderr, fmt, varg);
        va_end(varg);
        fprintf(stderr, "\n\n");
    }

    exit(255);
}

/*
 * OVSDB Select method
 */
bool ovsdb_select(char *table, json_t *where, int coln, char *colv[],
        json_t **jresult, json_t **acolumns)
{
    *jresult = NULL;
    json_t *columns = json_array();
    assert(columns != NULL);
    *acolumns = columns;

    json_t *jparam = json_pack("[ s, { s:s, s:s, s:o } ]",
            ovsh_opt_db,
            "op", "select",
            "table", table,
            "where", where);

    if (jparam == NULL)
    {
        DEBUG("Error creating JSON-RPC parameters (SELECT).");
        return false;
    }

    if (coln > 0)
    {
        /* Parse columns */
        if (!ovsh_parse_columns(columns, coln, colv))
        {
            ovsh_usage("Error parsing columns.");
        }

        if (json_object_set_new(json_array_get(jparam, 1), "columns", columns) != 0)
        {
            DEBUG("Error appending columns.");
            return false;
        }
    }

    json_t *jres = ovsdb_json_exec("transact", jparam);
    if (!ovsdb_json_error(jres))
    {
        return false;
    }
    *jresult = jres;
    return true;
}

bool ovsh_select(char *table, json_t *where, int coln, char *colv[])
{
    json_t *jres;
    json_t *columns;
    if (!ovsdb_select(table, where, coln, colv, &jres, &columns))
    {
        return false;
    }

    return ovsdb_json_show_result(jres, columns);
}

/*
 * OVSDB Mutate method
 */
bool ovsh_mutate(char *table, json_t *where, json_t *mutations)
{
    if (json_array_size(mutations) == 0)
    {
        // nothing to do
        return true;
    }

    json_t *jparam = json_pack("[ s, { s:s, s:s, s:o, s:o } ]",
            ovsh_opt_db,
            "op", "mutate",
            "table", table,
            "where", where,
            "mutations", mutations);

    if (jparam == NULL)
    {
        DEBUG("Error creating JSON-RPC parameters (MUTATE).");
        return false;
    }

    json_t *jres = ovsdb_json_exec("transact", jparam);
    if (!ovsdb_json_error(jres))
    {
        return false;
    }

    printf("Mutated: ");
    if (!ovsdb_json_show_count(jres))
    {
        return false;
    }

    return true;
}

/*
 * OVSDB Insert method
 */
bool ovsh_insert(char *table, json_t *where, int coln, char *colv[], json_t *parents)
{
    (void)where; /* Not supported on inserts */

    if (coln <= 0)
    {
        ovsh_usage("Insert requires at least 1 column.");
        return false;
    }

    json_t *columns = json_object();
    json_t *mutations = json_array();
    assert(columns != NULL);

    // parse mutations
    if (!ovsh_parse_mutations(mutations, &coln, colv))
    {
        ovsh_usage("Error parsing columns.");
    }

    /* Parse columns */
    if (!ovsh_parse_columns(columns, coln, colv))
    {
        ovsh_usage("Error parsing columns.");
    }

    json_t *jparam = NULL;
    if (json_array_size(parents) > 0) // insert with parent(s)
    {
        jparam = json_array();
        json_array_append_new(jparam, json_string(ovsh_opt_db));

        json_t *jtable = json_pack("{ s:s, s:s, s:s, s:o }",
                "op", "insert",
                "table", table,
                "uuid-name", "new_table_uuid",
                "row", columns);

        json_array_append_new(jparam, jtable);

        unsigned i;
        for (i=0; i < json_array_size(parents); i++)
        {
            json_t *a_parent = json_array_get(parents, i);

            json_t *j_parent_table = json_object_get(a_parent, "parent_table"); // json string
            json_t *j_parent_col = json_object_get(a_parent, "parent_col");     // json string
            json_t *j_parent_where = json_object_get(a_parent, "parent_where"); // json array

            DEBUG("  [%d] You specified PARENT: parent_table=%s, "
                  "parent_col=%s, parent_where=%s\n", i,
                   json_string_value(j_parent_table),
                   json_string_value(j_parent_col),
                   json_dumps(j_parent_where, 0));

            json_t *mutations = json_pack("[ [ s, s, [ s, [ [ s : s ] ] ] ] ]",
                    json_string_value(j_parent_col),
                    "insert",
                    "set",
                    "named-uuid",
                    "new_table_uuid");

            json_t *jmutator = json_pack("{ s:s, s:s, s:o, s:o}",
                    "op", "mutate",
                    "table", json_string_value(j_parent_table),
                    "where", j_parent_where,
                    "mutations", mutations);

            json_array_append_new(jparam, jmutator);
        }
    }
    else // regular insert
    {
        jparam = json_pack("[ s, { s:s, s:s, s:o } ]",
                ovsh_opt_db,
                "op", "insert",
                "table", table,
                "row", columns);

        if (jparam == NULL)
        {
            DEBUG("Error creating JSON-RPC parameters (INSERT).");
            return false;
        }
    }

    json_t *jres = ovsdb_json_exec("transact", jparam);
    if (!ovsdb_json_error(jres))
    {
        return false;
    }

    json_t *juuid = NULL;
    if (!ovsdb_json_show_uuid(jres, &juuid))
    {
        return false;
    }
    // also apply mutations if any
    // this is so that mutations can be used with upsert
    if (json_array_size(mutations) > 0)
    {
        json_t *where_uuid = json_array();
        json_t *w = json_array();
        json_array_append_new(w, json_string("_uuid"));
        json_array_append_new(w, json_string("=="));
        json_array_append_new(w, juuid);
        json_array_append_new(where_uuid, w);
        return ovsh_mutate(table, where_uuid, mutations);
    }
    return true;
}

/*
 * OVSDB Update method
 */
bool ovsh_update(char *table, json_t *where, int coln, char *colv[])
{
    if (coln <= 0)
    {
        ovsh_usage("Update requires at least 1 column.");
        return false;
    }

    json_t *columns = json_object();
    json_t *mutations = json_array();
    assert(columns != NULL);

    // parse mutations
    if (!ovsh_parse_mutations(mutations, &coln, colv))
    {
        ovsh_usage("Error parsing columns.");
    }

    /* Parse columns */
    if (!ovsh_parse_columns(columns, coln, colv))
    {
        ovsh_usage("Error parsing columns.");
    }

    json_t *jparam = json_pack("[ s, { s:s, s:s, s:o, s:o } ]",
            ovsh_opt_db,
            "op", "update",
            "table", table,
            "where", where,
            "row", columns);

    if (jparam == NULL)
    {
        DEBUG("Error creating JSON-RPC parameters (UPDATE).");
        return false;
    }

    json_t *jres = ovsdb_json_exec("transact", jparam);
    if (!ovsdb_json_error(jres))
    {
        return false;
    }

    if (!ovsdb_json_show_count(jres))
    {
        return false;
    }

    return ovsh_mutate(table, where, mutations);
}

/*
 * OVSDB upsert method
 */
bool ovsh_upsert(char *table, json_t *where, int coln, char *colv[], json_t *parents)
{
    if (coln <= 0)
    {
        ovsh_usage("Update requires at least 1 column.");
        return false;
    }

    // assert that where condition only contains == and no !=
    int i;
    for (i=0; i < ovsh_where_num; i++) {
        if (!strstr(ovsh_where_expr[i], "==")) {
            fprintf(stderr, "ERROR: Upsert: only == condition accepted (%s)\n", ovsh_where_expr[i]);
        }
    }

    // select:
    //  if 0 rows matched -> insert
    //  if 1 row matched -> update
    //  else error

    json_t *sel_jres = NULL;
    json_t *sel_columns = NULL;
    json_t *sel_jrows = NULL;
    if (!ovsdb_select(table, where, 0, NULL, &sel_jres, &sel_columns))
    {
        return false;
    }
    if (!ovsdb_json_get_result_rows(sel_jres, &sel_jrows))
    {
        return false;
    }
    int sel_count = json_array_size(sel_jrows);
    if (sel_count == 0)
    {
        // perform insert
        printf("Upsert: insert\n");
        // convert where == condition to := assignment and append colv
        int new_coln = ovsh_where_num + coln;
        char *new_colv[new_coln];
        int new_i = 0;
        char *s;
        for (i=0; i < ovsh_where_num; i++) {
            new_colv[new_i] = strdup(ovsh_where_expr[i]);
            s = strstr(new_colv[new_i], "==");
            if (!s) return false;
            memcpy(s, ":=", 2);
            new_i++;
        }
        for (i=0; i < coln; i++) {
            new_colv[new_i] = colv[i];
            new_i++;
        }
        return ovsh_insert(table, where, new_coln, new_colv, parents);
    }
    if (sel_count == 1)
    {
        // perform update
        printf("Upsert: update\n");
        return ovsh_update(table, where, coln, colv);
    }
    fprintf(stderr, "ERROR: Upsert: more than one row matched (%d)\n", sel_count);
    return false;
}

/*
 * OVSDB Wait method
 */
bool ovsh_wait(char *table, json_t *where, bool equals, int coln, char *colv[])
{
    if (coln <= 0)
    {
        ovsh_usage("Wait requires at least 1 column.");
        return false;
    }

    json_t *columns_v = json_array();
    assert(columns_v != NULL);
    json_t *columns_o = json_object();
    assert(columns_o != NULL);

    /* Parse columns -> array*/
    if (!ovsh_parse_columns(columns_v, coln, colv))
    {
        ovsh_usage("Error parsing columns.");
        return false;
    }

    /* Parse columns -> object */
    if (!ovsh_parse_columns(columns_o, coln, colv))
    {
        ovsh_usage("Error parsing columns.");
        return false;
    }

    json_t *jparam = json_pack("[ s, { s:s, s:s, s:o, s:o, s: [ o ], s:i, s:s } ]",
            ovsh_opt_db,
            "op", "wait",
            "table", table,
            "where", where,
            "columns", columns_v,
            "rows", columns_o,
            "timeout", ovsh_opt_timeout,
            "until", equals ? "==" : "!=");

    if (jparam == NULL)
    {
        DEBUG("Error creating JSON-RPC parameters (WAIT).");
        return false;
    }

    json_t *jres = ovsdb_json_exec("transact", jparam);
    if (!ovsdb_json_error(jres))
    {
        return false;
    }

    /* Nothing to show */

    return true;
}

/*
 * OVSDB Delete method
 */
bool ovsh_delete(char *table, json_t *where, int coln, char *colv[])
{
    (void)coln;
    (void)colv;

    json_t *jparam = json_pack("[ s, { s:s, s:s, s:o } ]",
            ovsh_opt_db,
            "op", "delete",
            "table", table,
            "where", where);

    if (jparam == NULL)
    {
        DEBUG("Error creating JSON-RPC parameters (DELETE).");
        return false;
    }

    json_t *jres = ovsdb_json_exec("transact", jparam);
    if (!ovsdb_json_error(jres))
    {
        return false;
    }

    if (!ovsdb_json_show_count(jres))
    {
        return false;
    }

    return true;
}

/*
 * Return true if string is encased in quotation marks, either ' or " 
 */
bool str_is_quoted(char *str)
{
    size_t len = strlen(str);

    if ((str[0] == '"' && str[len - 1] == '"') ||
        (str[0] == '\'' && str[len - 1] == '\''))
    {
        return true;
    }

    return false;
}

/*
 * This functions splits a string into a left value, an operator and a right value. It can be used to
 * parse structures as a=b, or a!=b etc.
 *
 * This function modifies the string expr
 * Param op is taken from the "delim" array
 */
bool str_parse_expr(char *expr, char *delim[], char **lval, char **op, char **rval)
{
    char *pop;
    char **pdelim;

    for (pdelim = delim; *pdelim != NULL; pdelim++)
    {
        /* Find the delimiter string */
        pop = strstr(expr, *pdelim);
        if (pop == NULL) continue;

        *op = *pdelim;

        *lval = expr;
        *rval = pop + strlen(*pdelim);
        *pop = '\0';

        return true;
    }

    /* Error parsing */
    return false;
}

/*
 * Guess the type of "str" and return a JSON object of the corresponding type.
 *
 * If str is:
 *      "\"...\"" -> Everything that starts with a quote and ends in a quote is assumed to be a string.
 *                   The resulting object is the value without quotes
 *      "true" -> bool, value set to true
 *      "false" -> bool, value set to false
 *      "XXXX" -> where X are digits only -> integer
 *      ["map", -> object
 *      ["set", -> object
 *      ["uuid", -> object
 *
 *      Everything else is assumed to be a string.
 */
json_t *json_value(char *str)
{
    if (str[0] == '\0') return json_string("");

    /*
     * Check for quoted string first
     */
    if (str_is_quoted(str))
    {
        return json_stringn(str + 1, strlen(str) - 2);
    }

    if (strcmp(str, "true") == 0)
    {
        return json_true();
    }
    else if (strcmp(str, "false") == 0)
    {
        return json_false();
    }

    /*
     * Try to convert it to an integer 
     */
    char *pend;
    json_int_t lval = strtoll(str, &pend, 0);

    /* All character were consumed during conversion -- it's an int */
    if (*pend == '\0')
    {
        return json_integer(lval);
    }
    // check if object
    char *str_map = "[\"map\",";
    char *str_set = "[\"set\",";
    char *str_uuid = "[\"uuid\",";
    if (!strncmp(str, str_map, strlen(str_map))
            || !strncmp(str, str_set, strlen(str_set))
            || !strncmp(str, str_uuid, strlen(str_uuid)))
    {
        // Try to parse rval as a json object
        json_t *jobj = json_loads(str, 0, NULL);
        if (jobj) {
            return jobj;
        }
        fprintf(stderr, "Error decoding as object value: %s\n", str);
        // fallthrough, treat as string for backward compatibility, eventually treat as error
    }

    /* Everything else is a string */
    return json_string(str);
}

// check if string is in uuid format "4303ab6d-2ab1-476b-9b19-283e69aad6ea"
// (but not in json ["uuid","4303ab6d-2ab1-476b-9b19-283e69aad6ea"])
// example: Wifi_Inet_State.if_uuid
bool is_str_uuid(const char *uuid)
{
    int i;
    int c;
    if (strlen(uuid) != UUID_STR_LEN) return false;
    for (i=0; i<UUID_STR_LEN; i++)
    {
        c = (unsigned char)uuid[i];
        if (i == 8 || i == 13 || i == 18 || i == 23) {
            if (c != '-') return false;
        } else {
            if (!isxdigit(c)) return false;
        }
    }
    return true;
}

// Convert UUID 4303ab6d-2ab1-476b-9b19-283e69aad6ea to 4303~d6ea
void abbrev_uuid(char *dest, int size, const char *uuid)
{
    if (size <= 0) return;
    if (!ovsh_opt_uuid_abbrev) {
        strscpy(dest, uuid, size);
    } else {
        int pos = strlen(uuid) - 4;
        if (pos < 0) pos = 0;
        char tmp[16]; // in case dest==uuid
        snprintf(tmp, sizeof(tmp), "%.4s~%.4s", uuid, uuid+pos);
        strscpy(dest, tmp, size);
    }
}

/*
 * Convert UUID object or set to compact string
 *
 * ["uuid","4303ab6d-2ab1-476b-9b19-283e69aad6ea"]
 * becomes:
 * 4303ab6d-2ab1-476b-9b19-283e69aad6ea
 *
 * ["set",[["uuid","467e19c2-480b-4562-92ec-9ceb79d30a79"],["uuid","ee251648-1702-4f32-933c-def02ae85853"]]]
 * becomes:
 * [467e19c2-480b-4562-92ec-9ceb79d30a79,ee251648-1702-4f32-933c-def02ae85853]
 */
bool json_compact_uuid(json_t *jval, char *dst, size_t dst_sz)
{
    const char *str;
    const char *uuid;
    str = json_string_value(json_array_get(jval, 0));
    if (str && (strcmp(str, "uuid") == 0))
    {
        uuid = json_string_value(json_array_get(jval, 1));
        if (uuid && *uuid)
        {
            abbrev_uuid(dst, dst_sz, uuid);
            return true;
        }
    }
    else if (str && (strcmp(str, "set") == 0))
    {
        // a set, check if all elements are uuids
        json_t *jset;
        json_t *item;
        size_t i, n, count = 0;
        jset = json_array_get(jval, 1);
        n = json_array_size(jset);
        json_array_foreach(jset, i, item) {
            str = json_string_value(json_array_get(item, 0));
            if (str && (strcmp(str, "uuid") == 0))
            {
                uuid = json_string_value(json_array_get(item, 1));
                if (uuid && *uuid)
                {
                    count++;
                    continue;
                }
            }
        }
        if (count < 1 || count != n) return false;
        bool first = true;
        int len;
        strcpy(dst, "[");
        json_array_foreach(jset, i, item) {
            uuid = json_string_value(json_array_get(item, 1));
            if (!uuid) return false;
            if (!first) strlcat(dst, ",", dst_sz);
            len = strlen(dst);
            dst += len;
            dst_sz -= len;
            abbrev_uuid(dst, dst_sz, uuid);
            first = false;
        }
        strlcat(dst, "]", dst_sz);
        return true;
    }
    return false;
}

/*
 * Convert JSON object to string
 */
bool json_stringify(json_t *jval, char *dst, size_t dst_sz)
{
    if (jval == NULL)
    {
        return "(null)";
    }
    else if (json_is_string(jval))
    {
        snprintf(dst, dst_sz, "%s", json_string_value(jval));
        if (ovsh_opt_uuid_compact && is_str_uuid(dst)) {
            abbrev_uuid(dst, dst_sz, dst);
        }
    }
    else if (json_is_integer(jval))
    {
        snprintf(dst, dst_sz, "%lld", json_integer_value(jval));
    }
    else if (json_is_boolean(jval))
    {
        if (json_is_true(jval))
        {
            snprintf(dst, dst_sz, "true");
        }
        else
        {
            snprintf(dst, dst_sz, "false");
        }
    }
    else if (json_is_object(jval) || json_is_array(jval))
    {
        if (ovsh_opt_uuid_compact && json_is_array(jval))
        {
            // try to compact uuid
            if (json_compact_uuid(jval, dst, dst_sz)) return true;
        }
        char *str = json_dumps(jval, JSON_COMPACT);
        snprintf(dst, dst_sz, "%s", str);
        free(str);
    }
    else
    {
        snprintf(dst, dst_sz, "(N/A)");
    }

    return true;
}

/*
 * Parse a --parent argument that specifies a relation to a parent table.
 *
 * Syntax: --parent <parent_table>:<parent_col>:<parent_where>
 *
 * E.g.:   --parent Port:interfaces:name==eth0,mac==xx:xx:xx:xx:xx:xx
 */
static bool ovsh_parse_parent(json_t *parent_where,  // json array
                              json_t *parent_table,  // json string
                              json_t *parent_col,    // json string
                              const char *_str)
{
    char str[OVSH_COL_STR];
    char *lval, *op, *rval;

    static char *delims[] =
    {
        ":",
        NULL,
    };


    if (strlen(_str) + 1 > sizeof(str))
    {
        return false;
    }
    strcpy(str, _str);

    if (!str_parse_expr(str, delims, &lval, &op, &rval))
    {
        DEBUG("Error parsing expression: %s (%s)\n", str, _str);
        return false;
    }
    json_string_set(parent_table, lval);

    strcpy(str, rval);
    if (!str_parse_expr(str, delims, &lval, &op, &rval))
    {
        DEBUG("Error parsing expression: %s (%s)\n", str, _str);
        return false;
    }
    json_string_set(parent_col, lval);

    strcpy(str, rval);
    if (!ovsh_parse_where(parent_where, str, true))
    {
        DEBUG("Error parsing WHERE statement: %s\n", str);
        return false;
    }

    return true;
}


/*
 * Parse a "WHERE" statement and build up a JSON object that is suitable for OVSDB
 */
static bool ovsh_parse_where_statement(json_t *where, char *_str, bool is_parent_where)
{
    char str[OVSH_COL_STR];

    bool retval = false;

    if (strlen(_str) + 1 > sizeof(str))
    {
        /* String too long */
        return false;
    }

    strcpy(str, _str);

    static char *where_delims[] =
    {
        "==",
        "!=",
        NULL,
    };

    char *lval, *op, *rval;
    if (!str_parse_expr(str, where_delims, &lval, &op, &rval))
    {
        DEBUG("Error parsing expression: %s", _str);
        return false;
    }

    /*
     * Create a JSON array from values
     */
    json_t *jop = json_array();
    if (jop == NULL)
    {
        DEBUG("Unable to create JSON array");
        return false;
    }

    if (json_array_append_new(jop, json_string(lval)) != 0)
    {
        DEBUG("Unable to append JSON array (lval)");
        goto error;
    }

    if (json_array_append_new(jop, json_string(op)) != 0)
    {
        DEBUG("Unable to append JSON array (op)");
        goto error;
    }

    if (json_array_append_new(jop, json_value(rval)) != 0)
    {
        DEBUG("Unable to append JSON array (rval)");
        goto error;
    }

    if (json_array_append(where, jop) != 0)
    {
        DEBUG("Unable to append JSON array (where)");
        goto error;
    }

    retval = true;

    /* Append to global where expr table, but only if this is a regular where
     * statement (not part of a --parent argument)  */
    if (!is_parent_where)
    {
        ovsh_where_num++;
        ovsh_where_expr = (char**)realloc(ovsh_where_expr, sizeof(char**) * ovsh_where_num);
        assert(ovsh_where_expr);
        ovsh_where_expr[ovsh_where_num-1] = strdup(_str);
    }

error:
    if (jop != NULL) json_decref(jop);

    return retval;
}

/*
 * Parse a "WHERE" argument (which may chain multiple WHERE statements separated
 * by a comma) and build up a JSON object that is suitable for OVSDB.
 */
static bool ovsh_parse_where(json_t *where, char *_str, bool is_parent_where)
{
    char str[OVSH_COL_STR];
    char *tok;


    strcpy(str, _str);
    tok = strtok(str, ",");
    while (tok != NULL)
    {
        if (!ovsh_parse_where_statement(where, tok, is_parent_where))
        {
            DEBUG("Error parsing WHERE statement: %s in WHERE argument: %s\n",
                   tok, _str);
            return false;
        }
        tok = strtok(NULL, ",");
    }

    return true;
}


/*
 * Parse a list of columns, generate an array object out of it
 */
static bool ovsh_parse_columns(json_t *columns, int colc, char *colv[])
{
    int ii;

    json_t *jrval = NULL;

    for (ii = 0; ii < colc; ii++)
    {
        char    col[OVSH_COL_STR];

        /*
         * Use only the "left" value of an expression
         */ 
        char *lval, *op, *rval;

        static char *delim[] =
        {
            ":=",
            "::",
            "~=",
            NULL
        };

        if (strlen(colv[ii]) + 1 > sizeof(col))
        {
            DEBUG("Column too big: %s\n", colv[ii]);
            return false;
        }

        strcpy(col, colv[ii]);

        if (!str_parse_expr(col, delim, &lval, &op, &rval))
        {
            /*Not an expression, use whole line as left value */
            lval = colv[ii];
            rval = NULL;
        }
        else
        {
            /* Depending on the operator, do different things with rval */
            if (strcmp(op, ":=") == 0)
            {
                /* := standard right value processing */
                jrval = json_value(rval);
            }
            else if (strcmp(op, "::") == 0)
            {
                /* Try to parse rval as a json object */
                jrval = json_loads(rval, 0, NULL);
                if (jrval == NULL)
                {
                    DEBUG("Error decoding column right value: %s\n", rval);
                    return false;
                }
            }
            else if (strcmp(op, "~=") == 0)
            {
                /* Force right value to string */
                jrval = json_string(rval);
            }
            else
            {
                DEBUG("Error decoding column operator: %s\n", op);
                return false;
            }
        }

        if (json_is_array(columns))
        {
            /* Append left value only */
            if (json_array_append_new(columns, json_string(lval)) != 0)
            {
                goto error;
            }
        }
        else if (json_is_object(columns) && rval != NULL)
        {
            if (json_object_set_new(columns, lval, jrval) != 0)
            {
                goto error;
            }
        }
        else
        {
            DEBUG("Parameter is not an object or array.");
            goto error;
        }

        jrval = NULL;
    }

    return true;

error:
    if (jrval != NULL) json_decref(jrval);
    return false;
}

bool ovsh_parse_mutations(json_t *mutations, int *colc, char *colv[])
{
    int ii, j;
    json_t *jrval = NULL;

    static char *delim[] =
    {
        ":ins:",
        ":del:",
        NULL
    };

    static char *mutator_op[] =
    {
        "insert",
        "delete",
        NULL
    };

    if (!json_is_array(mutations))
    {
        DEBUG("Parameter is not an array.");
        goto error;
    }
    int new_colc = 0;

    for (ii = 0; ii < *colc; ii++)
    {
        char col[OVSH_COL_STR];
        char *lval, *op, *rval;
        char *mop = NULL;

        if (strlen(colv[ii]) + 1 > sizeof(col))
        {
            DEBUG("Column too big: %s\n", colv[ii]);
            return false;
        }

        strcpy(col, colv[ii]);

        if (!str_parse_expr(col, delim, &lval, &op, &rval))
        {
            // add to new colv
            colv[new_colc] = colv[ii];
            new_colc++;
            continue;
        }
        for (j = 0; delim[j]; j++) {
            if (!strcmp(op, delim[j])) {
                mop = mutator_op[j];
                break;
            }
        }
        if (!mop) goto error;
        DEBUG("Mutation: [%d] %s %s %s\n", ii, lval, mop, rval);
        // treat rval as json object
        jrval = json_loads(rval, 0, NULL);
        if (jrval == NULL)
        {
            DEBUG("Error decoding column right value: %s\n", rval);
            return false;
        }
        json_t *m = json_array();

        // append lval
        if (json_array_append_new(m, json_string(lval)) != 0)
        {
            goto error;
        }
        // append op
        if (json_array_append_new(m, json_string(mop)) != 0)
        {
            goto error;
        }
        // append jrval
        if (json_array_append_new(m, jrval) != 0)
        {
            goto error;
        }
        jrval = NULL;
        // append mutation m to list of mutations
        if (json_array_append_new(mutations, m) != 0)
        {
            goto error;
        }
    }
    *colc = new_colc;

    return true;

error:
    fprintf(stderr, "ERROR: %s\n", __FUNCTION__);
    if (jrval != NULL) json_decref(jrval);
    return false;
}

/*
 * ===========================================================================
 *  OVSDB communication functions
 * ===========================================================================
 */

/*
 * Connect to a OVSDB database
 */
int ovsdb_connect(void)
{
    struct sockaddr_un  addr;

    int fd = -1;

    fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0)
    {
        DEBUG("Unable to open OVSDB socket: %s\n", strerror(errno));
        goto error;
    }

    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;

    char *sock_path = getenv(ENV_OVSDB_SOCK_PATH);
    if (!sock_path || !*sock_path) sock_path = OVSDB_SOCK_PATH;
    if (strlen(sock_path) >= sizeof(addr.sun_path))
    {
        DEBUG("Connect path too long!\n");
        goto error;
    }
    strcpy(addr.sun_path, sock_path);

    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) != 0)
    {
        DEBUG("Unable to connect to OVSDB: %s\n", strerror(errno));
        goto error;
    }

    return fd;

error:
    if (fd >= 0) close(fd);

    return -1;
}

void ovsdb_close(int fd)
{
    close(fd);
}

/*
 * Connect to OVSDB, execute a JSON RPC command, close the connection and return
 */
json_t *ovsdb_json_exec(char *method, json_t *params)
{
    char buf[128*1024];

    char   *str = NULL;
    json_t *jres = NULL;
    int     db = -1;

    json_t *jexec = json_pack("{ s:s, s:o, s:i }", "method", method, "params", params, "id", getpid());

    str = json_dumps(jexec, 0);
    DEBUG(">>>>>>> %s\n", str);

    db = ovsdb_connect();
    if (db < 0)
    {
        goto error;
    }

    ssize_t len = strlen(str);
    if (write(db, str, len) < len)
    {
        DEBUG("Error writing to OVSDB: %s\n", strerror(errno));
        goto error;
    }

    ssize_t nrd;
    nrd = read(db, buf, sizeof(buf));
    if (nrd < 0)
    {
        goto error;
    }

    buf[nrd] = '\0';

    DEBUG("<<<<<<< %s\n", buf);

    jres = json_loads(buf, JSON_PRESERVE_ORDER, NULL);

error:
    if (str != NULL) free(str);
    if (db > 0) ovsdb_close(db);

    return jres;
}

/*
 * Check if an error was reported from OVSDB, if so, display it and return false. Return true otherwise.
 */
bool ovsdb_json_error(json_t *jobj)
{
    json_t *jres = json_object_get(jobj, "result");
    if (jres == NULL)
    {
        fprintf(stderr, "Error: No \"result\" object is present.");
        return false;
    }

    /* Use the first object in the array */
    jres = json_array_get(jres, 0);

    json_t *jerror = json_object_get(jres, "error");
    if (jerror == NULL)
    {
        /* No error, return */
        return true;
    }
    fprintf(stderr, "Error: %s\n", json_string_value(jerror));

    json_t *jdetails = json_object_get(jres, "details");
    if (jdetails != NULL)
    {
        fprintf(stderr, "Details: %s\n", json_string_value(jdetails));
    }

    json_t *jsyntax = json_object_get(jres, "syntax");
    if (jsyntax != NULL)
    {
        fprintf(stderr, "Syntax: %s\n", json_string_value(jsyntax));
    }

    return false;
}

/*
 * Display the count of affected rows
 */
bool ovsdb_json_show_count(json_t *jobj)
{
    if (ovsh_opt_quiet)
    {
        return true;
    }

    json_t *jres = json_object_get(jobj, "result");
    if (jres == NULL)
    {
        fprintf(stderr, "Error: No \"result\" object is present.");
        return false;
    }

    /* Use the first object in the array */
    jres = json_array_get(jres, 0);
    if (jres == NULL)
    {
        fprintf(stderr, "Error: Result object is an empty array.");
        return false;
    }

    json_t *jcount = json_object_get(jres, "count");
    if (jcount == NULL)
    {
        fprintf(stderr, "Error: Expected \"count\" object in response.");
        return false;
    }

    if (ovsh_opt_format == OVSH_FORMAT_JSON)
    {
        char *str = json_dumps(jres, JSON_COMPACT);

        printf("%s\n", str);

        free(str);
    }
    else
    {
        printf("%lld\n", json_integer_value(jcount));
    }

    return true;
}


/*
 * Display the UUID, which is a result of an insert operation
 */
bool ovsdb_json_show_uuid(json_t *jobj, json_t **a_juuid)
{
    if (ovsh_opt_quiet)
    {
        return true;
    }

    if (a_juuid) *a_juuid = NULL;

    json_t *jres = json_object_get(jobj, "result");
    if (jres == NULL)
    {
        fprintf(stderr, "Error: No \"result\" object is present.");
        return false;
    }

    /* Use the first object in the array */
    jres = json_array_get(jres, 0);
    if (jres == NULL)
    {
        fprintf(stderr, "Error: Result object is an empty array.");
        return false;
    }

    json_t *juuid = json_object_get(jres, "uuid");
    if (juuid == NULL)
    {
        fprintf(stderr, "Error: Expected \"count\" object in response.");
        return false;
    }

    const char *str_uuid;

    /*
     * First element in array must be "uuid"
     */
    str_uuid = json_string_value(json_array_get(juuid, 0));
    if (str_uuid == NULL || (strcmp(str_uuid, "uuid") != 0))
    {
        fprintf(stderr, "Result object is not an UUID object.\n");
        return false;
    }

    /*
     * The second element is the UUID
     */
    str_uuid = json_string_value(json_array_get(juuid, 1));
    if (str_uuid == NULL)
    {
        fprintf(stderr, "Result objact contains an invalid UUID.\n");
        return false;
    }

    if (a_juuid) *a_juuid = juuid;

    if (ovsh_opt_format == OVSH_FORMAT_JSON)
    {
        char *str = json_dumps(jres, JSON_COMPACT);

        printf("%s\n", str);

        free(str);
    }
    else
    {
        printf("%s\n", str_uuid);
    }

    return true;
}

static int cmpstringp(const void *p1, const void *p2)
{
    return strcmp(* (char * const *) p1, * (char * const *) p2);
}

json_t * ovsdb_json_sort_array_str(json_t *jarr)
{
    size_t n = json_array_size(jarr);
    const char *str[n];
    json_t *sorted = json_array();
    json_t *value;
    size_t i;

    json_array_foreach(jarr, i, value) {
        str[i] = json_string_value(value);
    }
    qsort(str, n, sizeof(char*), cmpstringp);
    for (i=0; i<n; i++)
    {
        if (json_array_append_new(sorted, json_string(str[i])))
        {
            DEBUG("Error append to JSON array sorted: %s", str[i]);
            json_decref(sorted);
            return NULL;
        }
    }
    json_decref(jarr);
    return sorted;
}


bool ovsdb_json_get_result_rows(json_t *jobj, json_t **jrows)
{
    *jrows = NULL;

    json_t *jres = json_object_get(jobj, "result");
    if (jres == NULL)
    {
        fprintf(stderr, "Error: No \"result\" object is present.");
        return false;
    }

    /* Use the first object in the array */
    jres = json_array_get(jres, 0);
    if (jres == NULL)
    {
        fprintf(stderr, "Unexpected JSON result: Got empty array.");
        return false;
    }

    *jrows = json_object_get(jres, "rows");
    if (*jrows == NULL)
    {
        fprintf(stderr, "Error: Expected \"rows\" object in response.");
        return false;
    }
    return true;
}


bool ovsdb_json_show_result(json_t *jobj, json_t *columns)
{
    bool retval = false;
    json_t *pcolumns = NULL;
    json_t *jrows;

    if (!ovsdb_json_get_result_rows(jobj, &jrows))
    {
        goto error;
    }

    /*
     * If columns are empty, generate them from the first row
     */
    if (json_array_size(columns) > 0)
    {
        pcolumns = json_incref(columns);
    }
    else
    {
        pcolumns = json_array();
        if (pcolumns == NULL)
        {
            DEBUG("Unable to allocate JSON array for: pcolumns.");
            goto error;
        }

        json_t *first_row = json_array_get(jrows, 0);
        if (first_row == NULL)
        {
            DEBUG("Unable to get first row: Empty result.");
            goto error;
        }

        const char *key;
        json_t *value;
        json_object_foreach(first_row, key, value)
        {
            if (json_array_append_new(pcolumns, json_string(key)) != 0)
            {
                DEBUG("Error append to JSON array: pcolumns");
                json_decref(pcolumns);
                goto error;
            }
        }
    }

    // sort columns
    pcolumns = ovsdb_json_sort_array_str(pcolumns);

    /*
     * Show data according to command line options
     */
    switch (ovsh_opt_format)
    {
        case OVSH_FORMAT_JSON:
            retval = ovsdb_json_show_result_json(jrows);
            break;
        case OVSH_FORMAT_CUSTOM:
            retval = ovsdb_json_show_result_output(jrows, pcolumns);
            break;
        case OVSH_FORMAT_RAW:
            retval = ovsdb_json_show_result_raw(jrows, pcolumns);
            break;
        case OVSH_FORMAT_COLUMN:
            retval = ovsdb_json_show_result_column(jrows, pcolumns);
            break;
        case OVSH_FORMAT_MULTI_LINE:
            retval = ovsdb_json_show_result_multi(jrows, pcolumns, true);
            break;
        case OVSH_FORMAT_MULTI:
            retval = ovsdb_json_show_result_multi(jrows, pcolumns, false);
            break;
        case OVSH_FORMAT_TABLE:
            retval = ovsdb_json_show_result_table(jrows, pcolumns);
            break;
    }

error:
    if (pcolumns != NULL) json_decref(pcolumns);

    return retval;
}

/*
 * Show a result from a select method
 */
bool ovsdb_json_show_result_json(json_t *jrows)
{
    char *str = json_dumps(jrows, JSON_INDENT(4) | JSON_PRESERVE_ORDER);
    printf("%s\n", str);
    free(str);

    return true;
}

/*
 * Show custom format output
 */
bool ovsdb_json_show_result_output(json_t *jrows, json_t *columns)
{
    int     argc;
    char   *argv[256];
    json_t *jrow;
    size_t  nr;
    size_t  nc;

    for (nr = 0; nr < json_array_size(jrows); nr++)
    {
        char col_all[8192];

        char *pcol = col_all;

        argc = 0;

        argv[argc++] = "printf";
        argv[argc++] = ovsh_opt_format_output;

        jrow = json_array_get(jrows, nr);
        if (jrow == NULL)
        {
            DEBUG("Error, retrieved ROW is NULL.");
            return false;
        }

        for (nc = 0; nc < json_array_size(columns); nc++)
        {
            const char *col = json_string_value(json_array_get(columns, nc));

            if (!json_stringify(json_object_get(jrow, col), pcol, sizeof(col_all) - (pcol - col_all)))
            {
                DEBUG("Error converting JSON to string.");
                return false;
            }

            argv[argc++] = strdup(pcol);

            pcol += strlen(pcol) + 1;

            if (pcol > col_all + sizeof(col_all))
            {
                DEBUG("Buffer too small");
                return false;
            }
        }

        /* Pad end of array with NULL */
        argv[argc] = NULL;

        if (systemvp("printf", argv) != 0)
        {
            fprintf(stderr, "Exernal command failed: printf");
            return false;
        }
    }

    return true;
}

/*
 * Show raw result from a select method
 */
bool ovsdb_json_show_result_raw(json_t *jrows, json_t *columns)
{
    /*
     * Show table
     */
    json_t *jrow;
    size_t  nr;
    size_t  nc;
    char    col_str[OVSH_COL_STR];

    for (nr = 0; nr < json_array_size(jrows); nr++)
    {
        jrow = json_array_get(jrows, nr);
        if (jrow == NULL)
        {
            DEBUG("Error, retrieved ROW is NULL.");
            return false;
        }

        for (nc = 0; nc < json_array_size(columns); nc++)
        {
            const char *col = json_string_value(json_array_get(columns, nc));

            if (!json_stringify(json_object_get(jrow, col), col_str, sizeof(col_str)))
            {
                DEBUG("Error converting JSON to string.");
            }

            if (nc > 0)
                printf(" ");
            printf("%s", col_str);
        }
        printf("\n");
    }

    return true;
}

/*
 * Show a result from a select method
 */
static void ovsb_json_show_table_line(int colc, int colv[])
{
    int nc;

    fputc('+', stdout);
    for (nc = 0; nc < colc; nc++)
    {
        int ii;
        for (ii = 0; ii < colv[nc] + 2; ii++)
        {
            fputc('-', stdout);
        }
        fputc('+', stdout);
    }
    fputc('\n', stdout);
}

bool ovsdb_json_show_result_table(json_t *jrows, json_t *columns)
{
    int col_width[OVSH_COL_NUM];
    char col_str[OVSH_COL_STR];

    json_t *jrow;
    size_t nr;
    size_t nc;

    /*
     * Calculate table width by scanning all elements
     */
    memset(col_width, 0, sizeof(col_width));
    for (nr = 0; nr < json_array_size(jrows); nr++)
    {
        jrow = json_array_get(jrows, nr);
        if (jrow == NULL)
        {
            DEBUG("Error, retrieved ROW is NULL.");
            return false;
        }

        /* Scan maximum column size */
        for (nc = 0; nc < json_array_size(columns); nc++)
        {
            const char *col = json_string_value(json_array_get(columns, nc));

            if (!json_stringify(json_object_get(jrow, col), col_str, sizeof(col_str)))
            {
                DEBUG("Error converting JSON to string.");
            }

            if (col_width[nc] < (int)strlen(col_str))
            {
                col_width[nc] = strlen(col_str);
            }

            if (col_width[nc] < (int)strlen(col))
            {
                col_width[nc] = strlen(col);
            }
        }
    }

    /*
     * Show line
     */
    ovsb_json_show_table_line(json_array_size(columns), col_width);

    /*
     * Show headers
     */
    jrow = json_array_get(jrows, 0);
    for (nc = 0; nc < json_array_size(columns); nc++)
    {
        const char *col = json_string_value(json_array_get(columns, nc));

        printf("| %-*s ", col_width[nc], col);
    }
    printf("|\n");

    /*
     * Show line
     */
    ovsb_json_show_table_line(json_array_size(columns), col_width);

    /*
     * Show table
     */
    for (nr = 0; nr < json_array_size(jrows); nr++)
    {
        jrow = json_array_get(jrows, nr);
        if (jrow == NULL)
        {
            DEBUG("Error, retrieved ROW is NULL.");
            return false;
        }

        for (nc = 0; nc < json_array_size(columns); nc++)
        {
            const char *col = json_string_value(json_array_get(columns, nc));

            if (!json_stringify(json_object_get(jrow, col), col_str, sizeof(col_str)))
            {
                DEBUG("Error converting JSON to string.");
            }

            printf("| %-*s ", col_width[nc], col_str);
        }
        printf("|\n");
    }

    ovsb_json_show_table_line(json_array_size(columns), col_width);

    return true;
}

static void print_line(int len, char c)
{
    int i;
    for (i=0; i<len; i++) putchar(c);
    putchar('\n');
}

// show results in column
bool ovsdb_json_show_result_column(json_t *jrows, json_t *columns)
{
    int key_width = 0;
    int val_width = 0;
    int width;
    char str[OVSH_COL_STR];
    json_t *jcol;
    const char *col;
    json_t *jrow;
    size_t nr;
    size_t nc;
    int len;

    // Calculate key width by scanning all columns
    json_array_foreach(columns, nc, jcol) {
        len = strlen(json_string_value(jcol));
        if (len > key_width) key_width = len;
    }

    // val width
    json_array_foreach(jrows, nr, jrow) {
        json_array_foreach(columns, nc, jcol) {
            // key
            col = json_string_value(jcol);
            // value
            if (!json_stringify(json_object_get(jrow, col), str, sizeof(str)))
            {
                DEBUG("Error converting JSON to string.");
            }
            len = strlen(str);
            if (len > val_width) val_width = len;
        }
    }

    width = key_width + val_width + 3;

    // Show line
    print_line(width, '-');

    // print table
    json_array_foreach(jrows, nr, jrow) {
        json_array_foreach(columns, nc, jcol) {
            // key
            col = json_string_value(jcol);
            printf("%-*s : ", key_width, col);
            // value
            if (!json_stringify(json_object_get(jrow, col), str, sizeof(str)))
            {
                DEBUG("Error converting JSON to string.");
            }
            printf("%s\n", str);
        }
        print_line(width, '-');
    }

    return true;
}

// break at last comma within desired len or next if none found
// so actual len can be longer than desired len if no commas found
void break_line(char *str, int desired, char *remain, int size)
{
    int len = strlen(str);
    if (len <= desired)
    {
all:
        *remain = 0;
        return;
    }
    // break
    char *s;
    // find last comma within desired len
    s = str + desired - 1;
    while (*s != ',' && s > str) s--;
    if (s <= str)
    {
        // not found, look for first ,
        s = strchr(str, ',');
    }
    if (!s)
    {
        // no comma found, return everything
        goto all;
    }
    // split to first part and remain
    strscpy(remain, s + 1, size);
    str[s - str + 1] = 0;
    return;
}

// show results in columns
bool ovsdb_json_show_result_multi(json_t *jrows, json_t *columns, bool multi_line)
{
    int key_width = 0;
    int val_width = 0; // sum of all val col width
    int col_width[OVSH_COL_NUM]; // each val col width
    int width;
    char str[OVSH_COL_STR];
    char remain[OVSH_COL_NUM][OVSH_COL_STR];
    json_t *jcol;
    const char *col;
    json_t *jrow;
    size_t nr;
    size_t nc;
    int len;
    int min_desired;
    int desired;
    int term_col = 100;
    char *col_str;

    // Calculate key width by scanning all columns
    json_array_foreach(columns, nc, jcol) {
        len = strlen(json_string_value(jcol));
        if (len > key_width) key_width = len;
    }

    // Calculate desired column size based on number of rows and terminal size
    col_str = getenv("COLUMNS");
    if (col_str)
    {
        term_col = atoi(col_str);
        if (term_col < 80) term_col = 80;
    }
    nr = json_array_size(jrows);
    if (nr == 0) nr = 1;
    desired = (term_col - key_width - 3) / nr - (nr * 2);
    if (ovsh_opt_uuid_abbrev) min_desired = 22;
    else min_desired = UUID_STR_LEN;
    if (desired < min_desired) desired = min_desired;

    // val width
    json_array_foreach(jrows, nr, jrow) {
        col_width[nr] = 0;
        json_array_foreach(columns, nc, jcol) {
            // key
            col = json_string_value(jcol);
            // value
            if (!json_stringify(json_object_get(jrow, col), str, sizeof(str)))
            {
                DEBUG("Error converting JSON to string.");
            }
            if (multi_line)
            {
                int l;
                len = 0;
                do {
                    break_line(str, desired, remain[0], sizeof(remain[0]));
                    l = strlen(str);
                    if (l > len) len = l;
                    strcpy(str, remain[0]);
                } while (*remain[0]);
            }
            else
            {
                len = strlen(str);
            }
            if (len > col_width[nr]) col_width[nr] = len;
        }
        val_width += col_width[nr];
    }

    width = key_width + 2 + val_width + 3 * json_array_size(jrows);

    // Show line
    print_line(width, '-');

    // print table
    json_array_foreach(columns, nc, jcol) {
        // key
        col = json_string_value(jcol);
        printf("%-*s |", key_width, col);
        // value
        bool have_remain = false;
        json_array_foreach(jrows, nr, jrow) {
            if (!json_stringify(json_object_get(jrow, col), str, sizeof(str)))
            {
                DEBUG("Error converting JSON to string.");
            }
            if (multi_line) {
                break_line(str, col_width[nr], remain[nr], sizeof(remain[0]));
                if (*remain[nr]) have_remain = true;
            }
            printf(" %-*s |", col_width[nr], str);
        }
        printf("\n");
        while (multi_line && have_remain) {
            have_remain = false;
            printf("%-*s :", key_width, "");
            for (nr = 0; nr < json_array_size(jrows); nr++)
            {
                strcpy(str, remain[nr]);
                break_line(str, col_width[nr], remain[nr], sizeof(remain[0]));
                if (*remain[nr]) have_remain = true;
                printf(" %-*s :", col_width[nr], str);
            }
            printf("\n");
        }
    }
    print_line(width, '-');

    return true;
}

/*
 * ===========================================================================
 *  Miscellaneous
 * ===========================================================================
 */
/*
 * Execute command @p file
 */
int systemvp(const char *file, char *argv[])
{
    pid_t   c;
    int     status;

    c = fork();
    if (c == 0)
    {
        execvp(file, argv);

        return 1;
    }
    else
    {
        waitpid(c, &status, 0);

    }

    return WEXITSTATUS(status);
}

