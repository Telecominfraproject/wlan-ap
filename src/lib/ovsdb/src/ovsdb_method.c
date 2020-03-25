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
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <jansson.h>
#include <stdbool.h>
#include <errno.h>
#include <ev.h>
#include <assert.h>

#include "log.h"
#include "os_util.h"
#include "util.h"
#include "json_util.h"
#include "const.h"

#include "ovsdb_priv.h"

extern char *ovsdb_comment;

/*
 * Static list of transact functions in where statements.
 * Has to be in perfect sync with ovsdb_func_t list.
 */
static char * ovsdb_functions[] =
{
    "==",       /*OFUNC_EQ = 0,  */
    "!=",       /*OFUNC_NEQ,     */
    "<",        /*OFUNC_LT,      */
    "<=",       /*OFUNC_LTE,     */
    ">",        /*OFUNC_GT,      */
    ">=",       /*OFUNC_GTE,     */
    "includes", /*OFUNC_INC,     */
    "excludes", /*OFUNC_EXC,     */
};


static char * tran_operations[] =
{
    "insert",   /* OTR_INSERT,  */
    "select",   /* OTR_SELECT,  */
    "update",   /* OTR_UPDATE,  */
    "mutate",   /* OTR_MUTATE,  */
    "delete",   /* OTR_DELETE,  */
    "wait",     /* OTR_WAIT,    */
};

extern ds_tree_t json_rpc_handler_list;
extern int json_rpc_fd;

/**
 * Generate a new JSON RPC ID and return it
 *
 * Note that function is not thread safe
 */
int ovsdb_jsonrpc_id_new(void)
{
    static int jsonrpc_id = 1;  /* Start RPC sequence */

    return jsonrpc_id++;
}

/**
 * Return an OVDSB transaction operation identifer derived from the @p tran
 * parameter.
 */
json_t *ovsdb_tran_operation(ovsdb_tro_t tran)
{
    if (tran > OTR_WAIT)
    {
        LOGE("OVSDB: Invalid transaction operation id: %d\n", tran);
        assert(!"ovsdb_tran_operation() received invalid transaction operation id.");
    }

    return json_string(tran_operations[tran]);
}

static bool ovsdb_mon_sel_isprefix(const char * column_ex)
{
    /* assume prefix is non existing or wrong size */
    bool isprefix = false;

    if (   (strlen(column_ex) > (MON_SEL_INITIAL + 1))
        && (column_ex[MON_SEL_MODIFY] == '+' || column_ex[MON_SEL_MODIFY] == '-')
        && (column_ex[MON_SEL_DELETE] == '+' || column_ex[MON_SEL_DELETE] == '-')
        && (column_ex[MON_SEL_INSERT] == '+' || column_ex[MON_SEL_INSERT] == '-')
        && (column_ex[MON_SEL_INITIAL] == '+' || column_ex[MON_SEL_INITIAL] == '-')
       )
    {
        isprefix = true;
    }

    return isprefix;
}

/*
 * va list contains only column names with selector prefixes
 * Return array which is table_name value in monitor request
 */
static json_t * ovsdb_mon_tbl_val(int mon_flags, int argc, char *argv[])
{
    json_t * jo;
    json_t * jcarray;
    json_t * jsel;
    bool    isprefix;
    int i;

    jcarray = json_array();
    jo = json_object();

    for (i = 0; i < argc; i++)
    {
        isprefix = ovsdb_mon_sel_isprefix(argv[i]);
        /* for a moment, just ignore prefix */
        json_array_append_new(jcarray, json_string(isprefix ?
                                                    MON_SEL_CNAME_START(argv[i]) :
                                                    argv[i]));
    }

    if (json_array_size(jcarray) > 0)
    {
        json_object_set_new(jo, "columns", jcarray);
    }
    else
    {
        json_decref(jcarray);
    }

    /* By default, OVS sends updates for ALL operations */
    if (mon_flags != 0 && mon_flags != OMT_ALL)
    {
        jsel = json_object();
        if (mon_flags & OMT_INITIAL)
        {
            json_object_set_new(jsel, "initial", json_true());
        }

        if (mon_flags & OMT_INSERT)
        {
            json_object_set_new(jsel, "insert", json_true());
        }

        if (mon_flags & OMT_DELETE)
        {
            json_object_set_new(jsel, "delete", json_true());
        }

        if (mon_flags & OMT_MODIFY)
        {
            json_object_set_new(jsel, "modify", json_true());
        }

        json_object_set_new(jo, "select", jsel);
    }

    return jo;
}


