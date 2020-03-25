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

#define _GNU_SOURCE
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
#include "wm2.h"

#include "target.h"

#define MODULE_ID LOG_MODULE_ID_MAIN

#define WM2_RECALC_DELAY_SECONDS            30
#define WM2_DFS_FALLBACK_GRACE_PERIOD_SECONDS 10
#define REQUIRE(ctx, cond) if (!(cond)) { LOGW("%s: %s: failed check: %s", ctx, __func__, #cond); return; }
#define OVERRIDE(ctx, lv, rv) if (lv != rv) { lv = rv; LOGW("%s: overriding '%s' - this is target impl bug", ctx, #lv); }

struct wm2_delayed {
    ev_timer timer;
    struct ds_dlist_node list;
    char ifname[32];
    void (*fn)(const char *ifname, bool force);
    char workname[256];
};

ovsdb_table_t table_Wifi_Radio_Config;
ovsdb_table_t table_Wifi_Radio_State;
ovsdb_table_t table_Wifi_VIF_Config;
ovsdb_table_t table_Wifi_VIF_State;
ovsdb_table_t table_Wifi_Credential_Config;
ovsdb_table_t table_Wifi_Associated_Clients;
ovsdb_table_t table_Wifi_Master_State;
ovsdb_table_t table_Openflow_Tag;

static ds_dlist_t delayed_list = DS_DLIST_INIT(struct wm2_delayed, list);
static bool wm2_api2;

static bool
wm2_lookup_rstate_by_vstate(struct schema_Wifi_Radio_State *rstate,
                            const struct schema_Wifi_VIF_State *vstate);

static bool
wm2_lookup_rstate_by_freq_band(struct schema_Wifi_Radio_State *rstate,
                               const char *freqband);
/******************************************************************************
 *  PROTECTED definitions
 *****************************************************************************/
static struct wm2_delayed *
wm2_delayed_lookup(void (*fn)(const char *ifname, bool force),
                   const char *ifname)
{
    struct wm2_delayed *i;
    ds_dlist_foreach(&delayed_list, i)
        if (i->fn == fn && !strcmp(i->ifname, ifname))
            return i;
    return NULL;
}

static void
wm2_delayed_cancel(void (*fn)(const char *ifname, bool force),
                   const char *ifname)
{
    struct wm2_delayed *i;
    if (!(i = wm2_delayed_lookup(fn, ifname)))
        return;
    LOGD("%s: cancelling scheduled work %s", ifname, i->workname);
    ds_dlist_remove(&delayed_list, i);
    ev_timer_stop(EV_DEFAULT, &i->timer);
    free(i);
}

static void
wm2_delayed_cb(struct ev_loop *loop, ev_timer *timer, int revents)
{
    struct wm2_delayed *i;
    struct wm2_delayed j;
    i = (void *)timer;
    j = *i;
    LOGD("%s: running scheduled work %s", i->ifname, i->workname);
    ds_dlist_remove(&delayed_list, i);
    ev_timer_stop(EV_DEFAULT, &i->timer);
    free(i);
    j.fn(j.ifname, false);
}

static void
wm2_delayed(void (*fn)(const char *ifname, bool force),
            const char *ifname, unsigned int seconds,
            const char *workname)
{
    struct wm2_delayed *i;
    if ((i = wm2_delayed_lookup(fn, ifname)))
        return;
    if (!(i = malloc(sizeof(*i))))
        return;
    STRSCPY(i->ifname, ifname);
    STRSCPY(i->workname, workname);
    i->fn = fn;
    ev_timer_init(&i->timer, wm2_delayed_cb, seconds, 0);
    ev_timer_start(EV_DEFAULT, &i->timer);
    ds_dlist_insert_tail(&delayed_list, i);
    LOGI("%s: scheduling delayed (%us) work %s", ifname, seconds, workname);
}

#define wm2_delayed_recalc(fn, ifname) (wm2_delayed((fn), (ifname), WM2_RECALC_DELAY_SECONDS, #fn))
#define wm2_delayed_recalc_cancel wm2_delayed_cancel

#define wm2_timer(fn, ifname, timeout) (wm2_delayed((fn), (ifname), (timeout), #fn))
#define wm2_timer_cancel wm2_delayed_cancel

static bool
wm2_sta_has_connected(const struct schema_Wifi_VIF_State *oldstate,
                      const struct schema_Wifi_VIF_State *newstate)
{
    return !strcmp(oldstate->mode, "sta") &&
           strcmp(newstate->parent, oldstate->parent) &&
           !strlen(oldstate->parent) &&
           strlen(newstate->parent);
}

static bool
wm2_sta_has_reconnected(const struct schema_Wifi_VIF_State *oldstate,
                        const struct schema_Wifi_VIF_State *newstate)
{
    return !strcmp(oldstate->mode, "sta") &&
           strcmp(newstate->parent, oldstate->parent) &&
           strlen(oldstate->parent) &&
           strlen(newstate->parent);
}

static bool
wm2_sta_has_disconnected(const struct schema_Wifi_VIF_State *oldstate,
                         const struct schema_Wifi_VIF_State *newstate)
{
    return !strcmp(oldstate->mode, "sta") &&
           strcmp(newstate->parent, oldstate->parent) &&
           strlen(oldstate->parent) &&
           !strlen(newstate->parent);
}

struct wm2_fallback_parent {
    int channel;
    char bssid[64];
    char radio_name[128];
    char freq_band[128];
};

static unsigned int
wm2_get_fallback_parents(struct wm2_fallback_parent *parents,
                         unsigned int parent_max)
{
    const struct schema_Wifi_Radio_State *rs;
    struct wm2_fallback_parent *parent;
    unsigned int parent_num;
    void *buf;
    int n;
    int i;

    parent_num = 0;

    buf = ovsdb_table_select_where(&table_Wifi_Radio_State, NULL, &n);
    if (!buf)
        return parent_num;

    for (n--; n >= 0; n--) {
        rs = buf + (n * table_Wifi_Radio_State.schema_size);

        for (i = 0; i < rs->fallback_parents_len; i++) {
            if (parent_num >= parent_max) {
                LOGW("%s: %s we exceed parent max", rs->if_name, __func__);
                break;
            }

            parent = &parents[parent_num];
            parent->channel = rs->fallback_parents[i];
            STRSCPY_WARN(parent->bssid, rs->fallback_parents_keys[i]);
            STRSCPY_WARN(parent->radio_name, rs->if_name);
            STRSCPY_WARN(parent->freq_band, rs->freq_band);
            parent_num++;

            LOGD("%s: %s add fallback parent %s %d", parent->freq_band, parent->radio_name,
                 parent->bssid, parent->channel);
        }
    }

    free(buf);

    return parent_num;
}

