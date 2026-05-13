/*
 * Copyright 2021 Morse Micro
 */

#include <stdio.h>
#include "command.h"
#include "utilities.h"

struct PACKED command_phy_deaf
{
    /* 0 sets PHY into normal operation, 1 sets PHY into deaf/blocked mode */
    uint8_t enable;
};

static void usage(struct morsectrl *mors) {
    mctrl_print("\tphy_deaf <command>\n");
    mctrl_print("\t\t 1: \tput phy in a deaf mode, it will not be able to receive\n");
    mctrl_print("\t\t 2: \tput phy in a blocked mode, it will not be able to transmit\n");
    mctrl_print("\t\t 3: \tput phy in a deaf and blocked mode, "
                "it will not be able to receive or transmit\n");
    mctrl_print("\t\t 0: \treturn to normal operation\n");
}

int phy_deaf(struct morsectrl *mors, int argc, char *argv[])
{
    int ret  = -1;
    int8_t phy_deaf;
    struct command_phy_deaf *cmd;
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

    int error = str_to_int8_range(argv[1], &phy_deaf, 0, 3);
    if (error)
    {
        mctrl_err("Invalid command parameters\n");
        usage(mors);
        return -1;
    }

    cmd_tbuff = morsectrl_transport_cmd_alloc(mors->transport, sizeof(*cmd));
    rsp_tbuff = morsectrl_transport_resp_alloc(mors->transport, 0);

    if (!cmd_tbuff || !rsp_tbuff)
        goto exit;

    cmd = TBUFF_TO_CMD(cmd_tbuff, struct command_phy_deaf);
    cmd->enable = phy_deaf;
    ret = morsectrl_send_command(mors->transport, MORSE_TEST_COMMAND_PHY_DEAF,
                                 cmd_tbuff, rsp_tbuff);
exit:
    if (ret < 0)
    {
        mctrl_err("Failed to set phy_deaf mode\n");
    }

    if (cmd_tbuff)
        morsectrl_transport_buff_free(cmd_tbuff);
    if (rsp_tbuff)
        morsectrl_transport_buff_free(rsp_tbuff);
    return ret;
}

MM_CLI_HANDLER(phy_deaf, MM_INTF_REQUIRED, MM_DIRECT_CHIP_SUPPORTED);
