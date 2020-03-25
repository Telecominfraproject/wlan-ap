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

#ifndef __HAVE_DPP_TYPES_H__
#define __HAVE_DPP_TYPES_H__

#include "schema_consts.h"

#define MAC_ADDRESS_LEN 6
#define MAC_ADDR_EQ(a, b) (memcmp(a, b, MAC_ADDRESS_LEN) == 0)
#define MAC_ADDRESS_FORMAT "%02X:%02X:%02X:%02X:%02X:%02X"
#define MAC_ADDRESS_PRINT(mac) \
    (unsigned int) (mac)[0] & 0xFF, \
    (unsigned int) (mac)[1] & 0xFF, \
    (unsigned int) (mac)[2] & 0xFF, \
    (unsigned int) (mac)[3] & 0xFF, \
    (unsigned int) (mac)[4] & 0xFF, \
    (unsigned int) (mac)[5] & 0xFF
typedef uint8_t mac_address_t[MAC_ADDRESS_LEN];

#define MACADDR_STR_LEN (12 + 5 + 1)
typedef char mac_address_str_t[MACADDR_STR_LEN];

typedef mac_address_str_t   radio_bssid_t;

#define RADIO_IFNAME_LEN 16
typedef char ifname_t[RADIO_IFNAME_LEN];

#define RADIO_ESSID_LEN 128
typedef char radio_essid_t[RADIO_ESSID_LEN + 1];

#define RADIO_MAX_BIT_RATES             15
#define RADIO_COUNTRY_CODE_LEN          8
#define RADIO_NAME_LEN                  32


#define RADIO_MAX_CHANNELS              32+1 // only till 165 !!!

#define radio_get_chan_index(type, chan) \
    ((RADIO_TYPE_2G == type) ? (chan - 1) : ((chan - 36)/4))

typedef struct
{
    uint32_t                        avg;
    uint32_t                        min;
    uint32_t                        max;
    uint32_t                        num;
} dpp_avg_t;

typedef enum
{
    RADIO_802_11_AUTO = 0,
    RADIO_802_11_NG,
    RADIO_802_11_BG,
    RADIO_802_11_NA,
    RADIO_802_11_A,
    RADIO_802_11_AC,
    RADIO_MAX_802_11_PROTOCOLS
} radio_protocols_t;

typedef enum
{
    RADIO_CHAN_WIDTH_NONE = 0,
    RADIO_CHAN_WIDTH_UNKNOWN = 0,
    RADIO_CHAN_WIDTH_20MHZ,
    RADIO_CHAN_WIDTH_40MHZ,
    RADIO_CHAN_WIDTH_40MHZ_ABOVE,
    RADIO_CHAN_WIDTH_40MHZ_BELOW,
    RADIO_CHAN_WIDTH_80MHZ,
    RADIO_CHAN_WIDTH_160MHZ,
    RADIO_CHAN_WIDTH_80_PLUS_80MHZ,
    RADIO_CHAN_WIDTH_QTY
} radio_chanwidth_t;

typedef enum
{
    CLIENT_RADIO_WIDTH_20MHZ = 0,
    CLIENT_RADIO_WIDTH_40MHZ,
    CLIENT_RADIO_WIDTH_80MHZ,
    CLIENT_RADIO_WIDTH_160MHZ,
    CLIENT_RADIO_WIDTH_80_PLUS_80MHZ,
    CLIENT_MAX_RADIO_WIDTH_QTY
} client_mcs_width_t;

typedef enum
{
    RADIO_STATUS_DISABLED = 0,
    RADIO_STATUS_ENABLED  = 1
} radio_status_t;

typedef enum
{
    RADIO_CHAN_MODE_MANUAL = 0,
    RADIO_CHAN_MODE_AUTO = 1,
    RADIO_CHAN_MODE_AUTO_DFS = 2
} radio_chan_mode_t;

typedef enum
{
    RADIO_HT_20 = 0,
    RADIO_HT_40,
    RADIO_HT_20_40,
    RADIO_VHT_80,
    RADIO_HT_NONE
} radio_ht_mode_t;

typedef enum
{
    RADIO_TYPE_NONE = 0,
    RADIO_TYPE_2G,
    RADIO_TYPE_5G,
    RADIO_TYPE_5GL,
    RADIO_TYPE_5GU
} radio_type_t;

