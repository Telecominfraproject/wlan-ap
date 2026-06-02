/*
 * Copyright 2021 Morse Micro
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

struct PACKED set_ndp_probe_support
{
    uint8_t enabled;
    uint8_t requested_response_is_pv1;
    int8_t tx_bw_mhz;
};


static void usage(struct morsectrl *mors)
{
    mctrl_print("\tndpprobe [enable|disable]\n");
    mctrl_print("\t\t\t\t'enable' must always be included when configuring a parameter\n");
    mctrl_print("\t\t\t\t'disable' will stop sending normal probes as NDPs\n");
    mctrl_print("\t\t-r <value>\tSet desired probe response replies: 0 for PV0, 1 for PV1\n");
    mctrl_print("\t\t-b <value>\tTX bandwidth in MHz (1|2) or (-1) to use default from host\n");
}

int ndpprobe(struct morsectrl *mors, int argc, char *argv[])
{
    int ret = -1;
    int option;
    struct set_ndp_probe_support *cmd;
    struct morsectrl_transport_buff *cmd_tbuff;
    struct morsectrl_transport_buff *rsp_tbuff;
    int8_t tmp;

    if (argc < 2)
    {
        usage(mors);
        return -1;
    }

    cmd_tbuff = morsectrl_transport_cmd_alloc(mors->transport, sizeof(*cmd));
    rsp_tbuff = morsectrl_transport_resp_alloc(mors->transport, 0);

    if (!cmd_tbuff || !rsp_tbuff)
        goto exit;

    cmd = TBUFF_TO_CMD(cmd_tbuff, struct set_ndp_probe_support);

    cmd->enabled = 0;
    cmd->tx_bw_mhz = -1;
    cmd->requested_response_is_pv1 = 0;

    switch (expression_to_int(argv[1]))
    {
        case true:
            argc -= 1;
            argv += 1;
            cmd->enabled = 1;
            while ((option = getopt(argc, argv, "r:b:")) != -1)
            {
                switch (option)
                {
                    case 'r' :
                        if (str_to_int8_range(optarg, &tmp, 0, 1))
                        {
                            usage(mors);
                            ret = -1;
                            goto exit;
                        }
                        cmd->requested_response_is_pv1 = tmp;
                    break;

                    case 'b' :
                        if (str_to_int8(optarg, &tmp) &&
                                (cmd->tx_bw_mhz != -1 &&
                                cmd->tx_bw_mhz != 1 &&
                                cmd->tx_bw_mhz != 2))
                        {
                            usage(mors);
                            ret = -1;
                            goto exit;
                        }
                        cmd->tx_bw_mhz = tmp;
                    break;

                    case '?' :
                        usage(mors);
                        ret = -1;
                        goto exit;

                    default :
                        mctrl_err("Invalid argument\n");
                        usage(mors);
                        ret = -1;
                        goto exit;
                }
            }
        break;

        case false:
            cmd->enabled = 0;
        break;

        default:
            mctrl_err("Invalid command parameters\n");
            usage(mors);
            ret = -1;
            goto exit;
    }

    ret = morsectrl_send_command(mors->transport, MORSE_TEST_COMMAND_SET_NDP_PROBE_SUPPORT,
                                 cmd_tbuff, rsp_tbuff);
exit:
    if (ret < 0)
    {
        mctrl_err("Failed to set ndp probe support\n");
    }
    else
    {
        mctrl_print("\tNDP Probe support: %s\n", (cmd->enabled) ? "enabled" : "disabled");
        mctrl_print("\t\tRequested Probe Response type: PV%d\n",
            (cmd->requested_response_is_pv1) ? 1 : 0);
        if (cmd->tx_bw_mhz == -1)
            mctrl_print("\t\tTX BW of NDP Probes: default from host\n");
        else
            mctrl_print("\t\tTX BW of NDP Probes: %d MHz\n", cmd->tx_bw_mhz);
    }

    morsectrl_transport_buff_free(cmd_tbuff);
    morsectrl_transport_buff_free(rsp_tbuff);
    return ret;
}

MM_CLI_HANDLER(ndpprobe, MM_INTF_REQUIRED, MM_DIRECT_CHIP_SUPPORTED);
