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


enum dhcp_offload_opcode {
    /** Enable the DHCP client */
    MORSE_DHCP_CMD_ENABLE = 0,
    /** Do a DHCP discovery and obtain a lease */
    MORSE_DHCP_CMD_DO_DISCOVERY,
    /** Return the current lease */
    MORSE_DHCP_CMD_GET_LEASE,
    /** Clear the current lease */
    MORSE_DHCP_CMD_CLEAR_LEASE,
    /** Trigger a renewal of the current lease */
    MORSE_DHCP_CMD_RENEW_LEASE,
    /** Trigger a rebinding of the current lease */
    MORSE_DHCP_CMD_REBIND_LEASE,
    /** Ask the FW to send a lease update event to the driver */
    MORSE_DHCP_CMD_SEND_LEASE_UPDATE,
    /** Force uint32 */
    DHCP_CMD_LAST = UINT32_MAX
};

enum dhcp_offload_retcode {
    /** Command completed successfully */
    MORSE_DHCP_RET_SUCCESS = 0,
    /** DHCP Client is disabled */
    MORSE_DHCP_RET_NOT_ENABLED,
    /** DHCP Client is already enabled */
    MORSE_DHCP_RET_ALREADY_ENABLED,
    /** No current bound lease */
    MORSE_DHCP_RET_NO_LEASE,
    /** DHCP client already has a lease */
    MORSE_DHCP_RET_HAVE_LEASE,
    /** DHCP client is currently busy (discovering or renewing) */
    MORSE_DHCP_RET_BUSY,

    /** Force uint32 */
    MORSE_DHCP_RET_LAST = UINT32_MAX
};

struct PACKED command_dhcp_req
{
    uint32_t opcode;
};

struct PACKED command_dhcp_cfm
{
    uint32_t retcode;
    ipv4_addr_t my_ip;
    ipv4_addr_t netmask;
    ipv4_addr_t router;
    ipv4_addr_t dns;
};

static void print_error(enum dhcp_offload_retcode code)
{
    switch (code)
    {
        case MORSE_DHCP_RET_NOT_ENABLED:
        {
            mctrl_err("DHCP client is not enabled\n");
            break;
        }
        case MORSE_DHCP_RET_ALREADY_ENABLED:
        {
            mctrl_err("DHCP client is already enabled\n");
            break;
        }
        case MORSE_DHCP_RET_NO_LEASE:
        {
            mctrl_err("DHCP client does not have a lease\n");
            break;
        }
        case MORSE_DHCP_RET_HAVE_LEASE:
        {
            mctrl_err("DHCP client already has a lease\n");
            break;
        }
        case MORSE_DHCP_RET_BUSY:
        {
            mctrl_err("DHCP client is currently performing a discovery or renewal\n");
            break;
        }

        default:
            mctrl_err("DHCP client threw an error: %d\n", code);
            break;
    }
}

static void usage(struct morsectrl *mors)
{
    mctrl_print("\tdhcpc [enable | discover | get | clear | renew | rebind | update]\t");
    mctrl_print("configure DHCP client\n");
    mctrl_print("\t\tenable - enable DHCP client\n");
    mctrl_print("\t\tdiscover - do a discovery and obtain a lease\n");
    mctrl_print("\t\tget - get the current lease\n");
    mctrl_print("\t\tclear - clear the current lease\n");
    mctrl_print("\t\trenew - renew the current lease\n");
    mctrl_print("\t\trebind - rebind the current lease\n");
    mctrl_print("\t\tupdate - send a lease update to the driver\n");
}

