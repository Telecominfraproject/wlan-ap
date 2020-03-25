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

#ifndef __OVSDB__H__
#define __OVSDB__H__

#include <stdbool.h>
#include <stdio.h>
#include <ev.h>

#include "ds_tree.h"
#include "c_tricks.h"

/* Generate C-headers from PJS defines */
#include "ovsdb_jsonrpc.pjs.h"
#include "pjs_gen_h.h"

/* defines */
#define MON_SEL_MODIFY  0
#define MON_SEL_DELETE  1
#define MON_SEL_INSERT  2
#define MON_SEL_INITIAL 3
#define MON_SEL_CNAME_START(X)  X + 4

#define MON_SELPREFIX_ALL   "++++"
#define MON_SEL_MOD         "++++"

#define MONSEL_T            "+"
#define MONSEL_F            "-"

#define MON_SEL_CNAME

/* MONITOR column macros - prefix column name with selection specifier*/
#define MON_COLUMN(name, m, d, i, t)    m d i t name
#define MON_COLUMN_ALL(name)            MON_SELPREFIX_ALL name
#define MON_COLUMN_MOD(name)            MONSEL_T MONSEL_F MONSEL_F MONSEL_F name
#define MON_COLUMN_DEL(name)            MONSEL_F MONSEL_T MONSEL_F MONSEL_F name
#define MON_COLUMN_INS(name)            MONSEL_F MONSEL_F MONSEL_T MONSEL_F name
#define MON_COLUMN_INI(name)            MONSEL_F MONSEL_F MONSEL_F MONSEL_T name

/* Macros for checking select prefix */
#define MOD_SEL_CHK_MODIFY(c)           (c[0] == '+')
#define MOD_SEL_CHK_DELETE(c)           (c[1] == '+')
#define MOD_SEL_CHK_INSERT(c)           (c[2] == '+')
#define MOD_SEL_CHK_INITIAL(c)          (c[3] == '+')

/*
 * Provided you implement the _argv version of a function, you can use these macros to automatically generate
 * the regular and _va function flavors.
 */

/* Generate the regular function declaration */
#define OVSDB_GEN_DECL(method, ...) method(__VA_ARGS__, ...)
#define OVSDB_GEN_CALL(method, ...)                                             \
{                                                                               \
    va_list  va;                                                                \
    char    *argv[256];                                                         \
    char    *parg;                                                              \
                                                                                \
    int      argc = 0;                                                          \
                                                                                \
    va_start(va, VA_LAST(__VA_ARGS__));                                         \
                                                                                \
    while ((parg = va_arg(va, char *)) != NULL)                                 \
    {                                                                           \
        if (argc >= ARRAY_LEN(argv))                                            \
        {                                                                       \
            LOG(ERR, "ovsdb_ ## method ## _call_va() too many arguments.");     \
            return false;                                                       \
        }                                                                       \
                                                                                \
        argv[argc++] = parg;                                                    \
    }                                                                           \
                                                                                \
    va_end(va);                                                                 \
                                                                                \
    return method  ## _argv(__VA_ARGS__, argc, argv);                           \
}

#define OVSDB_VA_DECL(method, ...) method ## _va(__VA_ARGS__, va_list va)
#define OVSDB_VA_CALL(method, ...)                                              \
{                                                                               \
    char *argv[256];                                                            \
    char *parg;                                                                 \
                                                                                \
    int argc = 0;                                                               \
                                                                                \
    while ((parg = va_arg(va, char *)) != NULL)                                 \
    {                                                                           \
        if (argc >= ARRAY_LEN(argv))                                            \
        {                                                                       \
            LOG(ERR, "ovsdb_ ## method ## _call_va() too many arguments.");     \
            return false;                                                       \
        }                                                                       \
                                                                                \
        argv[argc++] = parg;                                                    \
    }                                                                           \
                                                                                \
    return method  ## _argv(__VA_ARGS__, argc, argv);                           \
}




/* Generate the _va function declaration */

/* typedefs */
typedef void json_rpc_response_t(int id, bool is_error, json_t *js, void * data);

typedef void ovsdb_update_process_t(int id, json_t *js, void * data);

/*
 * Supported methods
 */
