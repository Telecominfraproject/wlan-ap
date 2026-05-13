/*
 * Copyright 2022 Morse Micro
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "command.h"
#include "utilities.h"

struct PACKED set_long_sleep_config_command
{
    uint8_t long_sleep_enabled;
};


static void usage(struct morsectrl *mors)
{
    mctrl_print("\tlong_sleep [enable|disable]\n");
    mctrl_print("\t\t\t\t'enable' will enable long sleep (allow sleeping through DTIM)\n");
    mctrl_print("\t\t\t\t'disable' will disable long sleep\n");
}


int long_sleep(struct morsectrl *mors, int argc, char *argv[])
{
    int ret = -1;
    int enabled;
    struct set_long_sleep_config_command *cmd;
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

    cmd = TBUFF_TO_CMD(cmd_tbuff, struct set_long_sleep_config_command);
    cmd->long_sleep_enabled = enabled;

    ret = morsectrl_send_command(mors->transport, MORSE_COMMAND_SET_LONG_SLEEP_CONFIG,
                                 cmd_tbuff, rsp_tbuff);

exit:
    if (ret)
    {
        mctrl_err("Failed to set long sleep mode\n");
    }
    else
    {
        mctrl_print("\tLong Sleep Mode: %s\n",
            (cmd->long_sleep_enabled) ? "enabled" : "disabled");
    }

    morsectrl_transport_buff_free(cmd_tbuff);
    morsectrl_transport_buff_free(rsp_tbuff);
    return ret;
}

MM_CLI_HANDLER(long_sleep, MM_INTF_REQUIRED, MM_DIRECT_CHIP_SUPPORTED);
