/*
 * Copyright 2020 Morse Micro
 */

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "portable_endian.h"
#include "command.h"
#include "utilities.h"

enum morse_rpg_commands_id
{
    /* RPG Commands */
    MORSE_CMD_RPG_START_TX          = 0x100,
    MORSE_CMD_RPG_STOP_TX           = 0x101,
    MORSE_CMD_RPG_GET_STATS         = 0x102,
    MORSE_CMD_RPG_RESET_STATS       = 0x103,
    MORSE_CMD_RPG_SET_SOURCE_ADDR   = 0x104,
    MORSE_CMD_RPG_SET_DEST_ADDR     = 0x105,
    MORSE_CMD_RPG_FORCE_AMPDU       = 0x106
};

struct PACKED memcmd_rpg_start_tx
{
    int32_t  size;
    int32_t  count;
    uint8_t  random;
};

struct PACKED memcmd_rpg_get_statistics
{
    uint32_t total_rx_packets;
    uint32_t total_rx_packets_w_correct_fcs;
    uint32_t total_tx_packets;
    uint32_t rx_signal_field_errors;
};

struct PACKED morse_rpg_cmd_set_source_addr
{
    uint8_t source[6];
};

struct PACKED morse_rpg_cmd_set_destination_addr
{
    uint8_t destination[6];
};

struct PACKED morse_rpg_cmd_force_ampdu
{
    uint32_t number;
};

struct PACKED morse_rpg_cmd
{
    uint16_t id;
    union {
        uint8_t opaque[0];
        struct memcmd_rpg_start_tx start;
        struct morse_rpg_cmd_set_source_addr set_source;
        struct morse_rpg_cmd_set_destination_addr set_destination;
        struct morse_rpg_cmd_force_ampdu force_ampdu;
    };
};

static void usage(struct morsectrl *mors)
{
    mctrl_print("\trpg <command>\n");
    mctrl_print("\t\tstart [options] starts rpg\n");
    mctrl_print("\t\t    -l\t\tlistening mode (other options are overlooked)\n");
    mctrl_print("\t\t    -c <value>\tnumber of packets to send (default unlimited)\n"
           "\t\t    -s <value>\tspecifies the size of the packets to be sent (min: 24).\n"
           "\t\t              \tRandom if not specified.\n");
    mctrl_print("\t\t    -d\t\tdisables random packet contents (for casim)\n");
    mctrl_print("\t\tstop\t\tterminates rpg (if started)\n");
    mctrl_print("\t\tstats [options]\treads rpg stats collected (if started)\n");
    mctrl_print("\t\t    -r\t\treset the rpg stats (if started)\n");
    mctrl_print("\t\tsrcaddr [mac address]\n");
    mctrl_print("\t\t\t\tsets source mac address of rpg packets\n");
    mctrl_print("\t\tdstaddr [mac address]\n");
    mctrl_print("\t\t\t\tsets destination mac address of rpg packets\n");
    mctrl_print("\t\tampdu [number]\n");
    mctrl_print("\t\t\t\tforce using ampdu with [number] mpdu\n");
}

int rpg_get_cmd(char str[])
{
    if (strcmp("start", str) == 0) return MORSE_CMD_RPG_START_TX;
    else if (strcmp("stop", str) == 0) return MORSE_CMD_RPG_STOP_TX;
    else if (strcmp("stats", str) == 0) return MORSE_CMD_RPG_GET_STATS;
    else if (strcmp("reset", str) == 0) return MORSE_CMD_RPG_RESET_STATS;
    else if (strcmp("srcaddr", str) == 0) return MORSE_CMD_RPG_SET_SOURCE_ADDR;
    else if (strcmp("dstaddr", str) == 0) return MORSE_CMD_RPG_SET_DEST_ADDR;
    else if (strcmp("ampdu", str) == 0) return MORSE_CMD_RPG_FORCE_AMPDU;
    else
    {
        return -1;
    }
}

