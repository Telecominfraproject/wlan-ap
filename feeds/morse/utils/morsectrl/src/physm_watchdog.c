/*
 * Copyright 2023 Morse Micro
 */

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <stdint.h>
#include "command.h"

struct PACKED command_set_physm_watchdog
{
    /* physm watchdog */
    uint8_t physm_watchdog_en;
};

static void usage(struct morsectrl *mors) {
    mctrl_print("\tphysm_watchdog_en [enable|disable]\n");
    mctrl_print("\t\t\t\t'enable' will enable PHYSM watchdog with a timeout of 60ms\n");
    mctrl_print("\t\t\t\t'disable' will disable PHYPRM watchdog.\n");
}

int physm_watchdog_en(struct morsectrl *mors, int argc, char *argv[])
{
    int ret  = -1;
    uint8_t physm_watchdog_en;
    struct command_set_physm_watchdog *cmd;
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

    physm_watchdog_en = expression_to_int(argv[1]);
    if ((physm_watchdog_en != 0) && (physm_watchdog_en != 1))
    {
        mctrl_err("Valid values are 0 and 1\n");
        usage(mors);
        return -1;
    }

    cmd_tbuff = morsectrl_transport_cmd_alloc(mors->transport, sizeof(*cmd));
    rsp_tbuff = morsectrl_transport_resp_alloc(mors->transport, 0);

    if (!cmd_tbuff || !rsp_tbuff)
        goto exit;

    cmd = TBUFF_TO_CMD(cmd_tbuff, struct command_set_physm_watchdog);
    cmd->physm_watchdog_en = physm_watchdog_en;
    ret = morsectrl_send_command(mors->transport, MORSE_COMMAND_SET_PHYSM_WATCHDOG,
                                 cmd_tbuff, rsp_tbuff);
exit:
    if (ret < 0)
    {
        mctrl_err("Failed to set physm watchdog\n");
    }
    else
    {
        mctrl_print("\tPHYSM watchdog: %s\n",
            (cmd->physm_watchdog_en) ? "enabled" : "disabled");
    }

    morsectrl_transport_buff_free(cmd_tbuff);
    morsectrl_transport_buff_free(rsp_tbuff);
    return ret;
}

MM_CLI_HANDLER(physm_watchdog_en, MM_INTF_REQUIRED, MM_DIRECT_CHIP_SUPPORTED);