/**
 * Perform actual write to ovsdb, add callback in the callback list
 */
static int ovsdb_write_callback(const char *buf, size_t sz, void *data);

static bool ovsdb_write(json_rpc_response_t *callback, void * data, json_t * js)
{
    bool retval = false;
    struct rpc_response_handler *rh = NULL;

    if (json_dump_callback(js, ovsdb_write_callback, NULL, JSON_COMPACT) != 0)
    {
        LOG(ERR, "Error writing JSON to OVSDB.");
        goto error;
    }

    /* If we have a callback, insert it into the response tree */
    if (callback != NULL)
    {
        /*
         * Create the JSON RPC message response handler
         */
        rh = malloc(sizeof(struct rpc_response_handler));
        if (rh == NULL)
        {
            LOG(ERR, "JSON RPC: Unable to allocate rpc_response_handler");
            goto error;
        }

        json_t *jid = json_object_get(js, "id");
        if (jid == NULL || !json_is_integer(jid))
        {
            LOG(ERR, "JSON RPC: RPC call with callback requires a vlaid \"id\" field.");
            goto error;
        }

        rh->rrh_id = json_integer_value(jid);
        rh->rrh_callback = callback;
        rh->data = data;

        ds_tree_insert(&json_rpc_handler_list, rh, &rh->rrh_id);
    }

    retval = true;

error:
    if (!retval)
    {
        /* On errors, free the rh structure */
        if (rh != NULL) free(rh);
    }

    return retval;
}

int ovsdb_write_callback(const char *buf, size_t sz, void *data)
{
    ssize_t  nwr;

    (void)data;

    nwr = write(json_rpc_fd, buf, sz);
    if (nwr <= 0)
    {
        LOG(ERR, "JSON RPC: Error writing to socket.::error=%s|errno=%d", strerror(errno), errno);
        return -1;
    }
    else
    {
        /* Following line makes log too verbose - commented it until better solution found */
        /* LOG(DEBUG, "JSON RPC.::json=%s", buf);   */
    }

    return 0;
}

/**
 * Create a rpc method json, and send it to ovsdb
 *
 * INPUT arguments:
 *
 * callback - function to be called upon response received
 * data     - used data to be send in call back function
 * mt       - one of the available ovsdb methods
 * jparams  - json array, needed to matched with mt type
 *
 */
bool ovsdb_method_send(json_rpc_response_t *callback,
                              void * data,
                              ovsdb_mt_t mt,
                              json_t * jparams)
{

    char * method = NULL;

    json_t *js = NULL;
    bool retval = false;

    switch (mt)
    {
        case MT_ECHO:
            method = "echo";
            break;

        case MT_MONITOR:
            method = "monitor";
            break;

        case MT_TRANS:
            method = "transact";
            break;

        default:
            LOG(ERR, "unknown method");
            return false;
    }

    js = json_object();

    if (0 < json_object_set_new(js, "method", json_string(method)))
    {
        LOG(ERR, "Error adding method key.");
    }

    if (0 < json_object_set_new(js, "params", jparams))
    {
        LOG(ERR, "Error adding params array.");
    }

    if (0 < json_object_set_new(js, "id", json_integer(ovsdb_jsonrpc_id_new())))
    {
        LOG(ERR, "Error adding id key.");
    }

    retval = ovsdb_write(callback, data, js);

    if (js != NULL) json_decref(js);

    return retval;
}


/******************************************************************************
 * Public interface
 *****************************************************************************/

/**
 * This function assumes that submitted buffer contains valid json string
 * It also checks if submitted json request has "id" field and modifies it
 *
 * Upon response receive submitted callback functions was invoked
 *
 * NOTE: This function is meant to be used mainly for ovsdb raw CLI
 */
