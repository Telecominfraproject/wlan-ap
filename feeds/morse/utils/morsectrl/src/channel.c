/*
 * Copyright 2020 Morse Micro
 */

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

#include "portable_endian.h"
#include "command.h"
#include "channel.h"
#include "transport/transport.h"

static struct
{
    struct arg_lit *all_channels;
    struct arg_int *frequency;
    struct arg_int *operating_bandwidth;
    struct arg_int *primary_bandwidth;
    struct arg_int *primary_idx;
    struct arg_lit *ignore_reg_power;
    struct arg_lit *json_format;
} args;

int channel_init(struct morsectrl *mors, struct mm_argtable *mm_args)
{
    MM_INIT_ARGTABLE(mm_args, "Set or get channel parameters",
                     args.all_channels = arg_lit0("a", NULL, "prints all the channel "
                                                  "information i.e. full, DTIM, and current"),
                     args.frequency = arg_int0("c", NULL, "<freq>",
                                               "channel frequency in kHz"),
                     args.operating_bandwidth = arg_int0("o", NULL, "<operating BW>",
                                                         "operating bandwidth in MHz"),
                     args.primary_bandwidth = arg_int0("p", NULL, "<primary BW>",
                                                       "primary bandwidth in MHz"),
                     args.primary_idx = arg_int0("n", NULL, "<primary chan index>",
                                                 "primary 1 MHz channel index"),
                     #ifndef MORSE_CLIENT
                     args.ignore_reg_power = arg_lit0("r", NULL, "ignores regulatory max tx power"),
                     #endif
                     args.json_format = arg_lit0("j", NULL,
                                                 "prints full channel information in easily "
                                                 "parsable JSON format"));
    return 0;
}

