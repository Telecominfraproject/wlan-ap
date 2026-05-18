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

#define MBCA_CONFIG_MIN 1
#define MBCA_CONFIG_MAX 3
#define MIN_BEACON_GAP_MIN 5
#define MIN_BEACON_GAP_MAX 100
#define TBTT_ADJ_INT_MIN 30
#define TBTT_ADJ_INT_MAX 65
#define BEACON_TIMING_REP_INT_MIN 1
#define BEACON_TIMING_REP_INT_MAX 255
#define MBSS_SCAN_DURATION_MIN 2048
#define MBSS_SCAN_DURATION_MAX 10240

struct PACKED command_set_mbca_conf {
    /** Configuration to enable or disable MBCA TBTT Selection and Adjustment */
    uint8_t mbca_config;

    /** Beacon Timing Element Report interval */
    uint8_t beacon_timing_report_interval;

    /** Minimum gap between our beacon and neighbor beacons */
    uint8_t min_beacon_gap_ms;

     /** Initial scan duration to find neighbor mesh peers in the MBSS */
    uint16_t mbss_start_scan_duration_ms;

    /** TBTT adjustment timer interval in LMAC firmware */
    uint16_t tbtt_adj_interval_ms;
};

static void usage(struct morsectrl *mors)
{
    mctrl_print(
        "\tmbca -m <mbca config> -s <initial scan duration> -r <beacon timing report int> "
        "-g <min beacon gap> -i <tbtt adj int>\n");
    mctrl_print("\t\tconfigure Mesh beacon collision avoidance\n");
    mctrl_print("\t\tdo not use - for internal use by wpa_supplicant\n");
    mctrl_print("\t\t-m <value>\t1: To enable TBTT selection, 3: To enable TBTT selection and ");
    mctrl_print("adjustment\n");
    mctrl_print("\t\t-s <value>\tInitial scan duration in msecs to find peers. min:%u, max:%u\n",
        MBSS_SCAN_DURATION_MIN, MBSS_SCAN_DURATION_MAX);
    mctrl_print("\t\t-r <value>\tBeacon Timing Report interval. min:%u, max:%u\n",
        BEACON_TIMING_REP_INT_MIN, BEACON_TIMING_REP_INT_MAX);
    mctrl_print("\t\t-g <value>\tMinimum gap in msecs between our and neighbor's beacons. min:%u, ",
        MIN_BEACON_GAP_MIN);
    mctrl_print("max:%u\n", MIN_BEACON_GAP_MAX);
    mctrl_print("\t\t-i <value>\tTBTT adjustment timer interval in secs. min:%u, max:%u\n",
        TBTT_ADJ_INT_MIN, TBTT_ADJ_INT_MAX);
}

int mbca(struct morsectrl *mors, int argc, char *argv[])
{
    int ret = -1;
    struct command_set_mbca_conf *mbca_req = NULL;
    struct morsectrl_transport_buff *cmd_tbuff = NULL;
    struct morsectrl_transport_buff *rsp_tbuff = NULL;
    uint8_t temp;
    uint16_t temp_short;
    int option;

    if (argc == 0)
    {
        usage(mors);
        return 0;
    }

    if (argc != 11)
    {
        mctrl_err("Insufficient command parameters\n");
        usage(mors);
        ret = -1;
        goto exit;
    }

    cmd_tbuff = morsectrl_transport_cmd_alloc(mors->transport, sizeof(*mbca_req));
    if (!cmd_tbuff)
    {
        goto exit;
    }

    rsp_tbuff = morsectrl_transport_resp_alloc(mors->transport, 0);
    if (!rsp_tbuff)
    {
        goto exit;
    }

    mbca_req = TBUFF_TO_CMD(cmd_tbuff, struct command_set_mbca_conf);
    memset(mbca_req, 0, sizeof(*mbca_req));

    while ((option = getopt(argc, argv, "m:s:r:g:i:")) != -1)
    {
        switch (option) {
        case 'm':
            if (str_to_uint8_range(optarg, &temp, MBCA_CONFIG_MIN, MBCA_CONFIG_MAX) < 0)
            {
                mctrl_err("MBCA Config not a valid uint8_t value\n");
                usage(mors);
                ret = -1;
                goto exit;
            }
            mbca_req->mbca_config = temp;
            break;
        case 's':
            if (str_to_uint16_range(optarg, &temp_short, MBSS_SCAN_DURATION_MIN,
                MBSS_SCAN_DURATION_MAX) < 0)
            {
                mctrl_err("MBSS start scan duration %u must be within the range min %u :"
                    " max %u \n", temp_short, MBSS_SCAN_DURATION_MIN, MBSS_SCAN_DURATION_MAX);
                usage(mors);
                ret = -1;
                goto exit;
            }
            mbca_req->mbss_start_scan_duration_ms = temp_short;
            break;
        case 'r':
            if (str_to_uint8_range(optarg, &temp, BEACON_TIMING_REP_INT_MIN,
                BEACON_TIMING_REP_INT_MAX) < 0)
            {
                mctrl_err("Beacon Timing Report Interval %u must be within the range min %u : "
                    "max %u \n", temp, BEACON_TIMING_REP_INT_MIN, BEACON_TIMING_REP_INT_MAX);
                usage(mors);
                ret = -1;
                goto exit;
            }
            mbca_req->beacon_timing_report_interval = temp;
            break;
        case 'g':
            if (str_to_uint8_range(optarg, &temp, MIN_BEACON_GAP_MIN, MIN_BEACON_GAP_MAX) < 0)
            {
                mctrl_err("Min Beacon Gap %d must be within the range min %u : max %u \n", temp,
                    MIN_BEACON_GAP_MIN, MIN_BEACON_GAP_MAX);
                usage(mors);
                ret = -1;
                goto exit;
            }
            mbca_req->min_beacon_gap_ms = temp;
            break;
        case 'i':
            if (str_to_uint8_range(optarg, &temp, TBTT_ADJ_INT_MIN, TBTT_ADJ_INT_MAX) < 0)
            {
                mctrl_err("TBTT adjustment interval %d must be within the range min %u : max %u \n"
                    , temp, TBTT_ADJ_INT_MIN, TBTT_ADJ_INT_MAX);
                usage(mors);
                ret = -1;
                goto exit;
            }
            mbca_req->tbtt_adj_interval_ms = SECS_TO_MSECS(temp);
            break;
        case '?' :
            usage(mors);
            goto exit;
        default:
            mctrl_err("Invalid argument\n");
            usage(mors);
            ret = -1;
            goto exit;
        }
    }

    ret = morsectrl_send_command(mors->transport, MORSE_COMMAND_MBCA_SET_CONF, cmd_tbuff,
            rsp_tbuff);

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

MM_CLI_HANDLER(mbca, MM_INTF_REQUIRED, MM_DIRECT_CHIP_NOT_SUPPORTED);
