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

/**
 * This part of Hello World manager shows a simple example of a workflow
 * using OVSDB.
 *
 * Normal demo flow:
 *
 * 1) Table Node_Config is updated and the change is detected using OVSDB APIs.
 * 2) Check if this change is meant for Hello World manager by comparing
 *    the Node_Config.module field.
 * 3) If the change is meant for Hello World, we need to update our key-value
 *    demo storage using Node_Config.key and Node_Config.value.
 * 4) When demo storage is updated, we detect the change using libev, and
 *    update the Node_State accordingly.
 *
 * Special demo cases for FUT testing:
 *
 * 1) Failure to update Node_State after applying config
 *    (See: hello_world_demo_fail_to_update())
 * 2) Failure to write to system with false Node_State update
 *    (See: hello_world_demo_fail_to_write())
 */
#include <ev.h>
#include <stdbool.h>
#include <stdio.h>
#include <unistd.h>

#include "log.h"

#include "hello_world.h"

#define DEMO_STORAGE        "/tmp/hello-world-demo"
#define DEMO_MODULE         "hello-world"
#define DEMO_KEY            "demo"
#define DEMO_VALUE_FAIL_W   "fail-to-write"
#define DEMO_VALUE_FAIL_U   "fail-to-update"


static ev_stat demo_storage_watcher;


/**
 * This is a special case #1 that prevents updating the Node_State table,
 * which will cause a FUT level-1 failure (config and state table mismatch).
 *
 *  module : hello-world
 *  key    : demo
 *  value  : fail-to-update
 *
 */
bool hello_world_demo_fail_to_update(const char *key, const char *value)
{
    if (   !strcmp(DEMO_KEY, key)
        && !strcmp(DEMO_VALUE_FAIL_U, value))
    {
        return true;
    }

    return false;
}

/**
 * This is a special case #2 that prevents updating the demo storage, but
 * forcefully updates the Node_State table, which will cause a FUT level-2
 * failure (config and system state mismatch).
 *
 *  module : hello-world
 *  key    : demo
 *  value  : fail-to-write
 *
 */
bool hello_world_demo_fail_to_write(const char *key, const char *value)
{
    if (   !strcmp(DEMO_KEY, key)
        && !strcmp(DEMO_VALUE_FAIL_W, value))
    {
        // We will prevent writing to demo storage and forcefully update
        // the Node_State just for the sake of FUT tests.
        // Note that in real cases this is the wrong way of doing things, since
        // the Node_State must be updated according to the state on the system
        // (demo storage in our case).
        hello_world_ovsdb_state(false, DEMO_MODULE, key, value);
        return true;
    }

    return false;
}

/**
 * Normal operation: once the config is received, we need to write it to demo
 * storage to simulate system configuration.
 *
 * Note that we are only allowed to do that for configs with:
 *
 *  module : hello-world
 *  key    : *
 *  value  : *
 *
 */
void hello_world_demo_config(
        bool remove,
        const char *module,
        const char *key,
        const char *value)
{
    FILE *fp;

    // We only react to configs with "hello-world" module
    if (strcmp(DEMO_MODULE, module) != 0)
    {
        return;
    }

    // 'Remove entry'
    if (remove)
    {
        unlink(DEMO_STORAGE);
        return;
    }

    // Special case for FUT using "demo=fail-to-write"
    if (hello_world_demo_fail_to_write(key, value))
    {
        return;
    }

    // Write/update key=value into demo storage
    if ((fp = fopen(DEMO_STORAGE, "w")))
    {
        fprintf(fp, "%s=%s\n", key, value);
        fclose(fp);
    }
}

/**
 * Syncs changes of demo storage into Node_State table
 */
void demo_storage_onchage(struct ev_loop *loop, ev_stat *w, int revent)
{
    FILE *fp;
    char buf[256];
    char *key;
    char *value;

    // Storage was removed -- clear Node_State table entry
    if (!w->attr.st_nlink)
    {
        hello_world_ovsdb_state(true, DEMO_MODULE, NULL, NULL);
        return;
    }

    // Storage was updated -- update Node_State table entry
    if ((fp = fopen(DEMO_STORAGE, "r")))
    {
        if (fread(buf, 1, sizeof(buf), fp)
            && strstr(buf, "=")
            && strstr(buf, "\n"))
        {
            key = strtok(buf, "=");
            value = strtok(NULL, "\n");

            if (hello_world_demo_fail_to_update(key, value) == false)
            {
                hello_world_ovsdb_state(false, DEMO_MODULE, key, value);
            }
        }
        fclose(fp);
    }
}

/**
 * Initializes demo part of Hello World manager
 */
bool hello_world_demo_init(struct ev_loop *loop)
{
    // Register demo storage watcher using libev for updating Node_State table
    ev_stat_init(
            &demo_storage_watcher,
            demo_storage_onchage,
            DEMO_STORAGE,
            0.0);

    ev_stat_start(loop, &demo_storage_watcher);

    return true;
}
