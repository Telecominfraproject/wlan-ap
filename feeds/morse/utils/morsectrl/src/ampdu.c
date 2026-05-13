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
#include "utilities.h"


struct PACKED set_ampdu_command
{
    uint8_t ampdu_enabled;
};

static void usage(struct morsectrl *mors)
{
    mctrl_print("\tampdu [enable|disable]\n");
    mctrl_print("\t\t\t\t'enable' will enable AMPDU sessions. Must be run before association.\n");
    mctrl_print("\t\t\t\t'disable' will disable AMPDU sessions. Must be run before association.\n");
}

int ampdu(struct morsectrl *mors, int argc, char *argv[])
{
    int ret = -1;
    int enabled;
    struct morsectrl_transport_buff *cmd_tbuff;
    struct morsectrl_transport_buff *rsp_tbuff;
    struct set_ampdu_command *cmd;

    if (argc < 2)
    {
        usage(mors);
        return 0;
    }

    cmd_tbuff = morsectrl_transport_cmd_alloc(mors->transport, sizeof(*cmd));
    rsp_tbuff = morsectrl_transport_resp_alloc(mors->transport, 0);

    if (!cmd_tbuff || !rsp_tbuff)
        goto exit;

    cmd = TBUFF_TO_CMD(cmd_tbuff, struct set_ampdu_command);
    cmd->ampdu_enabled = 0;

    enabled = expression_to_int(argv[1]);

    if (enabled == -1)
    {
        mctrl_err("Invalid command parameters\n");
        usage(mors);
        ret = -1;
        goto exit;
    }

    cmd->ampdu_enabled = enabled;
    ret = morsectrl_send_command(mors->transport, MORSE_COMMAND_SET_AMPDU,
                                 cmd_tbuff, rsp_tbuff);
exit:
    if (ret)
    {
        mctrl_err("Failed to set AMPDU mode\n");
    }
    else
    {
        mctrl_print("\tAMPDU Mode: %s\n",
            (cmd->ampdu_enabled) ? "enabled" : "disabled");
    }

    morsectrl_transport_buff_free(cmd_tbuff);
    morsectrl_transport_buff_free(rsp_tbuff);
    return ret;
}

MM_CLI_HANDLER(ampdu, MM_INTF_REQUIRED, MM_DIRECT_CHIP_NOT_SUPPORTED);
