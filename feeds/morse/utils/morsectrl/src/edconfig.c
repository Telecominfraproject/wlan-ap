/*
 * Copyright 2022 Morse Micro
 *
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

struct PACKED command_energy_detection_mode_req
{
    /**
     * Mode for energy detection:
     * 0:auto - The default. System will automatically sense the medium and set the energy
     *          threshold / noise estimate.
     * 1:static - Set a static value for the specified parameter. The variable `value` must
     *            be set in this scenario.
     * 2:ignore - Only valid when param == 0 (energy threshold)
     *            Energy on air will be ignored when device goes to transmit.
     * 3:jammer - Only valid when param == 0 (energy threshold)
     *            Energy on air will be ignored if in-channel jammer is detected.
     */
    uint16_t mode;

    /**
     * Energy detect parameter to target
     * 0: energy threshold
     * 1: noise estimate
     */
    uint8_t param;

    /** 0 : value is in dbm
     *  1 : value is linear
     */
    uint8_t linear;

    /** If mode is static (1), then this will define the value to assign to specified parameter.
     *  Must evaluate to a positive linear value (after dBm conversion if applicable)
    */
    int16_t value;
};

static void usage(struct morsectrl *mors)
{
    mctrl_print("\tedconfig <target> <command>\n");
    mctrl_print("\t\t\t\tsets the current mode for DCF energy detection\n");
    mctrl_print(
        "\t\t<target> - 'energy' or 'noise' - Change the setting for energy detect threshold or "
        "noise estimate\n");
    mctrl_print("\t\t<command> -\n");
    mctrl_print(
        "\t\t\t\tautomatic - default, chip will automatically select energy dection threshold or "
        "noise estimate\n");
    mctrl_print(
        "\t\t\t\tstatic [dbm/linear] <threshold> - set a static energy dection threshold or "
        "noise estimate (in integer dBm or linear)\n");
    mctrl_print(
        "\t\t\t\tignore - tell the chip to ignore non-wlan energy completely"
        "(only valid for 'energy' target)\n");
    mctrl_print(
        "\t\t\t\tjammer - tell the chip to ignore non-wlan energy if in-channel jammer is detected"
        "(only valid for 'energy' target)\n");
}

int edconfig(struct morsectrl *mors, int argc, char *argv[])
{
    int ret = -1;
    struct command_energy_detection_mode_req *cmd;
    struct morsectrl_transport_buff *cmd_tbuff;
    struct morsectrl_transport_buff *rsp_tbuff;
    char* next;

    if (argc == 0)
    {
        usage(mors);
        return 0;
    }

    cmd_tbuff = morsectrl_transport_cmd_alloc(mors->transport, sizeof(*cmd));
    rsp_tbuff = morsectrl_transport_resp_alloc(mors->transport, 0);

    if (!cmd_tbuff || !rsp_tbuff)
        goto exit;

    cmd = TBUFF_TO_CMD(cmd_tbuff, struct command_energy_detection_mode_req);

    if ( argc < 3 || argc > 5)
    {
        mctrl_err("Invalid argument\n");
        usage(mors);
        ret = -1;
        goto exit;
    }

    // target checks
    if (0 == strcmp("energy", argv[1]))
    {
        cmd->param = 0;
    }
    else if (0 == strcmp("noise", argv[1]))
    {
        cmd->param = 1;
    }
    else
    {
        mctrl_err("Invalid target\n");
        usage(mors);
        ret = -1;
        goto exit;
    }

    // mode checks
    if (0 == strcmp("automatic", argv[2]))
    {
        cmd->mode = 0;
    }
    else if ( 0 == strcmp("ignore", argv[2]))
    {
        cmd->mode = 2;
    }
    else if ( 0 == strcmp("jammer", argv[2]))
    {
        cmd->mode = 3;
    }
    else if ( 0 == strcmp("static", argv[2]) )
    {
        if ( argc != 5 )
        {
            mctrl_err("Not enough arguments\n");
            usage(mors);
            ret = -1;
            goto exit;
        }

        if ( 0 == strcmp("dbm", argv[3]) )
        {
            cmd->linear = 0;
        }
        else if ( 0 == strcmp("linear", argv[3]) )
        {
            cmd->linear = 1;
        }
        else
        {
            mctrl_err("Invalid static threshold type (specify either 'dbm' or 'linear'\n");
            usage(mors);
            ret = -1;
            goto exit;
        }

        cmd->mode = 1;
        cmd->value = (int16_t)strtol(argv[4], &next, 10);

        if (next == argv[4]) /* valid integer could not be found */
        {
            mctrl_err("invalid threshold\n");
            usage(mors);
            ret = -1;
            goto exit;
        }
    }
    else
    {
        mctrl_err("Invalid mode\n");
        usage(mors);
        ret = -1;
        goto exit;
    }

    // good to go
    ret = morsectrl_send_command(mors->transport, MORSE_TEST_COMMAND_ENERGY_DETECTION_MODE,
                                 cmd_tbuff, rsp_tbuff);
exit:
    if (ret < 0)
    {
        mctrl_err("Failed to configure energy/noise threshold\n");
    }

    morsectrl_transport_buff_free(cmd_tbuff);
    morsectrl_transport_buff_free(rsp_tbuff);
    return ret;
}

MM_CLI_HANDLER(edconfig, MM_INTF_REQUIRED, MM_DIRECT_CHIP_SUPPORTED);
