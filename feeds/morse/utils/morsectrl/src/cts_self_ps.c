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

struct PACKED set_cts_self_ps_command
{
    /** The flag of this message */
    uint8_t enable;
};


static void usage(struct morsectrl *mors) {
    mctrl_print("\tcts_self_ps <value>\t\t'disable' to disable and 'enable' to enable\n");
}

int cts_self_ps(struct morsectrl *mors, int argc, char *argv[])
{
    int ret = -1;
    int enable;
    struct set_cts_self_ps_command *cmd;
    struct morsectrl_transport_buff *cmd_tbuff;
    struct morsectrl_transport_buff *rsp_tbuff;

    if (argc == 0)
    {
        usage(mors);
        return 0;
    }

    if (argc != 2)
    {
        mctrl_err("Invalid Command Parameters\n");
        usage(mors);
        return -1;
    }

    enable = expression_to_int(argv[1]);

    if (enable == -1)
    {
        mctrl_err("Invalid value.\n");
        usage(mors);
        return -1;
    }

    cmd_tbuff = morsectrl_transport_cmd_alloc(mors->transport, sizeof(*cmd));
    rsp_tbuff = morsectrl_transport_resp_alloc(mors->transport, 0);

    if (!cmd_tbuff || !rsp_tbuff)
        goto exit;

    cmd = TBUFF_TO_CMD(cmd_tbuff, struct set_cts_self_ps_command);
    cmd->enable = enable;
    ret = morsectrl_send_command(mors->transport, MORSE_COMMAND_SET_CTS_SELF_PS,
                                 cmd_tbuff, rsp_tbuff);

exit:
    if (ret)
    {
        mctrl_err("Failed to set CTS-to-self PS\n");
    }
    else
    {
        mctrl_print("\tCTS-to-Self PowerSave: %s\n",
            (cmd->enable) ? "enabled" : "disabled");
    }

    morsectrl_transport_buff_free(cmd_tbuff);
    morsectrl_transport_buff_free(rsp_tbuff);
    return ret;
}

MM_CLI_HANDLER(cts_self_ps, MM_INTF_REQUIRED, MM_DIRECT_CHIP_SUPPORTED);
