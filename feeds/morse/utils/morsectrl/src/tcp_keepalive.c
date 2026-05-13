/*
 * Copyright 2024 Morse Micro
 */

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <stdint.h>
#include "portable_endian.h"
#if defined(__WINDOWS__)
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#endif

#include "command.h"
#include "utilities.h"

#define TCP_KEEPALIVE_SET_CFG_PERIOD         BIT(0)
#define TCP_KEEPALIVE_SET_CFG_RETRY_COUNT    BIT(1)
#define TCP_KEEPALIVE_SET_CFG_RETRY_INTERVAL BIT(2)
#define TCP_KEEPALIVE_SET_CFG_SRC_IP_ADDR    BIT(3)
#define TCP_KEEPALIVE_SET_CFG_DEST_IP_ADDR   BIT(4)
#define TCP_KEEPALIVE_SET_CFG_SRC_PORT       BIT(5)
#define TCP_KEEPALIVE_SET_CFG_DEST_PORT      BIT(6)

#define TCP_KEEPALIVE_PARAM_PORT_MAX           (65535)
#define TCP_KEEPALIVE_PARAM_PERIOD_MAX         (65535)
#define TCP_KEEPALIVE_PARAM_RETRY_COUNT_MAX    (255)
#define TCP_KEEPALIVE_PARAM_RETRY_INTERVAL_MAX (255)

struct PACKED tcp_keepalive_command
{
    uint8_t enabled;
    uint8_t retry_count;
    uint8_t retry_interval_s;
    uint8_t set_cfgs;
    be32_t src_ip;
    be32_t dest_ip;
    be16_t src_port;
    be16_t dest_port;
    be16_t period_s;
};

static struct
{
    struct arg_rex *enable;
    struct arg_int *period_s;
    struct arg_int *retry_count;
    struct arg_int *retry_interval_s;
    struct arg_str *src_ip;
    struct arg_str *dest_ip;
    struct arg_int *src_port;
    struct arg_int *dest_port;
} args;

int tcp_keepalive_init(struct morsectrl *mors, struct mm_argtable *mm_args)
{
    MM_INIT_ARGTABLE(mm_args, "Configure TCP keepalive offload parameters",
        args.period_s = arg_int0("p", NULL, "<period>",
            "Period in seconds - range 1-65535"),
        args.retry_count = arg_int0("c", NULL, "<retry count>",
            "Number of retries - range 0-255"),
        args.retry_interval_s = arg_int0("i", NULL, "<retry interval>",
            "Seconds between retries - range 1-255"),
        args.src_ip = arg_str0("s", NULL, "<src IP>",
            "Source IP address in dotted decimal notation"),
        args.dest_ip = arg_str0("d", NULL, "<dest IP>",
            "Destination IP address in dotted decimal notation"),
        args.src_port = arg_int0("S", NULL, "<src port>",
            "TCP source port - range 1-65535"),
        args.dest_port = arg_int0("D", NULL, "<dest port>",
            "TCP destination port - range 1-65535"),
        args.enable = arg_rex1(NULL, NULL, "(enable|disable)", "{enable|disable}", 0,
            "enable/disable TCP keepalive offload"));
    return 0;
}