#define RADIO_TYPE_STR_2G       SCHEMA_CONSTS_RADIO_TYPE_STR_2G
#define RADIO_TYPE_STR_5G       SCHEMA_CONSTS_RADIO_TYPE_STR_5G
#define RADIO_TYPE_STR_5GL      SCHEMA_CONSTS_RADIO_TYPE_STR_5GL
#define RADIO_TYPE_STR_5GU      SCHEMA_CONSTS_RADIO_TYPE_STR_5GU

typedef enum
{
    RADIO_SCAN_TYPE_NONE = 0,
    RADIO_SCAN_TYPE_FULL,
    RADIO_SCAN_TYPE_ONCHAN,
    RADIO_SCAN_TYPE_OFFCHAN,
} radio_scan_type_t;

#define RADIO_SCAN_MAX_TYPE_QTY       3

typedef enum
{
    RADIO_QUEUE_TYPE_VI = 0,
    RADIO_QUEUE_TYPE_VO,
    RADIO_QUEUE_TYPE_BE,
    RADIO_QUEUE_TYPE_BK,
    RADIO_QUEUE_TYPE_CAB,
    RADIO_QUEUE_TYPE_BCN,
    RADIO_QUEUE_MAX_QTY,
    RADIO_QUEUE_TYPE_NONE = -1
} radio_queue_type_t;

#if !defined(TARGET_NATIVE)
#include <time.h>
#define TIME_NSEC_IN_SEC   1000000000
#define TIME_USEC_IN_SEC   1000000
#define TIME_MSEC_IN_SEC   1000
#define TIME_NSEC_PER_MSEC (TIME_NSEC_IN_SEC / TIME_MSEC_IN_SEC)

static inline uint64_t timespec_to_timestamp(const struct timespec *ts)
{
    return (uint64_t)ts->tv_sec * TIME_MSEC_IN_SEC + ts->tv_nsec / TIME_NSEC_PER_MSEC;
}

static inline uint64_t get_timestamp(void)
{
    struct timespec                 ts;

    memset (&ts, 0, sizeof (ts));
    if(clock_gettime(CLOCK_MONOTONIC, &ts) != 0)
    {
        return 0;
    }
    else
        return timespec_to_timestamp(&ts);
}
#endif /* ! TARGET_NATIVE */

typedef struct
{
    radio_type_t                    type;
    uint32_t                        chan;
    radio_chan_mode_t               chan_mode;
    radio_chanwidth_t               chanwidth;
    uint32_t                        tx_power;
    radio_protocols_t               protocol;
    char                            cntry_code[RADIO_COUNTRY_CODE_LEN];
    radio_status_t                  admin_status;
    char                            phy_name[RADIO_NAME_LEN];
    char                            if_name[RADIO_NAME_LEN];
} radio_entry_t;

typedef struct
{
    radio_type_t                    type;
    uint32_t                        index;
    char                            phy_name[RADIO_NAME_LEN];
    char                            if_name[RADIO_NAME_LEN];
} radio_info_t;

radio_entry_t * radio_get_config_from_type(
        radio_type_t                radio_type);

static inline char * radio_get_name_from_type(
		radio_type_t                radio_type)
{
    switch (radio_type)
    {
        case RADIO_TYPE_2G:
            return RADIO_TYPE_STR_2G;
            break;
        case RADIO_TYPE_5G:
            return RADIO_TYPE_STR_5G;
            break;
        case RADIO_TYPE_5GL:
            return RADIO_TYPE_STR_5GL;
            break;
        case RADIO_TYPE_5GU:
            return RADIO_TYPE_STR_5GU;
            break;
        default:
            break;
    }

    return NULL;
}

static inline char *radio_get_name_from_cfg(
        radio_entry_t              *radio_cfg)
{
    static char name[RADIO_IFNAME_LEN + 1 + RADIO_NAME_LEN]; // For space between the names

    if (NULL == radio_cfg) {
        return NULL;
    }
    memset (name, 0, sizeof(name));

    snprintf(name, sizeof(name), "%s %s",
            radio_get_name_from_type(radio_cfg->type),
            radio_cfg->phy_name);

    return name;
}

