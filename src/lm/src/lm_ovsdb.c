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

#include "os.h"
#include "util.h"
#include "ovsdb.h"
#include "ovsdb_update.h"
#include "ovsdb_sync.h"
#include "ovsdb_table.h"
#include "ovsdb_cache.h"
#include "schema.h"
#include "log.h"
#include "ds.h"
#include "json_util.h"
#include "target.h"

#include "lm.h"

#define MODULE_ID LOG_MODULE_ID_OVSDB

// Dummy configs for OVS table initialization
#define LM_CONFIG_DUMMY_NAME                "lm"
#define LM_CONFIG_DUMMY_PERIODICITY         "weekly"
#define LM_CONFIG_DUMMY_UPLOAD_TOKEN        ""
#define LM_CONFIG_DUMMY_UPLOAD_LOCATION     ""


static void lm_update_logging_state_file();

typedef struct
{
    const char *name;
    int         trigger;
} trigger_config_t;

/**
 * Globals
 */

extern lm_state_t g_state;

ovsdb_table_t table_AW_LM_Config;
ovsdb_table_t table_AW_LM_State; // Not used
ovsdb_table_t table_AW_Debug;

// Add modules to this data structure, if they need to dump data on log
// trigger events. Note that modules also need to be registered.
static trigger_config_t g_trigger_config[] = {
    { "FM", 0 },
    { NULL, 0 }
};

void lm_trigger_modules()
{
    int ii;
    for (ii = 0; g_trigger_config[ii].name; ii++)
    {
        g_trigger_config[ii].trigger += 1;
        LOGD("Triggering module %s [trigger: %d]",
             g_trigger_config[ii].name,
             g_trigger_config[ii].trigger);
    }

    // After counters are increased we need to update file.
    lm_update_logging_state_file();
}

void callback_AW_LM_Config(ovsdb_update_monitor_t *mon,
        struct schema_AW_LM_Config *old_rec,
        struct schema_AW_LM_Config *config)
{
    if (mon->mon_type == OVSDB_UPDATE_MODIFY)
    {
        // Update new upload location and token
        STRSCPY(g_state.config.upload_token,    config->upload_token);
        STRSCPY(g_state.config.upload_location, config->upload_location);

        // Notify other modules to dump their data
        lm_trigger_modules();

        // Run log pull script
        lm_update_state(LM_REASON_AW_LM_CONFIG);
    }
}

void lm_update_logging_state_file()
{
    // At each AW_Debug table update or at internal trigger_config update
    // we just transfer new contents to a file that is used by logging
    // library to handle dynamic log levels and triggers.
    // To do this we are using cached table and internal state and than
    // we join them into single JSON file. Example:
    //
    // {
    //  "LM": {
    //    "log_trigger": 0,
    //    "log_severity": "OVSDB;TRACE,INFO"
    //   },
    //  "WM": {
    //    "log_trigger": 1
    //   }
    // }
    //
    json_t *loggers_json = json_object();


    // Get data from AW_LM_State table
    int ii;
    for (ii = 0; g_trigger_config[ii].name; ii++)
    {
        json_t *logger = json_object();
        json_object_set_new(loggers_json, g_trigger_config[ii].name, logger);
        json_object_set_new(logger, "log_trigger", json_integer(g_trigger_config[ii].trigger));
    }

    // Get data from AW_Debug table
    ovsdb_cache_row_t *row;
    ovsdb_table_t     *table = &table_AW_Debug;
    ds_tree_foreach(&table->rows, row)
    {
        struct schema_AW_Debug *aw_debug = (void *)row->record;

        // Check if current logger is already in our loggers JSON. In
        // case it is not just crate new object.
        json_t *logger = json_object_get(loggers_json, aw_debug->name);
        if (!logger)
        {
            logger = json_object();
            json_object_set_new(loggers_json, aw_debug->name, logger);
        }

        json_object_set_new(logger, "log_severity", json_string(aw_debug->log_severity));
    }

    // Dump created JSON to file
    // first output to a tmp file then move over
    // this prevents double file change events with first event being an empty file
    char tmp_filename[256];
    snprintf(tmp_filename, sizeof(tmp_filename), "%s.tmp", g_state.log_state_file);
    if (json_dump_file(loggers_json, tmp_filename, JSON_INDENT(1)))
    {
        LOGE("Failed to dump loggers state to file %s", tmp_filename);
    }
    if (rename(tmp_filename, g_state.log_state_file)) {
        LOGE("Failed rename %s -> %s", tmp_filename, g_state.log_state_file);
    }

    json_decref(loggers_json);
}

