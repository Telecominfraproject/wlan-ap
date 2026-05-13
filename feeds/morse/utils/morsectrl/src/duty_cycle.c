/*
 * Copyright 2022 Morse Micro
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

/* Limits on duty cycle, expressed in  percent */
#define DUTY_CYCLE_MIN      (0.01)
#define DUTY_CYCLE_MAX      (100.0)

#define DUTY_CYCLE_SET_CFG_DUTY_CYCLE         BIT(0)
#define DUTY_CYCLE_SET_CFG_OMIT_CONTROL_RESP  BIT(1)
#define DUTY_CYCLE_SET_CFG_EXT                BIT(2)
#define DUTY_CYCLE_SET_CFG_BURST_RECORD_UNIT  BIT(3)

enum duty_cycle_mode
{
    DUTY_CYCLE_MODE_SPREAD = 0,
    DUTY_CYCLE_MODE_BURST = 1,
    DUTY_CYCLE_MODE_LAST = DUTY_CYCLE_MODE_BURST,
};

struct PACKED duty_cycle_configuration
{
    /** Omit control responses from duty cycle budget */
    uint8_t omit_control_responses;
    /** Target duty cycle in 100th of a %, i.e. 1..10000 */
    uint32_t duty_cycle;
};

struct PACKED duty_cycle_set_configuration_ext
{
    /** The length of each burst record in the window (us) - applicable in burst mode only */
    uint32_t burst_record_unit_us;
    /** Duty cycle mode, see @ref enum duty_cycle_mode */
    uint8_t mode;
};

struct PACKED duty_cycle_configuration_ext
{
    /** Airtime remaining (us) - applicable in burst mode only */
    uint32_t airtime_remaining_us;
    /** Burst window duration (us) - applicable in burst mode only */
    uint32_t burst_window_duration_us;
    /** Extension parameters that are configured */
    struct duty_cycle_set_configuration_ext set;
};

/** Set duty cycle command */
struct PACKED command_set_duty_cycle_req
{
    struct duty_cycle_configuration config;
    uint8_t set_cfgs;
    struct duty_cycle_set_configuration_ext config_ext;
};

struct PACKED command_get_duty_cycle_cfm
{
    struct duty_cycle_configuration config;
    struct duty_cycle_configuration_ext config_ext;
};

enum duty_cycle_cmd
{
    DUTY_CYCLE_CMD_DISABLE,
    DUTY_CYCLE_CMD_ENABLE,
    DUTY_CYCLE_CMD_AIRTIME
};

static void usage(struct morsectrl *mors)
{
    mctrl_print("\tduty_cycle <command>\tconfigure duty cycle mode. ");
    mctrl_print("omit command to retrieve settings.\n");
    mctrl_print("\t\tenable\t<value> [options]\n");
    mctrl_print("\t\t\t<value> set duty cycle in %% (%.2f-%.2f)\n",
        DUTY_CYCLE_MIN, DUTY_CYCLE_MAX);
    mctrl_print("\t\t\t-m <mode> mode of operation (0:spread, 1:burst). ");
    mctrl_print("default:0\n");
#if !defined(MORSE_CLIENT)
    mctrl_print("\t\t\t-u <unit> time unit of each burst record entry (us)\n");
#endif
    mctrl_print("\t\t\t-o enables or disables omitting control responses ");
    mctrl_print("from the duty cycle budget.\n");
    mctrl_print("\t\tdisable\n");
    mctrl_print("\t\tairtime\treturn remaining airtime (us), (burst mode only)\n");
}

static int duty_cycle_parse_cmd(const char *str)
{
    if (strcmp("enable", str) == 0)
        return DUTY_CYCLE_CMD_ENABLE;

    if (strcmp("disable", str) == 0)
        return DUTY_CYCLE_CMD_DISABLE;

    if (strcmp("airtime", str) == 0)
        return DUTY_CYCLE_CMD_AIRTIME;

    return -1;
}