static inline radio_type_t  radio_get_type_from_name(
        char                       *radio_name)
{
    if (!radio_name) {
        return RADIO_TYPE_NONE;
    }

    if (!strcmp(radio_name, RADIO_TYPE_STR_2G)) {
        return RADIO_TYPE_2G;
    }

    if (!strcmp(radio_name, RADIO_TYPE_STR_5G)) {
        return RADIO_TYPE_5G;
    }

    if (!strcmp(radio_name, RADIO_TYPE_STR_5GL)) {
        return RADIO_TYPE_5GL;
    }

    if (!strcmp(radio_name, RADIO_TYPE_STR_5GU)) {
        return RADIO_TYPE_5GU;
    }

    return RADIO_TYPE_NONE;
}

static inline char * radio_get_scan_name_from_type(
		radio_scan_type_t           scan_type)
{
    switch (scan_type)
    {
        case RADIO_SCAN_TYPE_ONCHAN:
            return "on-chan";
            break;
        case RADIO_SCAN_TYPE_OFFCHAN:
            return "off-chan";
            break;
        case RADIO_SCAN_TYPE_FULL:
            return "full";
            break;
        default:
            break;
    }

    return NULL;
}

static inline radio_scan_type_t radio_get_scan_type_from_index(
		uint32_t                    scan_index)
{
    switch (scan_index + 1) {
        case RADIO_SCAN_TYPE_ONCHAN:
        case RADIO_SCAN_TYPE_OFFCHAN:
        case RADIO_SCAN_TYPE_FULL:
            return scan_index + 1;
        default:
            break;
    }

    return -1;
}

static inline int radio_get_scan_index_from_type(
		radio_scan_type_t           scan_type)
{
    switch (scan_type) {
        case RADIO_SCAN_TYPE_ONCHAN:
        case RADIO_SCAN_TYPE_OFFCHAN:
        case RADIO_SCAN_TYPE_FULL:
            return scan_type - 1;
        default:
            break;
    }

    return -1;
}

static inline int radio_get_index_from_type(
		radio_type_t                radio_type)
{
    /* NOTE: 5G and 5GL are equal indexes */
    switch (radio_type) {
        case RADIO_TYPE_2G:
        case RADIO_TYPE_5G:
            return radio_type - 1;
            break;
        case RADIO_TYPE_5GL:
        case RADIO_TYPE_5GU:
            return radio_type - 2;
            break;
        default:
            break;
    }

    return -1;
}

static inline uint32_t radio_get_chan_from_mhz(
        uint32_t                    freq)
{
    if (freq == 2484)
        return 14;
    else if (freq < 2484)
        return (freq - 2407) / 5;
    return (freq - 5000) / 5;
}

static inline char * radio_get_queue_name_from_type(
        radio_queue_type_t          queue_type)
{
    switch (queue_type)
    {
        case RADIO_QUEUE_TYPE_BK:
            return "bk";
            break;
        case RADIO_QUEUE_TYPE_BE:
            return "be";
            break;
        case RADIO_QUEUE_TYPE_VI:
            return "vi";
            break;
        case RADIO_QUEUE_TYPE_VO:
            return "vo";
            break;
        case RADIO_QUEUE_TYPE_CAB:
            return "cab";
            break;
        case RADIO_QUEUE_TYPE_BCN:
            return "bcn";
            break;
        default:
            break;
    }

    return NULL;
}

typedef enum
{
    REPORT_TYPE_NONE = 0,
    REPORT_TYPE_RAW,
    REPORT_TYPE_AVERAGE,
    REPORT_TYPE_HISTOGRAM,
    REPORT_TYPE_PERCENTILE,
    REPORT_TYPE_DIFF
} report_type_t;

static inline char * report_get_name_from_type(
		report_type_t               report_type)
{
    switch (report_type)
    {
        case REPORT_TYPE_RAW:
            return "raw";
            break;
        case REPORT_TYPE_AVERAGE:
            return "average";
            break;
        case REPORT_TYPE_HISTOGRAM:
            return "histogram";
            break;
        case REPORT_TYPE_PERCENTILE:
            return "percentile";
            break;
        case REPORT_TYPE_DIFF:
            return "diff";
            break;
        default:
            break;
    }

    return NULL;
}

typedef enum
{
    RSSI_SOURCE_NONE = 0,
    RSSI_SOURCE_CLIENT,
    RSSI_SOURCE_PROBE,
    RSSI_SOURCE_NEIGHBOR
} rssi_source_t;

#endif /* __HAVE_DPP_TYPES_H__ */
