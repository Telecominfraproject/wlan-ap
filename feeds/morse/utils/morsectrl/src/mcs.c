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

struct PACKED set_mcs_command
{
    /** The flags of this message */
    uint32_t mcs;
};


static void usage(struct morsectrl *mors) {
    mctrl_print(
        "\tmcs <value>\t\tselects mcs mode(0~9) or type (auto) for auto rate control\n");
}

int mcs(struct morsectrl *mors, int argc, char *argv[])
{
    int ret = -1;
    uint32_t mcs;
    struct set_mcs_command *cmd;
    struct morsectrl_transport_buff *cmd_tbuff;
    struct morsectrl_transport_buff *rsp_tbuff;

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

    cmd = TBUFF_TO_CMD(cmd_tbuff, struct set_mcs_command);

    if (!strcmp(argv[1], "auto"))
    {
        mcs = 0x7FFFFFFF;
    }
    else
    {
        if (str_to_uint32_range(argv[1], &mcs, 0, 10))
        {
            mctrl_err("Invalid mcs value\n");
            usage(mors);
            ret = -1;
            goto exit;
        }
    }

    cmd->mcs = htole32(mcs);
    ret = morsectrl_send_command(mors->transport, MORSE_COMMAND_SET_MODULATION,
                                 cmd_tbuff, rsp_tbuff);

exit:
    if (ret < 0)
    {
        mctrl_err("Failed to set mcs\n");
    }

    morsectrl_transport_buff_free(cmd_tbuff);
    morsectrl_transport_buff_free(rsp_tbuff);
    return ret;
}

MM_CLI_HANDLER(mcs, MM_INTF_REQUIRED, MM_DIRECT_CHIP_SUPPORTED);
