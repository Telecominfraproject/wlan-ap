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

/* std libc */
#include <assert.h>
#include <ev.h>

/* internal */
#include <log.h>
#include <ovsdb.h>
#include <target.h>

/* unit */
#include <wm2.h>

static void
wm2_tests_clients_logger_cb(logger_t *self,
                            logger_msg_t *msg)
{
    static const char *expected[] = {
        "wm2_tests_clients: simple case",
        "Client '11:22:33:44:55:66' connected on 'sanity1' with key ''",
        "Client '11:22:33:44:55:66' disconnected from 'sanity1' with key ''",
        "wm2_tests_clients: roaming case",
        "Client '11:22:33:44:55:66' connected on 'sanity1' with key ''",
        "Client '11:22:33:44:55:66' roamed to 'sanity2' with key ''",
        "Client '11:22:33:44:55:66' removed from 'sanity1' with key ''",
        "Client '11:22:33:44:55:66' disconnected from 'sanity2' with key ''",
        "wm2_tests_clients: same-key re-connection case",
        "Client '11:22:33:44:55:66' connected on 'sanity1' with key ''",
        "Client '11:22:33:44:55:66' disconnected from 'sanity1' with key ''",
        "wm2_tests_clients: different-key re-connection case",
        "Client '11:22:33:44:55:66' connected on 'sanity1' with key ''",
        "Client '11:22:33:44:55:66' re-connected on 'sanity1' with key 'key'",
        "Client '11:22:33:44:55:66' disconnected from 'sanity1' with key 'key'",
    };
    static size_t i = 0;

    if (msg->lm_severity != LOG_SEVERITY_NOTICE)
        return;

    assert(i < ARRAY_SIZE(expected));
    if (!strstr(msg->lm_text, expected[i])) {
        printf("expected \"%s\" (%zu) but logged \"%s\"\n",
               expected[i], i, msg->lm_text);
        assert(0);
    }
    i++;
}

