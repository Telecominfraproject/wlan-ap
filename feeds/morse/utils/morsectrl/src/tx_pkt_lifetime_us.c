/*
 * Copyright 2022 Morse Micro
 */

#include <stdlib.h>
#include <stdio.h>

#include "command.h"
#include "utilities.h"

/* Lifetime packet expiry in us */
#define TX_PACKET_EXPIRY_MIN_US 50000
#define TX_PACKET_EXPIRY_MAX_US 500000

struct PACKED set_tx_pkt_lifetime_us_command
{
    uint32_t lifetime_us;
};

static void usage(struct morsectrl *mors) {
    mctrl_print(
        "\ttx_pkt_lifetime_us <value>\t\tset Tx-pkt lifetime expiry within %d-%d\n",
        TX_PACKET_EXPIRY_MIN_US, TX_PACKET_EXPIRY_MAX_US);
}

int tx_pkt_lifetime_us(struct morsectrl *mors, int argc, char *argv[])
{
    int ret = -1;
    struct set_tx_pkt_lifetime_us_command *cmd;
    struct morsectrl_transport_buff *cmd_tbuff;
    struct morsectrl_transport_buff *rsp_tbuff;
    uint32_t lifetime_us;

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

    cmd = TBUFF_TO_CMD(cmd_tbuff, struct set_tx_pkt_lifetime_us_command);

    if (str_to_uint32_range(argv[1], &lifetime_us,
            TX_PACKET_EXPIRY_MIN_US, TX_PACKET_EXPIRY_MAX_US))
    {
        mctrl_err("Invalid value [%d to %d] us\n",
                    TX_PACKET_EXPIRY_MIN_US, TX_PACKET_EXPIRY_MAX_US);
        usage(mors);
        ret = -1;
        goto exit;
    }

    cmd->lifetime_us = htole32(lifetime_us);

    ret = morsectrl_send_command(mors->transport, MORSE_COMMAND_SET_TX_PKT_LIFETIME_US,
                                 cmd_tbuff, rsp_tbuff);

exit:
    if (ret < 0)
    {
        mctrl_err("Failed to set tx pkt lifetime\n");
    }
    else
    {
        mctrl_print("\t Tx-pkt lifetime expriy is set : %d us\n", cmd->lifetime_us);
    }

    morsectrl_transport_buff_free(cmd_tbuff);
    morsectrl_transport_buff_free(rsp_tbuff);
    return ret;
}

MM_CLI_HANDLER(tx_pkt_lifetime_us, MM_INTF_REQUIRED, MM_DIRECT_CHIP_SUPPORTED);
