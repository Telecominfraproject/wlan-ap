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

#include <stdarg.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <netinet/in.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <jansson.h>
#include <stdint.h>

#include <ev.h>

#include "log.h"
#include "ovsdb.h"
#include "statem.h"
#include "schema.h"
#include "os_nif.h"
#include "os_types.h"
#include "target.h"
#include "json_util.h"

#include "dm.h"

#define DM_ECHO_STRING    "Hello JSON-RPC"

STATE_MACHINE_USE;

extern const char *app_build_ver_get();

/*
 * Echo request CB. Invoked on echo response received
 */
void echo_cb(int id, bool is_error, json_t *msg, void * data)
{
    (void)id;
    (void)is_error;
    (void)data;
    (void)msg;
    char * str;

    if (is_error)
    {
        str = json_dumps(msg, 0);
        LOG(ERR, "ECHO error::msg=%s\n", str);
        json_free(str);
        STATE_TRANSIT(STATE_ERROR);
    }
    else
    {
        LOG(DEBUG, "ECHO success");
        STATE_TRANSIT(STATE_CHECK_ID);
    }

}

/*
 * AWLAN_Node read request callback
 */
void select_awlan_cb(int id, bool is_error, json_t *msg, void * data)
{

    (void)id;
    (void)is_error;
    (void)data;
    (void)msg;
    json_t * rows;

    /* response itself is an array, extract it from response array */
    if (1 == json_array_size(msg))
    {
        rows = json_array_get(msg, 0);
    }
    else
    {
        LOG(ERR, "ERROR response array size::size=%d", (int)json_array_size(msg));
        STATE_TRANSIT(STATE_ERROR);
        return;
    }

    /* check the number of rows returned rows */
    if (0 == json_array_size(json_object_get(rows, "rows")))
    {
        /* table is empty, we need to update entity information */
        STATE_TRANSIT(STATE_INSERT_ID);
    }
    else if (1 == json_array_size(json_object_get(rows, "rows")))
    {
        /* row isn't empty, check if ID is already present */
        LOG(NOTICE, "AWLAN table not empty, to be updated");
        /* go into idle mode */
        STATE_TRANSIT(STATE_UPDATE_ID);
    }
    else
    {
        /* this table has constraint, having more then one row should be
         * possible, at least in theory
         */
        STATE_TRANSIT(STATE_ERROR);
        return;
    }
}

/*
 * AWLAN_Node table insert callback, called upon failed or successful
 * insert operation
 */
void insert_awlan_cb(int id, bool is_error, json_t *msg, void * data)
{

    (void)id;
    (void)is_error;
    (void)data;
    (void)msg;
    char * str;
    json_t * uuids = NULL;
    size_t index;
    json_t *value;
    json_t *oerr;

    str = json_dumps(msg, 0);
    LOG(NOTICE, "insert json response: %s\n", str);
    json_free(str);

    /* response itself is an array, extract it from response array  */
    if (1 == json_array_size(msg))
    {
        uuids = json_array_get(msg, 0);

        uuids = json_object_get(uuids, "uuid");

        if (NULL != uuids)
        {
            str = json_dumps (uuids, 0);
            LOG(NOTICE, "AWLAN_NODE::uuid=%s", str);
            json_free(str);
        }

        STATE_TRANSIT(STATE_START_MNG);
    }
    else
    {
        /* array longer then 1 means an error occurred          */
        /* process response & try to extract response error     */
        json_array_foreach(msg, index, value)
        {
            if (json_is_object(value))
            {
                oerr = json_object_get(value, "error");
                if (NULL != oerr)
                {
                    LOG(ERR, "OVSDB error::msg=%s", json_string_value(oerr));
                }

                oerr = json_object_get(value, "details");
                if (NULL != oerr)
                {
                    LOG(ERR, "OVSDB details::msg=%s", json_string_value(oerr));
                }
            }
        }

        /* in any case set state to ERROR   */
        STATE_TRANSIT(STATE_ERROR);
    }

}


/*
 * AWLAN_Node table update callback, called upon failed or successful
 * insert operation
 */
void update_awlan_cb(int id, bool is_error, json_t *msg, void * data)
{

    (void)id;
    (void)is_error;
    (void)data;
    (void)msg;
    char * str;
    json_t * uuids = NULL;
    size_t index;
    json_t *value;
    json_t *oerr;

    str = json_dumps(msg, 0);
    LOG(NOTICE, "update json response: %s\n", str);
    json_free(str);


    /* response itself is an array, extract it from response array  */
    if (1 == json_array_size(msg))
    {
        uuids = json_array_get(msg, 0);

        uuids = json_object_get(uuids, "uuid");

        if (NULL != uuids)
        {
            str = json_dumps (uuids, 0);
            LOG(NOTICE, "AWLAN_NODE::uuid=%s", str);
            json_free(str);
        }

        STATE_TRANSIT(STATE_START_MNG);
    }
    else
    {
        /* array longer then 1 means an error occurred          */
        /* process response & try to extract response error     */
        json_array_foreach(msg, index, value)
        {
            if (json_is_object(value))
            {
                oerr = json_object_get(value, "error");
                if (NULL != oerr)
                {
                    LOG(ERR, "OVSDB error::msg=%s", json_string_value(oerr));
                }

                oerr = json_object_get(value, "details");
                if (NULL != oerr)
                {
                    LOG(ERR, "OVSDB details::msg=%s", json_string_value(oerr));
                }
            }
        }

        /* in any case set state to ERROR */
        STATE_TRANSIT(STATE_ERROR);
    }

}


