/*
 * Copyright 2023 Morse Micro
 */
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "portable_endian.h"
#include "command.h"
#include "utilities.h"

#define RSSI_MARGIN_MIN 3
#define RSSI_MARGIN_MAX 30
#define BLACKLIST_TIMEOUT_MIN 10
#define BLACKLIST_TIMEOUT_MAX 600

struct PACKED command_set_dynamic_peering_conf {
    /** Enable or disable mesh dynamic peering */
    uint8_t enabled;

    /** RSSI margin to consider while selecting a peer to kick out */
    uint8_t rssi_margin;

    /** Kicked out peer is not allowed connection during this period */
    uint32_t blacklist_timeout;
};

static void usage(struct morsectrl *mors)
{
    mctrl_print(
        "\tdynamic_peering [enable|disable] -r <rssi margin> -t <blacklist timeout>\n");
    mctrl_print("\t\t\t\tEnable/Disable Mesh Dynamic Peering\n");
    mctrl_print("\t\t\t\tdo not use - for internal use by wpa_supplicant\n");
    mctrl_print("\t\t-r <value>\tRSSI margin to consider while selecting a peer to kick out");
    mctrl_print(" min:%u, max:%u\n", RSSI_MARGIN_MIN, RSSI_MARGIN_MAX);
    mctrl_print("\t\t-t <value>\tblacklist time for a kicked-out peer (seconds)");
    mctrl_print(" min:%u, max:%u\n", BLACKLIST_TIMEOUT_MIN, BLACKLIST_TIMEOUT_MAX);
}

int dynamic_peering(struct morsectrl *mors, int argc, char *argv[])
{
    int ret = -1;
    struct command_set_dynamic_peering_conf *dyn_peering_conf = NULL;
    struct morsectrl_transport_buff *cmd_tbuff = NULL;
    struct morsectrl_transport_buff *rsp_tbuff = NULL;
    uint8_t temp;
    uint32_t timeout;
    int option;
    int enabled;

    if (argc == 0)
    {
        usage(mors);
        return 0;
    }

    if (argc != 6)
    {
        mctrl_err("Insufficient command parameters\n");
        usage(mors);
        ret = -1;
        goto exit;
    }

    enabled = expression_to_int(argv[1]);
    if (enabled < 0)
    {
        mctrl_err("Invalid dynamic_peering command '%s'\n", argv[1]);
        usage(mors);
        ret = -1;
        goto exit;
    }

    cmd_tbuff = morsectrl_transport_cmd_alloc(mors->transport, sizeof(*dyn_peering_conf));
    if (!cmd_tbuff)
    {
        goto exit;
    }

    rsp_tbuff = morsectrl_transport_resp_alloc(mors->transport, 0);
    if (!rsp_tbuff)
    {
        goto exit;
    }

    dyn_peering_conf = TBUFF_TO_CMD(cmd_tbuff, struct command_set_dynamic_peering_conf);
    memset(dyn_peering_conf, 0, sizeof(*dyn_peering_conf));

    dyn_peering_conf->enabled = enabled;
    argc--;
    argv++;
    while ((option = getopt(argc, argv, "r:t:")) != -1)
    {
        switch (option) {
        case 'r':
            if (str_to_uint8_range(optarg, &temp, RSSI_MARGIN_MIN, RSSI_MARGIN_MAX) < 0)
            {
                mctrl_err("RSSI margin %u must be within the range min %u : max %u\n",
                    temp, RSSI_MARGIN_MIN, RSSI_MARGIN_MAX);
                ret = -1;
                goto exit;
            }
            dyn_peering_conf->rssi_margin = temp;
            break;
        case 't':
            if (str_to_uint32_range(optarg, &timeout, BLACKLIST_TIMEOUT_MIN,
                BLACKLIST_TIMEOUT_MAX) < 0)
            {
                mctrl_err("Blacklist timeout %u must be within the range min %u : max %u\n",
                    timeout, BLACKLIST_TIMEOUT_MIN, BLACKLIST_TIMEOUT_MAX);
                ret = -1;
                goto exit;
            }
            dyn_peering_conf->blacklist_timeout = timeout;
            break;
        default:
            mctrl_err("Invalid argument\n");
            usage(mors);
            ret = -1;
            goto exit;
        }
    }

    ret = morsectrl_send_command(mors->transport, MORSE_COMMAND_DYNAMIC_PEERING_SET_CONF,
        cmd_tbuff, rsp_tbuff);

exit:
    if (cmd_tbuff)
    {
        morsectrl_transport_buff_free(cmd_tbuff);
    }

    if (rsp_tbuff)
    {
        morsectrl_transport_buff_free(rsp_tbuff);
    }

    return ret;
}

MM_CLI_HANDLER(dynamic_peering, MM_INTF_REQUIRED, MM_DIRECT_CHIP_NOT_SUPPORTED);
