/*
 * Copyright 2020 Morse Micro
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

#define BANDWIDTH_DEFAULT 0xFF
#define PRIMARY_1MHZ_CHANNEL_INDEX_DEFAULT 0xFF

static void usage(struct morsectrl *mors) {
    mctrl_print("\tbw <value>\t\tsets bandwidth(1|2|4|8)\n");
    mctrl_print("\t\t\t\tor reads current bandwidth if no arguments were given\n");
}

int bw(struct morsectrl *mors, int argc, char *argv[])
{
    int ret = -1;
    uint8_t bw;
    struct command_set_channel_req *cmd;
    struct command_get_channel_cfm *resp;
    struct morsectrl_transport_buff *cmd_tbuff;
    struct morsectrl_transport_buff *rsp_tbuff;

    if (argc == 0)
    {
        usage(mors);
        return 0;
    }

    cmd_tbuff = morsectrl_transport_cmd_alloc(mors->transport, sizeof(*cmd));
    rsp_tbuff = morsectrl_transport_resp_alloc(mors->transport, sizeof(*resp));

    if (!cmd_tbuff || !rsp_tbuff)
        goto exit;

    cmd = TBUFF_TO_CMD(cmd_tbuff, struct command_set_channel_req);
    resp = TBUFF_TO_RSP(rsp_tbuff, struct  command_get_channel_cfm);

    ret = morsectrl_send_command(mors->transport, MORSE_COMMAND_GET_FULL_CHANNEL,
                                 cmd_tbuff, rsp_tbuff);
    if (ret < 0)
    {
        ret = -1;
        goto exit;
    }

    if (argc == 1)
    {
        mctrl_print("Current bw is (%d) \n", resp->operating_channel_bw_mhz);
    }
    else
    {
        if (argc != 2)
        {
            mctrl_err("Invalid command parameters\n");
            usage(mors);
            ret = -1;
            goto exit;
        }

        if (str_to_uint8(argv[1], &bw) ||
                !((bw == 1) || (bw == 2) || (bw == 4) || (bw == 8) || (bw == 16)))
        {
            mctrl_err("Invalid bandwidth.\n");
            usage(mors);
            ret = -1;
            goto exit;
        }

        cmd->operating_channel_freq_hz = resp->operating_channel_freq_hz;
        cmd->operating_channel_bw_mhz = bw;
        cmd->primary_channel_bw_mhz = BANDWIDTH_DEFAULT;
        cmd->primary_1mhz_channel_index = PRIMARY_1MHZ_CHANNEL_INDEX_DEFAULT;
        cmd->dot11_mode = 0; /* TODO */

        ret = morsectrl_send_command(mors->transport, MORSE_COMMAND_SET_CHANNEL,
                                     cmd_tbuff, rsp_tbuff);
    }

exit:
    if (ret < 0)
    {
        mctrl_err("Failed to set bw: error(%d)\n", ret);
    }
    morsectrl_transport_buff_free(cmd_tbuff);
    morsectrl_transport_buff_free(rsp_tbuff);
    return ret;
}

MM_CLI_HANDLER(bw, MM_INTF_REQUIRED, MM_DIRECT_CHIP_SUPPORTED);