typedef enum
{
    MT_ECHO,
    MT_MONITOR,
    MT_TRANS,
}ovsdb_mt_t;


/*
 * Supported transact operations
 */
typedef enum
{
    OTR_INSERT,
    OTR_SELECT,
    OTR_UPDATE,
    OTR_MUTATE,
    OTR_DELETE,
    OTR_WAIT,
}ovsdb_tro_t;


/*
 * Transact comparison options used in conditional object
 */
typedef enum
{
   OFUNC_EQ,
   OFUNC_NEQ,
   OFUNC_LT,
   OFUNC_LTE,
   OFUNC_GT,
   OFUNC_GTE,
   OFUNC_INC,
   OFUNC_EXC,
}ovsdb_func_t;

/*
 * Transact Column types options
 */
typedef enum
{
    OCLM_INT,
    OCLM_REAL,
    OCLM_BOOL,
    OCLM_STR,
    OCLM_UUID,
    OCLM_MAP,
    OCLM_SET,
}
ovsdb_col_t;


/*
 * Monitor request selections
 */
#define OMT_INITIAL     (1 << 0)            /* Initial */
#define OMT_INSERT      (1 << 1)            /* Insert */
#define OMT_DELETE      (1 << 2)            /* Delete */
#define OMT_MODIFY      (1 << 3)            /* Modify */

#define OMT_ALL         (OMT_INITIAL | OMT_INSERT | OMT_DELETE | OMT_MODIFY)

/* API */
bool ovsdb_init_loop(struct ev_loop *loop, const char *name);
bool ovsdb_init(const char *name);
bool ovsdb_ready(const char *name);
bool ovsdb_stop_loop(struct ev_loop *loop);
bool ovsdb_stop(void);

/*
 * This function allows user to send 'raw' json request
 *
 * At the moment it is used in dm CLI, but maybe used elsewhere
 * It is recommended not to (mis)use this functions
 */
bool ovsdb_method_json(json_rpc_response_t *callback,
                       void * data,
                       char * buffer,
                       size_t sz);

bool ovsdb_method_send(json_rpc_response_t *callback,
                       void * data,
                       ovsdb_mt_t mt,
                       json_t * jparams);

/*
 * Sync version of method send function
 */
json_t *ovsdb_method_send_s(ovsdb_mt_t mt, json_t * jparams);

/*
 * The following functions generate and send echo json method request
 *
 */
bool ovsdb_echo_call_argv(json_rpc_response_t *cb,
        void * data,
        int argc,
        char ** argv);

bool OVSDB_GEN_DECL(ovsdb_echo_call,
        json_rpc_response_t *callback,
        void *data);

bool OVSDB_VA_DECL(ovsdb_echo_call,
        json_rpc_response_t *callback,
        void *data);


/*
 * The following functions generate and send echo json method request
 * - synchronous versions
 */
bool ovsdb_echo_call_s_argv(int argc, char ** argv);

/*
 * The following functions generate and send monitor method json request
 *
 */
bool ovsdb_monit_call_argv(json_rpc_response_t *cb,
        void *data,
        int monid,
        char *table,
        int mon_flags,
        int argc,
        char **argv);

bool OVSDB_GEN_DECL(ovsdb_monit_call,
        json_rpc_response_t *callback,
        void *data,
        int monid,
        char *table,
        int mon_flags);

bool OVSDB_VA_DECL(ovsdb_monit_call,
        json_rpc_response_t *callback,
        void *data,
        int monid,
        char *table,
        int mon_flags);

/*
 * The following function creates and sends transaction method json
 *
 */
bool ovsdb_tran_call(json_rpc_response_t *cb,
                     void * data,
                     char * table,
                     ovsdb_tro_t operation,
                     json_t * where,
                     json_t * row);

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
                          json_t * row);

/*
 * This function creates a combined OVSDB transaction which
 * inserts a new row, and inserts it's UUID into a parent
 * table's column using mutate
 */
json_t * ovsdb_tran_insert_with_parent(const char * table,
                                       json_t * row,
                                       const char * parent_table,
                                       json_t * parent_where,
                                       const char * parent_column);

