/*
 * Copyright 2021 Morse Micro
 */

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <stdint.h>

#include "command.h"
#include "utilities.h"

struct PACKED set_control_response_command
{
    uint8_t direction;
    uint8_t control_response_bw_mhz;
};


static void usage(struct morsectrl *mors) {
    mctrl_print("\tcr <direction> <bandwidth>\n");
    mctrl_print("\t\t\t\tforces control response frames to use the specified bandwidth\n");
    mctrl_print("\t\t<direction>\tapply to outbound (0) or inbound(1)\n");
    mctrl_print("\t\t<bandwidth>\tbandwidth (MHz), or 0 to disable\n");
}

int cr(struct morsectrl *mors, int argc, char *argv[])
{
    int ret = -1;
    uint8_t bw_mhz;
    uint8_t direction;
    struct set_control_response_command *cmd;
    struct morsectrl_transport_buff *cmd_tbuff;
    struct morsectrl_transport_buff *rsp_tbuff;

    if (argc == 0)
    {
        usage(mors);
        return 0;
    }

    if (argc != 3)
    {
        mctrl_err("Invalid command parameters\n");
        usage(mors);
        return -1;
    }

    cmd_tbuff = morsectrl_transport_cmd_alloc(mors->transport, sizeof(*cmd));
    rsp_tbuff = morsectrl_transport_resp_alloc(mors->transport, 0);

    if (!cmd_tbuff || !rsp_tbuff)
        goto exit;

    cmd = TBUFF_TO_CMD(cmd_tbuff, struct set_control_response_command);

    if (str_to_uint8_range(argv[1], &direction, 0, 1) < 0)
    {
        mctrl_err("Invalid direction\n");
        usage(mors);
        ret = -1;
        goto exit;
    }

    if (str_to_uint8_range(argv[2], &bw_mhz, 0, 16) < 0)
    {
        mctrl_err("Invalid bandwidth\n");
        usage(mors);
        ret = -1;
        goto exit;
    }

    switch (bw_mhz)
    {
        case 0:
        case 1:
        case 2:
        case 4:
        case 8:
        case 16:
            break;
        default:
            mctrl_err("Invalid value\n");
            usage(mors);
            ret = -1;
            goto exit;
    }

    cmd->control_response_bw_mhz = bw_mhz;
    cmd->direction = direction;
    ret = morsectrl_send_command(mors->transport, MORSE_COMMAND_SET_CONTROL_RESPONSE,
                                 cmd_tbuff, rsp_tbuff);

exit:
    if (ret < 0)
    {
        mctrl_err("Failed to set cr\n");
    }

    morsectrl_transport_buff_free(cmd_tbuff);
    morsectrl_transport_buff_free(rsp_tbuff);
    return ret;
}

MM_CLI_HANDLER(cr, MM_INTF_REQUIRED, MM_DIRECT_CHIP_SUPPORTED);