bool ovsdb_method_json(json_rpc_response_t *callback, void * data,
                       char * buffer, size_t sz)
{
    (void)sz;

    json_t *js = NULL;
    json_error_t je;
    bool retval = false;

    js = json_loads(buffer, 0, &je);

    if (NULL == js)
    {
        LOG(DEBUG, "Error processing JSON message.::json=%s", buffer);
        LOG(DEBUG, "JSON parse error.::reason=%s", je.text);
    }
    else
    {
        if (NULL != json_object_get(js, "id"))
        {
            json_object_set(js, "id", json_integer(ovsdb_jsonrpc_id_new()));

            /* send this object to ovsdb */
            retval = ovsdb_write(callback, data, js);
        }
        else
        {
            LOG(DEBUG, "json string has no key: id");
        }
    }

    /* release json object */
    if (NULL != js) json_decref(js);

    return retval;
}



/**
 * Following functions are for creating monitor JSON requests
 *
 * mon_flags is a bit field composed of the following fields:
 *      - OMT_INITIAL
 *      - OMT_INSERT
 *      - OMT_DELETE
 *      - OMT_MODIFY
 *
 * If 0 is specified, OMT_ALL is assumed (monitor all operations).
 */

bool ovsdb_monit_call_argv(json_rpc_response_t *callback,
        void *data,
        int monid,
        char *table,
        int mon_flags,
        int argc,
        char *argv[])
{
    json_t * jparams;
    json_t * jtblval;
    json_t * jtbl;
    bool retval = false;

    jparams = json_array();

    /* add default DB name */
    json_array_append_new(jparams, json_string(OVSDB_DEF_DB));

    /* Second  parameter is user defined string, this if first
     * argument in variable list of arguments*/
    json_array_append_new(jparams, json_integer(monid));

    jtbl = json_object();

    jtblval = ovsdb_mon_tbl_val(mon_flags, argc, argv);

    json_object_set_new(jtbl, table, jtblval);

    /* Third parameter is table name */
    json_array_append_new(jparams, jtbl);

    retval = ovsdb_method_send(callback, data, MT_MONITOR, jparams);

    return retval;
}

bool OVSDB_GEN_DECL(ovsdb_monit_call, json_rpc_response_t *callback, void *data, int monid, char *table, int mon_flags)
{
    OVSDB_GEN_CALL(ovsdb_monit_call, callback, data, monid, table, mon_flags);
}

bool OVSDB_VA_DECL(ovsdb_monit_call, json_rpc_response_t *callback, void *data, int monid, char *table, int mon_flags)
{
    OVSDB_VA_CALL(ovsdb_monit_call, callback, data, monid, table, mon_flags);
}


/**
 * Following three functions are different forms for
 * creating and sending echo json request
 */
bool ovsdb_echo_call_argv(json_rpc_response_t *callback,
                          void * data,
                          int argc,
                          char *argv[])
{

    json_t * jparams;
    bool retval = false;
    int i;

    jparams = json_array();

    /* threat all arguments as string array member, just send them all*/
    for (i=0; i < argc; i++)
    {
        json_array_append_new(jparams, json_string(argv[i]));
    }

    retval = ovsdb_method_send(callback, data, MT_ECHO, jparams);

    return retval;
}


/* printf style echo API  ovsdb_echo_call*/
bool OVSDB_GEN_DECL(ovsdb_echo_call, json_rpc_response_t *callback, void *data)
{
    OVSDB_GEN_CALL(ovsdb_echo_call, callback, data);
}

/* vsprintf style echo API - ovsdb echo call */
bool OVSDB_VA_DECL(ovsdb_echo_call, json_rpc_response_t *callback, void *data)
{
    OVSDB_VA_CALL(ovsdb_echo_call, callback, data);
}


/**
 * Following three functions are different forms for
 * creating and sending echo json request
 */
bool ovsdb_echo_call_s_argv(int argc, char *argv[])
{

    json_t *    jparams;
    json_t *    response = NULL;
    int         i;
    const char *response_str;
    bool        ret = false;

    jparams = json_array();

    /* threat all arguments as string array member, just send them all */
    for (i=0; i < argc; i++)
    {
        json_array_append_new(jparams, json_string(argv[i]));
    }

    response = ovsdb_method_send_s(MT_ECHO, jparams);

    if (response == NULL)
    {
        LOG(ERR, "Error sync send echo!\n");
        return false;
    }

    /* check on echo */
    response_str = json_dumps_static(response, 0);

    if (strstr(response_str, argv[0]))
    {
        ret = true;
    }

    /* Free the response */
    json_decref(response);

    return ret;
}