static void
wm2_tests_clients(void)
{
    struct schema_Wifi_Associated_Clients client1;
    struct schema_Wifi_Associated_Clients client2;
    struct schema_Wifi_VIF_Config vconf1;
    struct schema_Wifi_VIF_State vstate1;
    struct schema_Wifi_VIF_State vstate2;
    struct schema_Wifi_VIF_State vstate3;
    struct schema_Openflow_Tag tag1;
    struct schema_Openflow_Tag tag2;
    struct schema_Openflow_Tag tag;
    static const char *oftag1 = "gutentag";
    static const char *oftag2 = "canttouchthis";
    static const char *key = "12345678";
    logger_t logger;

    memset(&vconf1, 0, sizeof(vconf1));
    memset(&vstate1, 0, sizeof(vstate1));
    memset(&vstate2, 0, sizeof(vstate2));
    memset(&vstate3, 0, sizeof(vstate3));
    memset(&client1, 0, sizeof(client1));
    memset(&client2, 0, sizeof(client2));
    memset(&logger, 0, sizeof(logger));
    memset(&tag1, 0, sizeof(tag1));
    memset(&tag2, 0, sizeof(tag2));
    memset(&tag, 0, sizeof(tag));

    logger.logger_fn = wm2_tests_clients_logger_cb,
    log_register_logger(&logger);

    SCHEMA_SET_STR(vstate1.if_name, "sanity1");
    SCHEMA_SET_STR(vstate2.if_name, "sanity2");
    SCHEMA_SET_STR(vconf1.if_name, vstate1.if_name);
    SCHEMA_KEY_VAL_APPEND(vconf1.security, "key", key);
    SCHEMA_KEY_VAL_APPEND(vconf1.security, "oftag", oftag1);
    SCHEMA_SET_STR(client1.mac, "11:22:33:44:55:66");
    SCHEMA_SET_STR(client1.state, "active");
    SCHEMA_SET_STR(tag1.name, oftag1);
    SCHEMA_SET_STR(tag2.name, oftag2);
    SCHEMA_VAL_APPEND(tag2.device_value, "11:22:33:44:55:66");

    ovsdb_table_delete_simple(&table_Wifi_Associated_Clients,
                              SCHEMA_COLUMN(Wifi_Associated_Clients, mac),
                              client1.mac);
    assert(1 == ovsdb_table_upsert(&table_Wifi_VIF_State, &vstate1, true));
    assert(1 == ovsdb_table_upsert(&table_Wifi_VIF_State, &vstate2, true));
    assert(1 == ovsdb_table_upsert(&table_Wifi_VIF_Config, &vconf1, true));
    assert(1 == ovsdb_table_upsert(&table_Openflow_Tag, &tag1, true));
    assert(1 == ovsdb_table_upsert(&table_Openflow_Tag, &tag2, true));

    LOGN("%s: simple case", __func__);
    assert(true == wm2_clients_update(&client1, vstate1.if_name, true));
    assert(true == ovsdb_table_select_one(&table_Wifi_Associated_Clients,
                                          SCHEMA_COLUMN(Wifi_Associated_Clients, mac),
                                          client1.mac,
                                          &client2));
    assert(client2.key_id_exists == true);
    assert(client2.oftag_exists == false);
    assert(client2.state_exists == true);
    assert(client2.uapsd_exists == client1.uapsd_exists);
    assert(client2.state_exists == client1.state_exists);
    assert(client2.kick_len == client1.kick_len);
    assert(!strcmp(client1.key_id, client2.key_id));
    assert(!strcmp(client1.state, client2.state));
    assert(true == ovsdb_table_select_one_where(&table_Wifi_VIF_State,
                                                ovsdb_tran_cond(OCLM_UUID,
                                                                "associated_clients",
                                                                OFUNC_INC,
                                                                client2._uuid.uuid),
                                                &vstate3));
    assert(true == wm2_clients_update(&client1, vstate1.if_name, false));
    assert(false == ovsdb_table_select_one_where(&table_Wifi_VIF_State,
                                                 ovsdb_tran_cond(OCLM_UUID,
                                                                 "associated_clients",
                                                                 OFUNC_INC,
                                                                 client2._uuid.uuid),
                                                 &vstate3));
    assert(false == ovsdb_table_select_one(&table_Wifi_Associated_Clients,
                                           SCHEMA_COLUMN(Wifi_Associated_Clients, mac),
                                           client1.mac,
                                           &client2));

    LOGN("%s: roaming case", __func__);
    assert(true == wm2_clients_update(&client1, vstate1.if_name, true));
    assert(true == ovsdb_table_select_one(&table_Wifi_Associated_Clients,
                                          SCHEMA_COLUMN(Wifi_Associated_Clients, mac),
                                          client1.mac,
                                          &client2));
    assert(true == wm2_clients_update(&client1, vstate2.if_name, true));
    assert(true == ovsdb_table_select_one(&table_Wifi_Associated_Clients,
                SCHEMA_COLUMN(Wifi_Associated_Clients, mac),
                client1.mac,
                &client2));
    assert(true == wm2_clients_update(&client1, vstate1.if_name, false));
    assert(true == ovsdb_table_select_one(&table_Wifi_Associated_Clients,
                SCHEMA_COLUMN(Wifi_Associated_Clients, mac),
                client1.mac,
                &client2));
    assert(true == wm2_clients_update(&client1, vstate2.if_name, false));
    assert(false == ovsdb_table_select_one(&table_Wifi_Associated_Clients,
                SCHEMA_COLUMN(Wifi_Associated_Clients, mac),
                client1.mac,
                &client2));

    LOGN("%s: same-key re-connection case", __func__);
    assert(true == wm2_clients_update(&client1, vstate1.if_name, true));
    assert(true == wm2_clients_update(&client1, vstate1.if_name, true));
    assert(true == wm2_clients_update(&client1, vstate1.if_name, true));
    assert(true == ovsdb_table_select_one(&table_Wifi_Associated_Clients,
                                          SCHEMA_COLUMN(Wifi_Associated_Clients, mac),
                                          client1.mac,
                                          &client2));
    assert(true == wm2_clients_update(&client1, vstate1.if_name, false));
    assert(false == ovsdb_table_select_one(&table_Wifi_Associated_Clients,
                                           SCHEMA_COLUMN(Wifi_Associated_Clients, mac),
                                           client1.mac,
                                           &client2));
    assert(true == wm2_clients_update(&client1, vstate1.if_name, false));
    assert(true == wm2_clients_update(&client1, vstate1.if_name, false));

    LOGN("%s: different-key re-connection case", __func__);
    assert(true == wm2_clients_update(&client1, vstate1.if_name, true));
    assert(true == ovsdb_table_select_one(&table_Wifi_Associated_Clients,
                                          SCHEMA_COLUMN(Wifi_Associated_Clients, mac),
                                          client1.mac,
                                          &client2));
    SCHEMA_SET_STR(client1.key_id, "key");
    assert(true == ovsdb_table_select_one(&table_Openflow_Tag,
                                          SCHEMA_COLUMN(Openflow_Tag, name),
                                          oftag1,
                                          &tag));
    assert(tag.device_value_len == 0);
    assert(true == ovsdb_table_select_one(&table_Openflow_Tag,
                                          SCHEMA_COLUMN(Openflow_Tag, name),
                                          oftag2,
                                          &tag));
    assert(tag.device_value_len == 1);
    assert(true == wm2_clients_update(&client1, vstate1.if_name, true));
    assert(true == ovsdb_table_select_one(&table_Openflow_Tag,
                                          SCHEMA_COLUMN(Openflow_Tag, name),
                                          oftag1,
                                          &tag));
    assert(tag.device_value_len == 1);
    assert(true == ovsdb_table_select_one(&table_Openflow_Tag,
                                          SCHEMA_COLUMN(Openflow_Tag, name),
                                          oftag2,
                                          &tag));
    assert(tag.device_value_len == 1);
    assert(true == ovsdb_table_select_one(&table_Wifi_Associated_Clients,
                                          SCHEMA_COLUMN(Wifi_Associated_Clients, mac),
                                          client1.mac,
                                          &client2));
    assert(true == wm2_clients_update(&client1, vstate1.if_name, false));
    assert(false == ovsdb_table_select_one(&table_Wifi_Associated_Clients,
                                           SCHEMA_COLUMN(Wifi_Associated_Clients, mac),
                                           client1.mac,
                                           &client2));
    assert(true == ovsdb_table_select_one(&table_Openflow_Tag,
                                          SCHEMA_COLUMN(Openflow_Tag, name),
                                          oftag1,
                                          &tag));
    assert(tag.device_value_len == 0);
    assert(true == ovsdb_table_select_one(&table_Openflow_Tag,
                                          SCHEMA_COLUMN(Openflow_Tag, name),
                                          oftag2,
                                          &tag));
    assert(tag.device_value_len == 1);

    assert(1 == ovsdb_table_delete(&table_Wifi_VIF_State, &vstate1));
    assert(1 == ovsdb_table_delete(&table_Wifi_VIF_State, &vstate2));
    assert(1 == ovsdb_table_delete(&table_Wifi_VIF_Config, &vconf1));
    assert(1 == ovsdb_table_delete(&table_Openflow_Tag, &tag1));
    assert(1 == ovsdb_table_delete(&table_Openflow_Tag, &tag2));

    log_unregister_logger(&logger);
}

int
main(int argc, const char **argv)
{
    target_log_open("WM", 0);
    backtrace_init();
    json_memdbg_init(EV_DEFAULT);
    log_severity_set(LOG_SEVERITY_NOTICE);
    assert(ovsdb_init_loop(EV_DEFAULT_ "WM"));

    OVSDB_TABLE_INIT(Wifi_Radio_Config, if_name);
    OVSDB_TABLE_INIT(Wifi_Radio_State, if_name);
    OVSDB_TABLE_INIT(Wifi_VIF_Config, if_name);
    OVSDB_TABLE_INIT(Wifi_VIF_State, if_name);
    OVSDB_TABLE_INIT(Wifi_Credential_Config, _uuid);
    OVSDB_TABLE_INIT(Wifi_Associated_Clients, _uuid);
    OVSDB_TABLE_INIT(Openflow_Tag, name);

    wm2_tests_clients();

    printf("TEST OK\n");
    return 0;
}
