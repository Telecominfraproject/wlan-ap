/*
 * Copyright 2023 Morse Micro
 */

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <stdint.h>
#ifndef MORSE_WIN_BUILD
#include <net/if.h>
#endif
#include "portable_endian.h"

#include "command.h"
#include "utilities.h"

#define BSS_MIN 0
#define BSS_MAX 2
#define BSS_ID_DEFAULT 0


struct PACKED set_mbssid_ie
{
    /** Maximum supported BSS to be updated in MBSSID IE */
    uint8_t max_bssid_indicator;

    /** Beacon or probe reponse transmitting interface name */
    char transmitter_iface[IFNAMSIZ];
};

static void usage(struct morsectrl *mors)
{
    mctrl_print("\tmbssid -t <transmitting BSS> -m <max bss id>\n");
    mctrl_print("\t\t\tAdvertise a BSS from another BSS's beacons\n");
    mctrl_print("\t\t-t <value>\tTransmitting interface name, eg: wlan0\n");
    mctrl_print("\t\t-m <value>\tMaximum number of BSSes supported\n");
}

int mbssid(struct morsectrl *mors, int argc, char *argv[])
{
    int ret = -1;
    int option;
    uint8_t max_bssid_indicator = BSS_ID_DEFAULT;
    char *transmitter_iface = "";
    struct set_mbssid_ie *cmd;
    struct morsectrl_transport_buff *cmd_tbuff;
    struct morsectrl_transport_buff *rsp_tbuff;

    if (argc == 0)
    {
        usage(mors);
        return 0;
    }

    if (argc < 2 || argc > 5)
    {
        usage(mors);
        return -1;
    }

    cmd_tbuff = morsectrl_transport_cmd_alloc(mors->transport, sizeof(*cmd));
    rsp_tbuff = morsectrl_transport_resp_alloc(mors->transport, 0);

    if (!cmd_tbuff || !rsp_tbuff)
        goto exit;

    cmd = TBUFF_TO_CMD(cmd_tbuff, struct set_mbssid_ie);

    memset(cmd, 0, sizeof(*cmd));
    while ((option = getopt(argc, argv, "t:m:")) != -1)
    {
        switch (option) {
        case 'm' :
            if (str_to_uint8_range(optarg, &max_bssid_indicator,
                BSS_MIN, BSS_MAX) < 0)
            {
                mctrl_err("Maximum supported BSS  %u must be within range min %u : max %u\n",
                        max_bssid_indicator, BSS_MIN, BSS_MAX);
                usage(mors);
                ret = -1;
                goto exit;
            }
            cmd->max_bssid_indicator = max_bssid_indicator;
            break;
        case 't' :
            transmitter_iface = optarg;
            if (transmitter_iface)
                strncpy(cmd->transmitter_iface, transmitter_iface,
                    sizeof(cmd->transmitter_iface) - 1);
            break;
        case '?' :
            usage(mors);
            goto exit;
        default :
            mctrl_err("Invalid argument\n");
            usage(mors);
            goto exit;
        }
    }

    if (max_bssid_indicator == BSS_ID_DEFAULT)
    {
        mctrl_err("Invalid max_bssid_indicator %d \n", max_bssid_indicator);
        usage(mors);
        goto exit;
    }
    if (transmitter_iface[0] == '\0')
    {
        mctrl_err("Invalid transmitter_iface %d \n", max_bssid_indicator);
        usage(mors);
        goto exit;
    }

    ret = morsectrl_send_command(mors->transport, MORSE_COMMAND_MBSSID_INFO,
                                 cmd_tbuff, rsp_tbuff);

exit:
    if (ret)
    {
        mctrl_err("Failed to set MBSSID IE info\n");
    }

    morsectrl_transport_buff_free(cmd_tbuff);
    morsectrl_transport_buff_free(rsp_tbuff);
    return ret;
}

MM_CLI_HANDLER(mbssid, MM_INTF_REQUIRED, MM_DIRECT_CHIP_NOT_SUPPORTED);
