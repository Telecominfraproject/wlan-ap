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
#include "utilities.h"

#define MAX_NSS (4)

struct PACKED set_transmission_rate
{
    int32_t mcs_index;
    int32_t bandwidth_mhz;
    int32_t tx_80211ah_format;
    int8_t use_traveling_pilots;
    int8_t use_sgi;
    uint8_t enabled;
    int8_t nss_idx;
    int8_t use_ldpc;
    int8_t use_stbc;
};


static void usage(struct morsectrl *mors)
{
    mctrl_print("\ttxrate [enable|disable]\n");
    mctrl_print("\t\t\t\t'enable' must always be included when configuring a parameter to force\n");
    mctrl_print("\t\t\t\t'disable' will reset all forced rate parameters\n");
    mctrl_print("\t\t-m <value>\tMCS index (0-10) or (-1) to use default in firmware\n");
    mctrl_print("\t\t-b <value>\ttx bandwidth in MHz or (-1) to use default in firmware\n");
    mctrl_print("\t\t-f <value>\tduplicate format (0, 1, 2) or (-1) to use default in firmware\n");
    mctrl_print("\t\t-t <value>\ttraveling pilots (0, 1) or (-1) to use default in firmware\n");
    mctrl_print("\t\t-s <value>\tshort guard interval (0, 1) or (-1) to use default in firmware\n");
    mctrl_print("\t\t-n <value>\tSpatial streams (1-%d) or (-1) to use default in firmware\n",
                MAX_NSS);
    mctrl_print("\t\t-l <value>\tLDPC (0, 1) or (-1) to use default in firmware\n");
    mctrl_print("\t\t-c <value>\tSTBC (0, 1) or (-1) to use default in firmware\n");
}

int txrate(struct morsectrl *mors, int argc, char *argv[])
{
    int ret = -1;
    int option;
    struct set_transmission_rate *cmd;
    struct morsectrl_transport_buff *cmd_tbuff;
    struct morsectrl_transport_buff *rsp_tbuff;
    int32_t tmp;

    if (argc < 2)
    {
        usage(mors);
        return 0;
    }

    cmd_tbuff = morsectrl_transport_cmd_alloc(mors->transport, sizeof(*cmd));
    rsp_tbuff = morsectrl_transport_resp_alloc(mors->transport, 0);

    if (!cmd_tbuff || !rsp_tbuff)
        goto exit;

    cmd = TBUFF_TO_CMD(cmd_tbuff, struct set_transmission_rate);
    cmd->mcs_index = -1;
    cmd->bandwidth_mhz = -1;
    cmd->tx_80211ah_format = -1;
    cmd->use_traveling_pilots = -1;
    cmd->use_sgi = -1;
    cmd->nss_idx = -1;
    cmd->use_ldpc = -1;
    cmd->use_stbc = -1;
    cmd->enabled = 0;

    switch (expression_to_int(argv[1]))
    {
        case true:
            argc -= 1;
            argv += 1;
            cmd->enabled = 1;
            while ((option = getopt(argc, argv, "m:b:f:t:s:n:l:c:")) != -1)
            {
                switch (option)
                {
                    case 'm' :
                        if (str_to_int32(optarg, &tmp))
                        {
                            mctrl_err("Invalid MCS index\n");
                            ret = -1;
                            goto exit;
                        }
                        cmd->mcs_index = htole32(tmp);
                    break;

                    case 'b' :
                        if (str_to_int32(optarg, &tmp))
                        {
                            mctrl_err("Invalid bandwidth\n");
                            ret = -1;
                            goto exit;
                        }
                        cmd->bandwidth_mhz = htole32(tmp);
                    break;

                    case 'f' :
                        if (str_to_int32(optarg, &tmp))
                        {
                            mctrl_err("Invalid duplicate format\n");
                            ret = -1;
                            goto exit;
                        }
                        cmd->tx_80211ah_format = htole32(tmp);
                    break;

                    case 't' :
                        if (str_to_int8(optarg, &cmd->use_traveling_pilots))
                        {
                            mctrl_err("Invalid travelling pilots arg\n");
                            ret = -1;
                            goto exit;
                        }
                    break;

                    case 's' :
                        if (str_to_int8(optarg, &cmd->use_sgi))
                        {
                            mctrl_err("Invalid duplicate format\n");
                            ret = -1;
                            goto exit;
                        }
                    break;

                    case 'n' :
                    {
                        /* Support default (-1) or 1 to MAX_NSS spatial streams. */
                        int8_t nss;

                        if (str_to_int8_range(optarg, &nss, -1, MAX_NSS) || nss == 0)
                        {
                            mctrl_err("Invalid spatial streams parameter: %d\n", nss);
                            ret = -1;
                            goto exit;
                        }
                        cmd->nss_idx = (nss == -1) ? -1 : NSS_TO_NSS_IDX(nss);
                    }
                    break;

                    case 'l' :
                        if (str_to_int8_range(optarg, &cmd->use_ldpc, -1, 1))
                        {
                            mctrl_err("Invalid LDPC parameter: %d\n", cmd->use_ldpc);
                            ret = -1;
                            goto exit;
                        }
                    break;

                    case 'c' :
                        if (str_to_int8_range(optarg, &cmd->use_stbc, -1, 1))
                        {
                            mctrl_err("Invalid STBC parameter: %d\n", cmd->use_stbc);
                            ret = -1;
                            goto exit;
                        }
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

    ret = morsectrl_send_command(mors->transport, MORSE_TEST_COMMAND_SET_TRANSMISSION_RATE,
                                 cmd_tbuff, rsp_tbuff);

exit:
    if (ret < 0)
    {
        mctrl_err("Failed to set transmission rate\n");
    }
    else if (cmd->enabled)
    {
        mctrl_print("Set the following transmission rate parameters:\n");

        if (cmd->mcs_index != -1)
            mctrl_print("\tMCS index: %d\n", cmd->mcs_index);
        if (cmd->bandwidth_mhz != -1)
            mctrl_print("\tTx Channel BW: %d (MHz)\n", cmd->bandwidth_mhz);
        if (cmd->tx_80211ah_format != -1)
            mctrl_print("\tTX format: %d\n", cmd->tx_80211ah_format);
        if (cmd->use_traveling_pilots != -1)
            mctrl_print("\tUse Traveling pilots: %d\n", cmd->use_traveling_pilots);
        if (cmd->use_sgi != -1)
            mctrl_print("\tUse Short Guard Interval: %d\n", cmd->use_sgi);
        if (cmd->nss_idx != -1)
            mctrl_print("\tSpatial Streams: %d\n", NSS_IDX_TO_NSS(cmd->nss_idx));
        if (cmd->use_ldpc != -1)
            mctrl_print("\tUse LDPC: %d\n", cmd->use_ldpc);
        if (cmd->use_stbc != -1)
            mctrl_print("\tUse STBC: %d\n", cmd->use_stbc);
    }
    else
    {
        mctrl_print("Disabled forced transmission rate\n");
    }

    morsectrl_transport_buff_free(cmd_tbuff);
    morsectrl_transport_buff_free(rsp_tbuff);
    return ret;
}

MM_CLI_HANDLER(txrate, MM_INTF_REQUIRED, MM_DIRECT_CHIP_SUPPORTED);
