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

#include <dlfcn.h>
#include <ev.h>          /* libev routines */
#include <getopt.h>      /* command line arguments */
#include <time.h>
#include <string.h>

#include "evsched.h"     /* ev helpers */
#include "log.h"         /* Logging routines */
#include "json_util.h"   /* json routines */
#include "os.h"          /* OS helpers */
#include "ovsdb.h"       /* ovsdb helpers */
#include "target.h"      /* target API */
#include "schema.h"
#include "ovsdb_utils.h"
#include "qm_conn.h"
#include "dppline.h"
#include "network_metadata.h" /* Network metadataAPI */
#include "fcm.h"         /* our api */
#include "fcm_priv.h"
#include "fcm_mgr.h"

static fcm_mgr_t fcm_mgr;

static int fcm_tree_node_cmp(void *a, void *b)
{
    char *name_a = a;
    char *name_b = b;
    return (strcmp(name_a, name_b));
}

static char * fcm_get_other_config_val(ds_tree_t *other_config, char *key)
{
    struct str_pair *pair;

    if ((other_config == NULL) || (key == NULL))
        return NULL;

    pair = ds_tree_find(other_config, key);
    if (pair == NULL) return NULL;

    LOGD("%s: other_config key : %s val : %s\n", __func__, key, pair->value);
    return pair->value;
}

static void fcm_get_plugin_configs(fcm_collector_t *collector,
                                   struct schema_FCM_Collector_Config *conf)
{
    char plugin_path[FCM_DSO_PATH_LEN] = {'\0'};
    char *path = NULL;
    char *init_fn = NULL;

    path = fcm_get_other_config_val(collector->collect_conf.other_config,
                                    FCM_DSO_PATH);
    if (path)
        strcpy(plugin_path, path);
    else
        strcpy(plugin_path, FCM_DSO_DFLT_PATH);
    sprintf(collector->dso_path, "%s%s%s%s", plugin_path, FCM_DSO_PREFIX,
            collector->collect_conf.name, FCM_DSO_TYPE);

    init_fn = fcm_get_other_config_val(collector->collect_conf.other_config,
                                       FCM_DSO_INIT);
    if (init_fn)
        strcpy(collector->dso_init, init_fn);
    LOGD("%s: Plugin name: %s\n", __func__, collector->dso_path);
}

static fcm_report_conf_t * fcm_get_report_config(char *name)
{
    fcm_mgr_t *mgr = NULL;
    fcm_report_conf_t *conf = NULL;
    ds_tree_t *report_tree = NULL;

    mgr = fcm_get_mgr();
    report_tree = &mgr->report_conf_tree;
    conf = ds_tree_find(report_tree, name);
    return conf;
}

static void fcm_set_plugin_params(fcm_collect_plugin_t *plugin,
                                  fcm_collect_conf_t *collect_conf,
                                  fcm_report_conf_t *report_conf)
{
    if (collect_conf->filter_name[0] != '\0')
        plugin->filters.collect = collect_conf->filter_name;
    if (report_conf->hist_filter[0]  != '\0')
        plugin->filters.hist = report_conf->hist_filter;
    if (report_conf->report_filter[0] != '\0')
        plugin->filters.report = report_conf->report_filter;
    plugin->sample_interval = collect_conf->sample_time;
    plugin->report_interval = report_conf->report_time;
    plugin->mqtt_topic = report_conf->mqtt_topic;
    plugin->fmt = report_conf->fmt;
}

static void fcm_clear_plugin_params(fcm_collect_plugin_t *plugin)
{
    plugin->filters.collect = NULL;
    plugin->filters.hist = NULL;
    plugin->report_interval = 0;
    plugin->filters.report = NULL;
    plugin->mqtt_topic = NULL;
    plugin->fmt = FCM_RPT_NO_FMT;
}

static void fcm_clear_report_ticks(fcm_collector_t *collector)
{
    collector->report.ticks = 0;
    collector->report.curr_ticks = 0;
    collector->report.report_time = 0;
}

static void fcm_set_report_params(fcm_collector_t *collector,
                                  fcm_report_conf_t *report_conf)
{
    if ((collector->collect_conf.sample_time == 0) ||
        (report_conf->report_time == 0))
    {
        fcm_clear_report_ticks(collector);
        return;
    }
    // if there is change in report_time update the ticks
    if (collector->report.report_time != report_conf->report_time)
    {
        // collector->report.curr_ticks = 0;
        collector->report.ticks =  report_conf->report_time /
                                   collector->collect_conf.sample_time;
        collector->report.report_time = report_conf->report_time;
    }
}

