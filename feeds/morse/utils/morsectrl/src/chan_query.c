/*
 * Copyright 2021 Morse Micro
 */

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <stdint.h>
#include "portable_endian.h"

#include "command.h"
#include "channel.h"
#include "utilities.h"

#define MAX_AVAIL_CHANNELS      (UINT8_MAX)

struct PACKED command_get_available_channels_cfm
{
    uint32_t num_channels;
    struct PACKED {
        uint32_t frequency_khz;
        uint8_t channel_5g;
        uint8_t channel_s1g;
        uint8_t bandwidth_mhz;
    } channels[MAX_AVAIL_CHANNELS];
};

static void usage(void)
{
    mctrl_print("\tchan_query [options]\n"
           "\t\t\t\treturns a list of available channels\n"
           "\t\t-j \t\tprint available channels in JSON format\n");
}

int chan_query(struct morsectrl *mors, int argc, char *argv[])
{
    int ret = -1;
    struct command_get_available_channels_cfm *query_cfm;
    struct morsectrl_transport_buff *cmd_tbuff;
    struct morsectrl_transport_buff *rsp_tbuff;
    bool json = false;

    if (argc == 0)
    {
        usage();
        return 0;
    }

    cmd_tbuff = morsectrl_transport_cmd_alloc(mors->transport, 0);
    rsp_tbuff = morsectrl_transport_resp_alloc(mors->transport, sizeof(*query_cfm));

    if (!cmd_tbuff || !rsp_tbuff)
    {
        goto err;
    }

    query_cfm = TBUFF_TO_RSP(rsp_tbuff, struct command_get_available_channels_cfm);

    if (argc < 3)
    {
        int option;
        while ((option = getopt(argc, argv, "j")) != -1)
        {
            switch (option)
            {
                case 'j' :
                    json = true;
                    break;
                case '?' :
                    usage();
                    goto err;
                default :
                    mctrl_err("Invalid argument\n");
                    usage();
                    goto err;
            }
        }
    }
    else
    {
        mctrl_err("Invalid argument\n");
        usage();
        goto err;
    }

    ret = morsectrl_send_command(mors->transport, MORSE_COMMAND_GET_AVAILABLE_CHANNELS,
            cmd_tbuff, rsp_tbuff);

    if (ret < 0)
    {
        mctrl_err("Failed to query available channels");
        goto err;
    }

    if (json)
    {
        mctrl_print("[");

        for (int i = 0; i < query_cfm->num_channels; i++)
        {
            mctrl_print("%s{\"s1g_channel\":%u,"
                    "\"center_frequency_khz\": %u,"
                    "\"bandwidth_mhz\": %u,"
                    "\"5g_channel\": %u}",
                    i == 0 ? "" : ",",
                    query_cfm->channels[i].channel_s1g, query_cfm->channels[i].frequency_khz,
                    query_cfm->channels[i].bandwidth_mhz, query_cfm->channels[i].channel_5g);
        }

        mctrl_print("]\n");
    }
    else
    {
        mctrl_print("Channel  Center Freq (kHz)  BW (MHz)  5g Mapped Channel\n");

        for (int i = 0; i < query_cfm->num_channels; i++)
        {
            mctrl_print("%7u  %17u  %8u  %17u\n",
                    query_cfm->channels[i].channel_s1g, query_cfm->channels[i].frequency_khz,
                    query_cfm->channels[i].bandwidth_mhz, query_cfm->channels[i].channel_5g);
        }
    }


err:
    morsectrl_transport_buff_free(cmd_tbuff);
    morsectrl_transport_buff_free(rsp_tbuff);

    return ret;
}

MM_CLI_HANDLER(chan_query, MM_INTF_REQUIRED, MM_DIRECT_CHIP_NOT_SUPPORTED);