/*
 * This function creates a combined OVSDB transaction which
 * deletes a rows by uuid in one table, and then removes
 * them from a parent table's column using mutate
 */
json_t * ovsdb_tran_delete_with_parent(const char * table,
                                       json_t * uuids,
                                       const char * parent_table,
                                       json_t * parent_where,
                                       const char * parent_column);

/*
 * OVSDB special value
 */
json_t *ovsdb_tran_special_value(const char *type, const char *value);

/*
 * OVSDB UUID value
 */
json_t *ovsdb_tran_uuid_json(const char *uuid);

/*
 * Convert JSON array to OVSDB set.
 * NULL array creates empty set.
 * If raw == true, assume full array passed in
 */
json_t * ovsdb_tran_array_to_set(json_t *js_array, bool raw);

/**
 * The following function creates "where" part of json request
 * used in transaction calls
 */
json_t * ovsdb_tran_cond(ovsdb_col_t,
                         const char * column,
                         ovsdb_func_t func,
                         const void * value);

/**
 * The following function creates "where" part of json request
 * used in transaction calls, without enclosing [] -- this
 * function does not create a complete where selector.
 */
json_t * ovsdb_tran_cond_single(char * column,
                                ovsdb_func_t func,
                                char * value);

json_t * ovsdb_tran_cond_single_json(const char * column,
                                ovsdb_func_t func,
                                json_t * value);

/**
 * Issue a synchronous request to OVSDB
 *
 */
json_t *ovsdb_method_send_s(
        ovsdb_mt_t mt,
        json_t * jparams);

/**
 * Synchronous version of ovsdb_tran_call()
 */
json_t *ovsdb_tran_call_s(
        const char * table,
        ovsdb_tro_t oper,
        json_t * where,
        json_t * row);

/*
 * This function uses ovsdb_method_send_s() to send a
 * transaction created with ovsdb_tran_insert_with_parent()
 */
bool ovsdb_insert_with_parent_s(char * table,
                                json_t * row,
                                char * parent_table,
                                json_t * parent_where,
                                char * parent_column);

/*
 * This function builds a uuid list from where clause,
 * then uses ovsdb_method_send_s() to send a * transaction
 * created with ovsdb_tran_delete_with_parent()
 */
json_t* ovsdb_delete_with_parent_res_s(const char * table,
                                json_t *where,
                                const char * parent_table,
                                json_t * parent_where,
                                const char * parent_column);

bool ovsdb_delete_with_parent_s(char * table,
                                json_t *where,
                                char * parent_table,
                                json_t * parent_where,
                                char * parent_column);

/**
 * The following set of functions filters all json key-value pairs
 * except those for which key names are submitted
 */
json_t *ovsdb_row_filter_argv(json_t * js, int argc, char ** argv);
json_t *OVSDB_GEN_DECL(ovsdb_row_filter, json_t *row);
json_t *OVSDB_VA_DECL(ovsdb_row_filter, json_t *row);

/*
 * Same as above, except it filters out the listed columns
 */
json_t *ovsdb_row_filtout_argv(json_t * js, int argc, char ** argv);
json_t *OVSDB_GEN_DECL(ovsdb_row_filtout, json_t *row);
json_t *OVSDB_VA_DECL(ovsdb_row_filtout, json_t *row);

int ovsdb_get_jsonrpc_id(void);

int ovsdb_register_update_cb(ovsdb_update_process_t *fn, void *data);
int ovsdb_unregister_update_cb(int mon_id);

/*
 * Global list of JSON-RPC handlers
 */
struct rpc_response_handler
{
    int                     rrh_id;                     /**< Response ID */
    json_rpc_response_t    *rrh_callback;               /**< Callback   */
    void                   *data;                       /**< User data  */
    ds_tree_node_t          rrh_node;                   /**< Node structure */
};

/*
 * Global list of JSON-RPC handlers
 */
struct rpc_update_handler
{
    int                     rrh_id;                     /**< Response ID */
    ovsdb_update_process_t    *rrh_callback;               /**< Callback   */
    void                   *data;                       /**< User data  */
    ds_tree_node_t          rrh_node;                   /**< Node structure */
};

#endif /* __OVSDB__H__ */