static void
wm2_parent_change(void)
{
    const char *parentchange = strfmta("%s/parentchange.sh", target_bin_dir());
    struct schema_Wifi_Radio_State rstate;
    struct wm2_fallback_parent parents[8];
    const struct wm2_fallback_parent *parent;
    unsigned int parents_num;
    unsigned int i;

    parents_num = wm2_get_fallback_parents(parents, ARRAY_SIZE(parents));
    if (!parents_num) {
        LOGI("%s seems no fallback parents found, restart managers", __func__);
        target_device_restart_managers();
        return;
    }

    parent = &parents[0];

    /* Simply choose 2.4 parent with our channel if possible */
    if (wm2_lookup_rstate_by_freq_band(&rstate, "2.4G")) {
        for (i = 0; i < parents_num; i++ ) {
            if (rstate.channel == parents[i].channel) {
                parent = &parents[i];
                break;
            }
        }
    }

    LOGN("%s run parentchange.sh %s %s %d", parent->freq_band, parent->radio_name,
         parent->bssid, parent->channel);
    WARN_ON(!strexa(parentchange, parent->radio_name, parents->bssid, strfmta("%d", parents->channel)));
    return;
}

static void
wm2_dfs_disconnect_check(const char *ifname, bool force)
{
    LOGN("%s %s called", ifname, __func__);
    wm2_parent_change();
}

static bool
wm2_sta_has_dfs_channel(const struct schema_Wifi_VIF_State *vstate)
{
    struct schema_Wifi_Radio_State rstate;
    bool status = false;
    int i;

    if (WARN_ON(!wm2_lookup_rstate_by_vstate(&rstate, vstate)))
        return status;

    if (WARN_ON(!rstate.channel_exists))
         return status;

    /* TODO check all channels base on number/width */
    for (i = 0; i < rstate.channels_len; i++) {
        if (rstate.channel != atoi(rstate.channels_keys[i]))
            continue;

        LOGI("%s: check channel %d (%d) %s", vstate->if_name, rstate.channel, vstate->channel, rstate.channels[i]);
        if (!strstr(rstate.channels[i], "allowed"))
            status = true;

        break;
    }

    return status;
}

static void
wm2_sta_handle_dfs_link_change(const struct schema_Wifi_VIF_State *oldstate,
                               const struct schema_Wifi_VIF_State *newstate)
{
    static const struct schema_Wifi_VIF_State empty;

    if (!newstate)
        newstate = &empty;

    if (wm2_sta_has_connected(oldstate, newstate) ||
        wm2_sta_has_reconnected(oldstate, newstate))
        wm2_timer_cancel(wm2_dfs_disconnect_check, oldstate->if_name);


    if (wm2_sta_has_disconnected(oldstate, newstate) &&
        wm2_sta_has_dfs_channel(oldstate)) {
            LOGN("%s: sta: dfs: disconnected from %s - arm fallback parents timer", oldstate->if_name, oldstate->parent);
            wm2_timer(wm2_dfs_disconnect_check, oldstate->if_name, WM2_DFS_FALLBACK_GRACE_PERIOD_SECONDS);
    }
}

static void
wm2_sta_print_status(const struct schema_Wifi_VIF_State *oldstate,
                     const struct schema_Wifi_VIF_State *newstate)
{
    static const struct schema_Wifi_VIF_State empty;

    if (!newstate)
        newstate = &empty;

    if (wm2_sta_has_connected(oldstate, newstate))
        LOGN("%s: sta: connected to %s on channel %d",
             newstate->if_name, newstate->parent, newstate->channel);

    if (wm2_sta_has_reconnected(oldstate, newstate))
        LOGN("%s: sta: re-connected from %s to %s on channel %d",
             oldstate->if_name, oldstate->parent, newstate->parent, newstate->channel);

    if (wm2_sta_has_disconnected(oldstate, newstate))
        LOGN("%s: sta: disconnected from %s", oldstate->if_name, oldstate->parent);
}

void wm2_radio_update_port_state(const char *cloud_vif_ifname)
{
    struct schema_Wifi_Master_State mstate;
    struct schema_Wifi_VIF_State vstate;
    struct schema_Wifi_VIF_Config vconf;
    char *filter[] = { "+",
                       SCHEMA_COLUMN(Wifi_Master_State,
                                     port_state),
                       NULL };
    bool active;
    int num;

    memset(&mstate, 0, sizeof(mstate));
    memset(&vstate, 0, sizeof(vstate));
    memset(&vconf, 0, sizeof(vconf));

    /* Note: I'm not comfortable relying on a provided
     *       vstate from caller because it may be from a
     *       partial update with filters and may not include
     *       all the necessary bits.
     *
     *       Don't care if this fails. If it does
     *       it means interface went away and is
     *       no longer active.
     */
    ovsdb_table_select_one_where(&table_Wifi_VIF_State,
                                 ovsdb_where_simple("if_name",
                                                    cloud_vif_ifname),
                                 &vstate);
    ovsdb_table_select_one_where(&table_Wifi_VIF_Config,
                                 ovsdb_where_simple("if_name",
                                                    cloud_vif_ifname),
                                 &vconf);

    /* FIXME: This is a band-aid for the time being before
     *        CM2 and Wifi_Master_State logic is reworked to
     *        be more flexible and possibly cloud-controlled
     *        to allow overlapping parent changing,
     *        fallbacks, priorities, etc.
     *
     *        This just tries to piggyback on the original
     *        design of Wifi_Master_State where CM1 (and CM2
     *        will too, for now) relies on port_state to
     *        decide whether a link is usable for
     *        onboarding.
     */

    /* The idea is for libtarget to update "parent" column
     * only when it is connected to parent AP in sta mode
     * and data-traffic ready (i.e. authorized, after
     * EAPOL exchanges).
     */
    active = false;

    if (vstate.parent_exists && vconf.parent_exists &&
        strlen(vstate.parent) && strlen(vconf.parent) &&
        !strcmp(vstate.parent, vconf.parent))
        active = true;

    if (vstate.parent_exists && !vconf.parent_exists && strlen(vstate.parent))
        active = true;

    if (vstate.mode_exists && !strcmp(vstate.mode, "ap"))
        active = true;

    if (!vstate.enabled_exists)
        active = false;

    if (!vstate.enabled)
        active = false;

    mstate.port_state_exists = true;
    snprintf(mstate.port_state,
             sizeof(mstate.port_state),
             active ? "active" : "inactive");

    num = ovsdb_table_update_where_f(&table_Wifi_Master_State,
                                     ovsdb_where_simple("if_name",
                                                        cloud_vif_ifname),
                                     &mstate,
                                     filter);

    LOGD("%s: updated (%d) master state to '%s'",
         cloud_vif_ifname, num, mstate.port_state);
}

static void
wm2_radio_update_port_state_set_inactive(const char *ifname)
{
    struct schema_Wifi_Master_State mstate;
    json_t *w;
    int n;

    memset(&mstate, 0, sizeof(mstate));
    mstate._partial_update = true;
    SCHEMA_SET_STR(mstate.port_state, "inactive");
    if (WARN_ON(!(w = ovsdb_where_simple(SCHEMA_COLUMN(Wifi_Master_State, if_name), ifname))))
        return;

    n = ovsdb_table_update_where(&table_Wifi_Master_State, w, &mstate);
    LOGD("%s: port state set to inactive: n=%d", ifname, n);
}

