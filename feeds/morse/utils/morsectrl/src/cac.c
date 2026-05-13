/*
 * Copyright 2023 Morse Micro
 */
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <math.h>

#include "portable_endian.h"
#include "command.h"
#include "utilities.h"

enum cac_command {
    CAC_COMMAND_DISABLE = 0,
    CAC_COMMAND_ENABLE = 1
};

struct PACKED command_cac_req {
    /** CAC subcommand */
    uint8_t cmd;
};

static void usage(struct morsectrl *mors)
{
    mctrl_print(
        "\tcac [enable|disable]\tenable Centralized Authentication Control on a STA interface\n");
    mctrl_print("\t\t\t\tdo not use - for internal use by wpa_supplicant\n");
}

int cac(struct morsectrl *mors, int argc, char *argv[])
{
    int cmd;
    int ret = -1;
    struct command_cac_req *cac_req = NULL;
    struct morsectrl_transport_buff *cmd_tbuff = NULL;
    struct morsectrl_transport_buff *rsp_tbuff = NULL;

    if (argc == 0)
    {
        usage(mors);
        return 0;
    }

    cmd_tbuff = morsectrl_transport_cmd_alloc(mors->transport, sizeof(*cac_req));
    if (!cmd_tbuff)
    {
        goto exit;
    }

    rsp_tbuff = morsectrl_transport_resp_alloc(mors->transport, 0);
    if (!rsp_tbuff)
    {
        goto exit;
    }

    cac_req = TBUFF_TO_CMD(cmd_tbuff, struct command_cac_req);

    if (argc != 2)
    {
        mctrl_err("Invalid command parameters\n");
        usage(mors);
        ret = -1;
        goto exit;
    }

    cmd = expression_to_int(argv[1]);
    if (cmd < 0)
    {
        mctrl_err("Invalid CAC command '%s'\n", argv[1]);
        usage(mors);
        ret = -1;
        goto exit;
    }

    cac_req->cmd = cmd ? CAC_COMMAND_ENABLE : CAC_COMMAND_DISABLE;

    ret = morsectrl_send_command(mors->transport, MORSE_COMMAND_CAC_SET, cmd_tbuff,
            rsp_tbuff);

exit:
    if (cmd_tbuff)
    {
        morsectrl_transport_buff_free(cmd_tbuff);
    }

    if (rsp_tbuff)
    {
        morsectrl_transport_buff_free(rsp_tbuff);
    }

    return ret;
}

MM_CLI_HANDLER(cac, MM_INTF_REQUIRED, MM_DIRECT_CHIP_NOT_SUPPORTED);
