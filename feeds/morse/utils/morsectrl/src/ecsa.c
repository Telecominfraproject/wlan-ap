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
#include "channel.h"
#include "utilities.h"

#define OPCLASS_DEFAULT 0xFF

struct PACKED set_ecsa_command
{
    /** Centre frequency of the operating channel */
    uint32_t operating_channel_freq_hz;

    /** Global Operating class */
    uint8_t opclass;

    /** Pimary channel bw in MHz */
    uint8_t primary_channel_bw_mhz;

    /** 1MHz channel index */
    uint8_t prim_1mhz_ch_idx;

    /** Operating channel bandwidth in MHz */
    uint8_t operating_channel_bw_mhz;

    /** Global Operating class for primary chan */
    uint8_t prim_opclass;
};

static void usage(struct morsectrl *mors)
{
    mctrl_print("\tecsa_info [options]\n");
    mctrl_print("\t\t\t\tSet frequency parameters for ECSA ie in probe response and beacon\n");
    mctrl_print("\t\t-g <value>\tglobal operating class\n");
    mctrl_print("\t\t-p <value>\tprimary channel bandwidth in MHz\n");
    mctrl_print("\t\t-n <value>\tprimary 1MHz channel index\n");
    mctrl_print("\t\t-o <value>\tOperating channel bandwidth in MHz\n");
    mctrl_print("\t\t-c <value>\tsets channel frequency in kHz\n");
    mctrl_print("\t\t-l <value>\tglobal operating class for primary channel\n");
}

int ecsa_info(struct morsectrl *mors, int argc, char *argv[])
{
    int ret = -1;
    int option;
    uint32_t freq_khz = 0;
    uint8_t primary_channel_bandwidth = BANDWIDTH_DEFAULT;
    uint8_t op_channel_bandwidth = BANDWIDTH_DEFAULT;
    uint8_t global_operating_class = OPCLASS_DEFAULT;
    uint8_t primary_1Mhz_chan_idx = PRIMARY_1MHZ_CHANNEL_INDEX_DEFAULT;
    uint8_t prim_chan_global_op_class = OPCLASS_DEFAULT;
    struct set_ecsa_command *cmd;
    struct morsectrl_transport_buff *cmd_tbuff;
    struct morsectrl_transport_buff *rsp_tbuff;

    if (argc == 0 || argc != 13)
    {
        usage(mors);
        return 0;
    }

    cmd_tbuff = morsectrl_transport_cmd_alloc(mors->transport, sizeof(*cmd));
    rsp_tbuff = morsectrl_transport_resp_alloc(mors->transport, 0);

    if (!cmd_tbuff || !rsp_tbuff)
        goto exit;

    cmd = TBUFF_TO_CMD(cmd_tbuff, struct set_ecsa_command);
    while ((option = getopt(argc, argv, "g:p:n:o:c:l:")) != -1)
    {
        switch (option)
        {
            case 'g' :
                if (str_to_uint8(optarg, &global_operating_class))
                {
                    mctrl_err("Invalid global operating class\n");
                    usage(mors);
                    goto exit;
                }
                break;
            case 'p' :
                if (str_to_uint8(optarg, &primary_channel_bandwidth))
                {
                    mctrl_err("Invalid primary channel bandwidth\n");
                    usage(mors);
                    goto exit;
                }
                break;
            case 'n' :
                if (str_to_uint8(optarg, &primary_1Mhz_chan_idx))
                {
                    mctrl_err("Invalid primary channel index\n");
                    usage(mors);
                    goto exit;
                }
                break;
            case 'o' :
                if (str_to_uint8(optarg, &op_channel_bandwidth))
                {
                    mctrl_err("Invalid op channel bandwidth\n");
                    usage(mors);
                    goto exit;
                }
                break;
            case 'c' :
                if (str_to_uint32_range(optarg, &freq_khz, MIN_FREQ_KHZ, MAX_FREQ_KHZ))
                {
                    mctrl_err("Invalid channel frequency %d. Must be between %d kHz and %d kHz\n",
                                freq_khz, MIN_FREQ_KHZ, MAX_FREQ_KHZ);
                    usage(mors);
                    goto exit;
                }
                break;
            case 'l':
                if (str_to_uint8(optarg, &prim_chan_global_op_class))
                {
                    mctrl_err("Invalid primary channel global op class\n");
                    usage(mors);
                    goto exit;
                }
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

    if (!freq_khz)
    {
        mctrl_err("Channel frequency [-c] option must be specified\n");
        usage(mors);
        goto exit;
    }

    if ((primary_channel_bandwidth == BANDWIDTH_DEFAULT) ||
        (global_operating_class == OPCLASS_DEFAULT) ||
        (prim_chan_global_op_class == OPCLASS_DEFAULT) ||
        (op_channel_bandwidth == BANDWIDTH_DEFAULT) ||
        (primary_1Mhz_chan_idx == PRIMARY_1MHZ_CHANNEL_INDEX_DEFAULT))
    {
        mctrl_err("Invalid input parameters: \n"
            "* primary_channel_bandwidth %d \n"
            "* global_operating_class %d \n"
            "* primary_ch_global_op_class %d \n"
            "* op_channel_bandwidth %d \n"
            "* primary_1Mhz_chan_idx %d \n \n",
            primary_channel_bandwidth,
            global_operating_class,
            prim_chan_global_op_class,
            op_channel_bandwidth,
            primary_1Mhz_chan_idx);
        usage(mors);
        goto exit;
    }



    cmd->primary_channel_bw_mhz = primary_channel_bandwidth;
    cmd->opclass = global_operating_class;
    cmd->prim_1mhz_ch_idx = primary_1Mhz_chan_idx;
    cmd->operating_channel_freq_hz = htole32(KHZ_TO_HZ(freq_khz));
    cmd->operating_channel_bw_mhz = op_channel_bandwidth;
    cmd->prim_opclass = prim_chan_global_op_class;

    ret = morsectrl_send_command(mors->transport, MORSE_COMMAND_SET_ECSA_S1G_INFO,
                                 cmd_tbuff, rsp_tbuff);

exit:
    if (ret)
    {
        mctrl_err("Failed to set ecsa info\n");
    }

    morsectrl_transport_buff_free(cmd_tbuff);
    morsectrl_transport_buff_free(rsp_tbuff);
    return ret;
}

MM_CLI_HANDLER(ecsa_info, MM_INTF_REQUIRED, MM_DIRECT_CHIP_NOT_SUPPORTED);
