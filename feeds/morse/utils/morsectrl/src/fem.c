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

struct PACKED set_fem_settings_command
{
    /** \ref enum fem_antenna */
    uint32_t tx_antenna;
    /** \ref enum fem_antenna */
    uint32_t rx_antenna;
    /** Bool, 1=enabled, 0=disabled */
    uint32_t lna_enabled;
    /** Bool, 1=enabled, 0=disabled */
    uint32_t pa_enabled;
};


static void usage(struct morsectrl *mors) {
    mctrl_print("\tfem <tx_antenna> <rx_antenna> <lna_enable> <pa_enable>\n");
    mctrl_print("\t\t0~2\t\tTX and RX antenna select (0 for auto, 1 for antenna 1...)\n");
    mctrl_print("\t\t0~1\t\tRX LNA and TX PA control\n");
}

int fem(struct morsectrl *mors, int argc, char *argv[])
{
    int ret = -1;
    uint32_t tx_antenna, rx_antenna, lna_enabled, pa_enabled;
    struct set_fem_settings_command *cmd;
    struct morsectrl_transport_buff *cmd_tbuff;
    struct morsectrl_transport_buff *rsp_tbuff;

    if (argc == 0)
    {
        usage(mors);
        return 0;
    }

    if (argc != 5)
    {
        mctrl_err("Invalid command parameters\n");
        usage(mors);
        return -1;
    }

    cmd_tbuff = morsectrl_transport_cmd_alloc(mors->transport, sizeof(*cmd));
    rsp_tbuff = morsectrl_transport_resp_alloc(mors->transport, 0);

    if (!cmd_tbuff || !rsp_tbuff)
        goto exit;

    cmd = TBUFF_TO_CMD(cmd_tbuff, struct set_fem_settings_command);

    if (str_to_uint32_range(argv[1], &tx_antenna, 0, 2))
    {
        mctrl_err("Invalid tx antenna, must be 0 (auto), 1 (antenna 1), 2 (antenna 2)\n");
        ret = -1;
        goto exit;
    }

    if (str_to_uint32_range(argv[2], &rx_antenna, 0, 2))
    {
        mctrl_err("Invalid rx antenna, must be 0 (auto), 1 (antenna 1), 2 (antenna 2)\n");
        ret = -1;
        goto exit;
    }

    if (str_to_uint32_range(argv[3], &lna_enabled, 0, 1))
    {
        mctrl_err("Invalid FEM LNA setting, must be 0 (disabled) or 1 (enabled)\n");
        ret = -1;
        goto exit;
    }

    if (str_to_uint32_range(argv[4], &pa_enabled, 0, 1))
    {
        mctrl_err("Invalid FEM PA setting, must be 0 (disabled) or 1 (enabled)\n");
        ret = -1;
        goto exit;
    }

    cmd->tx_antenna = htole32(tx_antenna);
    cmd->rx_antenna = htole32(rx_antenna);
    cmd->lna_enabled = htole32(lna_enabled);
    cmd->pa_enabled = htole32(pa_enabled);

    ret = morsectrl_send_command(mors->transport, MORSE_COMMAND_SET_FEM_SETTINGS,
                                 cmd_tbuff, rsp_tbuff);
exit:
    if (ret < 0)
    {
        mctrl_err("Failed to set FEM settings\n");
    }

    morsectrl_transport_buff_free(cmd_tbuff);
    morsectrl_transport_buff_free(rsp_tbuff);
    return ret;
}

MM_CLI_HANDLER(fem, MM_INTF_REQUIRED, MM_DIRECT_CHIP_SUPPORTED);
