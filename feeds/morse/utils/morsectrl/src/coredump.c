/*
 * Copyright 2022 Morse Micro
 */

#include <stdio.h>

#include "command.h"

static void usage(struct morsectrl *mors)
{
    mctrl_print("\tcoredump\t\tgenerates a FW coredump through the driver\n"
           "\t\t\t\twith pattern /var/log/mmcd_hostname_ip_date/\n");
}

int coredump(struct morsectrl *mors, int argc, char *argv[])
{
    int ret = -1;
    struct morsectrl_transport_buff *cmd_tbuff;
    struct morsectrl_transport_buff *rsp_tbuff;

    if (argc == 0)
    {
        usage(mors);
        return 0;
    }

    if (argc != 1)
    {
        mctrl_err("Invalid command parameters\n");
        usage(mors);
        return -1;
    }

    cmd_tbuff = morsectrl_transport_cmd_alloc(mors->transport, 0);
    rsp_tbuff = morsectrl_transport_resp_alloc(mors->transport, 0);

    if (!cmd_tbuff || !rsp_tbuff)
        goto exit;

    ret = morsectrl_send_command(mors->transport, MORSE_COMMAND_COREDUMP,
                                 cmd_tbuff, rsp_tbuff);
exit:
    if (ret < 0)
    {
        mctrl_err("Command coredump error (%d)\n", ret);
    }

    morsectrl_transport_buff_free(cmd_tbuff);
    morsectrl_transport_buff_free(rsp_tbuff);
    return ret;
}

MM_CLI_HANDLER(coredump, MM_INTF_REQUIRED, MM_DIRECT_CHIP_NOT_SUPPORTED);