static void fcm_reset_collect_interval(ev_timer *timer, unsigned int val)
{
    fcm_mgr_t *mgr = NULL;
    mgr = fcm_get_mgr();
    timer->repeat = val;
    ev_timer_again(mgr->loop, timer);
}

static bool fcm_apply_report_config_changes(fcm_collector_t *collector)
{
    fcm_collect_conf_t *collect_conf =  NULL;
    fcm_report_conf_t *report_conf = NULL;
    bool ret = false;

    collect_conf = &collector->collect_conf;
    report_conf =  fcm_get_report_config(collect_conf->report_name);
    if (report_conf)
    {
         fcm_set_plugin_params(&collector->plugin, collect_conf, report_conf);
         // update sample interval & report ticks
         fcm_set_report_params(collector, report_conf);
         ret = true;
    }
    else
    {
        // No report_config found may be deleted. Reset to zero
        fcm_clear_plugin_params(&collector->plugin);
        fcm_clear_report_ticks(collector);
        LOGD("%s: report config not found for collector : %s", \
              __func__, collector->collect_conf.name);
        ret = false;
    }
    return ret;
}

static void fcm_sample_timer_cb(struct ev_loop *loop, ev_timer *w, int revents)
{
    LOGD("%s: ***fcm sample timer expired***", __func__);
    fcm_collector_t *collector = NULL;

    collector = w->data;
    if (!collector) return;
    /*
     * Accept the report_config changes for each sample_timeout
     * to get the latest report_configs
     */
    fcm_apply_report_config_changes(collector);

    if (collector->plugin.collect_periodic)
        collector->plugin.collect_periodic(&collector->plugin);

    if (collector->report.ticks == 0)
    {
       LOGD("%s: No reporting as report time: %d\n",
              __func__, collector->report.ticks);
       collector->report.curr_ticks = 0;
       return;
    }

    collector->report.curr_ticks++;
    // report tick count reached
    if (collector->report.curr_ticks >= collector->report.ticks)
    {
        if (collector->plugin.send_report)
        {
            LOGD("%s: Send mqtt collector: %s report ticks: %d\n",
                 __func__, collector->collect_conf.name,
                 collector->report.curr_ticks);
          collector->plugin.send_report(&collector->plugin);
          collector->report.count++;
        }
        collector->report.curr_ticks = 0;
    }
}

static void collector_evinit(fcm_collector_t *collector,
                             fcm_collect_conf_t *collect_conf)
{
    ev_init(&collector->sample_timer, fcm_sample_timer_cb);
    collector->sample_timer.data = collector;
}

void init_collector_plugin(fcm_collector_t *collector)
{
    void (*plugin_init)(fcm_collect_plugin_t *collector_plugin);
    fcm_collect_conf_t *collect_conf = NULL;

    if (collector->plugin_init == NULL) return;
    *(void **)(&plugin_init) = collector->plugin_init;
    collect_conf = &collector->collect_conf;
    collector->plugin.fcm = collector;
    /* call the init function of plugin */
    plugin_init(&collector->plugin);
    fcm_reset_collect_interval(&collector->sample_timer,
                               collect_conf->sample_time);
    collector->initialized = true;
}

void init_pending_collector_plugin(ds_tree_t *collect_tree)
{
    fcm_collector_t *collector = NULL;

    ds_tree_foreach(collect_tree, collector)
    {
        if (collector->initialized) continue;
        //get the report config for collector
        if (fcm_apply_report_config_changes(collector) == false) continue;
        // report config configured for the collector
        init_collector_plugin(collector);
    }
}
static void init_collect_conf_node(fcm_collect_conf_t *collect_conf,
                              struct schema_FCM_Collector_Config *schema_conf)
{
    ds_tree_t *other_config = NULL;

    if (schema_conf->interval_present)
        collect_conf->sample_time = schema_conf->interval;
    if (schema_conf->filter_name_present)
    {
        strncpy(collect_conf->filter_name, schema_conf->filter_name,
                FCM_FLTR_NAME_LEN - 1);
        collect_conf->filter_name[FCM_FLTR_NAME_LEN - 1] = '\0';
    }
    if (schema_conf->report_name_present)
    {
        strncpy(collect_conf->report_name, schema_conf->report_name,
                FCM_RPT_NAME_LEN - 1);
        collect_conf->report_name[FCM_RPT_NAME_LEN - 1] = '\0';
    }
    if (schema_conf->other_config_present)
    {
        other_config = schema2tree(sizeof(schema_conf->other_config_keys[0]),
                                   sizeof(schema_conf->other_config[0]),
                                   schema_conf->other_config_len,
                                   schema_conf->other_config_keys,
                                   schema_conf->other_config);
        collect_conf->other_config = other_config;
    }
}

