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

#define S1G_CAPABILITY_FLAGS_WIDTH 4
#define SET_S1G_CAP_FLAGS          BIT(0)
#define SET_S1G_CAP_AMPDU_MSS      BIT(1)
#define SET_S1G_CAP_BEAM_STS       BIT(2)
#define SET_S1G_CAP_NUM_SOUND_DIMS BIT(3)
#define SET_S1G_CAP_MAX_AMPDU_LEXP BIT(4)

struct PACKED mm_capabilities
{
    /** Capability flags */
    uint32_t flags[S1G_CAPABILITY_FLAGS_WIDTH];
    /** The minimum A-MPDU start spacing required by firmware.*/
    uint8_t ampdu_mss;
    /** The beamformee STS capability value */
    uint8_t beamformee_sts_capability;
    /** Number of sounding dimensions */
    uint8_t number_sounding_dimensions;
    /** The maximum A-MPDU length. This is the exponent value such that
     * (2^(13 + exponent) - 1) is the length
     */
    uint8_t maximum_ampdu_length_exponent;
};

struct PACKED command_set_capabilities_req
{
    struct mm_capabilities capabilities;
    /** Which caps we are setting */
    uint8_t set_caps;
};

struct PACKED command_get_capabilities_req
{
};

struct PACKED command_get_capabilities_cfm
{
    struct mm_capabilities capabilities;
    /** Morse custom MMSS (Minimum MPDU Start Spacing) offset */
    uint8_t morse_mmss_offset;
};

static void usage(struct morsectrl *mors)
{
    mctrl_print("\tcapabilities [options]\tget or set the device capabilties manifest\n");
    mctrl_print("\t\tget capabilities if no options supplied, or\n");
    mctrl_print("\t\tset the 128-bit capabilities flags via four 32bit fields:\n");
    mctrl_print("\t\t\t-f <field 1> <field 2> <field 3> <field 4>\n");
    mctrl_print("\t\tset AMPDU capabilities:\n");
    mctrl_print("\t\t\t-a <ampdu minimum start spacing> <ampdu max length exponent>\n");
    mctrl_print("\t\tset beamformee STS value:\n");
    mctrl_print("\t\t\t-b <beamformee STS value>\n");
    mctrl_print("\t\tset number of sounding dimensions:\n");
    mctrl_print("\t\t\t-s <number of sounding dimensions>\n");
}

static void print_capabs(struct morsectrl *mors,
    struct command_get_capabilities_cfm *rsp_get_capabs)
{
    struct mm_capabilities *capabs = &rsp_get_capabs->capabilities;

    mctrl_print("Interface: %s\n", morsectrl_transport_get_ifname(mors->transport));

    for (int i = 0; i < MORSE_ARRAY_SIZE(capabs->flags); i++)
        mctrl_print("Flags %u: 0x%x\n", i, capabs->flags[i]);
    mctrl_print("A-MPDU MSS: %u\n", capabs->ampdu_mss);
    mctrl_print("Maximum A-MPDU length exponent: %u\n",
        capabs->maximum_ampdu_length_exponent);
    mctrl_print("Beamformee STS cap: %u\n",
        capabs->beamformee_sts_capability);
    mctrl_print("Number of sounding dimensions: %u\n",
        capabs->number_sounding_dimensions);
    mctrl_print("Custom MMSS (Minimum MPDU Start Spacing) offset: %u\n",
        rsp_get_capabs->morse_mmss_offset);
    return;
}

/* Only call this function when parsing arguments in a getopt while loop context */
static int parse_capability_flag_args(char *argv[], int argc,
                                      struct command_set_capabilities_req *cmd)
{
    int ret = 0;
    int flag_idx = 0;
    char *endptr;

    cmd->set_caps |= SET_S1G_CAP_FLAGS;
    /* Get the args manually - requires decrementing the optind */
    optind--;
    for (; optind < argc && *argv[optind] != '-'; optind++)
    {
        if (flag_idx == S1G_CAPABILITY_FLAGS_WIDTH)
            break;
        if (!argv[optind] || *argv[optind] == '-')
        {
            mctrl_err("Not enough args for -f");
            ret = -1;
            return ret;
        }
        errno = 0;
        cmd->capabilities.flags[flag_idx] = htole32(strtol(argv[optind], &endptr, 0));
        if (errno == ERANGE || endptr == argv[optind])
        {
            mctrl_err("Error when parsing capability flag %d\n", flag_idx + 1);
            ret = -1;
            return ret;
        }
        flag_idx++;
    }
    return 0;
}

static int parse_ampdu_flag_args(char *argv[],
                                 struct command_set_capabilities_req *cmd)
{
    int ret = 0;
    uint8_t tmp;

    cmd->set_caps |= SET_S1G_CAP_AMPDU_MSS;
    if (str_to_uint8(optarg, &tmp))
    {
        mctrl_err("Invalid AMPDU minimum start spacing\n");
        ret = -1;
        return ret;
    }
    cmd->capabilities.ampdu_mss = tmp;