int rpg(struct morsectrl *mors, int argc, char *argv[])
{
    int ret = -1;
    struct morse_rpg_cmd *cmd;
    struct memcmd_rpg_get_statistics *stats;
    struct morsectrl_transport_buff *cmd_tbuff;
    struct morsectrl_transport_buff *rsp_tbuff;
    int option;
    int32_t start_count, start_size;

    if (argc < 2)
    {
        usage(mors);
        return -1;
    }

    ret = rpg_get_cmd(argv[1]);
    if ( ret < 0 )
    {
        mctrl_err("Invalid rpg command '%s'\n", argv[1]);
        usage(mors);
        return -1;
    }

    cmd_tbuff = morsectrl_transport_cmd_alloc(mors->transport, sizeof(*cmd));
    rsp_tbuff = morsectrl_transport_resp_alloc(mors->transport, sizeof(*stats));

    if (!cmd_tbuff || !rsp_tbuff)
        goto exit;

    cmd = TBUFF_TO_CMD(cmd_tbuff, struct morse_rpg_cmd);
    stats = TBUFF_TO_RSP(rsp_tbuff, struct memcmd_rpg_get_statistics);
    cmd->id = ret;
    switch (cmd->id)
    {
        case MORSE_CMD_RPG_START_TX:
        {
            cmd->start.random = 1;
            argc -= 1;
            argv += 1;

            cmd->start.count = -1;
            cmd->start.size = -1;
            while ((option = getopt(argc, argv, "c:s:dl")) != -1)
            {
                switch (option)
                {
                    case 'c' :
                        if (cmd->start.count != -1) {
                            mctrl_err("Conflicting options\n");
                            usage(mors);
                            ret = -1;
                            goto exit;
                        }

                        if (str_to_int32(optarg, &start_count))
                        {
                            mctrl_err("Invalid argument\n");
                            usage(mors);
                            ret = -1;
                            goto exit;
                        }
                        cmd->start.count = start_count;
                        break;
                    case 's' :
                        if (cmd->start.size != -1) {
                            mctrl_err("Conflicting options\n");
                            usage(mors);
                            ret = -1;
                            goto exit;
                        }

                        if (str_to_int32(optarg, &start_size))
                        {
                            mctrl_err("Invalid argument\n");
                            usage(mors);
                            ret = -1;
                            goto exit;
                        }
                        cmd->start.size = start_size;
                        break;
                    case 'l' :
                        if (cmd->start.size != -1 || cmd->start.count != -1) {
                            mctrl_err("Conflicting options\n");
                            usage(mors);
                            ret = -1;
                            goto exit;
                        }
                        cmd->start.size = 0;
                        cmd->start.count = 0;
                        break;
                    case 'd' :
                        cmd->start.random = 0;
                        break;
                    case '?' :
                        usage(mors);
                        ret = -1;
                        goto exit;
                    default :
                        mctrl_err("Invalid argument\n");
                        usage(mors);
                        ret = -1;
                        goto exit;
                }
            }
        }
        break;
        case MORSE_CMD_RPG_SET_SOURCE_ADDR:
        {
            uint8_t* mac = cmd->set_source.source;

            if (argc < 3)
            {
                usage(mors);
                ret = -1;
                goto exit;
            }

            if (str_to_mac_addr(mac, argv[2]) < 0)
            {
                usage(mors);
                ret = -1;
                goto exit;
            }
        }
        break;
        case MORSE_CMD_RPG_SET_DEST_ADDR:
        {
            uint8_t* mac = cmd->set_destination.destination;

            if (argc < 3)
            {
                usage(mors);
                ret = -1;
                goto exit;
            }

            if (str_to_mac_addr(mac, argv[2]) < 0)
            {
                usage(mors);
                ret = -1;
                goto exit;
            }
        }
        break;
        case MORSE_CMD_RPG_FORCE_AMPDU:
        {
            if (argc < 3)
            {
                usage(mors);
                ret = -1;
                goto exit;
            }

            ret = sscanf(argv[2], "%uld", &(cmd->force_ampdu.number));

            if (ret != 1)
            {
                usage(mors);
                ret = -1;
                goto exit;
            }
        }
        break;
        case MORSE_CMD_RPG_GET_STATS:
        {
            argc -= 1;
            argv += 1;

            while ((option = getopt(argc, argv, "r")) != -1)
            {
                switch (option)
                {
                    case 'r' :
                        cmd->id = MORSE_CMD_RPG_RESET_STATS;
                        break;
                    case '?' :
                        usage(mors);
                        ret = -1;
                        goto exit;
                    default :
                        mctrl_err("Invalid argument\n");
                        usage(mors);
                        ret = -1;
                        goto exit;
                }
            }
        }
        break;
        case MORSE_CMD_RPG_RESET_STATS:
            mctrl_err("rpg reset is deprecated and replaced with rpg stats -r\n");
            /* fall through */
        default:
        {
            /* remaining commands take no argument or options */
            if (argc != 2)
            {
                mctrl_err("Error: rpg command '%s' takes no arguments\n", argv[1]);
                usage(mors);
                ret = -1;
                goto exit;
            }
        }
    }

    cmd->start.size = htole32(cmd->start.size);
    cmd->start.count = htole32(cmd->start.count);
    cmd->id = htole16(cmd->id);

    ret = morsectrl_send_command(mors->transport, MORSE_COMMAND_RPG,
                                 cmd_tbuff, rsp_tbuff);

    if ((!ret) && (cmd->id == MORSE_CMD_RPG_GET_STATS))
    {
        mctrl_print("total Rx = %d, RX FCS pass = %d, RX sig field fail = %d, total TX = %d\n",
               (int)stats->total_rx_packets,
               (int)stats->total_rx_packets_w_correct_fcs,
               (int)stats->rx_signal_field_errors,
               (int)stats->total_tx_packets);
    }

exit:
    if (ret < 0)
    {
        mctrl_err("Command rpg error (%d)\n", ret);
    }

    morsectrl_transport_buff_free(cmd_tbuff);
    morsectrl_transport_buff_free(rsp_tbuff);
    return ret;
}

MM_CLI_HANDLER(rpg, MM_INTF_REQUIRED, MM_DIRECT_CHIP_SUPPORTED);