/* send echo request and check if communication with ovsdb works */
bool act_check_ovsdb()
{
    bool retval = false;

    retval = ovsdb_echo_call(echo_cb, NULL, DM_ECHO_STRING, NULL);

    return retval;
}


/* send AWLAN_Node table read request -
 * to check if we need to re-write entity information
 */
bool act_check_id (void)
{
    bool retval = false;

    retval = ovsdb_tran_call(select_awlan_cb,
                             NULL,
                             "AWLAN_Node",
                             OTR_SELECT,
                             NULL,
                             NULL);

    return retval;
}


void fill_entity_data(struct schema_AWLAN_Node * s_awlan_node)
{
    char buff[TARGET_BUFF_SZ];

    if (NULL == s_awlan_node)
    {
        return;
    }
    memset(s_awlan_node, 0, sizeof(struct schema_AWLAN_Node));

    if (true == target_serial_get(buff, sizeof(buff)))
    {
        strcpy(s_awlan_node->serial_number, buff);
    }
    else
    {
        /* get some dummy value in this case    */
        strcpy(s_awlan_node->serial_number,"1234567890");
    }
    s_awlan_node->serial_number_exists = true;

    if (true == target_id_get(buff, sizeof(buff)))
    {
        strcpy(s_awlan_node->id, buff);
    }
    else
    {
        /* get some dummy value in this case    */
        strcpy(s_awlan_node->id,"1234567890");
    }
    s_awlan_node->id_exists = true;

    if (true == target_sku_get(buff, sizeof(buff)))
    {
        strcpy(s_awlan_node->sku_number, buff);
        s_awlan_node->sku_number_exists = true;
    }

    if (true == target_hw_revision_get(buff, sizeof(buff)))
    {
        strcpy(s_awlan_node->revision, buff);
    }
    else
    {
        strcpy(s_awlan_node->revision, "1");
    }

    if (true == target_sw_version_get(buff, sizeof(buff)))
    {
        strcpy(s_awlan_node->firmware_version, buff);
    }
    else
    {
        strcpy(s_awlan_node->firmware_version, app_build_ver_get());
    }

    if (true == target_platform_version_get(buff, sizeof(buff)))
    {
        strcpy(s_awlan_node->platform_version, buff);
    }

    if (true == target_model_get(buff, sizeof(buff)))
    {
        strcpy(s_awlan_node->model, buff);
    }
    else
    {
        strcpy(s_awlan_node->model, TARGET_NAME);
    }
    s_awlan_node->model_exists = true;

    LOG(NOTICE, "Device entity {serial=%s id=%s version=%s platform=%s sku=%s}",
            s_awlan_node->serial_number,
            s_awlan_node->id,
            s_awlan_node->firmware_version,
            s_awlan_node->platform_version,
            (strlen(s_awlan_node->sku_number) > 0
             ? s_awlan_node->sku_number
             : "empty")
       );
}

/*
 * Try to fill in AWLAN_Node table with Node entity data
 */
bool act_insert_entity (void)
{
    bool retval = false;
    struct schema_AWLAN_Node s_awlan_node;

    /* populate date to be inserted */
    fill_entity_data(&s_awlan_node);

    /* fill the row with NODE data */
    retval = ovsdb_tran_call(insert_awlan_cb,
                             NULL,
                             "AWLAN_Node",
                             OTR_INSERT,
                             NULL,
                             schema_AWLAN_Node_to_json(&s_awlan_node, NULL));

    return retval;
}

/*
 * Try to fill in AWLAN_Node table with Node entity data
 */
bool act_update_entity (void)
{
    bool retval = false;
    struct schema_AWLAN_Node s_awlan_node;

    /* populate date to be inserted */
    fill_entity_data(&s_awlan_node);

    /* fill the row with NODE data */
    retval = ovsdb_tran_call(update_awlan_cb,
                             NULL,
                             "AWLAN_Node",
                             OTR_UPDATE,
                             /* single row table  */
                             ovsdb_tran_cond(OCLM_STR, "id", OFUNC_NEQ, "empty"),
                             ovsdb_row_filter(
                                 schema_AWLAN_Node_to_json(&s_awlan_node, NULL),
                                 "id",
                                 "firmware_url",
                                 "firmware_version",
                                 "platform_version",
                                 "model",
                                 "revision",
                                 "serial_number",
                                 "sku_number",
                                 "upgrade_status",
                                 "upgrade_timer",
                                 NULL)
                             );

    return retval;
}
