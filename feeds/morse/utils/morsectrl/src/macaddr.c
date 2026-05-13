/*
 * Copyright 2022 Morse Micro
 */

#include <stdio.h>
#include <unistd.h>

#include "command.h"
#include "utilities.h"

struct PACKED command_mac_addr_req
{
    uint8_t write;
    uint8_t mac_octet[MAC_ADDR_LEN];
};

struct PACKED command_mac_addr_cfm
{
    uint8_t mac_octet[MAC_ADDR_LEN];
};

static void usage(struct morsectrl *mors)
{
    mctrl_print(
        "\tmacaddr [-w <mac_addr>]\treads the MAC address of the chip if -w was not passed\n");
    mctrl_print(
        "\t\t-w <mac_addr>\twrites the given 'XX:XX:XX:XX:XX:XX' MAC  address to the chip\n");
    mctrl_print("\t\t\t\t(this is not reversible)\n");
}

int macaddr(struct morsectrl *mors, int argc, char *argv[])
{
    int ret = -1, option;
    struct command_mac_addr_req *cmd;
    struct command_mac_addr_cfm *resp;
    struct morsectrl_transport_buff *cmd_tbuff;
    struct morsectrl_transport_buff *rsp_tbuff;

    if (!argc)
    {
        usage(mors);
        return 0;
    }

    cmd_tbuff = morsectrl_transport_cmd_alloc(mors->transport, sizeof(*cmd));
    rsp_tbuff = morsectrl_transport_resp_alloc(mors->transport, sizeof(*resp));
    if (!cmd_tbuff || !rsp_tbuff)
        goto exit;

    cmd = TBUFF_TO_CMD(cmd_tbuff, struct command_mac_addr_req);
    resp = TBUFF_TO_RSP(rsp_tbuff, struct command_mac_addr_cfm);
    cmd->write = false;

    switch (argc)
    {
        case 1:
        case 3:
            while ((option = getopt(argc, argv, "w:")) != -1)
            {
                uint8_t *mac_octet = cmd->mac_octet;

                switch (option)
                {
                    case 'w' :
                        cmd->write = true;
                        if (str_to_mac_addr(mac_octet, optarg) < 0)
                        {
                            mctrl_err("Invalid MAC address\n");
                            ret = -1;
                            goto exit;
                        }
                        break;
                    default :
                        usage(mors);
                        goto exit;
                }
            }
            break;
        default:
            mctrl_err("Invalid arguments\n");
            usage(mors);
            goto exit;
    }

    ret = morsectrl_send_command(mors->transport, MORSE_COMMAND_MAC_ADDR,
                                 cmd_tbuff, rsp_tbuff);
exit:
    if (ret)
        mctrl_err("Command macaddr Failed(%d)\n", ret);
    else
    {
        uint8_t *mac_octet = resp->mac_octet;
        mctrl_print("Chip MAC address: %02x:%02x:%02x:%02x:%02x:%02x\n",
               mac_octet[0], mac_octet[1], mac_octet[2], mac_octet[3],
               mac_octet[4], mac_octet[5]);
    }
    morsectrl_transport_buff_free(cmd_tbuff);
    morsectrl_transport_buff_free(rsp_tbuff);
    return ret;
}

MM_CLI_HANDLER(macaddr, MM_INTF_REQUIRED, MM_DIRECT_CHIP_SUPPORTED);
