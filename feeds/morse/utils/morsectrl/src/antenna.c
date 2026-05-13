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

struct PACKED set_antenna_command
{
    /** \ref enum fem_antenna */
    uint32_t tx_antenna;
    /** \ref enum fem_antenna */
    uint32_t rx_antenna;
};


static void usage(struct morsectrl *mors) {
    mctrl_print("\tantenna <tx_antenna> <rx_antenna>\n");
    mctrl_print("\t\t1-2\t\tTX antenna select\n");
    mctrl_print("\t\t1-2\t\tRX antenna select\n");
}

int antenna(struct morsectrl *mors, int argc, char *argv[])
{
    int ret = -1;
    uint32_t tx_antenna, rx_antenna;
    struct set_antenna_command *cmd;
    struct morsectrl_transport_buff *cmd_tbuff;
    struct morsectrl_transport_buff *rsp_tbuff;

    if (argc == 0)
    {
        usage(mors);
        return 0;
    }

    if (argc != 3)
    {
        mctrl_err("Invalid command parameters\n");
        usage(mors);
        return -1;
    }

    if (str_to_uint32_range(argv[1], &tx_antenna, 1, 2))
    {
        mctrl_err("Invalid tx antenna, must be 1 (antenna 1), 2 (antenna 2)\n");
        return -1;
    }

    if (str_to_uint32_range(argv[2], &rx_antenna, 1, 2))
    {
        mctrl_err("Invalid rx antenna, must be  1 (antenna 1), 2 (antenna 2)\n");
        return -1;
    }

    cmd_tbuff = morsectrl_transport_cmd_alloc(mors->transport, sizeof(*cmd));
    rsp_tbuff = morsectrl_transport_resp_alloc(mors->transport, 0);

    if (!cmd_tbuff || !rsp_tbuff)
        goto exit;

    cmd = TBUFF_TO_CMD(cmd_tbuff, struct set_antenna_command);
    cmd->tx_antenna = htole32(tx_antenna);
    cmd->rx_antenna = htole32(rx_antenna);

    ret = morsectrl_send_command(mors->transport, MORSE_COMMAND_SET_ANTENNA,
                                 cmd_tbuff, rsp_tbuff);

exit:
    if (ret < 0)
    {
        mctrl_err("Failed to set antenna\n");
    }

    morsectrl_transport_buff_free(cmd_tbuff);
    morsectrl_transport_buff_free(rsp_tbuff);
    return ret;
}

MM_CLI_HANDLER(antenna, MM_INTF_REQUIRED, MM_DIRECT_CHIP_SUPPORTED);
