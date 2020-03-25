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

/*
 * Band Steering Manager - Interface Pair
 */

#ifndef __BM_PAIR_H__
#define __BM_PAIR_H__

#ifndef OVSDB_UUID_LEN
#define OVSDB_UUID_LEN      37
#endif /* OVSDB_UUID_LEN */

/***************************************************************************************/

typedef void* bsal_t;

typedef struct {
    bool                    enabled;
    bsal_t                  bsal;

    bsal_ifconfig_t         ifcfg[BSAL_BAND_COUNT];
    bsal_neigh_info_t       self_neigh[BSAL_BAND_COUNT];
    uint16_t                kick_debounce_thresh;
    uint16_t                kick_debounce_period;
    uint16_t                success_threshold;
    uint16_t                stats_report_interval;
    uint8_t                 chan_util_hwm;
    uint8_t                 chan_util_lwm;
    uint8_t                 debug_level;
    bool                    gw_only;

    char                    uuid[OVSDB_UUID_LEN];
    ds_tree_node_t          dst_node;
} bm_pair_t;


/*****************************************************************************/

extern bool             bm_pair_init(void);
extern bool             bm_pair_cleanup(void);
extern ds_tree_t *      bm_pair_get_tree(void);
extern bm_pair_t *      bm_pair_find_by_uuid(char *uuid);
extern bm_pair_t *      bm_pair_find_by_bsal(bsal_t bsal);
extern bm_pair_t *      bm_pair_find_by_ifname(const char *ifname);
extern bsal_band_t      bsal_band_find_by_ifname(const char *ifname);

#endif /* __BM_PAIR_H__ */
