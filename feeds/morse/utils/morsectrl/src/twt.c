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

#define TWT_WAKE_DURATION_UNIT                      (256)
#define TWT_WAKE_INTERVAL_EXPONENT_MAX_VAL          (31)
#define TWT_WAKE_DURATION_MAX_US                    (65280) /* UINT8_MAX * TWT_WAKE_DURATION_UNIT */
#define TWT_MAX_SETUP_COMMAND_VAL                   (7)
#define TWT_MAX_FLOW_ID_VAL                         (7)

typedef enum {
    TWT_CONF_SUBCMD_CONFIGURE,
    TWT_CONF_SUBCMD_FORCE_INSTALL_AGREEMENT,
    TWT_CONF_SUBCMD_REMOVE_AGREEMENT,
    TWT_CONF_SUBCMD_CONFIGURE_EXPLICIT,
} twt_subcommands_t;

struct PACKED command_set_twt_conf {
    /** The target wake time (TSF) for the first TWT service period */
    uint64_t target_wake_time;
    /** Wake interval (us) */
    union {
        uint64_t wake_interval_us;
        struct {
            uint16_t wake_interval_mantissa;
            uint8_t wake_interval_exponent;
            uint8_t __padding[5];
        } explicit;
    };
    /** Minimum wake duration during TWT service period (us) */
    uint32_t wake_duration_us;
    /** TWT setup command to use (0: request, 1: suggest, 2: demand) */
    uint8_t twt_setup_command;
    uint8_t __padding[3];
};

struct PACKED command_twt_req {
    /** TWT subcommands, see @ref twt_subcommands_t */
    uint8_t cmd;
    /** The flow (TWT) identifier for the agreement to set, install or remove */
    uint8_t flow_id;
    union {
        uint8_t opaque[0];
        struct command_set_twt_conf set_twt_conf;
    };
};

static void usage(struct morsectrl *mors)
{
    mctrl_print(
        "\ttwt <command>\tinstall or remove a TWT agreement on a STA interface (test only)\n");
    mctrl_print("\t\tconf [options]\n");
    mctrl_print("\t\t    -w <wake interval>\twake interval (us)\n");
    mctrl_print(
            "\t\t    -d <min wake duration>\tminimum wake duration during TWT service period (us). "
            "Max value:%d\n", TWT_WAKE_DURATION_MAX_US);
    mctrl_print("\t\t    -c <setup command>\ttwt setup commad to use "
           "(0: request, 1: suggest, 2: demand)\n");
#if !defined(MORSE_CLIENT)
    mctrl_print("\t\tinstall [options]\n");
    mctrl_print("\t\t    -f <flow id>\tflow id for TWT agreement\n");
    mctrl_print("\t\t    -w <wake interval>\twake interval(us)\n");
    mctrl_print(
            "\t\t    -d <min wake duration>\tminimum wake duration during TWT service period (us). "
            "Max value:%d\n", TWT_WAKE_DURATION_MAX_US);
    mctrl_print("\t\t    -t <target wake time>\tthe target wake time (TSF) for the first TWT "
            "service period\n");
    mctrl_print("\t\texplicit [options]\n");
    mctrl_print(
            "\t\t    -d <min wake duration>\tminimum wake duration during TWT service period (us). "
            "Max value:%d\n", TWT_WAKE_DURATION_MAX_US);
    mctrl_print("\t\t    -m <wake interval mantissa>\twake interval mantissa\n");
    mctrl_print("\t\t    -e <wake interval exponent>\twake interval exponent\n");
    mctrl_print("\t\t    -c <setup command>\ttwt setup commad to use "
           "(0: request, 1: suggest, 2: demand)\n");
    mctrl_print("\t\tremove [options]\n");
    mctrl_print("\t\t    -f <flow id>\tflow id for TWT agreement\n");
#endif
}

static int twt_get_cmd(char str[])
{
    if (strcmp("conf", str) == 0) return TWT_CONF_SUBCMD_CONFIGURE;
#if !defined(MORSE_CLIENT)
    if (strcmp("install", str) == 0) return TWT_CONF_SUBCMD_FORCE_INSTALL_AGREEMENT;
    else if (strcmp("remove", str) == 0) return TWT_CONF_SUBCMD_REMOVE_AGREEMENT;
    else if (strcmp("explicit", str) == 0) return TWT_CONF_SUBCMD_CONFIGURE_EXPLICIT;
#endif
    else
    {
        return -1;
    }
}

