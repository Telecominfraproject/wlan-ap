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

struct PACKED set_turbo_mode
{
    uint32_t aid;
    uint16_t vif_id;
    uint8_t enabled;
};


static void usage(struct morsectrl *mors)
{
    mctrl_print("\tturbo [enable|disable]\n");
    mctrl_print("\t\t\t\t'enable' will enable Morse Micro turbo mode\n");
    mctrl_print("\t\t\t\t'disable' will disable Morse Micro turbo mode\n");
}

int turbo(struct morsectrl *mors, int argc, char *argv[])
{
    int ret = -1;
    int enabled;
    struct set_turbo_mode *cmd;
    struct morsectrl_transport_buff *cmd_tbuff;
    struct morsectrl_transport_buff *rsp_tbuff;

    if (argc < 2)
    {
        usage(mors);
        return 0;
    }

    enabled = expression_to_int(argv[1]);

    if (enabled == -1)
    {
        mctrl_err("Invalid command parameters\n");
        usage(mors);
        return -1;
    }

    cmd_tbuff = morsectrl_transport_cmd_alloc(mors->transport, sizeof(*cmd));
    rsp_tbuff = morsectrl_transport_resp_alloc(mors->transport, 0);

    if (!cmd_tbuff || !rsp_tbuff)
        goto exit;

    cmd = TBUFF_TO_CMD(cmd_tbuff, struct set_turbo_mode);
    cmd->aid = 0;       /* Unused */
    cmd->vif_id = 0;    /* Unused */
    cmd->enabled = enabled;
    ret = morsectrl_send_command(mors->transport, MORSE_COMMAND_TURBO,
                                 cmd_tbuff, rsp_tbuff);

exit:
    if (ret)
    {
        mctrl_err("Failed to set turbo mode\n");
    }
    else
    {
        mctrl_print("\tTurbo Mode: %s\n",
            (cmd->enabled) ? "enabled" : "disabled");
    }

    morsectrl_transport_buff_free(cmd_tbuff);
    morsectrl_transport_buff_free(rsp_tbuff);
    return ret;
}

MM_CLI_HANDLER(turbo, MM_INTF_REQUIRED, MM_DIRECT_CHIP_SUPPORTED);