/*
 * Following set of three functions allow user to filter row jsons, i.e.
 * to exclude unwanted keys from the element
 *
 * The reasoning behind these functions is to use default schema generator,
 * to the "full" row, and then to leave in given json object only those
 * rows needed for modify, mu
 *
 */
json_t *ovsdb_row_filter_argv(json_t * row, int argc, char ** argv)
{
    const char *key;
    json_t *value;

    if (NULL == row)
    {
        goto end;
    }

    /* iterate through all keys in given json and remove unwanted */
    json_object_foreach(row, key, value)
    {
        if (!is_inarray(key, argc, argv))
        {
            /* remove the key value pair thats not in the provided
               list of keys
            */
            if (0 != json_object_del(row, key))
            {
                LOG(ERR, "error deleting key %s", key);
            }
        }
        else
        {
            LOG(TRACE, "filter keep key: %s", key);
        }
    }

end:
    return row;
}

json_t *OVSDB_GEN_DECL(ovsdb_row_filter, json_t *row)
{
     OVSDB_GEN_CALL(ovsdb_row_filter, row);
}

json_t *OVSDB_VA_DECL(ovsdb_row_filter, json_t *row)
{
     OVSDB_VA_CALL(ovsdb_row_filter, row);
}

/*
 * Same as ovsdb_row_filter..(), except it filters out the columns present IN the list
 */
json_t *ovsdb_row_filtout_argv(json_t * row, int argc, char ** argv)
{
    const char *key;
    json_t *value;

    if (NULL == row)
    {
        goto end;
    }

    /* iterate through all keys in given json and remove unwanted */
    json_object_foreach(row, key, value)
    {
        if (is_inarray(key, argc, argv))
        {
            /* remove the key thats is in the list of keys */
            json_object_del(row, key);
            LOG(TRACE, "filter out key: %s", key);
        }
    }

end:
    return row;
}

json_t *OVSDB_GEN_DECL(ovsdb_row_filtout, json_t *row)
{
     OVSDB_GEN_CALL(ovsdb_row_filtout, row);
}

json_t *OVSDB_VA_DECL(ovsdb_row_filtout, json_t *row)
{
     OVSDB_VA_CALL(ovsdb_row_filtout, row);
}



/*
 * The following is a set of functions used for ovsdb transaction method
 * the top functions is ovsdb_tran_call, whiles other are utility functions
 * created to enable easy & fast creation of where conditions or creation
 * of raw to be inserted
 */

/*
 * OVSDB special value
 */
json_t *ovsdb_tran_special_value(const char *type, const char *value)
{
    json_t *js;

    js = json_array();
    json_array_append_new(js, json_string(type));
    json_array_append_new(js, json_string(value));

    return js;
}

/*
 * OVSDB UUID value
 */
json_t *ovsdb_tran_uuid_json(const char *uuid)
{
    return ovsdb_tran_special_value("uuid", uuid);
}

/**
 * Create a OVS "where" selector
 */
json_t * ovsdb_tran_cond_single_json(
        const char * column,
        ovsdb_func_t func,
        json_t * value)
{
    json_t * js;
    /* for a moment ignore column, assume all values are string */

    js = json_array();
    json_array_append_new(js, json_string(column));
    json_array_append_new(js, json_string(ovsdb_functions[func]));
    json_array_append_new(js, value);

    return js;
}

json_t * ovsdb_tran_cond_single(
        char * column,
        ovsdb_func_t func,
        char * value)
{
    return ovsdb_tran_cond_single_json(column, func, json_string(value));
}

/*
 * This functions generates conditional array in <where:<> filed
 */
json_t * ovsdb_tran_cond(ovsdb_col_t col_type,
                         const char * column,
                         ovsdb_func_t func,
                         const void * value)
{
    const json_int_t *jint = value;
    json_t * jval = NULL;
    json_t * jtop;

    /* wrap-up into additional array */
    jtop = json_array();

    switch(col_type) {

        // Value is a string
        case OCLM_STR:
            jval = json_string(value);
            break;

        // Value is a UUID
        case OCLM_UUID:
            jval = ovsdb_tran_uuid_json(value);
            break;

        case OCLM_BOOL:
            jval = json_boolean(value);
            break;

        case OCLM_INT:
            jval = json_integer(*jint);
            break;

        default:
            assert(!"Invalid col_type passed to ovsdb_tran_cond()");

    }

    json_array_append_new(jtop, ovsdb_tran_cond_single_json(column, func, jval));
    return jtop;
}

