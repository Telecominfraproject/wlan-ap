/*
 * Copyright 2022 Morse Micro
 */

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <stdint.h>
#include "command.h"
#include "utilities.h"

struct PACKED command_tdc_pg_disable
{
    /* 0 sets LNA as default, 1 sets LNA in bypass mode */
    uint8_t tdc_pg_disable;
};

static void usage(struct morsectrl *mors) {
    mctrl_print("\ttdc_pg_disable [0|1]\n");
    mctrl_print("\t\t\t\t'1' will disable TDC clock gating\n");
    mctrl_print("\t\t\t\t'0' will keep the default configuration\n");
}

int tdc_pg_disable(struct morsectrl *mors, int argc, char *argv[])
{
    int ret = -1;
    int8_t tdc_pg_disable;
    struct command_tdc_pg_disable *cmd;
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


    tdc_pg_disable = expression_to_int(argv[1]);
    if (tdc_pg_disable == -1)
    {
        mctrl_err("Invalid command parameters\n");
        usage(mors);
        return -1;
    }

    cmd_tbuff = morsectrl_transport_cmd_alloc(mors->transport, sizeof(*cmd));
    rsp_tbuff = morsectrl_transport_resp_alloc(mors->transport, 0);

    if (!cmd_tbuff || !rsp_tbuff)
        goto exit;

    cmd = TBUFF_TO_CMD(cmd_tbuff, struct command_tdc_pg_disable);
    cmd->tdc_pg_disable = tdc_pg_disable;

    ret = morsectrl_send_command(mors->transport, MORSE_TEST_COMMAND_TDC_PG_DISABLE,
                                 cmd_tbuff, rsp_tbuff);

exit:
    if (ret < 0)
    {
        mctrl_err("failed to change the status of energy-based AGC \n");
    }

    morsectrl_transport_buff_free(cmd_tbuff);
    morsectrl_transport_buff_free(rsp_tbuff);
    return ret;
}

MM_CLI_HANDLER(tdc_pg_disable, MM_INTF_REQUIRED, MM_DIRECT_CHIP_SUPPORTED);