int dhcpc(struct morsectrl *mors, int argc, char *argv[])
{
    int ret = 0;

    struct command_dhcp_req *cmd_dhcp;
    struct command_dhcp_cfm *rsp_dhcp;
    struct morsectrl_transport_buff *cmd_tbuff = NULL;
    struct morsectrl_transport_buff *rsp_tbuff = NULL;

    if (argc == 0)
    {
        usage(mors);
        return 0;
    }

    if (argc != 2)
    {
        mctrl_err("Invalid command parameters\n");
        usage(mors);
        return -1;
    }

    cmd_tbuff = morsectrl_transport_cmd_alloc(mors->transport, sizeof(*cmd_dhcp));
    rsp_tbuff = morsectrl_transport_resp_alloc(mors->transport, sizeof(*rsp_dhcp));
    if (!cmd_tbuff || !rsp_tbuff)
    {
        ret = -1;
        goto exit;
    }

    cmd_dhcp = TBUFF_TO_CMD(cmd_tbuff, struct command_dhcp_req);
    rsp_dhcp = TBUFF_TO_RSP(rsp_tbuff, struct command_dhcp_cfm);

    if (cmd_dhcp == NULL ||
        rsp_dhcp == NULL)
    {
        ret = -1;
        goto exit;
    }

    /* assume vif_id 0 */
    memset(cmd_dhcp, 0, sizeof(*cmd_dhcp));

    if (strcmp(argv[1], "enable") == 0)
    {
        cmd_dhcp->opcode = MORSE_DHCP_CMD_ENABLE;
    }
    else if (strcmp(argv[1], "discover") == 0)
    {
        cmd_dhcp->opcode = MORSE_DHCP_CMD_DO_DISCOVERY;
    }
    else if (strcmp(argv[1], "get") == 0)
    {
        cmd_dhcp->opcode = MORSE_DHCP_CMD_GET_LEASE;
    }
    else if (strcmp(argv[1], "clear") == 0)
    {
        cmd_dhcp->opcode = MORSE_DHCP_CMD_CLEAR_LEASE;
    }
    else if (strcmp(argv[1], "renew") == 0)
    {
        cmd_dhcp->opcode = MORSE_DHCP_CMD_RENEW_LEASE;
    }
    else if (strcmp(argv[1], "rebind") == 0)
    {
        cmd_dhcp->opcode = MORSE_DHCP_CMD_REBIND_LEASE;
    }
    else if (strcmp(argv[1], "update") == 0)
    {
        cmd_dhcp->opcode = MORSE_DHCP_CMD_SEND_LEASE_UPDATE;
    }
    else
    {
        ret = -1;
        mctrl_err("Invalid command parameters\n");
        usage(mors);
        goto exit;
    }

    ret = morsectrl_send_command(mors->transport,
                                 MORSE_COMMAND_DHCP_OFFLOAD,
                                 cmd_tbuff,
                                 rsp_tbuff);

    if (ret < 0)
    {
        mctrl_err("Command error (%d)\n", ret);
        goto exit;
    }
    else if (rsp_dhcp->retcode != MORSE_DHCP_RET_SUCCESS)
    {
        print_error(rsp_dhcp->retcode);
        goto exit;
    }
    else
    {
        if (cmd_dhcp->opcode == MORSE_DHCP_CMD_GET_LEASE)
        {
            mctrl_print("Current DHCP Lease\n");
            mctrl_print("IP Address: %d.%d.%d.%d\n", rsp_dhcp->my_ip.octet[0],
                    rsp_dhcp->my_ip.octet[1], rsp_dhcp->my_ip.octet[2], rsp_dhcp->my_ip.octet[3]);
            mctrl_print("Netmask: %d.%d.%d.%d\n", rsp_dhcp->netmask.octet[0],
                    rsp_dhcp->netmask.octet[1], rsp_dhcp->netmask.octet[2],
                    rsp_dhcp->netmask.octet[3]);
            mctrl_print("Router Address: %d.%d.%d.%d\n", rsp_dhcp->router.octet[0],
                    rsp_dhcp->router.octet[1], rsp_dhcp->router.octet[2],
                    rsp_dhcp->router.octet[3]);
            mctrl_print("DNS Address: %d.%d.%d.%d\n", rsp_dhcp->dns.octet[0],
                    rsp_dhcp->dns.octet[1], rsp_dhcp->dns.octet[2], rsp_dhcp->dns.octet[3]);
        }
    }

exit:
    morsectrl_transport_buff_free(cmd_tbuff);
    morsectrl_transport_buff_free(rsp_tbuff);

    return ret;
}

MM_CLI_HANDLER(dhcpc, MM_INTF_REQUIRED, MM_DIRECT_CHIP_NOT_SUPPORTED);
