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

#define MESH_ID_LEN_MAX 32

#define MESH_BEACONLESS_MODE_DISABLE 0
#define MESH_BEACONLESS_MODE_ENABLE 1
#define PEER_LINKS_MIN 0
#define PEER_LINKS_MAX 10

struct PACKED set_mesh_config
{
    /** Mesh ID Len */
    uint8_t mesh_id_len;

    /** Mesh ID, equivalent to SSID in infra */
    uint8_t mesh_id[MESH_ID_LEN_MAX];

    /** Mesh beaconless mode enabled/disabled */
    uint8_t mesh_beaconless_mode;

    /** Maximum number of peer links */
    uint8_t max_plinks;
};

static void usage(struct morsectrl *mors)
{
    mctrl_print("\tmesh_config -m <mesh id> [-b <beaconless mode>] -p <max_peer_links>\n");
    mctrl_print("\t\t\tConfigure Mesh\n");
    mctrl_print("\t\t-m <value>\tMesh ID as a hex string\n");
    mctrl_print("\t\t-b <value>\tMesh beaconless mode. 1: enable, 0: disable\n");
    mctrl_print("\t\t-p <value>\tMaximum number of peer links. Min:%u, Max:%u\n",
        PEER_LINKS_MIN, PEER_LINKS_MAX);
    mctrl_print("\t\t\t\tdo not use - for internal use by wpa_supplicant\n");
}

int mesh_config(struct morsectrl *mors, int argc, char *argv[])
{
    int ret = -1;
    int option;
    size_t length = 0;
    struct set_mesh_config *cmd;
    struct morsectrl_transport_buff *cmd_tbuff;
    struct morsectrl_transport_buff *rsp_tbuff;
    uint8_t temp = 0;

    if (argc == 0)
    {
        usage(mors);
        return 0;
    }

    if (argc < 5 || argc > 7)
    {
        usage(mors);
        return -1;
    }

    cmd_tbuff = morsectrl_transport_cmd_alloc(mors->transport, sizeof(*cmd));
    rsp_tbuff = morsectrl_transport_resp_alloc(mors->transport, 0);

    if (!cmd_tbuff || !rsp_tbuff)
        goto exit;

    cmd = TBUFF_TO_CMD(cmd_tbuff, struct set_mesh_config);

    memset(cmd, 0, sizeof(*cmd));

    while ((option = getopt(argc, argv, "m:b:p:")) != -1)
    {
        switch (option) {
        case 'b' :
            if (str_to_uint8_range(optarg, &temp,
                MESH_BEACONLESS_MODE_DISABLE, MESH_BEACONLESS_MODE_ENABLE) < 0)
            {
                mctrl_err("Mesh beaconless mode %d must be either %u or %u\n",
                        temp,
                        MESH_BEACONLESS_MODE_DISABLE,
                        MESH_BEACONLESS_MODE_ENABLE);
                usage(mors);
                ret = -1;
                goto exit;
            }
            cmd->mesh_beaconless_mode = temp;
            break;
        case 'm' :
            length = strlen(optarg);

            if (!length || (length & 1))
            {
                mctrl_err("Invalid Mesh ID hex string length\n");
                ret = -1;
                goto exit;
            }
            length = length / 2;

            if (length > sizeof(cmd->mesh_id))
            {
                mctrl_err("Mesh ID invalid length:%zu, max allowed length is:%zu\n",
                        length, sizeof(cmd->mesh_id));
                ret = -1;
                goto exit;
            }

            if (hexstr2bin(optarg, cmd->mesh_id, length))
            {
                mctrl_err("Invalid Mesh ID hex string\n");
                ret = -1;
                goto exit;
            }
            cmd->mesh_id_len = length;
            break;
        case 'p' :
            if (str_to_uint8_range(optarg, &temp, PEER_LINKS_MIN, PEER_LINKS_MAX) < 0)
            {
                mctrl_err("Max peer links %d must be within in the range min %u max %u\n",
                        temp,
                        PEER_LINKS_MIN,
                        PEER_LINKS_MAX);
                usage(mors);
                ret = -1;
                goto exit;
            }
            cmd->max_plinks = temp;
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

    ret = morsectrl_send_command(mors->transport, MORSE_COMMAND_MESH_CONFIG,
                                 cmd_tbuff, rsp_tbuff);
exit:
    if (ret)
    {
        mctrl_err("Failed to set Mesh Config info\n");
        usage(mors);
    }

    morsectrl_transport_buff_free(cmd_tbuff);
    morsectrl_transport_buff_free(rsp_tbuff);
    return ret;
}

MM_CLI_HANDLER(mesh_config, MM_INTF_REQUIRED, MM_DIRECT_CHIP_NOT_SUPPORTED);