int tcp_keepalive(struct morsectrl *mors, int argc, char *argv[])
{
    int ret = -1;
    struct morsectrl_transport_buff *cmd_tbuff;
    struct morsectrl_transport_buff *rsp_tbuff;
    struct tcp_keepalive_command *cmd;
    int add_arg_count = 0;

    cmd_tbuff = morsectrl_transport_cmd_alloc(mors->transport, sizeof(*cmd));
    rsp_tbuff = morsectrl_transport_resp_alloc(mors->transport, 0);

    if (!cmd_tbuff || !rsp_tbuff)
    {
        goto exit;
    }

    cmd = TBUFF_TO_CMD(cmd_tbuff, struct tcp_keepalive_command);
    memset(cmd, 0, sizeof(*cmd));

    add_arg_count =
            args.enable->count +
            args.period_s->count +
            args.retry_count->count +
            args.retry_interval_s->count +
            args.src_ip->count +
            args.dest_ip->count +
            args.src_port->count +
            args.dest_port->count;

    if (add_arg_count == 0)
    {
        mctrl_err("No parameters specified\n");
        goto exit;
    }

    if (args.enable->count)
    {
        if (strcmp("enable", args.enable->sval[0]) == 0)
        {
            cmd->enabled = 1;
        }
        else if (strcmp("disable", args.enable->sval[0]) == 0)
        {
            cmd->enabled = 0;
        }
    }

    if (args.period_s->count)
    {
        if (args.period_s->ival[0] < 1 || args.period_s->ival[0] > TCP_KEEPALIVE_PARAM_PERIOD_MAX)
        {
            mctrl_err("Invalid period %d\n", args.period_s->ival[0]);
            goto exit;
        }
        cmd->period_s = htole16(args.period_s->ival[0]);
        cmd->set_cfgs |= TCP_KEEPALIVE_SET_CFG_PERIOD;
    }

    if (args.retry_count->count)
    {
        if (args.retry_count->ival[0] < 0 ||
                        args.retry_count->ival[0] > TCP_KEEPALIVE_PARAM_RETRY_COUNT_MAX)
        {
            mctrl_err("Invalid retry count %d\n", args.retry_count->ival[0]);
            goto exit;
        }
        cmd->retry_count = args.retry_count->ival[0];
        cmd->set_cfgs |= TCP_KEEPALIVE_SET_CFG_RETRY_COUNT;
    }

    if (args.retry_interval_s->count)
    {
        if (args.retry_interval_s->ival[0] < 1 ||
                        args.retry_interval_s->ival[0] > TCP_KEEPALIVE_PARAM_RETRY_INTERVAL_MAX)
        {
            mctrl_err("Invalid retry interval %d\n", args.retry_interval_s->ival[0]);
            goto exit;
        }
        cmd->retry_interval_s = args.retry_interval_s->ival[0];
        cmd->set_cfgs |= TCP_KEEPALIVE_SET_CFG_RETRY_INTERVAL;
    }

    if (args.src_ip->count)
    {
        if (inet_pton(AF_INET, args.src_ip->sval[0], &cmd->src_ip) != 1)
        {
            mctrl_err("Invalid source IP address %s\n", args.src_ip->sval[0]);
            goto exit;
        }
        cmd->set_cfgs |= TCP_KEEPALIVE_SET_CFG_SRC_IP_ADDR;
    }

    if (args.dest_ip->count)
    {
        if (inet_pton(AF_INET, args.dest_ip->sval[0], &cmd->dest_ip) != 1)
        {
            mctrl_err("Invalid destination IP address %s\n", args.dest_ip->sval[0]);
            goto exit;
        }
        cmd->set_cfgs |= TCP_KEEPALIVE_SET_CFG_DEST_IP_ADDR;
    }

    if (args.src_port->count)
    {
        if (args.src_port->ival[0] > TCP_KEEPALIVE_PARAM_PORT_MAX)
        {
            mctrl_err("Invalid source port %d\n", args.src_port->ival[0]);
            goto exit;
        }
        cmd->src_port = htobe16(args.src_port->ival[0]);
        cmd->set_cfgs |= TCP_KEEPALIVE_SET_CFG_SRC_PORT;
    }

    if (args.dest_port->count)
    {
        if (args.dest_port->ival[0] > TCP_KEEPALIVE_PARAM_PORT_MAX)
        {
            mctrl_err("Invalid destination port %d\n", args.dest_port->ival[0]);
            goto exit;
        }
        cmd->dest_port = htobe16(args.dest_port->ival[0]);
        cmd->set_cfgs |= TCP_KEEPALIVE_SET_CFG_DEST_PORT;
    }

    ret = morsectrl_send_command(mors->transport, MORSE_COMMAND_SET_TCP_KEEPALIVE,
                                 cmd_tbuff, rsp_tbuff);
    if (ret < 0)
    {
        mctrl_err("TCP keepalive command failed - error(%d)\n", ret);
        goto exit;
    }

exit:
    morsectrl_transport_buff_free(cmd_tbuff);
    morsectrl_transport_buff_free(rsp_tbuff);

    return ret;
}

MM_CLI_HANDLER(tcp_keepalive, MM_INTF_REQUIRED, MM_DIRECT_CHIP_SUPPORTED);