static void update_collect_conf_node(fcm_collect_conf_t *collect_conf,
                       struct schema_FCM_Collector_Config *schema_conf)
{
    ds_tree_t *other_config = NULL;

    if (schema_conf->interval_changed)
        collect_conf->sample_time = schema_conf->interval;
    if (schema_conf->filter_name_changed)
    {
        strncpy(collect_conf->filter_name, schema_conf->filter_name,
                FCM_FLTR_NAME_LEN - 1);
        collect_conf->filter_name[FCM_FLTR_NAME_LEN - 1] = '\0';
    }
    if (schema_conf->report_name_changed)
    {
        strncpy(collect_conf->report_name, schema_conf->report_name,
                FCM_RPT_NAME_LEN - 1);
        collect_conf->report_name[FCM_RPT_NAME_LEN - 1] = '\0';
    }
    if (schema_conf->other_config_changed)
    {
        free_str_tree(collect_conf->other_config);
        other_config = schema2tree(sizeof(schema_conf->other_config_keys[0]),
                                   sizeof(schema_conf->other_config[0]),
                                   schema_conf->other_config_len,
                                   schema_conf->other_config_keys,
                                   schema_conf->other_config);
        collect_conf->other_config = other_config;
    }
}

static fcm_collector_t *lookup_collect_config(ds_tree_t *collect_tree,
                                              char *name)
{
    fcm_collector_t *collector = NULL;

    collector = ds_tree_find(collect_tree, name);
    if (collector) return collector;

    collector = calloc(1, sizeof(*collector));
    if (collector == NULL)
    {
        LOGE("Memory allocation failure\n");
        return NULL;
    }
    strcpy(collector->collect_conf.name, name);
    ds_tree_insert(collect_tree, collector, collector->collect_conf.name);
    LOGD("%s: New collector plugin added %s", __func__, collector->collect_conf.name);
    return collector;
}

static fcm_report_conf_t *lookup_report_config(ds_tree_t *conf_tree, char *name)
{
    fcm_report_conf_t *conf = NULL;

    conf = fcm_get_report_config(name);
    if (conf) return conf;

    conf = calloc(1, sizeof(*conf));
    if (conf == NULL)
    {
        LOGE("Memory allocation failure\n");
        return NULL;
    }
    strcpy(conf->name, name);
    ds_tree_insert(conf_tree, conf, conf->name);
    LOGD("%s: New report config added %s", __func__, conf->name);
    return conf;
}

static fcm_rpt_fmt_t fmt_string_to_enum (char *format)
{
    fcm_rpt_fmt_t fmt = FCM_RPT_NO_FMT;
    if (strcasecmp(format, "cumulative") == 0)
        fmt = FCM_RPT_FMT_CUMUL;
    else if (strcasecmp(format, "delta") == 0)
        fmt = FCM_RPT_FMT_DELTA;
    else if (strcasecmp(format, "raw")  == 0)
        fmt = FCM_RPT_FMT_RAW;
    return fmt;
}

static void init_report_conf_node(fcm_report_conf_t *report_conf,
                                  struct schema_FCM_Report_Config *schema_conf)
{
    ds_tree_t *other_config = NULL;

    if (schema_conf->interval_present)
        report_conf->report_time = schema_conf->interval;
    if (schema_conf->format_present)
        report_conf->fmt = fmt_string_to_enum(schema_conf->format);
    if (schema_conf->hist_interval_present)
       report_conf->hist_time = schema_conf->hist_interval;
    if (schema_conf->hist_filter_present)
    {
        strncpy(report_conf->hist_filter, schema_conf->hist_filter,
                FCM_HIST_NAME_LEN - 1);
        report_conf->hist_filter[FCM_HIST_NAME_LEN - 1] = '\0';
    }
    if (schema_conf->report_filter_present)
    {
        strncpy(report_conf->report_filter, schema_conf->report_filter,
                FCM_RPT_NAME_LEN - 1);
        report_conf->report_filter[FCM_RPT_NAME_LEN - 1] = '\0';
    }
    if (schema_conf->mqtt_topic_present)
    {
        strncpy(report_conf->mqtt_topic, schema_conf->mqtt_topic,
                FCM_MQTT_TOPIC_LEN - 1);
        report_conf->mqtt_topic[FCM_MQTT_TOPIC_LEN - 1] = '\0';
    }
    if (schema_conf->other_config_present)
    {
        other_config = schema2tree(sizeof(schema_conf->other_config_keys[0]),
                                   sizeof(schema_conf->other_config[0]),
                                   schema_conf->other_config_len,
                                   schema_conf->other_config_keys,
                                   schema_conf->other_config);
        report_conf->other_config = other_config;
    }
}