static int get_duty_cycle(struct morsectrl *mors, bool burst_airtime_only)
{
    int ret = -1;
    struct command_set_duty_cycle_req *cmd;
    struct command_get_duty_cycle_cfm *resp;
    struct morsectrl_transport_buff *cmd_tbuff =
        morsectrl_transport_cmd_alloc(mors->transport, sizeof(*cmd));
    struct morsectrl_transport_buff *rsp_tbuff =
        morsectrl_transport_resp_alloc(mors->transport, sizeof(*resp));

    if (!cmd_tbuff || !rsp_tbuff)
        goto exit;

    cmd = TBUFF_TO_CMD(cmd_tbuff, struct command_set_duty_cycle_req);
    resp = TBUFF_TO_RSP(rsp_tbuff, struct command_get_duty_cycle_cfm);

    ret = morsectrl_send_command(mors->transport, MORSE_COMMAND_GET_DUTY_CYCLE,
                                     cmd_tbuff, rsp_tbuff);

    if (ret < 0)
    {
        mctrl_err("Failed to read duty cycle: error (%d)\n", ret);
        goto exit;
    }

    if (burst_airtime_only)
    {
        if (resp->config_ext.set.mode == DUTY_CYCLE_MODE_BURST)
        {
            mctrl_print("%u\n", resp->config_ext.airtime_remaining_us);
        }
        else
        {
            mctrl_err("Command not supported when in spread mode\n");
            ret = -1;
        }

        goto exit;
    }

    mctrl_print("Mode: %s\n",
            (resp->config_ext.set.mode == DUTY_CYCLE_MODE_BURST) ? "burst" : "spread");
    mctrl_print("Configured duty cycle: %.2f%%\n", (float)(resp->config.duty_cycle) / 100);
    mctrl_print("Control responses omitted from duty cycle calculation: %d\n",
            resp->config.omit_control_responses);

    if (resp->config_ext.set.mode == DUTY_CYCLE_MODE_BURST)
    {
        mctrl_print("Airtime remaining (us): %u\n", resp->config_ext.airtime_remaining_us);
        mctrl_print("Burst window duration (us): %u\n",
                resp->config_ext.burst_window_duration_us);
    }

exit:
    morsectrl_transport_buff_free(cmd_tbuff);
    morsectrl_transport_buff_free(rsp_tbuff);
    return ret;
}

static int set_duty_cycle(struct morsectrl *mors, struct duty_cycle_configuration *cfg,
                          struct duty_cycle_set_configuration_ext *cfg_ext, uint8_t set_cfgs)
{
    int ret = -1;
    struct command_set_duty_cycle_req *cmd;
    struct morsectrl_transport_buff *cmd_tbuff =
        morsectrl_transport_cmd_alloc(mors->transport, sizeof(*cmd));
    struct morsectrl_transport_buff *rsp_tbuff =
        morsectrl_transport_resp_alloc(mors->transport, 0);

    if (!cmd_tbuff || !rsp_tbuff)
        goto exit_set;

    cmd = TBUFF_TO_CMD(cmd_tbuff, struct command_set_duty_cycle_req);

    memset(cmd, 0, sizeof(*cmd));

    /* Range checked by caller */
    cmd->set_cfgs = set_cfgs;
    cmd->config.duty_cycle = cfg->duty_cycle;
    cmd->config.omit_control_responses = cfg->omit_control_responses;

    if (cmd->set_cfgs & DUTY_CYCLE_SET_CFG_EXT)
    {
        cmd->config_ext.mode = cfg_ext->mode;
        if (cmd->set_cfgs & DUTY_CYCLE_SET_CFG_BURST_RECORD_UNIT)
        {
            cmd->config_ext.burst_record_unit_us = cfg_ext->burst_record_unit_us;
        }
    }

    /* Send duty cycle command directly to the firmware if driver commands are not supported. */
    if (morsectrl_transport_has_driver(mors->transport))
    {
        ret = morsectrl_send_command(mors->transport, MORSE_COMMAND_DRIVER_SET_DUTY_CYCLE,
                                     cmd_tbuff, rsp_tbuff);
    }
    else
    {
        ret = morsectrl_send_command(mors->transport, MORSE_COMMAND_SET_DUTY_CYCLE,
                                     cmd_tbuff, rsp_tbuff);
    }

exit_set:
    if (ret < 0)
    {
        mctrl_err("Failed to set duty cycle: error (%d)\n", ret);
    }
    morsectrl_transport_buff_free(cmd_tbuff);
    morsectrl_transport_buff_free(rsp_tbuff);
    return ret;
}