#define CHANGED_MAP_STRSTR(conf, state, name, force) \
    (force || schema_changed_map(conf->name##_keys, \
                                 conf->name, \
                                 state->name##_keys, \
                                 state->name, \
                                 conf->name##_len, \
                                 state->name##_len, \
                                 sizeof(*conf->name##_keys), \
                                 sizeof(*conf->name), \
                                 (smap_cmp_fn_t)strncmp, \
                                 (smap_cmp_fn_t)strncmp))

#define CHANGED_MAP_INTSTR(conf, state, name, force) \
    (force || schema_changed_map(conf->name##_keys, \
                                 conf->name, \
                                 state->name##_keys, \
                                 state->name, \
                                 conf->name##_len, \
                                 state->name##_len, \
                                 sizeof(*conf->name##_keys), \
                                 sizeof(*conf->name),\
                                 (smap_cmp_fn_t)memcmp, \
                                 (smap_cmp_fn_t)strncmp ))

#define CHANGED_MAP_STRINT(conf, state, name, force) \
    (force || schema_changed_map(conf->name##_keys, \
                                 conf->name, \
                                 state->name##_keys, \
                                 state->name, \
                                 conf->name##_len, \
                                 state->name##_len, \
                                 sizeof(*conf->name##_keys), \
                                 sizeof(*conf->name),\
                                 (smap_cmp_fn_t)strncmp, \
                                 (smap_cmp_fn_t)memcmp ))

#define CHANGED_SET(conf, state, name, force) \
    (force || schema_changed_set(conf->name, \
                                 state->name, \
                                 conf->name##_len, \
                                 state->name##_len, \
                                 sizeof(*conf->name), \
                                 strncmp))

#define CHANGED_SET_CASE(conf, state, name, force) \
    (force || schema_changed_set(conf->name, \
                                 state->name, \
                                 conf->name##_len, \
                                 state->name##_len, \
                                 sizeof(*conf->name), \
                                 strncasecmp))

#define CHANGED_INT(conf, state, name, force) \
    (conf->name##_exists && ((state->name##_exists && (conf->name != state->name)) || \
                             !state->name##_exists || \
                             force))

#define CHANGED_STR(conf, state, name, force) \
    (conf->name##_exists && ((state->name##_exists && strcmp(conf->name, state->name)) || \
                             !state->name##_exists || \
                             force))

#define CHANGED_STR_CASE(conf, state, name, force) \
    (conf->name##_exists && ((state->name##_exists && strcasecmp(conf->name, state->name)) || \
                             !state->name##_exists || \
                             force))

