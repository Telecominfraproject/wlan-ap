/*
 * Copyright 2022 Morse Micro
 */


#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <stdint.h>
#include "portable_endian.h"

#include "command.h"
#include "utilities.h"

struct PACKED set_bcn_rssi_threshold_command
{
    /** The threshold in dB */
    uint8_t threshold_db;
};


static void usage(struct morsectrl *mors) {
    mctrl_print("\tbcn_rssi_threshold <value>\tselect in between '0-100'dB to set threshold\n");
}

int bcn_rssi_threshold(struct morsectrl *mors, int argc, char *argv[])
{
    int ret = -1;
    struct set_bcn_rssi_threshold_command *cmd;
    struct morsectrl_transport_buff *cmd_tbuff;
    struct morsectrl_transport_buff *rsp_tbuff;
    uint8_t threshold_db;

    if (argc == 0)
    {
        usage(mors);
        return 0;
    }

    if (argc != 2)
    {
        mctrl_err("Invalid command parameters\n");
        usage(mors);
        return -1;
    }

    cmd_tbuff = morsectrl_transport_cmd_alloc(mors->transport, sizeof(*cmd));
    rsp_tbuff = morsectrl_transport_resp_alloc(mors->transport, 0);

    if (!cmd_tbuff || !rsp_tbuff)
        goto exit;

    cmd = TBUFF_TO_CMD(cmd_tbuff, struct set_bcn_rssi_threshold_command);

    if (str_to_uint8_range(argv[1], &threshold_db, 0, 100))
    {
        mctrl_err("Invalid value [0 to 100]\n");
        usage(mors);
        ret = -1;
        goto exit;
    }

    cmd->threshold_db = threshold_db;
    ret = morsectrl_send_command(mors->transport, MORSE_COMMAND_SET_BCN_RSSI_THRESHOLD,
                                 cmd_tbuff, rsp_tbuff);

exit:
    if (ret)
    {
        mctrl_err("Failed to set beacon rssi change threshold\n");
    }
    else
    {
        mctrl_print("\tBeacon RSSI change  Threshold set to : %d dB\n", (cmd->threshold_db));
    }

    morsectrl_transport_buff_free(cmd_tbuff);
    morsectrl_transport_buff_free(rsp_tbuff);
    return ret;
}

MM_CLI_HANDLER(bcn_rssi_threshold, MM_INTF_REQUIRED, MM_DIRECT_CHIP_SUPPORTED);
