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

#include "fsm.h"
#include "log.h"
static const struct fsm_type types_map[] =
{
    {
        .ovsdb_type = "parser",
        .fsm_type = FSM_PARSER,
    },
    {
        .ovsdb_type = "web_cat_provider",
        .fsm_type = FSM_WEB_CAT,
    },
    {
        .ovsdb_type = "dpi",
        .fsm_type = FSM_DPI,
    },
};

/**
 * @brief sets the session type based on the ovsdb values
 *
 * @param the ovsdb configuration
 * @return an integer representing the type of service
 */
int
fsm_service_type(struct schema_Flow_Service_Manager_Config *conf)
{
    const struct fsm_type *map;
    size_t nelems;
    size_t len;
    size_t i;
    int cmp;

    len = strlen(conf->type);

    /* Ensure backward compatibility if no type is provided */
    if (len == 0) return FSM_PARSER;

    /* Walk the known types */
    nelems = (sizeof(types_map) / sizeof(types_map[0]));
    map = types_map;
    for (i = 0; i < nelems; i++)
    {
        cmp = strcmp(conf->type, map->ovsdb_type);
        if (cmp == 0) return map->fsm_type;

        map++;
    }

    /* No known type recognized */
    return FSM_UNKNOWN_SERVICE;
}


/**
 * @brief duplicate session configuration
 *
 * @param from session holding the original conf
 * @praram to session targeted for duplication
 * @return true if the duplication succeeded, false otherwise
 */
bool
fsm_dup_conf(struct fsm_session *from, struct fsm_session *to)
{
    struct fsm_session_conf *fconf;
    struct fsm_session_conf *tconf;
    ds_tree_t *from_other_config;
    ds_tree_t *to_other_config;
    struct str_pair *from_pair;
    struct str_pair *to_pair;

    fconf = from->conf;
    tconf = calloc(1, sizeof(*tconf));
    if (tconf == NULL) return false;
    to->conf = tconf;

    tconf->handler = strdup(to->name);
    if (tconf->handler == NULL) goto err_free_tconf;

    /* Duplicate other_config */
    from_other_config = fconf->other_config;
    if (from_other_config == NULL) return true;

    to_other_config = calloc(1, sizeof(*to_other_config));
    if (to_other_config == NULL) goto err_free_handler;

    ds_tree_init(to_other_config, str_tree_cmp, struct str_pair, pair_node);
    from_pair = ds_tree_head(from_other_config);
    while (from_pair != NULL)
    {
        to_pair = get_pair(from_pair->key, from_pair->value);
        if (to_pair == NULL) goto err_free_other_config;

        ds_tree_insert(to_other_config, to_pair, to_pair->key);
        from_pair = ds_tree_next(from_other_config, from_pair);
    }

    /* remove dso_init entry */
    to_pair = ds_tree_find(to_other_config, "dso_init");
    if (to_pair != NULL)
    {
        ds_tree_remove(to_other_config, to_pair);
        free_str_pair(to_pair);
    }
    tconf->other_config = to_other_config;

    return true;

err_free_other_config:
    free_str_tree(to_other_config);

err_free_handler:
    free(tconf->handler);

err_free_tconf:
    free(tconf);

    return false;
}

/**
 * @brief create a web_cat session based on a service plugin
 *
 * Backward compatibility function: a parser plugin may hold the settings
 * for the web categorization service. Create a web categorization session
 * based on these settings.
 * @param session the parser session
 * @return true if the duplication succeeded, false otherwise
 */