static void update_report_conf_node(fcm_report_conf_t *report_conf,
                                    struct schema_FCM_Report_Config *schema_conf)
{
    ds_tree_t *other_config = NULL;

    if (schema_conf->interval_changed)
        report_conf->report_time = schema_conf->interval;
    if (schema_conf->format_changed)
        report_conf->fmt = fmt_string_to_enum(schema_conf->format);
    if (schema_conf->hist_interval_changed)
       report_conf->hist_time = schema_conf->hist_interval;
    if (schema_conf->hist_filter_changed)
    {
        strncpy(report_conf->hist_filter, schema_conf->hist_filter,
                FCM_HIST_NAME_LEN - 1);
        report_conf->hist_filter[FCM_HIST_NAME_LEN - 1] = '\0';
    }
    if (schema_conf->report_filter_changed)
    {
        strncpy(report_conf->report_filter, schema_conf->report_filter,
                FCM_RPT_NAME_LEN - 1);
        report_conf->report_filter[FCM_RPT_NAME_LEN - 1] = '\0';
    }
    if (schema_conf->mqtt_topic_changed)
    {
        strncpy(report_conf->mqtt_topic, schema_conf->mqtt_topic,
                FCM_MQTT_TOPIC_LEN - 1);
        report_conf->mqtt_topic[FCM_MQTT_TOPIC_LEN - 1] = '\0';
    }
    if (schema_conf->other_config_changed)
    {
        free_str_tree(report_conf->other_config);
        other_config = schema2tree(sizeof(schema_conf->other_config_keys[0]),
                                   sizeof(schema_conf->other_config[0]),
                                   schema_conf->other_config_len,
                                   schema_conf->other_config_keys,
                                   schema_conf->other_config);
        report_conf->other_config = other_config;
    }
}

void init_report_config(struct schema_FCM_Report_Config *conf)
{
    fcm_mgr_t *mgr = NULL;
    ds_tree_t *report_conf_tree = NULL;
    fcm_report_conf_t *report_conf_node = NULL;

    mgr = fcm_get_mgr();
    report_conf_tree = &mgr->report_conf_tree;
    report_conf_node = lookup_report_config(report_conf_tree, conf->name);
    if (report_conf_node == NULL) return;
    init_report_conf_node(report_conf_node, conf);
    // call any pending collector_plugin init waiting for report_config
    init_pending_collector_plugin(&mgr->collect_tree);
}

void update_report_config(struct schema_FCM_Report_Config *conf)
{
    fcm_mgr_t *mgr = NULL;
    ds_tree_t *report_conf_tree = NULL;
    fcm_report_conf_t *report_conf_node = NULL;

    mgr = fcm_get_mgr();
    report_conf_tree = &mgr->report_conf_tree;
    report_conf_node = lookup_report_config(report_conf_tree, conf->name);

    if (report_conf_node == NULL) return;

    update_report_conf_node(report_conf_node, conf);
}

void delete_report_config(struct schema_FCM_Report_Config *conf)
{
    fcm_mgr_t *mgr = NULL;
    ds_tree_t *report_conf_tree = NULL;
    fcm_report_conf_t *report_conf_node = NULL;

    mgr = fcm_get_mgr();
    report_conf_tree = &mgr->report_conf_tree;
    report_conf_node = ds_tree_find(report_conf_tree, conf->name);
    if (report_conf_node == NULL) return;
    ds_tree_remove(report_conf_tree, report_conf_node);
    free(report_conf_node);
}

