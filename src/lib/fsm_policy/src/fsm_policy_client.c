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

#include "ds_tree.h"
#include "fsm_policy.h"
#include "log.h"

/**
 * @brief compare sessions
 *
 * @param a session pointer
 * @param b session pointer
 * @return 0 if sessions matches
 */
static int client_cmp(void *a, void *b)
{
    uintptr_t p_a = (uintptr_t)a;
    uintptr_t p_b = (uintptr_t)b;

    if (p_a ==  p_b) return 0;
    if (p_a < p_b) return -1;
    return 1;
}

void fsm_policy_client_init(void)
{
    struct fsm_policy_session *mgr;
    ds_tree_t *tree;

    mgr = get_mgr();
    tree = &mgr->clients;

    ds_tree_init(tree, client_cmp, struct fsm_policy_client, client_node);
}

void fsm_policy_register_client(struct fsm_policy_client *client)
{
    struct fsm_policy_session *mgr;
    struct fsm_policy_client *p_client;
    struct policy_table *table;
    ds_tree_t *tree;
    char *default_name = "default";
    char *name;

    if (client == NULL) return;
    if (client->session == NULL) return;

    mgr = get_mgr();
    tree = &mgr->clients;
    p_client = ds_tree_find(tree, client->session);
    if (p_client != NULL)
    {
        /* Update the external client's table */
        client->table = p_client->table;

        /* Update the internal client */
        p_client->update_client = client->update_client;

        LOGD("%s: updating client %s", __func__, p_client->name);

        return;
    }

    /* Allocate a client, add it to the tree */
    p_client = calloc(1, sizeof(*p_client));
    if (p_client == NULL) return;

    name = (client->name == NULL ? default_name : client->name);
    p_client->name = strdup(name);
    if (p_client->name == NULL) goto err_free_client;
    p_client->session = client->session;
    p_client->update_client = client->update_client;
    table = ds_tree_find(&mgr->policy_tables, name);
    p_client->table = table;
    client->table = table;
    ds_tree_insert(tree, p_client, p_client->session);

    LOGD("%s: registered client %s", __func__, p_client->name);

    return;

err_free_client:
    free(p_client);
}


void fsm_policy_deregister_client(struct fsm_policy_client *client)
{
    struct fsm_policy_session *mgr;
    struct fsm_policy_client *p_client;

    if (client == NULL) return;
    if (client->session == NULL) return;

    mgr = get_mgr();
    p_client = ds_tree_find(&mgr->clients, client->session);
    if (p_client == NULL) return;

    ds_tree_remove(&mgr->clients, p_client);
    free(p_client->name);
    free(p_client);

    client->table = NULL;
}

void fsm_policy_update_clients(struct policy_table *table)
{
    struct fsm_policy_session *mgr;
    struct fsm_policy_client *client;
    char *default_name = "default";
    ds_tree_t *tree;

    mgr = get_mgr();
    tree = &mgr->clients;
    client = ds_tree_head(tree);

    while (client != NULL)
    {
        char *name;
        bool update;
        int cmp;

        name = (client->name == NULL ? default_name : client->name);
        cmp = strcmp(name, table->name);
        update = ((cmp == 0) && (client->update_client != NULL));
        if (update) client->update_client(client->session, table);

        client = ds_tree_next(tree, client);
    }
}