int duty_cycle(struct morsectrl *mors, int argc, char *argv[])
{
    int option;
    struct duty_cycle_configuration cfg = { 0 };
    struct duty_cycle_set_configuration_ext cfg_ext = { 0 };
    uint8_t set_cfgs = 0;

    if (argc == 0)
    {
        usage(mors);
        return 0;
    }

    if (argc == 1)
    {
        /* No command supplied, user wants to get duty cycle settings */
        return get_duty_cycle(mors, false);
    }

    int cmd = duty_cycle_parse_cmd(argv[1]);
    if (cmd == -1)
    {
        mctrl_err("Invalid command\n");
        usage(mors);
        return -1;
    }

    if (cmd == DUTY_CYCLE_CMD_AIRTIME)
    {
        /* User want's to get airtime information */
        return get_duty_cycle(mors, true);
    }
    if (cmd == DUTY_CYCLE_CMD_ENABLE)
    {
        float duty_cycle;

        if (argc < 3)
        {
            mctrl_err("Invalid command parameters\n");
            usage(mors);
            return -1;
        }

        /* Specify what is being set in this command */
        set_cfgs |= DUTY_CYCLE_SET_CFG_DUTY_CYCLE;
        set_cfgs |= DUTY_CYCLE_SET_CFG_EXT;

        /* Parse duty cycle settings */
        duty_cycle = strtof(argv[2], NULL);
        if ((duty_cycle < (float)DUTY_CYCLE_MIN) || (duty_cycle > (float)DUTY_CYCLE_MAX))
        {
            mctrl_err("Invalid duty cycle %f (%.2f-%.2f).\n",
                    duty_cycle, DUTY_CYCLE_MIN, DUTY_CYCLE_MAX);
            usage(mors);
            return -1;
        }

        cfg.duty_cycle = (uint32_t)(duty_cycle * 100);
        cfg_ext.mode = DUTY_CYCLE_MODE_SPREAD; /* default mode */

        argc -= 2;
        argv += 2;
        while ((option = getopt(argc, argv, "m:u:o")) != -1)
        {
            switch (option)
            {
                case 'o':
                {
                    cfg.omit_control_responses = 1;
                    set_cfgs |= DUTY_CYCLE_SET_CFG_OMIT_CONTROL_RESP;
                    break;
                }
                case 'm':
                {
                    uint8_t val;

                    if (str_to_uint8_range(optarg, &val, 0, DUTY_CYCLE_MODE_LAST) < 0)
                    {
                        mctrl_err("Duty cycle mode of operation not valid\n");
                        usage(mors);
                        return -1;
                    }

                    cfg_ext.mode = val;
                    break;
                }
#if !defined(MORSE_CLIENT)
                case 'u':
                {
                    uint32_t val;

                    if (str_to_uint32(optarg, &val) < 0)
                    {
                        mctrl_err("Invalid value for the unit of burst mode records\n");
                        usage(mors);
                        return -1;
                    }

                    cfg_ext.burst_record_unit_us = val;
                    set_cfgs |= DUTY_CYCLE_SET_CFG_BURST_RECORD_UNIT;
                    break;
                }
#endif
                default:
                {
                    mctrl_err("Unknown option to enable command\n");
                    usage(mors);
                    return -1;
                }
            }
        }
    }
    else if (cmd == DUTY_CYCLE_CMD_DISABLE)
    {
        set_cfgs |= DUTY_CYCLE_SET_CFG_DUTY_CYCLE;
        cfg.duty_cycle = (uint32_t)(100 * 100); /* 100% dc indicates disabled */
    }

    return set_duty_cycle(mors, &cfg, &cfg_ext, set_cfgs);
}

MM_CLI_HANDLER(duty_cycle, MM_INTF_REQUIRED, MM_DIRECT_CHIP_SUPPORTED);
