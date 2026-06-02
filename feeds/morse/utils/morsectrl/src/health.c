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

static void usage(struct morsectrl *mors) {
    mctrl_print("\thealth\tchecks the health status of the cores\n");
}

int health(struct morsectrl *mors, int argc, char *argv[])
{
    int ret = -1;
    struct morsectrl_transport_buff *cmd_tbuff;
    struct morsectrl_transport_buff *rsp_tbuff;

    if (argc == 0)
    {
        usage(mors);
        return 0;
    }

    cmd_tbuff = morsectrl_transport_cmd_alloc(mors->transport, 0);
    rsp_tbuff = morsectrl_transport_resp_alloc(mors->transport, 0);

    if (!cmd_tbuff || !rsp_tbuff)
        goto exit;

    if (argc != 1)
    {
        mctrl_err("Invalid command parameters\n");
        usage(mors);
        ret = -1;
        goto exit;
    }

    ret = morsectrl_send_command(mors->transport, MORSE_COMMAND_HEALTH_CHECK,
                                 cmd_tbuff, rsp_tbuff);

exit:
    if (ret < 0)
        mctrl_err("health check: failed\n");
    else
        mctrl_print("health check: success\n");

    morsectrl_transport_buff_free(cmd_tbuff);
    morsectrl_transport_buff_free(rsp_tbuff);
    return ret;
}

MM_CLI_HANDLER(health, MM_INTF_REQUIRED, MM_DIRECT_CHIP_SUPPORTED);
