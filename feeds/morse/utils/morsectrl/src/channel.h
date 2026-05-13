/*
 * Copyright 2020 Morse Micro
 */

#pragma once

#define BANDWIDTH_DEFAULT                   0xFF
#define FREQUENCY_DEFAULT                   0xFFFFFFFF
#define PRIMARY_1MHZ_CHANNEL_INDEX_DEFAULT  0xFF
#define MIN_FREQ_KHZ                        750000
#define MAX_FREQ_KHZ                        950000

#define KHZ_TO_HZ(x) (x * 1000)

/** Set channel command, used in channel and bandwidth commands */
struct PACKED command_set_channel_req
{
    /** Centre frequency of the operating channel */
    uint32_t operating_channel_freq_hz;

    /** Operating channel bandwidth in MHz */
    uint8_t operating_channel_bw_mhz;

    /** Primary channel bandwidth in MHz */
    uint8_t primary_channel_bw_mhz;

    /** 1MHz channel index (see morse_firmware host_api.h for explanation) */
    uint8_t primary_1mhz_channel_index;

    /** dot11 protocol mode */
    uint8_t dot11_mode;

    /** Set to 1 to apply S1G regulatory max power, 0 otherwise */
    uint8_t s1g_chan_power;
};

struct PACKED command_get_channel_cfm
{
    /** Centre frequency of the operating channel */
    uint32_t operating_channel_freq_hz;

    /** Operating channel bandwidth in MHz */
    uint8_t operating_channel_bw_mhz;

    /** Primary channel bandwidth in MHz */
    uint8_t primary_channel_bw_mhz;

    /**
     * The index of the 1MHz channel within the operating channel.
     * This is a value 0 for 1MHz channel, 0-1 for 2MHz,
     * 0-3 for 4MHz, 0-7 for 8MHz and 0-15 for 16MHz.
     */
    uint8_t primary_1mhz_channel_index;
};