/*
 * Convert JSON array to OVSDB set.
 * NULL array creates empty set
 * If raw == true, assume full array passed in
 */
json_t * ovsdb_tran_array_to_set(json_t *js_array, bool raw)
{
    json_t * js;
    json_t * js_set_array;

    if (!js_array) {
        js_set_array = json_array();
    }
    else if (raw) {
        js_set_array = js_array;
    }
    else {
        // single entry
        js_set_array = json_array();
        json_array_append_new(js_set_array, js_array);
    }

    js = json_array();
    json_array_append_new(js, json_string("set"));
    json_array_append_new(js, js_set_array);

    return js;
}

bool ovsdb_tran_comment(json_t *js_array, ovsdb_tro_t oper, json_t *where)
{
    char comment[128];

    char *op = NULL;
    json_t *js = NULL;

    if (ovsdb_comment == NULL) return true;

    STRSCPY(comment, ovsdb_comment);

    switch (oper)
    {
        case OTR_INSERT:
            op = "insert";
            break;

        case OTR_UPDATE:
            op = "update";
            break;

        case OTR_MUTATE:
            op = "mutate";
            break;

        case OTR_DELETE:
            op = "delete";
            break;

        default:
            return true;
    }

    strscat(comment, " - ", sizeof(comment));
    strscat(comment, op, sizeof(comment));

    /* Collapse the where 2-dimensional array to a string */
    if (json_is_array(where))
    {
        size_t ii;

        for (ii = 0; ii < json_array_size(where); ii++)
        {
            json_t *js = json_array_get(where, ii);
            size_t ij;

            strscat(comment, ", ", sizeof(comment));

            if (!json_is_array(js))
            {
                strscat(comment, "{uknown}", sizeof(comment));
                continue;
            }

            for (ij = 0; ij < json_array_size(js); ij++)
            {
                const char *str = json_string_value(json_array_get(js, ij));

                if (str == NULL)
                {
                    strscat(comment, "<null>", sizeof(comment));
                }
                else
                {
                    strscat(comment, str, sizeof(comment));
                }
            }
        }
    }

    js = json_object();
    if (js == NULL) goto error;

    if (json_object_set_new(js, "op", json_string("comment")) != 0)
    {
        goto error;
    }

    if (json_object_set_new(js, "comment", json_string(comment)) != 0)
    {
        goto error;
    }

    if (json_array_append_new(js_array, js) != 0)
    {
        goto error;
    }

    return true;

error:
    if (js != NULL) json_decref(js);

    return false;
}

/*
 * This function is for creating large, complex transactions
 *
 * In case jarray is NULL, new object is created and returned,
 * otherwise new transaction is appended to it.
 *
 * If js_obj is NULL, a new transaction object is created, otherwise
 * it would be used as the base transaction object
 *
 * After creating complex transaction with this API, call
 * ovsdb_method_json to send it to ovsdb
 */
json_t * ovsdb_tran_multi(json_t * jarray,
                          json_t * js_obj,
                          const char * table,
                          ovsdb_tro_t oper,
                          json_t * where,
                          json_t * row)
{
    /* create main json array transaction object in case it is NULL */
    if (jarray == NULL)
    {
        jarray = json_array();
        json_array_append_new(jarray, json_string(OVSDB_DEF_DB));
    }

    /* Insert comment as the first thing */
    ovsdb_tran_comment(jarray, oper, where);

    if (js_obj == NULL)
    {
        js_obj = json_object();
    }

    json_object_set_new(js_obj, "table", json_string(table));
    json_object_set_new(js_obj, "op", ovsdb_tran_operation(oper));

    /* In case there  is not select statement, where has to empty array*/
    if (NULL != where)
    {
        json_object_set_new(js_obj, "where", where);
    }
    else if(oper != OTR_INSERT)
    {
        json_object_set_new(js_obj, "where", json_array());
    }

    if (NULL != row)
    {
        if (oper == OTR_MUTATE)
        {
            json_object_set_new(js_obj, "mutations", row);
        }
        else {
            json_object_set_new(js_obj, "row", row);
        }
    }

    json_array_append_new(jarray, js_obj);
    return jarray;
}