    if (!argv[optind] || *argv[optind] == '-')
    {
        mctrl_err("Not enough args for -a\n");
        ret = -1;
        return ret;
    }
    cmd->set_caps |= SET_S1G_CAP_MAX_AMPDU_LEXP;

    if (str_to_uint8(argv[optind], &tmp))
    {
        mctrl_err("Invalid AMPDU max length exponent\n");
        ret = -1;
        return ret;
    }
    cmd->capabilities.maximum_ampdu_length_exponent = tmp;

    /* Need to manually increment the optind after getting the second sub-arg */
    optind++;
    return ret;
}

int capabilities(struct morsectrl *mors, int argc, char *argv[])
{
    int ret = -1;
    int option;
    bool is_get = false;
    uint8_t tmp;
    struct command_get_capabilities_req *cmd_get_capabs;
    struct command_get_capabilities_cfm *rsp_get_capabs;
    struct command_set_capabilities_req *cmd_set_capabs;
    struct morsectrl_transport_buff *cmd_get_tbuff;
    struct morsectrl_transport_buff *rsp_get_tbuff;
    struct morsectrl_transport_buff *cmd_set_tbuff;
    struct morsectrl_transport_buff *rsp_set_tbuff;

    if (argc == 0)
    {
        usage(mors);
        return 0;
    }

    cmd_get_tbuff = morsectrl_transport_cmd_alloc(mors->transport, sizeof(*cmd_get_capabs));
    rsp_get_tbuff = morsectrl_transport_resp_alloc(mors->transport, sizeof(*rsp_get_capabs));
    cmd_set_tbuff = morsectrl_transport_cmd_alloc(mors->transport, sizeof(*cmd_set_capabs));
    rsp_set_tbuff = morsectrl_transport_resp_alloc(mors->transport, 0);

    if (!cmd_get_tbuff || !rsp_get_tbuff || !cmd_set_tbuff || !rsp_set_tbuff)
        goto exit;

    cmd_set_capabs = TBUFF_TO_CMD(cmd_set_tbuff, struct command_set_capabilities_req);
    cmd_get_capabs = TBUFF_TO_CMD(cmd_get_tbuff, struct command_get_capabilities_req);
    rsp_get_capabs = TBUFF_TO_RSP(rsp_get_tbuff, struct command_get_capabilities_cfm);

    memset(cmd_set_capabs, 0, sizeof(*cmd_set_capabs));
    memset(cmd_get_capabs, 0, sizeof(*cmd_get_capabs));

    if (argc == 1)
        is_get = true;

    while ((option = getopt(argc, argv, "f:a:b:s:")) != -1)
    {
        switch (option)
        {
        case 'f' :
            ret = parse_capability_flag_args(argv, argc, cmd_set_capabs);
            if (ret < 0)
                goto exit;
            break;
        case 'a' :
            ret = parse_ampdu_flag_args(argv, cmd_set_capabs);
            if (ret < 0)
                goto exit;
            break;
        case 'b' :
            cmd_set_capabs->set_caps |= SET_S1G_CAP_BEAM_STS;
            if (str_to_uint8(optarg, &tmp))
            {
                mctrl_err("Invalid beamformee STS value\n");
                ret = -1;
                goto exit;
            }
            cmd_set_capabs->capabilities.beamformee_sts_capability = tmp;
            break;
        case 's' :
            cmd_set_capabs->set_caps |= SET_S1G_CAP_NUM_SOUND_DIMS;
            if (str_to_uint8(optarg, &tmp))
            {
                mctrl_err("Invalid number of sounding dimensions\n");
                ret = -1;
                goto exit;
            }
            cmd_set_capabs->capabilities.number_sounding_dimensions = tmp;
            break;
        default:
            usage(mors);
            ret = -1;
            goto exit;
        }
    }
    if (is_get)
    {
        ret = morsectrl_send_command(mors->transport,
                                     MORSE_COMMAND_GET_CAPABILITIES,
                                     cmd_get_tbuff, rsp_get_tbuff);

        if (ret >= 0)
            print_capabs(mors, rsp_get_capabs);
    }
    else
    {
        ret = morsectrl_send_command(mors->transport,
                                     MORSE_TEST_SET_CAPABILITIES,
                                     cmd_set_tbuff, rsp_set_tbuff);
    }

exit:
    if (ret < 0)
    {
        mctrl_err("Failed to set capabilities\n");
    }

    morsectrl_transport_buff_free(cmd_set_tbuff);
    morsectrl_transport_buff_free(rsp_set_tbuff);
    morsectrl_transport_buff_free(cmd_get_tbuff);
    morsectrl_transport_buff_free(rsp_get_tbuff);

    return ret;
}

MM_CLI_HANDLER(capabilities, MM_INTF_REQUIRED, MM_DIRECT_CHIP_SUPPORTED);
