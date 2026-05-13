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

struct PACKED command_set_keep_alive_offload
{
    /** The value of the BSS max idle period as it appears in the IE */
    uint16_t bss_max_idle_period;
    /** Set to TRUE to interpret the value of BSS max idle period as per 11ah spec */
    uint8_t interpret_as_11ah;
};


static void usage(struct morsectrl *mors)
{
    mctrl_print("\tkeepalive <bss max idle period> [-a]\n");
    mctrl_print("\t\t<bss max idle period>\tthe bss max idle period as seen in IE\n");
    mctrl_print(
        "\t\t[-a]                 \toptional, set to interpret idle period as per 11ah spec\n");
}

int keepalive(struct morsectrl *mors, int argc, char *argv[])
{
    int ret = -1;
    struct command_set_keep_alive_offload *cmd;
    struct morsectrl_transport_buff *cmd_tbuff;
    struct morsectrl_transport_buff *rsp_tbuff;
    uint16_t bss_max_idle_period;

    if (argc == 0)
    {
        usage(mors);
        return 0;
    }

    if (argc < 2 || argc > 3)
    {
        mctrl_err("Invalid arguments\n");
        usage(mors);
        return -1;
    }

    cmd_tbuff = morsectrl_transport_cmd_alloc(mors->transport, sizeof(*cmd));
    rsp_tbuff = morsectrl_transport_resp_alloc(mors->transport, 0);

    if (!cmd_tbuff || !rsp_tbuff)
        goto exit;

    cmd = TBUFF_TO_CMD(cmd_tbuff, struct command_set_keep_alive_offload);

    cmd->interpret_as_11ah = false;

    if (str_to_uint16(argv[1], &bss_max_idle_period))
    {
        mctrl_err("Invalid bss max idle period: %s\n", argv[1]);
        ret = -1;
        goto exit;
    }
    cmd->bss_max_idle_period = htole16(bss_max_idle_period);

    if (argc == 3)
    {
        if (strcmp(argv[2], "-a") != 0)
        {
            mctrl_err("Invalid argument: %s\n", argv[2]);
            usage(mors);
            ret = -1;
            goto exit;
        }

        cmd->interpret_as_11ah = true;
    }

    ret = morsectrl_send_command(mors->transport, MORSE_COMMAND_SET_KEEP_ALIVE_OFFLOAD,
        cmd_tbuff, rsp_tbuff);

    if (ret)
    {
        mctrl_err("Failed to send keepalive offload command\n");
    }

exit:
    morsectrl_transport_buff_free(cmd_tbuff);
    morsectrl_transport_buff_free(rsp_tbuff);
    return ret;
}

MM_CLI_HANDLER(keepalive, MM_INTF_REQUIRED, MM_DIRECT_CHIP_NOT_SUPPORTED);
