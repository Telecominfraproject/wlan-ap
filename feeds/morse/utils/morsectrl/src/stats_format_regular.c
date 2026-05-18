/*
 * Copyright 2022 Morse Micro
 */

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <inttypes.h>

#include "portable_endian.h"
#include "command.h"
#include "offchip_statistics.h"
#include "stats_format.h"
#include "utilities.h"


/** Regular formatting functions for morsectrl statistics */

static void print_dec(const char *key, const uint8_t *buf, uint32_t len)
{
    mctrl_print("%s:%" PRId64 "\n", key, get_signed_value_as_int64(buf, len));
}


static void print_udec(const char *key, const uint8_t *buf, uint32_t len)
{
    mctrl_print("%s: %" PRIu64 "\n", key, get_unsigned_value_as_uint64(buf, len));
}


static void print_hex(const char *key, const uint8_t *buf, uint32_t len)
{
    mctrl_print("%s: 0x%" PRIx64 "\n", key, get_unsigned_value_as_uint64(buf, len));
}


static void print_0hex(const char *key, const uint8_t *buf, uint32_t len)
{
    mctrl_print("%s: 0x%0*" PRIx64 "\n", key, len * 2, get_unsigned_value_as_uint64(buf, len));
}


static void print_ampdu_aggregates(const char *key, const uint8_t *buf, uint32_t len)
{
    ampdu_count_t *count = (ampdu_count_t *)buf;
    mctrl_print("%s: ", key);
    for (int i = 0; i < MORSE_ARRAY_SIZE(count->count); i++)
    {
        mctrl_print("%u ", count->count[i]);
    }
    mctrl_print("\n");
}


static void print_ampdu_bitmap(const char *key, const uint8_t *buf, uint32_t len)
{
    ampdu_bitmap_t *bitmap = (ampdu_bitmap_t *)buf;
    mctrl_print("%s: ", key);
    for (int i = 0; i < MORSE_ARRAY_SIZE(bitmap->bitmap); i++)
    {
        mctrl_print("%u ", bitmap->bitmap[i]);
    }
    mctrl_print("\n");
}


static void print_txop(const char *key, const uint8_t *buf, uint32_t len)
{
    struct txop_statistics *txop_stats = (struct txop_statistics *)buf;
    uint32_t duration_avg = 0, packets_avg = 0;

    if (txop_stats->count)
    {
        packets_avg = (uint32_t)(txop_stats->pkts / txop_stats->count);
        duration_avg = (uint32_t)(txop_stats->duration / txop_stats->count);
    }

    mctrl_print("%s: ", key);
    mctrl_print("TXOP count: %u\n", txop_stats->count);
    mctrl_print("Total TXOP time: %" PRIu64 "\n", txop_stats->duration);
    mctrl_print("Average TXOP time: %u\n", duration_avg);
    mctrl_print("Total TXOP Tx packets: %u\n", txop_stats->pkts);
    mctrl_print("Average TXOP Tx packets: %u\n", packets_avg);
}


static void print_pageset(const char *key, const uint8_t *buf, uint32_t len)
{
    struct pageset_stats *pageset = (struct pageset_stats *)buf;

    mctrl_print("%s: \n", key);
    for (int i = 0; i < NUM_PAGESETS; i++)
    {
        mctrl_print("Pageset %d\n", i);
        mctrl_print("\tallocated: %d\n", pageset->pages_allocated[i]);
        mctrl_print("\ttotal: %d\n", pageset->pages_to_allocate[i]);
    }
}


static void print_retries(const char *key, const uint8_t *buf, uint32_t len)
{
    struct retry_stats *retries = (struct retry_stats *)buf;
    mctrl_print("%s: \n", key);
    mctrl_print("Retry\tCount\tAvg Time\n");
    mctrl_print("=====\t=====\t========\n");

    for (int i = 0; i < APP_STATS_COUNT; i++)
    {
        mctrl_print("%d\t%u\t%u\n", i, retries->count[i],
        retries->count[i] ? (uint32_t)(retries->sum[i]/retries->count[i]) : 0);
    }
}


static void print_raw(const char *key, const uint8_t *buf, uint32_t len)
{
    raw_stats_t *raw_stats = (raw_stats_t *)buf;
    mctrl_print("%s: \n", key);
    mctrl_print("RAW Assignments\n\tValid:");

    for (uint8_t i = 0; i < MORSE_ARRAY_SIZE(raw_stats->assignments); i++)
    {
        mctrl_print(" %d", raw_stats->assignments[i]);
    }
    mctrl_print("\n");
    mctrl_print("\tTruncated by tbtt: %d\n", raw_stats->assignments_truncated_from_tbtt);
    mctrl_print("\tInvalid: %d\n", raw_stats->invalid_assignments);
    mctrl_print("\tAlready past: %d\n", raw_stats->already_past_assignment);
    mctrl_print("Delayed due to RAW\n");
    mctrl_print("\tFrom aci queue: %d\n", raw_stats->aci_frames_delayed);
    mctrl_print("\tFrom bc/mc queue: %d\n", raw_stats->bc_mc_frames_delayed);
    mctrl_print("\tFrom abs time queue: %d\n", raw_stats->abs_frames_delayed);
    mctrl_print("\tFrame crosses slot: %d\n", raw_stats->frame_crosses_slot_delayed);
}


