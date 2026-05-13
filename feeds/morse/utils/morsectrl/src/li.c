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

#define UNSCALED_INTERVAL_MAX               ((2 << 14) - 1)

struct PACKED set_li_command
{
    /** The flags of this message */
    uint16_t li;
};


static void usage(struct morsectrl *mors) {
    mctrl_print("\tli <unscaled_int> <scale_idx>\t\tsets listen interval.\n");
    mctrl_print("\t\t\t\tScale index: 0=1, 1=10, 2=1000, 3=10000\n");
    mctrl_print("\t\t\t\tIf the node is an AP this set the max listen interval\n");
}

int li(struct morsectrl *mors, int argc, char *argv[])
{
    int ret = -1;
    uint32_t unscaled_interval;
    uint8_t scale_idx;
    struct set_li_command *cmd;
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

    cmd = TBUFF_TO_CMD(cmd_tbuff, struct set_li_command);

    if (str_to_uint32_range(argv[1], &unscaled_interval, 0, UNSCALED_INTERVAL_MAX))
    {
        mctrl_err("Invalid unscaled interval\n");
        usage(mors);
        ret = -1;
        goto exit;
    }

    if (str_to_uint8(argv[2], &scale_idx))
    {
        mctrl_err("Invalid scale index\n");
        usage(mors);
        ret = -1;
        goto exit;
    }

    /* Max is same as mask */
    cmd->li = htole32((unscaled_interval & UNSCALED_INTERVAL_MAX) | scale_idx << 14);
    ret = morsectrl_send_command(mors->transport, MORSE_COMMAND_SET_LISTEN_INTERVAL,
                                 cmd_tbuff, rsp_tbuff);

exit:
    if (ret < 0)
    {
        mctrl_err("Failed to set li\n");
    }

    morsectrl_transport_buff_free(cmd_tbuff);
    morsectrl_transport_buff_free(rsp_tbuff);
    return ret;
}

MM_CLI_HANDLER(li, MM_INTF_REQUIRED, MM_DIRECT_CHIP_NOT_SUPPORTED);