bool init_collect_config(struct schema_FCM_Collector_Config *conf)
{
    void (*plugin_init)(fcm_collect_plugin_t *collector_plugin);
    char *error = NULL;
    fcm_mgr_t *mgr = NULL;
    fcm_collector_t *collector = NULL;
    fcm_collect_conf_t *collect_conf = NULL;
    ds_tree_t *collect_tree = NULL;

    mgr = fcm_get_mgr();
    collect_tree = &mgr->collect_tree;
    collector = lookup_collect_config(collect_tree, conf->name);
    if (collector == NULL) return false;

    collect_conf = &collector->collect_conf;
    init_collect_conf_node(collect_conf, conf);
    fcm_get_plugin_configs(collector, conf);
    collector_evinit(collector, collect_conf);

    dlerror();
    collector->handle = dlopen(collector->dso_path, RTLD_NOW);
    if (collector->handle == NULL)
    {
        LOGE("%s: dlopen %s failed: %s", __func__,
              collector->dso_path, dlerror());
        return false;
    }
    dlerror();
    *(void **)(&plugin_init) = dlsym(collector->handle, collector->dso_init);
    error = dlerror();
    if (error != NULL)
    {
        LOGE("%s: could not get init symbol %s: %s",
             __func__, collector->dso_init, error);
        dlclose(collector->handle);
        return false;
    }
    collector->plugin_init = plugin_init;
    if (fcm_apply_report_config_changes(collector))
        init_collector_plugin(collector);
    else
        LOGD("%s: Report config not available at plugin_init time: %s",
              __func__, collector->collect_conf.name);
    return true;
}

void update_collect_config(struct schema_FCM_Collector_Config *conf)
{
    fcm_mgr_t *mgr = NULL;
    fcm_collector_t *collector = NULL;
    fcm_collect_conf_t *collect_conf = NULL;
    ds_tree_t *collect_tree = NULL;

    mgr = fcm_get_mgr();
    collect_tree = &mgr->collect_tree;
    collector = lookup_collect_config(collect_tree, conf->name);
    if (collector == NULL) return;

    collect_conf = &collector->collect_conf;
    update_collect_conf_node(collect_conf, conf);
    // <TBD>: For config specific changes
    fcm_apply_report_config_changes(collector);
    fcm_reset_collect_interval(&collector->sample_timer,
                               collect_conf->sample_time);
}

void delete_collect_config(struct schema_FCM_Collector_Config *conf)
{
    fcm_mgr_t *mgr = NULL;
    fcm_collector_t *collector = NULL;
    ds_tree_t *collect_tree = NULL;

    mgr = fcm_get_mgr();
    collect_tree = &mgr->collect_tree;
    collector = lookup_collect_config(collect_tree, conf->name);
    if (collector == NULL) return;

    // stop the sample timer
    fcm_reset_collect_interval(&collector->sample_timer, 0);
    ev_timer_stop(mgr->loop, &collector->sample_timer);
    if (collector->plugin.close_plugin)
    {
        collector->plugin.close_plugin(&collector->plugin);
        LOGD("%s: Plugin %s is closed\n", __func__, conf->name);
    }
    dlclose(collector->handle);
    ds_tree_remove(collect_tree, collector);
    free(collector);
}

bool fcm_init_mgr(struct ev_loop *loop)
{
    fcm_mgr.loop = loop;
    ds_tree_init(&fcm_mgr.collect_tree, fcm_tree_node_cmp,
                 fcm_collector_t, node);
    ds_tree_init(&fcm_mgr.report_conf_tree, fcm_tree_node_cmp,
                 fcm_report_conf_t, node);
    LOGD("FCM Manager Initialized\n");
    return true;
}

fcm_mgr_t * fcm_get_mgr(void)
{
    return &fcm_mgr;
}


char * fcm_get_mqtt_hdr_node_id(void)
{
    fcm_mgr_t *mgr = NULL;

    mgr = fcm_get_mgr();
    return mgr->mqtt_headers[FCM_HEADER_NODE_ID];
}


char * fcm_get_mqtt_hdr_loc_id(void)
{
    fcm_mgr_t *mgr = NULL;

    mgr = fcm_get_mgr();
    return mgr->mqtt_headers[FCM_HEADER_LOCATION_ID];
}


char * fcm_plugin_get_other_config(fcm_collect_plugin_t *plugin, char *key)
{
    fcm_collector_t *collector = NULL;
    char *ret = NULL;

    collector = plugin->fcm;
    ret = fcm_get_other_config_val(collector->collect_conf.other_config, key);
    LOGD("%s: key : %s val : %s\n", __func__, key, ret);
    return ret;
}