/*
 * This function creates a combined OVSDB transaction which
 * inserts a new row, and inserts it's UUID into a parent
 * table's column using mutate
 */
json_t * ovsdb_tran_insert_with_parent(const char * table,
                                       json_t * row,
                                       const char * parent_table,
                                       json_t * parent_where,
                                       const char * parent_column)
{
    json_t * js;
    json_t * jarray;
    json_t * js_obj;
    json_t * js_mutations;
    json_t * js_named_uuid;

    /* First transaction: Insert row into table with named UUID */
    js_obj = json_object();
    json_object_set_new(js_obj, "uuid-name", json_string("child_id"));
    jarray = ovsdb_tran_multi(NULL,         // NULL = first transaction
                              js_obj,       // Initial transact object
                              table,        // Table for insert
                              OTR_INSERT,   // Insert operation
                              NULL,         // No where clause
                              row);         // Row to insert

    /* Second transaction: Parent table mutation to insert UUID */
    js_named_uuid = json_array();
    json_array_append_new(js_named_uuid, json_string("named-uuid"));
    json_array_append_new(js_named_uuid, json_string("child_id"));

    js = json_array();
    json_array_append_new(js, json_string(parent_column));
    json_array_append_new(js, ovsdb_tran_operation(OTR_INSERT));
    json_array_append_new(js, ovsdb_tran_array_to_set(js_named_uuid, false));

    js_mutations = json_array();
    json_array_append_new(js_mutations, js);

    jarray = ovsdb_tran_multi(jarray,       // Append to first one
                              NULL,         // New trans obj
                              parent_table, // Parent Table
                              OTR_MUTATE,   // Mutate operation
                              parent_where, // Where cause
                              js_mutations);// Mutations

    return jarray;
}

/*
 * This function creates a combined OVSDB transaction which
 * deletes a rows by uuid in one table, and then removes
 * them from a parent table's column using mutate
 */
json_t * ovsdb_tran_delete_with_parent(const char * table,
                                       json_t * uuids,
                                       const char * parent_table,
                                       json_t * parent_where,
                                       const char * parent_column)
{
    json_t * js;
    json_t * jarray = NULL;
    json_t * jwhere;
    json_t * js_uuid;
    json_t * js_mutations;
    const char *uuid;
    size_t index;

    /* Create a sub-transaction per UUID to be deleted */
    json_array_foreach(uuids, index, js_uuid) {
        uuid = json_string_value(json_array_get(js_uuid, 1));
        jwhere = ovsdb_tran_cond(OCLM_UUID, "_uuid", OFUNC_EQ, uuid);
        jarray = ovsdb_tran_multi(jarray,       // Append transaction
                                  NULL,         // New trans obj
                                  table,        // Table for insert
                                  OTR_DELETE,   // Delete operation
                                  jwhere,       // Where clause
                                  NULL);        // No row info
    }

    /* Final sub-transaction: Parent table mutation to delete UUIDs */
    js = json_array();
    json_array_append_new(js, json_string(parent_column));
    json_array_append_new(js, ovsdb_tran_operation(OTR_DELETE));
    json_array_append_new(js, ovsdb_tran_array_to_set(uuids, true));

    js_mutations = json_array();
    json_array_append_new(js_mutations, js);

    jarray = ovsdb_tran_multi(jarray,       // Append transaction
                              NULL,         // New trans obj
                              parent_table, // Parent Table
                              OTR_MUTATE,   // Mutate operation
                              parent_where, // Where cause
                              js_mutations);// Mutations

    return jarray;
}

/* main function for sending transaction JSON requests */
bool ovsdb_tran_call(json_rpc_response_t *cb,
                     void * data,
                     char * table,
                     ovsdb_tro_t oper,
                     json_t * where,
                     json_t * row)
{
    return ovsdb_method_send(cb, data, MT_TRANS,
              ovsdb_tran_multi(NULL, NULL, table, oper, where, row));
}