void callback_AW_Debug(ovsdb_update_monitor_t *mon,
        struct schema_AW_Debug *old_rec,
        struct schema_AW_Debug *record,
        ovsdb_cache_row_t *row)
{
    if (mon->mon_type == OVSDB_UPDATE_MODIFY ||
        mon->mon_type == OVSDB_UPDATE_NEW ||
        mon->mon_type == OVSDB_UPDATE_DEL)
    {
        lm_update_logging_state_file();
    }
}

bool lm_ovsdb_set_severity(const char *logger_name, const char *severity)
{
    struct schema_AW_Debug aw_debug;

    MEMZERO(aw_debug);
    STRSCPY(aw_debug.name, logger_name);
    STRSCPY(aw_debug.log_severity, severity);

    if (!ovsdb_table_upsert_simple(&table_AW_Debug,
                SCHEMA_COLUMN(AW_Debug, name),
                (char*)logger_name,
                &aw_debug,
                NULL))
    {
        LOGE("Upsert into AW_Debug failed! [name:=%s, log_severity:=%s]",
                aw_debug.name, aw_debug.log_severity);
        return false;
    }
    return true;
}

int lm_ovsdb_init_AW_Debug_table()
{
    // Init state table from persistent file
    LOGI("Initializing LM table "
         "(Init OvS.AW_Debug table)");

    json_t *loggers = json_load_file(g_state.log_state_file, 0, NULL);
    if (!loggers)
    {
        LOGI("Unable to read data from persistent file \"%s\"!",
             g_state.log_state_file);
        return 0;
    }

    const char *logger_name;
    json_t *logger_config;
    json_object_foreach(loggers, logger_name, logger_config)
    {

        // Upsert log severity in AW_Debug, if it exists for current logger.
        json_t *severity = json_object_get(logger_config, "log_severity");
        if (severity &&
            json_is_string(severity) &&
            json_string_value(severity))
        {
            lm_ovsdb_set_severity(logger_name, json_string_value(severity));
        }

    }

    json_decref(loggers);

    return 0;
}

int lm_ovsdb_init_AW_LM_Config_table()
{
    struct schema_AW_LM_Config aw_lm_config;

    LOGI("Initializing LM tables "
         "(Init OvS.AW_LM_Config table)");

    MEMZERO(aw_lm_config);
    STRSCPY(aw_lm_config.name,            LM_CONFIG_DUMMY_NAME);
    STRSCPY(aw_lm_config.periodicity,     LM_CONFIG_DUMMY_PERIODICITY);
    STRSCPY(aw_lm_config.upload_token,    LM_CONFIG_DUMMY_UPLOAD_TOKEN);
    STRSCPY(aw_lm_config.upload_location, LM_CONFIG_DUMMY_UPLOAD_LOCATION);
    aw_lm_config.periodicity_exists       = true;
    aw_lm_config.upload_token_exists      = true;
    aw_lm_config.upload_location_exists   = true;

    if (!ovsdb_table_upsert(&table_AW_LM_Config, &aw_lm_config, false))
    {
        return -1;
    }

    return 0;
}

int lm_ovsdb_init_tables()
{
    int retval = 0;

    if (lm_ovsdb_init_AW_Debug_table())
    {
        LOGE("Initializing LM tables "
             "(Failed to setup OvS.AW_Debug table)");
        retval = -1;
    }

    if (lm_ovsdb_init_AW_LM_Config_table())
    {
        LOGE("Initializing LM tables "
             "(Failed to setup OvS.AW_LM_Config table)");
        retval = -1;
    }

    return retval;
}

int lm_ovsdb_init()
{
    LOGI("Initializing LM tables");

    // Initialize OVSDB tables
    OVSDB_TABLE_INIT_NO_KEY(AW_LM_Config);
    OVSDB_TABLE_INIT_NO_KEY(AW_Debug);

    if (lm_ovsdb_init_tables())
    {
        LOGE("Initializing LM tables "
             "(Failed to setup tables)");
        return -1;
    }

    // Initialize OVSDB monitor callbacks after DB is initialized
    OVSDB_TABLE_MONITOR(AW_LM_Config, false);

    if (g_state.log_state_file)
    {
        // We need to monitor this table only in case, if we are
        // mirroring its data to log_state_file.
        OVSDB_CACHE_MONITOR(AW_Debug, false);
    }

    return 0;
}