#define CMP(cmp, name) \
    (changed |= (changedf->name = ((cmp(conf, state, name, changedf->_uuid)) && \
                                   (LOGD("%s: '%s' changed", conf->if_name, #name), 1))))

static bool
wm2_vconf_changed(const struct schema_Wifi_VIF_Config *conf,
                  const struct schema_Wifi_VIF_State *state,
                  struct schema_Wifi_VIF_Config_flags *changedf)
{
    int changed = 0;

    memset(changedf, 0, sizeof(*changedf));
    changed |= (changedf->_uuid = strcmp(conf->_uuid.uuid, state->vif_config.uuid));
    CMP(CHANGED_INT, enabled);
    CMP(CHANGED_INT, ap_bridge);
    CMP(CHANGED_INT, vif_radio_idx);
    CMP(CHANGED_INT, uapsd_enable);
    CMP(CHANGED_INT, group_rekey);
    CMP(CHANGED_INT, ft_psk);
    CMP(CHANGED_INT, ft_mobility_domain);
    CMP(CHANGED_INT, vlan_id);
    CMP(CHANGED_INT, wds);
    CMP(CHANGED_INT, rrm);
    CMP(CHANGED_INT, btm);
    CMP(CHANGED_INT, dynamic_beacon);
    CMP(CHANGED_STR, bridge);
    CMP(CHANGED_STR, mac_list_type);
    CMP(CHANGED_STR, mode);
    CMP(CHANGED_STR_CASE, parent);
    CMP(CHANGED_STR, ssid);
    CMP(CHANGED_STR, ssid_broadcast);
    CMP(CHANGED_STR, min_hw_mode);
    CMP(CHANGED_SET_CASE, mac_list);
    CMP(CHANGED_MAP_STRSTR, security);

    if (changed)
        LOGD("%s: changed (forced=%d)", conf->if_name, changedf->_uuid);

    return changed;
}

static bool
wm2_rconf_changed(const struct schema_Wifi_Radio_Config *conf,
                  const struct schema_Wifi_Radio_State *state,
                  struct schema_Wifi_Radio_Config_flags *changedf)
{
    int changed = 0;

    memset(changedf, 0, sizeof(*changedf));
    changed |= (changedf->_uuid = strcmp(conf->_uuid.uuid, state->radio_config.uuid));
    CMP(CHANGED_INT, channel);
    CMP(CHANGED_INT, channel_sync);
    CMP(CHANGED_INT, enabled);
    CMP(CHANGED_INT, thermal_shutdown);
    CMP(CHANGED_INT, thermal_integration);
    CMP(CHANGED_INT, thermal_downgrade_temp);
    CMP(CHANGED_INT, thermal_upgrade_temp);
    CMP(CHANGED_INT, tx_chainmask);
    CMP(CHANGED_INT, tx_power);
    CMP(CHANGED_INT, bcn_int);
    CMP(CHANGED_INT, dfs_demo);
    CMP(CHANGED_STR, channel_mode);
    CMP(CHANGED_STR, country);
    CMP(CHANGED_STR, freq_band);
    CMP(CHANGED_STR, ht_mode);
    CMP(CHANGED_STR, hw_mode);
    CMP(CHANGED_MAP_STRSTR, hw_config);
    CMP(CHANGED_MAP_INTSTR, temperature_control);
    CMP(CHANGED_MAP_STRINT, fallback_parents);

    if (changed)
        LOGD("%s: changed (forced=%d)", conf->if_name, changedf->_uuid);

    return changed;
}

#undef CMP
#define CMP(cmp, name) \
    (changed |= (changedf->name = ((cmp(conf, state, name, changedf->_uuid)) && \
                                   (LOGD("%s: '%s' changed", conf->mac, #name), 1))))

static bool
wm2_client_changed(const struct schema_Wifi_Associated_Clients *conf,
                   const struct schema_Wifi_Associated_Clients *state,
                   struct schema_Wifi_Associated_Clients_flags *changedf)
{
    int changed = 0;

    memset(changedf, 0, sizeof(*changedf));
    CMP(CHANGED_INT, uapsd);
    CMP(CHANGED_STR, key_id);
    CMP(CHANGED_STR, state);
    CMP(CHANGED_SET, capabilities);
    CMP(CHANGED_MAP_STRSTR, kick);

    if (changed)
        LOGD("%s: changed", conf->mac);

    return changed;
}

#undef CHANGED_STR
#undef CHANGED_INT
#undef CHANGED_SET
#undef CHANGED_MAP_STRSTR
#undef CHANGED_MAP_STRINT
#undef CHANGED_MAP_INTSTR
#undef CMP

static void
wm2_vconf_init_del(struct schema_Wifi_VIF_Config *vconf, const char *ifname)
{
    memset(vconf, 0, sizeof(*vconf));
    STRSCPY(vconf->if_name, ifname);
    vconf->if_name_present = true;
    vconf->if_name_exists = true;
    vconf->enabled_present = true;
    vconf->enabled_changed = true;
    vconf->enabled_exists = true;
}

static void
wm2_vstate_init(struct schema_Wifi_VIF_State *vstate, const char *ifname)
{
    memset(vstate, 0, sizeof(*vstate));
    STRSCPY(vstate->if_name, ifname);
}

static bool
wm2_lookup_rconf_by_vif_ifname(struct schema_Wifi_Radio_Config *rconf,
                               const char *ifname)
{
    const struct schema_Wifi_Radio_Config *rc;
    const struct schema_Wifi_Radio_State *rs;
    struct schema_Wifi_VIF_Config vconf;
    struct schema_Wifi_VIF_State vstate;
    json_t *where;
    void *buf;
    int n;
    int i;

    memset(rconf, 0, sizeof(*rconf));

    if (!(where = ovsdb_where_simple(SCHEMA_COLUMN(Wifi_VIF_Config, if_name), ifname)))
        return false;
    if (ovsdb_table_select_one_where(&table_Wifi_VIF_Config, where, &vconf)) {
        if ((buf = ovsdb_table_select_where(&table_Wifi_Radio_Config, NULL, &n))) {
            for (n--; n >= 0; n--) {
                rc = buf + (n * table_Wifi_Radio_Config.schema_size);
                for (i = 0; i < rc->vif_configs_len; i++) {
                    if (!strcmp(rc->vif_configs[i].uuid, vconf._uuid.uuid)) {
                        memcpy(rconf, rc, sizeof(*rc));
                        free(buf);
                        LOGD("%s: found radio %s via vif config", ifname, rconf->if_name);
                        return true;
                    }
                }
            }
            free(buf);
        }
    }

    if (!(where = ovsdb_where_simple(SCHEMA_COLUMN(Wifi_VIF_State, if_name), ifname)))
        return false;
    if (ovsdb_table_select_one_where(&table_Wifi_VIF_State, where, &vstate)) {
        if ((buf = ovsdb_table_select_where(&table_Wifi_Radio_State, NULL, &n))) {
            for (n--; n >= 0; n--) {
                rs = buf + (n * table_Wifi_Radio_State.schema_size);
                for (i = 0; i < rs->vif_states_len; i++) {
                    if (!strcmp(rs->vif_states[i].uuid, vstate._uuid.uuid)) {
                        if (!(where = ovsdb_where_simple(SCHEMA_COLUMN(Wifi_Radio_Config, if_name), rs->if_name)))
                            continue;
                        if (ovsdb_table_select_one_where(&table_Wifi_Radio_Config, where, rconf)) {
                            free(buf);
                            LOGD("%s: found radio %s via vif state", ifname, rconf->if_name);
                            return true;
                        }
                    }
                }
            }
            free(buf);
        }
    }

    return false;
}

static bool
wm2_lookup_rconf_by_ifname(struct schema_Wifi_Radio_Config *rconf,
                           const char *ifname)
{
    json_t *where = ovsdb_where_simple(SCHEMA_COLUMN(Wifi_Radio_Config, if_name), ifname);
    if (!where)
        return false;
    return ovsdb_table_select_one_where(&table_Wifi_Radio_Config, where, rconf);
}

static bool
wm2_lookup_vconf_by_ifname(struct schema_Wifi_VIF_Config *vconf,
                           const char *ifname)
{
    json_t *where = ovsdb_where_simple(SCHEMA_COLUMN(Wifi_VIF_Config, if_name), ifname);
    if (!where)
        return false;
    return ovsdb_table_select_one_where(&table_Wifi_VIF_Config, where, vconf);
}

static bool
wm2_lookup_rstate_by_freq_band(struct schema_Wifi_Radio_State *rstate,
                               const char *freqband)
{
    json_t *where = ovsdb_where_simple(SCHEMA_COLUMN(Wifi_Radio_State, freq_band), freqband);
    if (!where)
        return false;
    return ovsdb_table_select_one_where(&table_Wifi_Radio_State, where, rstate);
}

static bool
wm2_lookup_rstate_by_vstate(struct schema_Wifi_Radio_State *rstate,
                            const struct schema_Wifi_VIF_State *vstate)
{
    const struct schema_Wifi_Radio_State *rs;
    void *buf;
    int n;
    int i;

    memset(rstate, 0, sizeof(*rstate));

    buf = ovsdb_table_select_where(&table_Wifi_Radio_State, NULL, &n);
    if (!buf)
        return false;

    for (n--; n >= 0; n--) {
        rs = buf + (n * table_Wifi_Radio_State.schema_size);
        for (i = 0; i < rs->vif_states_len; i++) {
            if (!strcmp(rs->vif_states[i].uuid, vstate->_uuid.uuid)) {
                memcpy(rstate, rs, sizeof(*rs));
                free(buf);
                LOGD("%s: found radio state %s via vif state", vstate->if_name, rs->if_name);
                return true;
            }
        }
    }

    free(buf);
    return false;
}

static int
wm2_cconf_get(const struct schema_Wifi_VIF_Config *vconf,
              struct schema_Wifi_Credential_Config *cconfs,
              int size)
{
    json_t *where;
    int n;

    memset(cconfs, 0, sizeof(*cconfs) * size);

    for (n = 0; n < vconf->credential_configs_len && size > 0; n++) {
        if (!(where = ovsdb_where_uuid("_uuid", vconf->credential_configs[n].uuid)))
            continue;
        if (!ovsdb_table_select_one_where(&table_Wifi_Credential_Config, where, cconfs)) {
            LOGW("%s: failed to retrieve credential config %s",
                 vconf->if_name, vconf->credential_configs[n].uuid);
            continue;
        }
        cconfs++, size--;
    }

    if (size == 0 && n < vconf->credential_configs_len) {
        LOGW("%s: credential config truncated to %d entries, "
             "please adjust the size at compile time!",
             vconf->if_name, n);
    }

    return n;
}

static void
wm2_vconf_recalc(const char *ifname, bool force)
{
    struct schema_Wifi_Radio_Config rconf;
    struct schema_Wifi_Radio_State rstate;
    struct schema_Wifi_VIF_Config vconf;
    struct schema_Wifi_VIF_State vstate;
    struct schema_Wifi_Credential_Config cconfs[8];
    struct schema_Wifi_VIF_Config_flags vchanged;
    json_t *where;
    int num_cconfs;
    bool want;
    bool has;

    LOGD("%s: recalculating", ifname);

    memset(&rconf, 0, sizeof(rconf));

    if (!(want = ovsdb_table_select_one(&table_Wifi_VIF_Config,
                                        SCHEMA_COLUMN(Wifi_VIF_Config, if_name),
                                        ifname,
                                        &vconf)))
        wm2_vconf_init_del(&vconf, ifname);

    if (!(has = ovsdb_table_select_one(&table_Wifi_VIF_State,
                                       SCHEMA_COLUMN(Wifi_VIF_State, if_name),
                                       ifname,
                                       &vstate)))
        wm2_vstate_init(&vstate, ifname);

    if (want && vconf.mode_exists && !strcmp(vconf.mode, "sta")) {
        if (!vconf.enabled_exists || !vconf.enabled) {
            LOGW("%s: overriding 'enabled'; conf.db.bck may need fixing, or it's cloud bug PIR-11055", ifname);
            vconf.enabled_exists = true;
            vconf.enabled = true;
        }
    }

    if (!want && !has)
        return;

    if (!wm2_lookup_rconf_by_vif_ifname(&rconf, vconf.if_name)) {
        LOGI("%s: no radio config found, will retry later", vconf.if_name);
        return;
    }

    if (!ovsdb_table_select_one(&table_Wifi_Radio_State,
                                SCHEMA_COLUMN(Wifi_Radio_State, if_name),
                                rconf.if_name,
                                &rstate)) {
        /* This essentialy handles the initial config setup
         * with 3rd party middleware.
         */
        LOGI("%s: no radio state found, will retry later", vconf.if_name);
        return;
    }

#if 0
    /* I originally intended to defer vconf configuration
     * until after radio is fully configred too. However
     * this presents a chicken-egg problem where target
     * implementation may not be able to set up certain
     * rconf knobs, e.g. channel, until there's at least 1
     * vif created. But if rconf/rstate don't match how
     * would first vif be ever created then?
     *
     * Forcing target implementation to fake rstate to match
     * rconf is asking for trouble and breaks the very idea
     * of desired config and system state.
     *
     * I'm leaving this in case someone ever thinks of
     * comparing rconf and rstate.
     */
    if (wm2_rconf_changed(&rconf, &rstate, &rchanged)) {
        LOGI("%s: radio config doesn't match radio state, will retry later", vconf.if_name);
        return;
    }
#endif

    num_cconfs = wm2_cconf_get(&vconf, cconfs, sizeof(cconfs)/sizeof(cconfs[0]));

    if (has && strlen(SCHEMA_KEY_VAL(vconf.security, "key")) < 8) {
        LOGD("%s: overriding 'ssid' and 'security' for onboarding", ifname);
        vconf.ssid_exists = vstate.ssid_exists;
        STRSCPY(vconf.ssid, vstate.ssid);
        memcpy(vconf.security, vstate.security, sizeof(vconf.security));
    }

    if (!wm2_vconf_changed(&vconf, &vstate, &vchanged) && !force)
        return;

    if (vconf.dynamic_beacon_exists && vconf.dynamic_beacon &&
        vconf.ssid_broadcast_exists &&
        !strcmp("enabled", vconf.ssid_broadcast)) {
            LOGW("%s: failed to configure, dynamic beacon can be set only for hidden networks",
                 vconf.if_name);
    }

    wm2_radio_update_port_state(vconf.if_name);

    LOGI("%s@%s: changed, configuring", ifname, rconf.if_name);

    if (vchanged.parent)
        LOGN("%s: topology change: parent '%s' -> '%s'", ifname, vstate.parent, vconf.parent);

    if (want && !has) {
        if (WARN_ON(!(where = ovsdb_where_uuid(SCHEMA_COLUMN(Wifi_Radio_State, radio_config),
                                               rconf._uuid.uuid))))
            return;
        if (WARN_ON(ovsdb_table_upsert_with_parent(&table_Wifi_VIF_State,
                                                   &vstate,
                                                   false, // uuid not needed
                                                   NULL,
                                                   // parent:
                                                   SCHEMA_TABLE(Wifi_Radio_State),
                                                   where,
                                                   SCHEMA_COLUMN(Wifi_Radio_State, vif_states)) != 1))
            return;
    }

    if (!target_vif_config_set2(&vconf, &rconf, cconfs, &vchanged, num_cconfs)) {
        LOGW("%s: failed to configure, will retry later", ifname);
        wm2_delayed_recalc(wm2_vconf_recalc, ifname);
        return;
    }

    if (has && !want) {
        ovsdb_table_delete_simple(&table_Wifi_VIF_State,
                                  SCHEMA_COLUMN(Wifi_VIF_State, if_name),
                                  ifname);
        wm2_sta_print_status(&vstate, NULL);
        wm2_radio_update_port_state(vconf.if_name);
    }

    wm2_delayed_recalc_cancel(wm2_vconf_recalc, ifname);
}

static void
wm2_rconf_recalc_vifs(const struct schema_Wifi_Radio_Config *rconf)
{
    struct schema_Wifi_VIF_Config vconf;
    struct schema_Wifi_VIF_State *vstate;
    json_t *where;
    void *buf;
    int i;
    int n;

    /* For VIF_Config to be properly configured (created)
     * the associated parent Radio_Config must be known.
     * However OVSDB events can come in reverse order such
     * as vif_configs don't contain VIF_Config uuid at a
     * given time.
     *
     * This makes sure all vifs are in-sync VIF_Config vs
     * VIF_State and by proxy handle the described corner
     * case by hooking up to Radio_Config recalculations
     * which will eventually contain updated vif_configs.
     */
    for (i = 0; i < rconf->vif_configs_len; i++) {
        if (!(where = ovsdb_where_uuid("_uuid", rconf->vif_configs[i].uuid)))
            continue;
        if (ovsdb_table_select_one_where(&table_Wifi_VIF_Config, where, &vconf))
            wm2_vconf_recalc(vconf.if_name, false);
    }

    /* If WM2 misses (due to a bug, crash, etc) a VIF_Config
     * row removal then it is a good idea to deconfigure
     * (and remove) orphaned VIF_State rows
     */
    if ((buf = ovsdb_table_select_where(&table_Wifi_VIF_State, NULL, &n))) {
        for (n--; n >= 0; n--) {
            vstate = buf + (n * sizeof(*vstate));
            if (!(where = ovsdb_where_simple(SCHEMA_COLUMN(Wifi_VIF_Config, if_name), vstate->if_name)))
                continue;
            if (!ovsdb_table_select_one_where(&table_Wifi_VIF_Config, where, &vconf)) {
                LOGI("%s: removing stale config", vstate->if_name);
                wm2_vconf_recalc(vstate->if_name, false);
            }
        }
        free(buf);
    }
}

static void
wm2_rconf_init_del(struct schema_Wifi_Radio_Config *rconf, const char *ifname)
{
    memset(rconf, 0, sizeof(*rconf));
    STRSCPY(rconf->if_name, ifname);
    rconf->if_name_present = true;
    rconf->if_name_exists = true;
    rconf->enabled_present = true;
    rconf->enabled_changed = true;
    rconf->enabled_exists = true;
}

static bool
wm2_rconf_lookup_sta(struct schema_Wifi_VIF_State *vstate,
                     const struct schema_Wifi_Radio_State *rstate)
{
    struct schema_Wifi_VIF_State *vstates;
    int i;
    int n;

    memset(vstate, 0, sizeof(*vstate));
    if (!(vstates = ovsdb_table_select_where(&table_Wifi_VIF_State, json_array(), &n)))
        return false;
    while (n--)
        if (!strcmp(vstates[n].mode, "sta"))
            for (i = 0; i < rstate->vif_states_len; i++)
                if (!strcmp(rstate->vif_states[i].uuid, vstates[n]._uuid.uuid))
                    memcpy(vstate, &vstates[n], sizeof(*vstate));
    free(vstates);
    return strlen(vstate->if_name) > 0;
}

static bool
wm2_rconf_recalc_fixup_channel(struct schema_Wifi_Radio_Config *rconf,
                               const struct schema_Wifi_Radio_State *rstate)
{
    struct schema_Wifi_Radio_Config_flags rchanged;
    struct schema_Wifi_VIF_Config_flags vchanged;
    struct schema_Wifi_VIF_Config vconf;
    struct schema_Wifi_VIF_State vstate;

    if (!wm2_rconf_changed(rconf, rstate, &rchanged))
        return false;
    if (!rchanged.channel)
        return false;
    if (!wm2_rconf_lookup_sta(&vstate, rstate))
        return false;
    if (!ovsdb_table_select_one(&table_Wifi_VIF_Config,
                                SCHEMA_COLUMN(Wifi_VIF_Config, if_name),
                                vstate.if_name,
                                &vconf))
        return false;
    if (!vstate.channel_exists)
        return false;
    if (wm2_vconf_changed(&vconf, &vstate, &vchanged) && vchanged.parent)
        return false;

    /* FIXME: This will not work with multiple sta vaps for
     * fallbacks. This needs to be solved with a new
     * Radio_Config column expressing sta/ap channel policy,
     * i.e. when radio_config channel is more important than
     * sta vif channel in cases of sta csa, sta (re)assoc.
     */
    LOGW("%s: ignoring channel change %d -> %d because sta vif %s is connected on %d, see CAES-600",
            rconf->if_name, rstate->channel, rconf->channel,
            vstate.if_name, vstate.channel);
    rconf->channel = vstate.channel;
    return true;
}

static void
wm2_rconf_recalc_fixup_nop_channel(struct schema_Wifi_Radio_Config *rconf,
                                   const struct schema_Wifi_Radio_State *rstate)
{
    struct schema_Wifi_Radio_Config_flags rchanged;
    int i;

    /* After RADAR hit channel is not available because of NOP */
    if (!rconf->channel_exists)
        return;
    if (!rstate->channel_exists)
        return;
    if (!wm2_rconf_changed(rconf, rstate, &rchanged))
        return;
    if (!rchanged.channel)
        return;

    for (i = 0; i < rstate->channels_len; i++) {
        if (atoi(rstate->channels_keys[i]) != rconf->channel)
            continue;

        if (strstr(rstate->channels[i], "nop_started")) {
            LOGW("%s: ignoring channel change %d -> %d because of NOP active on %d",
                 rconf->if_name, rstate->channel, rconf->channel, rconf->channel);
            rconf->channel = rstate->channel;
            break;
        }
    }
}

static void
wm2_rconf_recalc_fixup_tx_chainmask(struct schema_Wifi_Radio_Config *rconf)
{
    if (!rconf->tx_chainmask_exists && !rconf->thermal_tx_chainmask_exists)
        return;
    if (!rconf->thermal_tx_chainmask_exists)
        return;

    rconf->tx_chainmask = rconf->thermal_tx_chainmask;
    rconf->tx_chainmask_exists = true;
}

static void
wm2_rconf_recalc(const char *ifname, bool force)
{
    struct schema_Wifi_Radio_Config rconf;
    struct schema_Wifi_Radio_State rstate;
    struct schema_Wifi_Radio_Config_flags changed;
    bool want;
    bool has;

    LOGD("%s: recalculating", ifname);

    if (!(want = ovsdb_table_select_one(&table_Wifi_Radio_Config,
                                        SCHEMA_COLUMN(Wifi_Radio_Config, if_name),
                                        ifname,
                                        &rconf)))
        wm2_rconf_init_del(&rconf, ifname);

    if (!(has = ovsdb_table_select_one(&table_Wifi_Radio_State,
                                       SCHEMA_COLUMN(Wifi_Radio_State, if_name),
                                       ifname,
                                       &rstate)))
        memset(&rstate, 0, sizeof(rstate));

    if (want) {
        if (!rconf.enabled_exists || !rconf.enabled) {
            LOGW("%s: overriding 'enabled'; conf.db.bck needs fixing, or it's cloud bug PIR-12794", ifname);
            rconf.enabled_exists = true;
            rconf.enabled = true;
        }
    }

    if (rconf.channel_sync_exists) {
        if (rconf.channel_sync) {
            LOGW("%s: forcing reconfig", rconf.if_name);
            force = true;
        }
        rconf.channel_sync_exists = false;
    }

    if (want)
        if (wm2_rconf_recalc_fixup_channel(&rconf, &rstate))
            wm2_delayed_recalc(wm2_rconf_recalc, ifname);

    if (want)
        if (rconf.channel_exists && rconf.vif_configs_len == 0) {
            LOGD("%s: ignoring rconf channel %d: no vifs available yet",
                  rconf.if_name, rconf.channel);
            rconf.channel = rstate.channel;
        }

    if (want) {
        wm2_rconf_recalc_fixup_nop_channel(&rconf, &rstate);
        wm2_rconf_recalc_fixup_tx_chainmask(&rconf);
    }

    if (!wm2_rconf_changed(&rconf, &rstate, &changed) && !force)
        goto recalc;

    LOGI("%s: changed, configuring", ifname);

    if ((changed.channel || changed.ht_mode) && rconf.channel) {
        LOGN("%s: topology change: channel %d @ %s -> %d @ %s",
             ifname,
             rstate.channel, rstate.ht_mode,
             rconf.channel, rconf.ht_mode);
    }

    if (!target_radio_config_set2(&rconf, &changed)) {
        LOGW("%s: failed to configure, will retry later", ifname);
        wm2_delayed_recalc(wm2_rconf_recalc, ifname);
        return;
    }

    if (has && !want) {
        ovsdb_table_delete_simple(&table_Wifi_Radio_State,
                                  SCHEMA_COLUMN(Wifi_Radio_State, if_name),
                                  ifname);
    }

    wm2_delayed_recalc_cancel(wm2_rconf_recalc, ifname);
recalc:
    wm2_rconf_recalc_vifs(&rconf);
}

static void
wm2_op_vconf(const struct schema_Wifi_VIF_Config *vconf,
             const char *phy)
{
    struct schema_Wifi_VIF_Config tmp;
    json_t *where;

    memcpy(&tmp, vconf, sizeof(tmp));
    LOGI("%s @ %s: updating vconf", vconf->if_name, phy);
    REQUIRE(vconf->if_name, strlen(vconf->if_name) > 0);
    REQUIRE(vconf->if_name, vconf->_partial_update);
    if (!(where = ovsdb_where_simple(SCHEMA_COLUMN(Wifi_Radio_Config, if_name), phy)))
        return;
    REQUIRE(vconf->if_name, ovsdb_table_upsert_with_parent(&table_Wifi_VIF_Config,
                                                           &tmp,
                                                           false, // uuid not needed
                                                           NULL,
                                                           // parent:
                                                           SCHEMA_TABLE(Wifi_Radio_Config),
                                                           where,
                                                           SCHEMA_COLUMN(Wifi_Radio_Config, vif_configs)));
    LOGI("%s @ %s: updated vconf", vconf->if_name, phy);
    wm2_delayed_recalc(wm2_vconf_recalc, vconf->if_name);
}

static void
wm2_op_rconf(const struct schema_Wifi_Radio_Config *rconf)
{
    struct schema_Wifi_Radio_Config tmp;
    memcpy(&tmp, rconf, sizeof(tmp));
    LOGI("%s: updating rconf", rconf->if_name);
    REQUIRE(rconf->if_name, strlen(rconf->if_name) > 0);
    REQUIRE(rconf->if_name, rconf->_partial_update);
    OVERRIDE(rconf->if_name, tmp.vif_configs_present, false);
    tmp.vif_configs_len = 0;
    tmp.vif_configs_present = true;
    REQUIRE(rconf->if_name, 1 == ovsdb_table_upsert_f(&table_Wifi_Radio_Config, &tmp, false, NULL));
    LOGI("%s: updated rconf", rconf->if_name);
    wm2_delayed_recalc(wm2_rconf_recalc, rconf->if_name);
}

static void
wm2_op_vstate(const struct schema_Wifi_VIF_State *vstate)
{
    struct schema_Wifi_Radio_Config rconf;
    struct schema_Wifi_VIF_Config vconf;
    struct schema_Wifi_VIF_State state;
    struct schema_Wifi_VIF_State oldstate;
    json_t *where;
    int i;

    memcpy(&state, vstate, sizeof(state));

    LOGD("%s: updating vif state", state.if_name);
    REQUIRE(state.if_name, strlen(state.if_name) > 0);
    REQUIRE(state.if_name, state._partial_update);
    OVERRIDE(state.if_name, state.associated_clients_present, false);
    OVERRIDE(state.if_name, state.vif_config_present, false);

    str_tolower(state.parent);
    for (i = 0; i < state.mac_list_len; i++)
        str_tolower(state.mac_list[i]);

    if (!(wm2_lookup_rconf_by_vif_ifname(&rconf, state.if_name)))
        return;

    if (!wm2_lookup_vconf_by_ifname(&vconf, state.if_name))
        goto recalc;

    memset(&oldstate, 0, sizeof(oldstate));
    if ((where = ovsdb_where_simple(SCHEMA_COLUMN(Wifi_VIF_State, if_name), state.if_name)))
        ovsdb_table_select_one_where(&table_Wifi_VIF_State, where, &oldstate);

    /* Workaround for PIR-11008 */
    if (!state.ft_psk_exists) {
        state.ft_psk_exists = true;
        state.ft_psk = 0;
    }

    state.vif_config_exists = true;
    state.vif_config_present = true;
    memcpy(&state.vif_config, &vconf._uuid, sizeof(vconf._uuid));

    if (!(where = ovsdb_where_uuid(SCHEMA_COLUMN(Wifi_Radio_State, radio_config),
                                   rconf._uuid.uuid)))
        return;
    REQUIRE(state.if_name, ovsdb_table_upsert_with_parent(&table_Wifi_VIF_State,
                                                          &state,
                                                          false, // uuid not needed
                                                          NULL,
                                                          // parent:
                                                          SCHEMA_TABLE(Wifi_Radio_State),
                                                          where,
                                                          SCHEMA_COLUMN(Wifi_Radio_State, vif_states)));
    LOGI("%s: updated vif state", state.if_name);
recalc:
    /* Reconnect workaround is for CAES-771 */
    if (wm2_sta_has_reconnected(&oldstate, &state))
        wm2_radio_update_port_state_set_inactive(state.if_name);
    wm2_sta_print_status(&oldstate, &state);
    wm2_sta_handle_dfs_link_change(&oldstate, &state);
    wm2_radio_update_port_state(state.if_name);
    wm2_delayed_recalc(wm2_vconf_recalc, state.if_name);
}

static void
wm2_op_rstate(const struct schema_Wifi_Radio_State *rstate)
{
    struct schema_Wifi_Radio_Config rconf;
    struct schema_Wifi_Radio_State state;

    memcpy(&state, rstate, sizeof(state));

    LOGD("%s: updating radio state", state.if_name);
    REQUIRE(state.if_name, strlen(state.if_name) > 0);
    REQUIRE(state.if_name, state._partial_update);
    OVERRIDE(state.if_name, state.radio_config_present, false);
    OVERRIDE(state.if_name, state.vif_states_present, false);
    OVERRIDE(state.if_name, state.channel_sync_present, false);
    OVERRIDE(state.if_name, state.channel_mode_present, false);

    if (!wm2_lookup_rconf_by_ifname(&rconf, state.if_name))
        goto recalc;

    memcpy(&state.radio_config, &rconf._uuid, sizeof(rconf._uuid));
    state.radio_config_exists = true;
    state.radio_config_present = true;

    if (rconf.channel_mode_exists) {
        state.channel_mode_exists = true;
        state.channel_mode_present = true;
        STRSCPY(state.channel_mode, rconf.channel_mode);
    }

    REQUIRE(state.if_name, 1 == ovsdb_table_upsert_f(&table_Wifi_Radio_State, &state, false, NULL));
    LOGI("%s: updated radio state", state.if_name);
recalc:
    wm2_delayed_recalc(wm2_rconf_recalc, state.if_name);
}

static void
wm2_op_client(const struct schema_Wifi_Associated_Clients *client,
              const char *vif,
              bool associated)
{
    struct schema_Wifi_Associated_Clients tmp;
    char ifname[32];
    memcpy(&tmp, client, sizeof(tmp));
    REQUIRE(vif, tmp._partial_update);
    STRSCPY(ifname, vif);
    wm2_clients_update(&tmp, ifname, associated);
}

static void
wm2_op_clients(const struct schema_Wifi_Associated_Clients *clients,
               int num,
               const char *vif)
{
    struct schema_Wifi_Associated_Clients_flags changed;
    struct schema_Wifi_Associated_Clients *ovs_clients;
    struct schema_Wifi_VIF_State vstate;
    json_t *where;
    int i;
    int j;

    if (WARN_ON(!(where = ovsdb_where_simple(SCHEMA_COLUMN(Wifi_VIF_State, if_name), vif))))
        return;
    if (!ovsdb_table_select_one_where(&table_Wifi_VIF_State, where, &vstate))
        return;
    if (WARN_ON(!(ovs_clients = calloc(vstate.associated_clients_len, sizeof(*ovs_clients)))))
        return;

    for (i = 0; i < vstate.associated_clients_len; i++) {
        if (WARN_ON(!(where = ovsdb_where_uuid("_uuid", vstate.associated_clients[i].uuid))))
            goto free;
        if (WARN_ON(!ovsdb_table_select_one_where(&table_Wifi_Associated_Clients, where, ovs_clients + i)))
            goto free;
    }

    if (!schema_changed_set(clients, ovs_clients,
                            num, vstate.associated_clients_len,
                            sizeof(*clients),
                            strncasecmp))
        goto free;

    LOGI("%s: syncing clients", vif);

    for (i = 0; i < vstate.associated_clients_len; i++) {
        for (j = 0; j < num; j++)
            if (!strcasecmp(ovs_clients[i].mac, clients[j].mac))
                break;
        if (j == num) {
            LOGI("%s: removing stale client %s", vif, ovs_clients[i].mac);
            wm2_op_client(ovs_clients + i, vif, false);
        }
    }

    for (i = 0; i < num; i++) {
        for (j = 0; j < vstate.associated_clients_len; j++)
            if (!strcasecmp(clients[i].mac, ovs_clients[j].mac))
                break;
        if (j == vstate.associated_clients_len || wm2_client_changed(clients + i, ovs_clients + j, &changed)) {
            LOGI("%s: adding/updating client %s", vif, clients[i].mac);
            wm2_op_client(clients + i, vif, true);
        }
    }

free:
    free(ovs_clients);
}

static void
wm2_op_flush_clients(const char *vif)
{
    struct schema_Wifi_Associated_Clients client;
    struct schema_Wifi_VIF_State vstate;
    json_t *where;
    int i;

    if (!(where = ovsdb_where_simple(SCHEMA_COLUMN(Wifi_VIF_State, if_name), vif)))
        return;
    if (!ovsdb_table_select_one_where(&table_Wifi_VIF_State, where, &vstate))
        return;

    LOGI("%s: flushing clients", vif);
    for (i = 0; i < vstate.associated_clients_len; i++) {
        if (!(where = ovsdb_where_uuid("_uuid", vstate.associated_clients[i].uuid)))
            continue;
        if (!ovsdb_table_select_one_where(&table_Wifi_Associated_Clients, where, &client))
            continue;
        LOGI("%s: flushing client %s", vif, client.mac);
        if (!(where = ovsdb_where_uuid("_uuid", vstate.associated_clients[i].uuid)))
            continue;
        ovsdb_table_delete_where(&table_Wifi_Associated_Clients, where);
    }
}

static const struct target_radio_ops rops = {
    .op_vconf = wm2_op_vconf,
    .op_rconf = wm2_op_rconf,
    .op_vstate = wm2_op_vstate,
    .op_rstate = wm2_op_rstate,
    .op_client = wm2_op_client,
    .op_clients = wm2_op_clients,
    .op_flush_clients = wm2_op_flush_clients,
};

static void
callback_Wifi_Radio_Config(
        ovsdb_update_monitor_t          *mon,
        struct schema_Wifi_Radio_Config *old_rec,
        struct schema_Wifi_Radio_Config *rconf,
        ovsdb_cache_row_t *row)
{
    if (wm2_api2) {
        LOGD("%s: ovsdb updated", rconf->if_name);
        wm2_rconf_recalc(rconf->if_name, false);
    } else {
        callback_Wifi_Radio_Config_v1(mon, old_rec, rconf, row);
    }
}

static void
callback_Wifi_VIF_Config(
        ovsdb_update_monitor_t          *mon,
        struct schema_Wifi_VIF_Config   *old_rec,
        struct schema_Wifi_VIF_Config   *vconf,
        ovsdb_cache_row_t               *row)
{
    if (wm2_api2) {
        LOGD("%s: ovsdb updated", vconf->if_name);
        wm2_vconf_recalc(vconf->if_name, false);
    } else {
        callback_Wifi_VIF_Config_v1(mon, old_rec, vconf, row);
    }
}

static void
wm2_radio_config_bump(void)
{
    struct schema_Wifi_Radio_Config *rconf;
    struct schema_Wifi_VIF_Config *vconf;
    void *buf;
    int n;

    if ((buf = ovsdb_table_select_where(&table_Wifi_Radio_Config, NULL, &n))) {
        for (n--; n >= 0; n--) {
            rconf = buf + (n * sizeof(*rconf));
            LOGI("%s: bumping", rconf->if_name);
            wm2_rconf_recalc(rconf->if_name, true);
        }
        free(buf);
    }

    if ((buf = ovsdb_table_select_where(&table_Wifi_VIF_Config, NULL, &n))) {
        for (n--; n >= 0; n--) {
            vconf = buf + (n * sizeof(*vconf));
            LOGI("%s: bumping", vconf->if_name);
            wm2_vconf_recalc(vconf->if_name, true);
        }
        free(buf);
    }
}

/******************************************************************************
 *  PUBLIC API definitions
 *****************************************************************************/
int
wm2_radio_init_v2(void)
{
    ovsdb_table_delete_where(&table_Wifi_Associated_Clients, json_array());

    if (target_radio_config_need_reset()) {
        ovsdb_table_delete_where(&table_Wifi_Radio_Config, json_array());
        ovsdb_table_delete_where(&table_Wifi_Radio_State, json_array());
        ovsdb_table_delete_where(&table_Wifi_VIF_Config, json_array());
        ovsdb_table_delete_where(&table_Wifi_VIF_State, json_array());
        if (!target_radio_config_init2()) {
            LOGE("Failed to initialize radio");
            return -1;
        }
    } else {
        /* This is intended to bump target when WM is
         * restarted, e.g. due to a crash. It may end up
         * calling even if Config matches State. This is
         * intended as to give an opportunity for target
         * implementation to register event/socket listeners
         * it needs to operate.
         *
         * This is intended to be nicer variant of
         * target_radio_has_config() because it doesn't
         * imply complete reconfiguration.
         */
        wm2_radio_config_bump();
    }
    return 0;
}

int
wm2_radio_init(void)
{
    LOGD("Initializing radios");

    // Initialize OVSDB tables
    OVSDB_TABLE_INIT(Wifi_Radio_Config, if_name);
    OVSDB_TABLE_INIT(Wifi_Radio_State, if_name);
    OVSDB_TABLE_INIT(Wifi_VIF_Config, if_name);
    OVSDB_TABLE_INIT(Wifi_VIF_State, if_name);
    OVSDB_TABLE_INIT(Wifi_Credential_Config, _uuid);
    OVSDB_TABLE_INIT(Wifi_Associated_Clients, _uuid);
    OVSDB_TABLE_INIT(Wifi_Master_State, if_name);
    OVSDB_TABLE_INIT(Openflow_Tag, name);

    if ((wm2_api2 = target_radio_init(&rops))) {
        LOGI("Using new API v2");
        wm2_radio_init_v2();
    } else {
        LOGW("Using old deprecated API v1");
        wm2_radio_config_init_v1();
    }

    // Initialize OVSDB monitor callbacks
    OVSDB_CACHE_MONITOR(Wifi_Radio_Config, true);
    OVSDB_CACHE_MONITOR(Wifi_VIF_Config, true);

    return 0;
}
