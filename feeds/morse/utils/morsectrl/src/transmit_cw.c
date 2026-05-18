/*
 * Copyright 2022 Morse Micro
 */

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <stdint.h>

#include "portable_endian.h"
#include "command.h"
#include "utilities.h"

struct PACKED transmit_cw_command
{
    /** The flags of this message */
    int32_t start;
    int32_t tone_frequency_hz;
    int32_t power_dbm;
};


static void usage(struct morsectrl *mors) {
    mctrl_print("\ttransmit_cw [start|stop] <tone_frequency_hz> <power_dbm>\n");
    mctrl_print(
        "\t\t\t\tstart continuous wave transmission for given frequency and at given power\n");
    mctrl_print("\t\t\t\tor stop the continuous wave transmission\n");
    mctrl_print("\t\t\t\tPossible frequencies:\n");
    mctrl_print(
        "\t\t\t\t\tOFDM tones, other frequencies that are an integer multiple of (BW / 4000)\n");
}

int transmit_cw(struct morsectrl *mors, int argc, char *argv[])
{
    int ret = -1;
    int32_t tone_frequency_hz;
    int32_t power_dbm;
    int32_t start;
    struct transmit_cw_command *cmd;
    struct morsectrl_transport_buff *cmd_tbuff;
    struct morsectrl_transport_buff *rsp_tbuff;

    switch (argc)
    {
        case 0:
            usage(mors);
            return 0;
        case 2:
            if (strncmp(argv[1], "stop", 4) == 0)
            {
                start = 0;
                tone_frequency_hz = 0;
                power_dbm = 0;
            }
            else
            {
                usage(mors);
                return -1;
            }
            break;
        case 4:
            if (strncmp(argv[1], "start", 5) == 0)
            {
                start = 1;
            }
            else
            {
                usage(mors);
                return -1;
            }

            if (str_to_int32(argv[2], &tone_frequency_hz)) {
                mctrl_err("Invalid tone frequency\n");
                usage(mors);
                return -1;
            }

            if (str_to_int32(argv[3], &power_dbm)) {
                mctrl_err("Invalid power value\n");
                usage(mors);
                return -1;
            }
            break;
        default:
            mctrl_err("Invalid command parameters\n");
            usage(mors);
            return -1;
    }

    cmd_tbuff = morsectrl_transport_cmd_alloc(mors->transport, sizeof(*cmd));
    rsp_tbuff = morsectrl_transport_resp_alloc(mors->transport, 0);

    if (!cmd_tbuff || !rsp_tbuff)
        goto exit;

    cmd = TBUFF_TO_CMD(cmd_tbuff, struct transmit_cw_command);
    cmd->start = htole32(start);
    cmd->tone_frequency_hz = htole32(tone_frequency_hz);
    cmd->power_dbm = htole32(power_dbm);
    ret = morsectrl_send_command(mors->transport, MORSE_TEST_COMMAND_TRANSMIT_CW,
                                 cmd_tbuff, rsp_tbuff);

exit:
    if (ret < 0)
    {
        mctrl_err("Failed to execute command\n");
    }

    morsectrl_transport_buff_free(cmd_tbuff);
    morsectrl_transport_buff_free(rsp_tbuff);
    return ret;
}

MM_CLI_HANDLER(transmit_cw, MM_INTF_REQUIRED, MM_DIRECT_CHIP_SUPPORTED);