int twt(struct morsectrl *mors, int argc, char *argv[])
{
    int cmd_id;
    int ret = -1;
    int option;
    struct morsectrl_transport_buff *cmd_tbuff = NULL;
    struct morsectrl_transport_buff *rsp_tbuff = NULL;
    uint8_t flow_id = 0; /* flow id always set to 0 for now */
    uint32_t wake_duration_us = 0;
    uint64_t wake_interval_us = 0;
    uint16_t wake_interval_mantissa = 0;
    uint8_t wake_interval_exponent = 0;
    uint64_t target_wake_time = 0;
    uint8_t setup_cmd = 0;
    uint32_t temp;

    if (argc == 0)
    {
        usage(mors);
        return 0;
    }

    if (argc < 3)
    {
        mctrl_err("Invalid command parameters\n");
        usage(mors);
        ret = -1;
        goto exit;
    }

    cmd_id = twt_get_cmd(argv[1]);
    if (cmd_id < 0)
    {
        mctrl_err("Invalid TWT command '%s'\n", argv[1]);
        usage(mors);
        ret = -1;
        goto exit;
    }

    rsp_tbuff = morsectrl_transport_resp_alloc(mors->transport, 0);

    if (!rsp_tbuff)
        goto exit;

    argc -= 1;
    argv += 1;

    switch (cmd_id)
    {
        case TWT_CONF_SUBCMD_CONFIGURE:
        case TWT_CONF_SUBCMD_CONFIGURE_EXPLICIT:
        case TWT_CONF_SUBCMD_FORCE_INSTALL_AGREEMENT:
        {
            struct command_twt_req *twt_conf = NULL;

            cmd_tbuff = morsectrl_transport_cmd_alloc(mors->transport, sizeof(*twt_conf));
            if (!cmd_tbuff)
                goto exit;

            twt_conf = TBUFF_TO_CMD(cmd_tbuff, struct command_twt_req);

            while ((option = getopt(argc, argv, "f:w:d:t:c:m:e:")) != -1)
            {
                switch (option)
                {
                    case 'f' :
                        if (str_to_uint32_range(optarg, &temp, 0, TWT_MAX_FLOW_ID_VAL) < 0)
                        {
                            mctrl_err("Flow ID not valid\n");
                            usage(mors);
                            ret = -1;
                            goto exit;
                        }
                        flow_id = temp;
                        break;
                    case 'w' :
                    {
                        if (str_to_uint64(optarg, &wake_interval_us) < 0)
                        {
                            mctrl_err("Wake interval is not a valid uint64_t value\n");
                            usage(mors);
                            ret = -1;
                            goto exit;
                        }

                        break;
                    }
                    case 'd' :
                        if (str_to_uint32_range(optarg, &wake_duration_us,
                            0, TWT_WAKE_DURATION_MAX_US) < 0)
                        {
                            mctrl_err("Wake duration cannot exceed %u us\n",
                                    TWT_WAKE_DURATION_MAX_US);
                            usage(mors);
                            ret = -1;
                            goto exit;
                        }
                        break;
                    case 't':
                    {
                        if (str_to_uint64(optarg, &target_wake_time) < 0)
                        {
                            mctrl_err("Target Wake Time is not a valid uint64_t value\n");
                            usage(mors);
                            ret = -1;
                            goto exit;
                        }
                        break;
                    }
                    case 'c':
                    {
                        if (str_to_uint32_range(optarg, &temp, 0, TWT_MAX_SETUP_COMMAND_VAL) < 0)
                        {
                            mctrl_err("Setup command is not valid\n");
                            usage(mors);
                            ret = -1;
                            goto exit;
                        }
                        setup_cmd = temp;
                        break;
                    }
                    case 'm':
                    {
                        if (str_to_uint16(optarg, &wake_interval_mantissa))
                        {
                            mctrl_err("Wake interval mantissa is not valid\n");
                            usage(mors);
                            ret = -1;
                            goto exit;
                        }
                        break;
                    }
                    case 'e':
                    {
                        if (str_to_uint8_range(optarg, &wake_interval_exponent, 0,
                            TWT_WAKE_INTERVAL_EXPONENT_MAX_VAL) < 0)
                        {
                            mctrl_err("Wake interval exponent cannot exceed %u\n",
                                TWT_WAKE_INTERVAL_EXPONENT_MAX_VAL);
                            usage(mors);
                            ret = -1;
                            goto exit;
                        }
                        break;
                    }
                    default :
                    {
                        mctrl_err("Invalid argument (%c)\n", option);
                        usage(mors);
                        ret = -1;
                        goto exit;
                    }
                }
            }

            twt_conf->flow_id = flow_id;
            twt_conf->cmd = cmd_id;

            if (cmd_id == TWT_CONF_SUBCMD_CONFIGURE_EXPLICIT)
            {
                /* Set here for logging later on */
                wake_interval_us = wake_interval_mantissa * (1ULL << wake_interval_exponent);
                twt_conf->set_twt_conf.explicit.wake_interval_exponent = wake_interval_exponent;
                twt_conf->set_twt_conf.explicit.wake_interval_mantissa = wake_interval_mantissa;
            }
            else
            {
                twt_conf->set_twt_conf.wake_interval_us = wake_interval_us;
            }

            twt_conf->set_twt_conf.wake_duration_us = wake_duration_us;
            twt_conf->set_twt_conf.twt_setup_command = setup_cmd;

            if (cmd_id == TWT_CONF_SUBCMD_FORCE_INSTALL_AGREEMENT)
                twt_conf->set_twt_conf.target_wake_time = target_wake_time;
        }
        break;
        case TWT_CONF_SUBCMD_REMOVE_AGREEMENT:
        {
            struct command_twt_req* remove = NULL;

            cmd_tbuff = morsectrl_transport_cmd_alloc(mors->transport, sizeof(*remove));

            if (!cmd_tbuff)
                goto exit;

            remove = TBUFF_TO_CMD(cmd_tbuff, struct command_twt_req);

            while ((option = getopt(argc, argv, "f:")) != -1)
            {
                switch (option)
                {
                    case 'f' :
                        if (str_to_uint32_range(optarg, &temp, 0, TWT_MAX_FLOW_ID_VAL) < 0)
                        {
                            mctrl_err("Flow ID not valid\n");
                            usage(mors);
                            ret = -1;
                            goto exit;
                        }
                        flow_id = temp;
                        break;
                    default :
                        mctrl_err("Invalid argument (%c)\n", option);
                        usage(mors);
                        ret = -1;
                        goto exit;
                }
            }

            remove->flow_id = flow_id;
            remove->cmd = cmd_id;
        }
        break;
        default:
        {
            mctrl_err("Error: TWT command '%s'\n", argv[1]);
            usage(mors);
            ret = -1;
            goto exit;
        }
    }

    ret = morsectrl_send_command(mors->transport, MORSE_COMMAND_TWT_SET_CONF, cmd_tbuff,
            rsp_tbuff);

exit:
    if (ret)
    {
        mctrl_err("Command error (%d)\n", ret);
    }
    else if (cmd_id == TWT_CONF_SUBCMD_CONFIGURE ||
        cmd_id == TWT_CONF_SUBCMD_CONFIGURE_EXPLICIT ||
        cmd_id == TWT_CONF_SUBCMD_FORCE_INSTALL_AGREEMENT)
    {
        mctrl_print("Installed TWT Agreement[flowid:%d]\n", flow_id);
        mctrl_print("\tWake interval: %" PRId64 " us\n", wake_interval_us);
        mctrl_print("\tWake duration: %d us\n", wake_duration_us);
        mctrl_print("\tTarget Wake Time: %" PRId64 "\n", target_wake_time);
        mctrl_print("\tImplict: true\n");
    }
    else if (cmd_id == TWT_CONF_SUBCMD_REMOVE_AGREEMENT)
    {
        mctrl_print("Removed TWT Agreement[flowid:%d]\n", flow_id);
    }

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

MM_CLI_HANDLER(twt, MM_INTF_REQUIRED, MM_DIRECT_CHIP_NOT_SUPPORTED);
