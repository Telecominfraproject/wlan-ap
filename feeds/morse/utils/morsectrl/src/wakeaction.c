/*
 * Copyright 2022 Morse Micro
 */
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <stdint.h>

#include "command.h"
#include "utilities.h"

struct PACKED command_send_wake_action_frame_req
{
    uint8_t dest_addr[6];
    uint32_t payload_size;
    uint8_t payload[0];
};


static void usage(struct morsectrl *mors) {
    mctrl_print("\twakeaction <destination MAC address> <hex string payload>\n");
    mctrl_print("\t\t\t\tsends a wake action frame with the given payload to a destination\n");
    mctrl_print("\t\t\t\tmac address.\n");
}

int wakeaction(struct morsectrl *mors, int argc, char *argv[])
{
    int ret = -1;
    struct command_send_wake_action_frame_req* cmd = NULL;
    struct morsectrl_transport_buff *cmd_tbuff = NULL;
    struct morsectrl_transport_buff *rsp_tbuff = NULL;
    int payload_size = 0;

    if (argc == 0)
    {
        usage(mors);
        return 0;
    }

    if (argc != 3)
    {
        usage(mors);
        return -1;
    }

    payload_size = strlen(argv[2]);
    if ((payload_size % 2) != 0)
    {
        mctrl_err("Invalid hex string, length must be a multiple of 2\n");
        usage(mors);
        return -1;
    }

    payload_size = (payload_size / 2);
    cmd_tbuff = morsectrl_transport_cmd_alloc(mors->transport, sizeof(*cmd) + payload_size);
    rsp_tbuff = morsectrl_transport_resp_alloc(mors->transport, 0);

    if (!cmd_tbuff || !rsp_tbuff)
        goto exit;

    cmd = TBUFF_TO_CMD(cmd_tbuff, struct command_send_wake_action_frame_req);
    if (!cmd)
        goto exit;

    cmd->payload_size = payload_size;

    if (hexstr2bin(argv[2], cmd->payload, payload_size) < 0)
    {
        mctrl_err("Invalid hex string\n");
        usage(mors);
        goto exit;
    }

    if (str_to_mac_addr(cmd->dest_addr, argv[1]) < 0)
    {
        mctrl_err("Invalid MAC address - must be in the format aa:bb:cc:dd:ee:ff\n");
        usage(mors);
        goto exit;
    }

    ret = morsectrl_send_command(mors->transport,
        MORSE_COMMAND_SEND_WAKE_ACTION_FRAME, cmd_tbuff, rsp_tbuff);

exit:
    if (ret)
    {
        mctrl_err("Failed to send wake action frame\n");
    }
    else
    {
        mctrl_print("Wake action frame scheduled for transmission\n");
    }

    if (cmd_tbuff)
        morsectrl_transport_buff_free(cmd_tbuff);

    if (rsp_tbuff)
        morsectrl_transport_buff_free(rsp_tbuff);

    return ret;
}

MM_CLI_HANDLER(wakeaction, MM_INTF_REQUIRED, MM_DIRECT_CHIP_NOT_SUPPORTED);
