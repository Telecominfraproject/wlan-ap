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

struct PACKED set_txop_command
{
    /** The flags of this message */
    uint8_t txop;
};

static void usage(struct morsectrl *mors) {
    mctrl_print("\ttxop [0-9]\t\tminimum packets to start TXOP (0 for disable)\n");
}

int txop(struct morsectrl *mors, int argc, char *argv[])
{
    int ret = -1;
    uint32_t txop;
    struct set_txop_command *cmd;
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

    cmd = TBUFF_TO_CMD(cmd_tbuff, struct set_txop_command);

    if (str_to_uint32_range(argv[1], &txop, 0, 10))
    {
        mctrl_err("Invalid txop value.\n");
        usage(mors);
        ret = -1;
        goto exit;
    }

    cmd->txop = htole32(txop);
    ret = morsectrl_send_command(mors->transport, MORSE_COMMAND_SET_TXOP,
                                 cmd_tbuff, rsp_tbuff);

exit:
    if (ret < 0)
    {
        mctrl_err("Failed to set txop\n");
    }

    morsectrl_transport_buff_free(cmd_tbuff);
    morsectrl_transport_buff_free(rsp_tbuff);
    return ret;
}

MM_CLI_HANDLER(txop, MM_INTF_REQUIRED, MM_DIRECT_CHIP_SUPPORTED);
