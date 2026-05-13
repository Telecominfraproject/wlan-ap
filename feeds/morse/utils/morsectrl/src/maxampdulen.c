/*
 * Copyright 2022 Morse Micro
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


struct PACKED set_max_ampdu_length_command
{
    int32_t n_bytes;
};

static void usage(struct morsectrl *mors)
{
    mctrl_print("\tmaxampdulen <bytes>\n");
    mctrl_print("\t\t\t\tset the max ampdu length the chip is allowed to aggregate\n");
    mctrl_print("\t\t\t\tset to (-1) to reset to chip default\n");
}

int maxampdulen(struct morsectrl *mors, int argc, char *argv[])
{
    int ret = -1;
    struct set_max_ampdu_length_command *cmd;
    struct morsectrl_transport_buff *cmd_tbuff;
    struct morsectrl_transport_buff *rsp_tbuff;
    int32_t n_bytes;

    if (argc < 2)
    {
        usage(mors);
        return 0;
    }

    cmd_tbuff = morsectrl_transport_cmd_alloc(mors->transport, sizeof(*cmd));
    rsp_tbuff = morsectrl_transport_resp_alloc(mors->transport, 0);

    if (!cmd_tbuff || !rsp_tbuff)
        goto exit;

    cmd = TBUFF_TO_CMD(cmd_tbuff, struct set_max_ampdu_length_command);

    if (str_to_int32(argv[1], &n_bytes))
    {
        mctrl_err("Invalid ampdu length\n");
        ret = -1;
        goto exit;
    }

    cmd->n_bytes = htole32(n_bytes);

    ret = morsectrl_send_command(mors->transport, MORSE_TEST_COMMAND_SET_MAX_AMPDU_LENGTH,
                                 cmd_tbuff, rsp_tbuff);
exit:
    if (ret)
    {
        mctrl_err("Failed to set max ampdu length: %d\n", ret);
    }
    else
    {
        if (cmd->n_bytes == -1)
            mctrl_print("Reset max ampdu length to chip default\n");
        else
            mctrl_print("Set max ampdu length to: %d\n", cmd->n_bytes);
    }

    morsectrl_transport_buff_free(cmd_tbuff);
    morsectrl_transport_buff_free(rsp_tbuff);
    return ret;
}

MM_CLI_HANDLER(maxampdulen, MM_INTF_REQUIRED, MM_DIRECT_CHIP_SUPPORTED);