int channel(struct morsectrl *mors, int argc, char *argv[])
{
    int ret = -1;
    uint32_t freq_khz = 0;
    uint8_t op_channel_bandwidth = BANDWIDTH_DEFAULT;
    uint8_t primary_channel_bandwidth = BANDWIDTH_DEFAULT;
    uint8_t primary_1mhz_channel_index = PRIMARY_1MHZ_CHANNEL_INDEX_DEFAULT;
    struct command_set_channel_req *cmd_set;
    struct command_get_channel_cfm *resp_get;
    bool set_freq = false;
    bool get_all_channels = false;
    bool json = false;
    uint8_t s1g_chan_power = 1;
    struct morsectrl_transport_buff *cmd_set_tbuff;
    struct morsectrl_transport_buff *rsp_set_tbuff;
    struct morsectrl_transport_buff *cmd_get_tbuff;
    struct morsectrl_transport_buff *rsp_get_tbuff;

    cmd_set_tbuff = morsectrl_transport_cmd_alloc(mors->transport, sizeof(*cmd_set));
    rsp_set_tbuff = morsectrl_transport_resp_alloc(mors->transport, 0);
    cmd_get_tbuff = morsectrl_transport_cmd_alloc(mors->transport, 0);
    rsp_get_tbuff = morsectrl_transport_resp_alloc(mors->transport, sizeof(*resp_get));

    if (!cmd_set_tbuff || !rsp_set_tbuff || !cmd_get_tbuff || !rsp_get_tbuff)
        goto exit;

    cmd_set = TBUFF_TO_CMD(cmd_set_tbuff, struct command_set_channel_req);
    resp_get = TBUFF_TO_RSP(rsp_get_tbuff, struct command_get_channel_cfm);

    if (args.frequency->count)
    {
        freq_khz = args.frequency->ival[0];
        set_freq = true;
    }

    if (args.operating_bandwidth->count)
    {
        op_channel_bandwidth = args.operating_bandwidth->ival[0];
        set_freq = true;
    }

    if (args.primary_bandwidth->count)
    {
        primary_channel_bandwidth = args.primary_bandwidth->ival[0];
        set_freq = true;
    }

    if (args.primary_idx->count)
    {
        primary_1mhz_channel_index = args.primary_idx->ival[0];
        set_freq = true;
    }

    json = (args.json_format->count > 0);

    get_all_channels = (args.all_channels->count > 0);

#ifndef MORSE_CLIENT
    s1g_chan_power = (args.ignore_reg_power > 0);
#endif

    if (set_freq)
    {
        if (!freq_khz)
        {
            mctrl_err("Channel frequency [-c] option must be specified\n");
            goto exit;
        }

        cmd_set->operating_channel_freq_hz = htole32(KHZ_TO_HZ(freq_khz));
        cmd_set->operating_channel_bw_mhz = op_channel_bandwidth;
        cmd_set->primary_channel_bw_mhz = primary_channel_bandwidth;
        cmd_set->primary_1mhz_channel_index = primary_1mhz_channel_index;
        cmd_set->dot11_mode = 0; /* TODO */
        cmd_set->s1g_chan_power = s1g_chan_power;

        ret = morsectrl_send_command(mors->transport, MORSE_COMMAND_SET_CHANNEL,
                                     cmd_set_tbuff, rsp_set_tbuff);

        if (ret < 0)
        {
            mctrl_err("Failed to set channel: error(%d)\n", ret);
            goto exit;
        }
    }

    ret = morsectrl_send_command(mors->transport, MORSE_COMMAND_GET_FULL_CHANNEL,
                                 cmd_get_tbuff, rsp_get_tbuff);

    if (ret < 0)
    {
        mctrl_err("Failed to get channel frequency: error(%d)\n", ret);
        goto exit;
    }
    if (json)
    {
        mctrl_print("{\n" \
               "    \"channel_frequency\":%d,\n" \
               "    \"channel_op_bw\":%d,\n" \
               "    \"channel_primary_bw\":%d,\n" \
               "    \"channel_index\":%d,\n" \
               "    \"bw_mhz\":%d\n" \
               "}\n",
               (resp_get->operating_channel_freq_hz / 1000),
               resp_get->operating_channel_bw_mhz,
               resp_get->primary_channel_bw_mhz,
               resp_get->primary_1mhz_channel_index,
               resp_get->operating_channel_bw_mhz);
    }
    else
    {
        mctrl_print("Full Channel Information\n" \
               "\tOperating Frequency: %d kHz\n" \
               "\tOperating BW: %d MHz\n" \
               "\tPrimary BW: %d MHz\n" \
               "\tPrimary Channel Index: %d\n",
               (resp_get->operating_channel_freq_hz / 1000),
               resp_get->operating_channel_bw_mhz,
               resp_get->primary_channel_bw_mhz,
               resp_get->primary_1mhz_channel_index);
    }

    if (get_all_channels)
    {
        ret = morsectrl_send_command(mors->transport, MORSE_COMMAND_GET_DTIM_CHANNEL,
                                     cmd_get_tbuff, rsp_get_tbuff);
        if (ret < 0)
        {
            mctrl_err("Failed to get channel frequency: error(%d)\n", ret);
            goto exit;
        }

        mctrl_print("DTIM Channel Information\n" \
               "\tOperating Frequency: %d kHz\n" \
               "\tOperating BW: %d MHz\n" \
               "\tPrimary BW: %d MHz\n" \
               "\tPrimary Channel Index: %d\n",
               (resp_get->operating_channel_freq_hz / 1000),
               resp_get->operating_channel_bw_mhz,
               resp_get->primary_channel_bw_mhz,
               resp_get->primary_1mhz_channel_index);

        ret = morsectrl_send_command(mors->transport, MORSE_COMMAND_GET_CURRENT_CHANNEL,
                                     cmd_get_tbuff, rsp_get_tbuff);
        if (ret < 0)
        {
            mctrl_err("Failed to get channel frequency: error(%d)\n", ret);
            goto exit;
        }

        mctrl_print("Current Channel Information\n" \
               "\tOperating Frequency: %d kHz\n" \
               "\tOperating BW: %d MHz\n" \
               "\tPrimary BW: %d MHz\n" \
               "\tPrimary Channel Index: %d\n",
               (resp_get->operating_channel_freq_hz / 1000),
               resp_get->operating_channel_bw_mhz,
               resp_get->primary_channel_bw_mhz,
               resp_get->primary_1mhz_channel_index);
    }

exit:
    morsectrl_transport_buff_free(cmd_set_tbuff);
    morsectrl_transport_buff_free(rsp_set_tbuff);
    morsectrl_transport_buff_free(cmd_get_tbuff);
    morsectrl_transport_buff_free(rsp_get_tbuff);
    return ret;
}

MM_CLI_HANDLER(channel, MM_INTF_REQUIRED, MM_DIRECT_CHIP_SUPPORTED);