bool
fsm_dup_web_cat_session(struct fsm_session *session)
{
    union fsm_plugin_ops *plugin_ops;
    struct fsm_session *service;
    ds_tree_t *sessions;
    struct fsm_mgr *mgr;
    char *service_name;
    bool ret;

    mgr = fsm_get_mgr();

    /* Get the service name */
    service_name = session->ops.get_config(session, "provider");
    if (service_name == NULL) return false;

    /* Look up the service */
    sessions = fsm_get_sessions();
    service = ds_tree_find(sessions, service_name);
    if (service != NULL)
    {
        session->service = service;
        return true;
    }

    service = calloc(1, sizeof(struct fsm_session));
    if (service == NULL) return NULL;

    service->name = strdup(service_name);
    if (service->name == NULL) goto err_free_session;

    service->type = FSM_WEB_CAT;

    service->ops.send_report = fsm_send_report;
    service->ops.get_config = fsm_get_other_config_val;

    plugin_ops = calloc(1, sizeof(*plugin_ops));
    if (plugin_ops == NULL) goto err_free_name;

    ret = fsm_dup_conf(session, service);
    if (!ret) goto err_free_ops;

    service->p_ops = plugin_ops;
    service->mqtt_headers = mgr->mqtt_headers;
    service->location_id = mgr->location_id;
    service->node_id = mgr->node_id;
    service->loop = mgr->loop;

    ret = fsm_parse_dso(service);
    if (!ret) goto err_free_conf;

    ret = mgr->init_plugin(service);
    if (!ret)
    {
        LOGE("%s: plugin handler %s initialization failed",
             __func__, session->name);
        goto err_free_dso_path;

    }
    session->service = service;
    ds_tree_insert(sessions, service, service->name);

    return true;

err_free_dso_path:
    free(service->dso);

err_free_conf:
    fsm_free_session_conf(service->conf);

err_free_ops:
    free(plugin_ops);

err_free_plugin_ops:
    free(plugin_ops);

err_free_name:
    free(service->name);

err_free_session:
    free(service);

    return false;
}


/**
 * @brief associate a parser session to a web cat session
 *
 * @param session the parser session requesting a service
 * @return true if the service was not needed or was created, false otherwise
 */
bool
fsm_get_web_cat_service(struct fsm_session *session)
{
    struct fsm_session *service;
    char *provider_plugin;
    ds_tree_t *sessions;
    char *service_name;

    if (session->type == FSM_WEB_CAT) return true;

    sessions = fsm_get_sessions();

    /*
     * If the parser plugin advertizes a provider_plugin, assume an ovsdb entry
     * dedicated to this provider. It might not be there yet. In this case, the
     * parser seession will be updated later on.
     */
    provider_plugin = session->ops.get_config(session, "provider_plugin");
    if (provider_plugin != NULL)
    {
        service = ds_tree_find(sessions, provider_plugin);
        session->service = service;
        return true;
    }

    service_name = session->ops.get_config(session, "provider");
    /* If he parser plugin does not advertize a requested provider, bail */
    if (service_name == NULL) return true;

    return fsm_dup_web_cat_session(session);
}

/**
 * @brief associate a web cat session to parser sessions
 *
 * @param session the web cat session
 * @param op add (1) delete (0)
 */
void
fsm_web_cat_service_update(struct fsm_session *session, int op)
{
    struct fsm_session *parser;
    char *provider_plugin;
    ds_tree_t *sessions;
    int cmp;

    /* Validate the input session */
    if (session->type != FSM_WEB_CAT) return;

    sessions = fsm_get_sessions();
    parser = ds_tree_head(sessions);
    while (parser != NULL)
    {
        provider_plugin = parser->ops.get_config(parser, "provider_plugin");
        if (provider_plugin != NULL)
        {
            bool update;

            /* Check if the parser binds to the web cat provider */
            cmp = strcmp(session->name, provider_plugin);
            update = (cmp == 0);

            /* Check if the parser is up to date */
            if (op == 0) update &= (parser->service == session);
            if (op == 1) update &= (parser->service != session);
            if (update)
            {
                /* Set the service, call the parser update */
                if (op == FSM_SERVICE_ADD) parser->service = session;
                if (op == FSM_SERVICE_DELETE) parser->service = NULL;
                if (parser->ops.update != NULL) parser->ops.update(parser);
            }
        }
        parser = ds_tree_next(sessions, parser);
    }
}
