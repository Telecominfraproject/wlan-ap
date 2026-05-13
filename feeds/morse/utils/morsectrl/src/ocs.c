/*
 * Copyright 2022 Morse Micro
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "portable_endian.h"
#include "command.h"
#include "channel.h"
#include "transport/transport.h"
#include "utilities.h"

enum ocs_subcmd
{
    OCS_SUBCMD_CONFIG = 1,
    OCS_SUBCMD_STATUS,
};

struct PACKED command_ocs_config_req
{
    uint32_t operating_channel_freq_hz;
    uint8_t operating_channel_bw_mhz;
    uint8_t primary_channel_bw_mhz;
    uint8_t primary_1mhz_channel_index;
};

struct PACKED command_ocs_req
{
    uint32_t subcmd;
    union
    {
        uint8_t opaque[0];
        struct command_ocs_config_req config;
    };
};

struct PACKED command_ocs_status_cfm
{
    uint8_t running;
};

struct PACKED command_ocs_cfm
{
    uint32_t subcmd;
    union
    {
        uint8_t opaque[0];
        struct command_ocs_status_cfm status;
    };
};

static void usage()
{
    mctrl_print("\tocs [config [options] | status]\n");
    mctrl_print("\t\tconfig: sets OCS config\n");
    mctrl_print("\t\t\t-c <value>\tsets channel frequency (kHz)\n");
    mctrl_print("\t\t\t-o <value>\toperating bandwidth (MHz)\n");
    mctrl_print("\t\t\t-p <value>\tprimary bandwidth (MHz)\n");
    mctrl_print("\t\t\t-n <value>\tprimary 1 MHz channel index\n");
    mctrl_print("\t\tstatus: gets OCS status\n");
}

static int ocs_cmd_config(int argc, char *argv[], struct command_ocs_req *req)
{
    int option;
    uint32_t val;

    if (argc != 9)
    {
        return -EINVAL;
    }

    memset(req, 0, sizeof(*req));

    req->subcmd = OCS_SUBCMD_CONFIG;

    while ((option = getopt(argc, argv, "c:o:p:n:")) != -1)
    {
        switch (option)
        {
        case 'c':
            if (str_to_uint32(optarg, &val) < 0)
            {
                return -EINVAL;
            }
            req->config.operating_channel_freq_hz = htole32(KHZ_TO_HZ(val));
            break;
        case 'o':
            if (str_to_uint32(optarg, &val) < 0)
            {
                return -EINVAL;
            }
            req->config.operating_channel_bw_mhz = val;
            break;
        case 'p':
            if (str_to_uint32(optarg, &val) < 0)
            {
                return -EINVAL;
            }
            req->config.primary_channel_bw_mhz = val;
            break;
        case 'n':
            if (str_to_uint32(optarg, &val) < 0)
            {
                return -EINVAL;
            }
            req->config.primary_1mhz_channel_index = val;
            break;
        default:
            return -EINVAL;
        }
    }

    return 0;
}

static int ocs_cmd_status(int argc, char *argv[], struct command_ocs_req *req)
{
    if (argc != 1)
    {
        return -EINVAL;
    }

    req->subcmd = OCS_SUBCMD_STATUS;

    return 0;
}

static void ocs_cfm_status(struct command_ocs_cfm *cfm)
{
    mctrl_print("%s: running %u\n", __func__, cfm->status.running);
}

int ocs(struct morsectrl *mors, int argc, char *argv[])
{
    int ret = 0;
    struct morsectrl_transport_buff *cmd_tbuff;
    struct morsectrl_transport_buff *resp_tbuff;
    struct command_ocs_req *req;
    struct command_ocs_cfm *cfm;
    uint32_t subcmd;

    if (argc < 2)
    {
        usage();
        return -EINVAL;
    }

    --argc;
    ++argv;

    if (!strcmp(argv[0], "config"))
    {
        subcmd = htole32(OCS_SUBCMD_CONFIG);
    }
    else if (!strcmp(argv[0], "status"))
    {
        subcmd = htole32(OCS_SUBCMD_STATUS);
    }
    else
    {
        usage();
        return -EINVAL;
    }

    cmd_tbuff = morsectrl_transport_cmd_alloc(mors->transport, sizeof(*req));
    resp_tbuff = morsectrl_transport_resp_alloc(mors->transport, sizeof(*cfm));
    if (!cmd_tbuff || !resp_tbuff)
    {
        ret = -ENOMEM;
        goto out;
    }

    req = TBUFF_TO_CMD(cmd_tbuff, struct command_ocs_req);
    cfm = TBUFF_TO_RSP(resp_tbuff, struct command_ocs_cfm);

    switch (subcmd)
    {
    case OCS_SUBCMD_CONFIG:
        ret = ocs_cmd_config(argc, argv, req);
        break;
    case OCS_SUBCMD_STATUS:
        ret = ocs_cmd_status(argc, argv, req);
        break;
    }

    if (ret < 0)
    {
        usage();
        goto out;
    }

    ret = morsectrl_send_command(mors->transport, MORSE_COMMAND_OCS_REQ, cmd_tbuff, resp_tbuff);
    if (ret < 0)
    {
        mctrl_err("%s: Error %d in sending %s\n", __func__, ret, STR(MORSE_COMMAND_OCS_REQ));
        goto out;
    }

    switch (cfm->subcmd) {
    case OCS_SUBCMD_CONFIG:
        /* Nothing to do */
        break;
    case OCS_SUBCMD_STATUS:
        ocs_cfm_status(cfm);
        break;
    default:
        /* Should not be possible */
        break;
    }

out:
    morsectrl_transport_buff_free(cmd_tbuff);
    morsectrl_transport_buff_free(resp_tbuff);

    return ret;
}

MM_CLI_HANDLER(ocs, MM_INTF_REQUIRED, MM_DIRECT_CHIP_NOT_SUPPORTED);
