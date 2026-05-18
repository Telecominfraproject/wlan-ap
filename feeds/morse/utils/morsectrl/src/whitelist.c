/*
 * Copyright 2024 Morse Micro
 */

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include "portable_endian.h"
#if defined(__WINDOWS__)
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#endif

#include "command.h"
#include "transport/transport.h"

#define IPADDR_STR_LEN (16)

#define WHITELIST_FLAGS_CLEAR BIT(0)

#define WHITELIST_PARAM_PORT_MAX (65535)

/** Whitelist config command */
struct PACKED command_whitelist {
    /** Flags */
    uint8_t flags;

    /** IP protocol */
    uint8_t ip_protocol;

    /** Link layer protocol */
    be16_t llc_protocol;

    /** Source IP address */
    be32_t src_ip;

    /** Destination IP address */
    be32_t dest_ip;

    /** Netmask */
    be32_t netmask;

    /** TCP or UDP source port */
    be16_t src_port;

    /** TCP or UDP destination port */
    be16_t dest_port;
};

static struct
{
    struct arg_lit *clear;
    struct arg_int *llc_protocol;
    struct arg_int *ip_protocol;
    struct arg_str *src_ip;
    struct arg_str *dest_ip;
    struct arg_str *netmask;
    struct arg_int *src_port;
    struct arg_int *dest_port;
} args;

int whitelist_init(struct morsectrl *mors, struct mm_argtable *mm_args)
{
    MM_INIT_ARGTABLE(mm_args, "Configure the packet whitelist filter",
        args.llc_protocol = arg_int0("l", NULL, "<LLC proto>",
            "Link layer protocol - e.g. 0x0800 for IPv4"),
        args.ip_protocol = arg_int0("i", NULL, "<IPv4 proto>",
            "IPv4 protocol - e.g. 6 for TCP or 17 for UDP"),
        args.src_ip = arg_str0("s", NULL, "<src IP>",
            "Source IP address in dotted decimal notation"),
        args.dest_ip = arg_str0("d", NULL, "<dest IP>",
            "Destination IP address in dotted decimal notation"),
        args.netmask = arg_str0("n", NULL, "<dest IP>",
            "Network mask for IP addresses in dotted decimal notation"),
        args.src_port = arg_int0("S", NULL, "<src port>",
            "UDP or TCP source port - range 1-65535"),
        args.dest_port = arg_int0("D", NULL, "<dest port>",
            "UDP or TCP destination port - range 1-65535"),
        args.clear = arg_lit0("c", NULL,
            "Clear all whitelist entries"));
    return 0;
}

int whitelist(struct morsectrl *mors, int argc, char *argv[])
{
    int ret = -1;
    struct morsectrl_transport_buff *cmd_tbuff;
    struct morsectrl_transport_buff *rsp_tbuff;
    struct command_whitelist *cmd;
    int add_arg_count = 0;

    cmd_tbuff = morsectrl_transport_cmd_alloc(mors->transport, sizeof(*cmd));
    rsp_tbuff = morsectrl_transport_resp_alloc(mors->transport, 0);

    if (!cmd_tbuff || !rsp_tbuff)
    {
        goto exit;
    }

    cmd = TBUFF_TO_CMD(cmd_tbuff, struct command_whitelist);
    memset(cmd, 0, sizeof(*cmd));

    add_arg_count = args.llc_protocol->count +
            args.ip_protocol->count +
            args.src_ip->count +
            args.dest_ip->count +
            args.netmask->count +
            args.src_port->count +
            args.dest_port->count;

    if (args.clear->count == 1)
    {
        if (add_arg_count != 0)
        {
            mctrl_err("Invalid parameters specified for Clear operation\n");
            goto exit;
        }
        cmd->flags |= WHITELIST_FLAGS_CLEAR;
    }
    else
    {
        if (add_arg_count == 0)
        {
            mctrl_err("No filter parameters specified\n");
            goto exit;
        }
    }

    if (args.llc_protocol->count)
    {
        cmd->llc_protocol = htobe16(args.llc_protocol->ival[0]);
    }

    if (args.ip_protocol->count)
    {
        cmd->ip_protocol = args.ip_protocol->ival[0];
    }

    if (args.src_ip->count)
    {
        if (inet_pton(AF_INET, args.src_ip->sval[0], &cmd->src_ip) != 1)
        {
            mctrl_err("Invalid source IP address %s\n", args.src_ip->sval[0]);
            goto exit;
        }
    }

    if (args.dest_ip->count)
    {
        if (inet_pton(AF_INET, args.dest_ip->sval[0], &cmd->dest_ip) != 1)
        {
            mctrl_err("Invalid destination IP address %s\n", args.dest_ip->sval[0]);
            goto exit;
        }
    }

    if (args.netmask->count)
    {
        if (inet_pton(AF_INET, args.netmask->sval[0], &cmd->netmask) != 1)
        {
            mctrl_err("Invalid netmask %s\n", args.netmask->sval[0]);
            goto exit;
        }
        if (args.src_ip->count == 0 && args.dest_ip->count == 0)
        {
                mctrl_err("Netmask provided without source or destination IP address\n");
                goto exit;
        }
        if (cmd->src_ip && ((cmd->src_ip & cmd->netmask) != cmd->src_ip))
        {
                mctrl_err("Netmask is invalid for source IP address\n");
                goto exit;
        }
        if (cmd->dest_ip && ((cmd->dest_ip & cmd->netmask) != cmd->dest_ip))
        {
                mctrl_err("Netmask is invalid for destination IP address\n");
                goto exit;
        }
    }

    if (args.src_port->count)
    {
        if (args.src_port->ival[0] > WHITELIST_PARAM_PORT_MAX)
        {
            mctrl_err("Invalid source port %d\n", args.src_port->ival[0]);
            goto exit;
        }
        cmd->src_port = htobe16(args.src_port->ival[0]);
    }

    if (args.dest_port->count)
    {
        if (args.dest_port->ival[0] > WHITELIST_PARAM_PORT_MAX)
        {
            mctrl_err("Invalid destination port %d\n", args.dest_port->ival[0]);
            goto exit;
        }
        cmd->dest_port = htobe16(args.dest_port->ival[0]);
    }

    ret = morsectrl_send_command(mors->transport, MORSE_COMMAND_SET_WHITELIST,
                                 cmd_tbuff, rsp_tbuff);
    if (ret < 0)
    {
        mctrl_err("Whitelist command failed - error(%d)\n", ret);
        goto exit;
    }

exit:
    morsectrl_transport_buff_free(cmd_tbuff);
    morsectrl_transport_buff_free(rsp_tbuff);

    return ret;
}

MM_CLI_HANDLER(whitelist, MM_INTF_REQUIRED, MM_DIRECT_CHIP_SUPPORTED);
