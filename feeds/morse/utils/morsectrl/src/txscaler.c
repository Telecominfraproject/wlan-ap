/*
 * Copyright 2021 Morse Micro
 */

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <stdint.h>
#include <math.h>

#include "portable_endian.h"
#include "command.h"
#include "utilities.h"

struct PACKED set_tx_scaler_command
{
    /** The flags of this message */
    int32_t tx_scaler;
};


static void usage(struct morsectrl *mors) {
    mctrl_print("\ttxscaler <value>\tscales tx power (-15 to +15 dB, requires DVT firmware)\n");
}

static int32_t db2linear(int32_t db)
{
    return((int32_t) (pow(10.0, (db/20.0))*65536));
}

int txscaler(struct morsectrl *mors, int argc, char *argv[])
{
    int ret = -1;
    int32_t tx_scaler;
    struct set_tx_scaler_command *cmd;
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

    if (str_to_int32_range(argv[1], &tx_scaler, -30, 30))
    {
        mctrl_err("Invalid txscaler value.\n");
        usage(mors);
        return -1;
    }

    cmd_tbuff = morsectrl_transport_cmd_alloc(mors->transport, sizeof(*cmd));
    rsp_tbuff = morsectrl_transport_resp_alloc(mors->transport, 0);

    if (!cmd_tbuff || !rsp_tbuff)
        goto exit;

    cmd = TBUFF_TO_CMD(cmd_tbuff, struct set_tx_scaler_command);
    cmd->tx_scaler = htole32(db2linear(tx_scaler));
    ret = morsectrl_send_command(mors->transport, MORSE_TEST_COMMAND_SET_TX_SCALER,
                                 cmd_tbuff, rsp_tbuff);

exit:
    if (ret < 0)
    {
        mctrl_err("Failed to set txscaler\n");
    }

    morsectrl_transport_buff_free(cmd_tbuff);
    morsectrl_transport_buff_free(rsp_tbuff);
    return ret;
}

MM_CLI_HANDLER(txscaler, MM_INTF_REQUIRED, MM_DIRECT_CHIP_SUPPORTED);
