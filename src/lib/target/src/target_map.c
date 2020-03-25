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
#include <getopt.h>
#include <string.h>
#include <unistd.h>
#include <ev.h>

#include "os.h"
#include "log.h"
#include "ds.h"
#include "ds_tree.h"
#include "util.h"

#define MODULE_ID LOG_MODULE_ID_TARGET

#define MAP_IFNAME_LEN      32

typedef struct
{
    char            if_name[MAP_IFNAME_LEN];
    char            map_name[MAP_IFNAME_LEN];
    ds_tree_node_t  node_if;
    ds_tree_node_t  node_map;
} target_map_t;

typedef struct
{
    bool init;
    ds_tree_t root_if;
    ds_tree_t root_map;
} target_map_entry_t;

static target_map_entry_t g_target_map_table;

/*
 *  PUBLIC definitions
 */
bool target_map_init(void)
{
    if (g_target_map_table.init) return true;

    ds_tree_init(&g_target_map_table.root_if, (ds_key_cmp_t*)strcmp, target_map_t, node_if);
    ds_tree_init(&g_target_map_table.root_map, (ds_key_cmp_t*)strcmp, target_map_t, node_map);

    g_target_map_table.init = true;

    return true;
}

bool target_map_close(void)
{
    target_map_t *map;
    ds_tree_iter_t iter;

    if (!g_target_map_table.init)
        return true;

    // Same map is allocated for both (root_if and root_map)
    for (   map = ds_tree_ifirst(&iter, &g_target_map_table.root_if);
            map != NULL;
            map = ds_tree_inext(&iter)) {
        ds_tree_iremove(&iter);
        free(map);
        map = NULL;
    }

    g_target_map_table.init = false;

   return true;
}

bool target_map_insert(char *if_name, char *map_name)
{
    if (!g_target_map_table.init) return false;
    if (!if_name || !map_name) return false;

    target_map_t *map;

    if (!(map = calloc(1, sizeof(*map)))) {
        return false;
    }

    STRSCPY(map->if_name, if_name);
    STRSCPY(map->map_name, map_name);

    ds_tree_insert(&g_target_map_table.root_if, map, map->if_name);
    ds_tree_insert(&g_target_map_table.root_map, map, map->map_name);

    LOGD("Mapping %s -> %s", map->if_name, map->map_name);

    return true;
}

char* target_map_ifname(char *if_name)
{
    if (if_name && g_target_map_table.init) {
        target_map_t* map = ds_tree_find(&g_target_map_table.root_if, if_name);
        if (map) {
            return map->map_name;
        }
    }

    return if_name;
}

char* target_unmap_ifname(char *map_name)
{
    if (map_name && g_target_map_table.init) {
        target_map_t* map = ds_tree_find(&g_target_map_table.root_map, map_name);
        if (map) {
            return map->if_name;
        }
    }

    return map_name;
}

bool target_map_ifname_exists(char *if_name)
{
    if (if_name && g_target_map_table.init) {
        target_map_t* map = ds_tree_find(&g_target_map_table.root_if, if_name);
        if (map) {
            return true;
        }
    }

    return false;
}

bool target_unmap_ifname_exists(char *map_name)
{
    if (map_name && g_target_map_table.init) {
        target_map_t* map = ds_tree_find(&g_target_map_table.root_map, map_name);
        if (map) {
            return true;
        }
    }

    return false;
}