static void print_calibration(const char *key, const uint8_t *buf, uint32_t len)
{
    managed_calibration_stats_t *calib_stats = (managed_calibration_stats_t *)buf;
    mctrl_print("%s: \n", key);
    mctrl_print("Manged Calibration\n");
    mctrl_print("\tQuiet calibration granted: %d\n",
            calib_stats->quiet_calibration_granted);
    mctrl_print("\tQuiet calibration rejected: %d\n",
            calib_stats->quiet_calibration_rejected);
    mctrl_print("\tQuiet calibration cancelled: %d\n",
            calib_stats->quiet_calibration_cancelled);
    mctrl_print("\tNon-Quiet calibration granted: %d\n",
            calib_stats->non_quiet_calibration_granted);
    mctrl_print("\tCalibration complete: %d\n", calib_stats->calibration_complete);
}


static void print_duty_cycle(const char *key, const uint8_t *buf, uint32_t len)
{
    duty_cycle_stats_t *duty_cycle_stats = (duty_cycle_stats_t *)buf;
    mctrl_print("%s: \n", key);
    mctrl_print("Duty Cycle Target (%%): %d.%02d\n",
            duty_cycle_stats->target_duty_cycle / 100,
            duty_cycle_stats->target_duty_cycle % 100);
    mctrl_print("Duty Cycle TX On (us): %" PRIu64 "\n", duty_cycle_stats->total_t_air);
    mctrl_print("Duty Cycle TX Off (Blocked) (us): %" PRIu64 "\n", duty_cycle_stats->total_t_off);
    mctrl_print("Duty Cycle Max toff (us): %" PRIu64 "\n", duty_cycle_stats->max_t_off);
    mctrl_print("Duty Cycle Early Frames: %u\n", duty_cycle_stats->num_early);
}


static void print_mac_state(const char *key, const uint8_t *buf, uint32_t len)
{
    uint64_t mac_state;
    const uint8_t desc_len = 39;
    memcpy(&mac_state, buf, sizeof(mac_state));
    mctrl_print("%s: \n", key);
    mctrl_print("\n    %-*s :%" PRId64 "\n", desc_len, "RX state",
        BMGET(mac_state, ENCODE_MAC_STATE_RX_STATE));
    mctrl_print("    %-*s :%" PRId64 "\n", desc_len, "TX state",
        BMGET(mac_state, ENCODE_MAC_STATE_TX_STATE));
    mctrl_print("    %-*s :%" PRId64 "\n", desc_len, "Channel config",
        BMGET(mac_state, ENCODE_MAC_STATE_CHANNEL_CONFIG));
    mctrl_print("    %-*s :%" PRId64 "\n", desc_len, "Managed calibration state",
        BMGET(mac_state, ENCODE_MAC_STATE_MGD_CALIB_STATE));
    mctrl_print("    %-*s :%" PRId64 "\n", desc_len, "Powersave enabled",
        BMGET(mac_state, ENCODE_MAC_STATE_PS_EN));
    mctrl_print("    %-*s :%" PRId64 "\n", desc_len, "Dynamic powersave offload enabled",
        BMGET(mac_state, ENCODE_MAC_STATE_DYN_PS_OFFLOAD_EN));
    mctrl_print("    %-*s :%" PRId64 "\n", desc_len, "STA PS state",
        BMGET(mac_state, ENCODE_MAC_STATE_STA_PS_STATE));
    mctrl_print("    %-*s :%" PRId64 "\n", desc_len, "Is waiting on dynamic powersave timeout",
        BMGET(mac_state, ENCODE_MAC_STATE_WAITING_ON_DYN_PS));
    mctrl_print("    %-*s :%" PRId64 "\n", desc_len, "TX blocked by host cmd",
        BMGET(mac_state, ENCODE_MAC_STATE_TX_BLOCKED));
    mctrl_print("    %-*s :%" PRId64 "\n", desc_len, "Is waiting for medium sync",
        BMGET(mac_state, ENCODE_MAC_STATE_WAITING_MED_SYNC));
    mctrl_print("    %-*s :%" PRId64 "\n", desc_len, "N packets in QoS queues",
        BMGET(mac_state, ENCODE_MAC_STATE_N_PKTS_IN_QUEUES));
}




static void print_default(const char *key, const uint8_t *buf, uint32_t len)
{
    /* Not implemented prior, use default hexdump in previous switch statement */
    mctrl_print("%s :", key);
    hexdump(buf, len);
    mctrl_print("\n");
}

void hexdump(const uint8_t *buf, uint32_t len)
{
    for (int i = 0; i < len; i++)
    {
        mctrl_print("%02X ", buf[i]);
    }
}

/**
 * Array of function pointers indexed by the TLV format key
 */
static const struct format_table table = {
    .format_func = {
        [MORSE_STATS_FMT_DEC] = print_dec,
        [MORSE_STATS_FMT_U_DEC] = print_udec,
        [MORSE_STATS_FMT_HEX] = print_hex,
        [MORSE_STATS_FMT_0_HEX] = print_0hex,
        [MORSE_STATS_FMT_AMPDU_AGGREGATES] = print_ampdu_aggregates,
        [MORSE_STATS_FMT_AMPDU_BITMAP] = print_ampdu_bitmap,
        [MORSE_STATS_FMT_TXOP] =  print_txop,
        [MORSE_STATS_FMT_PAGESET] = print_pageset,
        [MORSE_STATS_FMT_RETRIES] = print_retries,
        [MORSE_STATS_FMT_RAW] = print_raw,
        [MORSE_STATS_FMT_CALIBRATION] = print_calibration,
        [MORSE_STATS_FMT_DUTY_CYCLE] = print_duty_cycle,
        [MORSE_STATS_FMT_MAC_STATE] = print_mac_state,
        /* Add new function pointers here */
        /* [MORSE_STATS_NEW_TLV_FORMAT] = print_new_format */

        [MORSE_STATS_FMT_LAST] = print_default,
    }
};


const struct format_table* stats_format_regular_get_formatter_table()
{
    return &table;
}
