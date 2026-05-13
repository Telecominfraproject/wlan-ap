/*
 * Copyright 2022 Morse Micro
 */
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <math.h>

#include "portable_endian.h"
#include "command.h"
#include "utilities.h"

/** No upper bound value for airtime duration */
#define AIRTIME_UNLIMITED 0

#define NUM_BOUNDS_VALUES 2

#define SET_MPSW_CFG_AIRTIME_BOUNDS  BIT(0)
#define SET_MPSW_CFG_PKT_SPC_WIN_LEN BIT(1)
#define SET_MPSW_CFG_ENABLED         BIT(2)

struct PACKED mpsw_configuration
{
    /** The maximum allowable packet airtime duration */
    uint32_t airtime_max_us;
    /** The minimum packet airtime duration to trigger spacing */
    uint32_t airtime_min_us;
    /** The length of time to close the tx window between packets */
    uint32_t packet_space_window_length_us;
    /** Whether to enable airtime bounds checking and packet spacing enforcement */
    uint8_t  enable;
};

struct PACKED command_mpsw_cfg_req
{
    struct mpsw_configuration config;
    uint8_t set_cfgs;
};

struct PACKED command_mpsw_cfg_cfm
{
    struct mpsw_configuration config;
};

static void usage(struct morsectrl *mors)
{
    mctrl_print("\tmpsw <opts>\t");
    mctrl_print("configure or query (with no args) the minimum packet spacing window parameters\n");
    mctrl_print("\t\t-b <lower bound us> <upper bound us>\n");
    mctrl_print("\t\t-w <packet spacing window length us>\n");
    mctrl_print("\t\t-e <disable or enable mpsw 0|1>\n");
}

static void print_mpsw_cfg(struct mpsw_configuration *cfg)
{
    mctrl_print("                 MPSW Active: %d\n", cfg->enable);
    mctrl_print("       Airtime Minimum Bound: %u\n", cfg->airtime_min_us);
    mctrl_print("       Airtime Maximum Bound: %u\n", cfg->airtime_max_us);
    mctrl_print("Packet Spacing Window Length: %u\n", cfg->packet_space_window_length_us);
}

/* Only call this function when parsing arguments in a getopt while loop context */
static int parse_bounds_flag_args(char *argv[], int argc,
                                  struct command_mpsw_cfg_req *cmd)
{
    int flag_idx = 0;
    uint32_t tmp;

    optind--;
    for (; optind < argc && *argv[optind] != '-'; optind++)
    {
        if (flag_idx == NUM_BOUNDS_VALUES)
        {
            break;
        }
        if (!argv[optind] || *argv[optind] == '-')
        {
            mctrl_err("Not enough args for -b");
            return -1;
        }
        if (flag_idx == 0)
        {
            if (str_to_uint32(argv[optind], &tmp))
            {
                return -1;
            }
            cmd->config.airtime_min_us = tmp;
        }
        if (flag_idx == 1)
        {
            if (str_to_uint32(argv[optind], &tmp))
            {
                return -1;
            }
            cmd->config.airtime_max_us = tmp;
        }
        flag_idx++;
    }

    if (((cmd->config.airtime_min_us > cmd->config.airtime_max_us) &&
         (cmd->config.airtime_max_us != AIRTIME_UNLIMITED)) ||
         (cmd->config.airtime_min_us == cmd->config.airtime_max_us))
    {
        mctrl_err("airtime_min (%d) must be < airtime max (%d), or airtime max must be 0\n",
               cmd->config.airtime_min_us,
               cmd->config.airtime_max_us);
        return -1;
    }

    cmd->set_cfgs |= SET_MPSW_CFG_AIRTIME_BOUNDS;
    cmd->config.airtime_min_us = htole32(cmd->config.airtime_min_us);
    cmd->config.airtime_max_us = htole32(cmd->config.airtime_max_us);
    return 0;
}


int mpsw(struct morsectrl *mors, int argc, char *argv[])
{
    int ret = -1;
    int option;
    int enable;
    uint32_t tmp;

    struct command_mpsw_cfg_req *cmd_mpsw;
    struct command_mpsw_cfg_cfm *rsp_mpsw;
    struct morsectrl_transport_buff *cmd_tbuff = NULL;
    struct morsectrl_transport_buff *rsp_tbuff = NULL;

    if (argc == 0)
    {
        usage(mors);
        return 0;
    }

    cmd_tbuff = morsectrl_transport_cmd_alloc(mors->transport, sizeof(*cmd_mpsw));
    rsp_tbuff = morsectrl_transport_resp_alloc(mors->transport, sizeof(*rsp_mpsw));
    if (!cmd_tbuff || !rsp_tbuff)
    {
        ret = -1;
        goto exit;
    }

    cmd_mpsw = TBUFF_TO_CMD(cmd_tbuff, struct command_mpsw_cfg_req);
    rsp_mpsw = TBUFF_TO_RSP(rsp_tbuff, struct command_mpsw_cfg_cfm);

    if (cmd_mpsw == NULL ||
        rsp_mpsw == NULL)
    {
        goto exit;
    }

    memset(cmd_mpsw, 0, sizeof(*cmd_mpsw));

    while ((option = getopt(argc, argv, "b:w:e:")) != -1)
    {
        switch (option)
        {
            case 'b':
                ret = parse_bounds_flag_args(argv, argc, cmd_mpsw);
                if (ret < 0)
                {
                    mctrl_err("Failed to parse values for -b\n");
                    goto exit;
                }
                break;
            case 'w':
                if (str_to_uint32(optarg, &tmp))
                {
                    mctrl_err("Invalid value for -w\n");
                    ret = -1;
                    goto exit;
                }
                cmd_mpsw->set_cfgs |= SET_MPSW_CFG_PKT_SPC_WIN_LEN;
                cmd_mpsw->config.packet_space_window_length_us = htole32(tmp);
                break;
            case 'e':
                enable = expression_to_int(optarg);
                if (enable == -1)
                {
                    mctrl_err("Invalid value (%d) for -e\n", enable);
                    ret = -1;
                    goto exit;
                }
                cmd_mpsw->set_cfgs |= SET_MPSW_CFG_ENABLED;
                cmd_mpsw->config.enable = enable;
                break;
            default:
                usage(mors);
                ret = -1;
                goto exit;
        }
    }

    ret = morsectrl_send_command(mors->transport,
                                 MORSE_COMMAND_MPSW_CONFIG,
                                 cmd_tbuff,
                                 rsp_tbuff);

exit:
    if (ret)
    {
        mctrl_err("Command error (%d)\n", ret);
    }
    else
    {
        print_mpsw_cfg(&rsp_mpsw->config);
    }

    morsectrl_transport_buff_free(cmd_tbuff);

    morsectrl_transport_buff_free(rsp_tbuff);

    return ret;
}

MM_CLI_HANDLER(mpsw, MM_INTF_REQUIRED, MM_DIRECT_CHIP_SUPPORTED);
