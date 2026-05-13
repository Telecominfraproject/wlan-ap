/*
 * Copyright 2022 Morse Micro
 *
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "offchip_statistics.h"

/**
 * Bit field definitions for mac_state statistic
 **/
#define ENCODE_MAC_STATE_RX_STATE           (0x000000000000000F)
#define ENCODE_MAC_STATE_TX_STATE           (0x00000000000000F0)
#define ENCODE_MAC_STATE_CHANNEL_CONFIG     (0x0000000000000F00)
#define ENCODE_MAC_STATE_MGD_CALIB_STATE    (0x0000000000007000)
#define ENCODE_MAC_STATE_STA_PS_STATE       (0x0000000000038000)
#define ENCODE_MAC_STATE_TX_BLOCKED         (0x0000000000080000)
#define ENCODE_MAC_STATE_WAITING_MED_SYNC   (0x0000000000100000)
#define ENCODE_MAC_STATE_PS_EN              (0x0000000000200000)
#define ENCODE_MAC_STATE_DYN_PS_OFFLOAD_EN  (0x0000000000400000)
#define ENCODE_MAC_STATE_WAITING_ON_DYN_PS  (0x0000000000800000)
#define ENCODE_MAC_STATE_N_PKTS_IN_QUEUES   (0x00000000FF000000)

/* The maximum number of bits in the NDP bitmap */
#define DOT11AH_NDP_MAX_BITMAP_BIT          (16)

/**
 * A counter of the number of times an MPDU is received successfully for each position in the AMPDU
 **/
typedef struct __attribute__((packed)) {
    uint32_t bitmap[DOT11AH_NDP_MAX_BITMAP_BIT];
} ampdu_bitmap_t;

/**
 * A counter of the number of aggregates
 */
typedef struct __attribute__((packed)) {
    /* This count starts at zero and goes to 16, so it's 17 elements */
    uint32_t count[17];
} ampdu_count_t;

#define MAC_MAX_RETRY_COUNT 10
 /* Account for 0 retry, for more than
  * MAC_MAX_RETRY_COUNT, and fail */
#define APP_STATS_COUNT (MAC_MAX_RETRY_COUNT + 3)

struct __attribute__((packed)) retry_stats {
    uint64_t start, stop;
    uint64_t sum[APP_STATS_COUNT];
    uint32_t count[APP_STATS_COUNT];
};


#define NUM_PAGESETS 2
struct __attribute__((packed)) pageset_stats {
    uint32_t pages_allocated[NUM_PAGESETS];
    uint32_t pages_to_allocate[NUM_PAGESETS];
};



/** TX OP statistics, Note: Changes made to this struct must be reflected in statistics.py parser */
struct __attribute__((packed)) txop_statistics {
    uint64_t    duration;
    uint32_t    count;
    uint32_t    pkts;
    uint32_t    max_pkts_in_txop;
    uint32_t    lost_beacons;
    uint8_t     beacon_lost;
};


/**
 * RAW (Restricted Access Window) statistics
 */
typedef struct PACKED {
    /** Currently only supporting up to 8 assignments */
    uint32_t assignments[8];
    /** A counter of assignments that get truncated due to the next tbtt */
    uint32_t assignments_truncated_from_tbtt;
    /** Number of invalid assignments (/unsupported) observed */
    uint32_t invalid_assignments;
    /** Number of assignments that are valid but system time has already passed */
    uint32_t already_past_assignment;
    /** ACI delayed frames due to RAW */
    uint32_t aci_frames_delayed;
    /** Broadcast / Multicast delayed frames due to RAW */
    uint32_t bc_mc_frames_delayed;
    /** Absolute time frames delayed due to RAW */
    uint32_t abs_frames_delayed;
    /** Frames that could've been sent in the RAW but were too long for slot */
    uint32_t frame_crosses_slot_delayed;
} raw_stats_t;

typedef struct {
    /** A quiet calibration was granted */
    uint32_t quiet_calibration_granted;
    /** A non-quiet calibration was granted */
    uint32_t non_quiet_calibration_granted;
    /** A quiet calibration was in progress, but then cancelled */
    uint32_t quiet_calibration_cancelled;
    /** A quiet calibration was rejected */
    uint32_t quiet_calibration_rejected;
    /** A quiet/non-quiet calibration completed */
    uint32_t calibration_complete;
} managed_calibration_stats_t;

typedef struct
{
    /** Total transmitter 'on air' (Tair) time in us */
    uint64_t total_t_air;
    /** Total transmitter 'blocked' (Toff) time in us */
    uint64_t total_t_off;
    /** Configured duty cycle restriction (100th of a percent). 10000 is no restriction. */
    uint32_t target_duty_cycle;
    /** Number of packets ignoring duty cycle restrictions. */
    uint32_t num_early;
    /** Maximum time the t_off timer is started for */
    uint64_t max_t_off;
} duty_cycle_stats_t;


struct PACKED stats_response
{
    /** The contents of the response */
    uint8_t stats[2048];
};


/** Enum type for printing format  */
enum format_type
{
    FORMAT_REGULAR,
    FORMAT_JSON,
    FORMAT_JSON_PPRINT,
    /* Add additional formats here  */
};


typedef void (*format_func_t)(const char *key, const uint8_t *buf, uint32_t len);

struct format_table {
    format_func_t format_func[MORSE_STATS_FMT_LAST + 1];
};

/** Regular format function  */
const struct format_table* stats_format_regular_get_formatter_table();
void hexdump(const uint8_t *buf, uint32_t len);

/** JSON format functions  */
const struct format_table* stats_format_json_get_formatter_table();
void stats_format_json_init();
void stats_format_json_set_pprint(bool pprint);
