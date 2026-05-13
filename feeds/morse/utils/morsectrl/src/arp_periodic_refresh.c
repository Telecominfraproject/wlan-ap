/*
 * Copyright 2024 Morse Micro
 */

#include <errno.h>
#include <stdint.h>
#include <stdio.h>

#include "morsectrl.h"
#include "command.h"
#include "utilities.h"

/** Max ARP refresh period in seconds, calculated to prevent overflow after
 * conversion to milliseconds
 */
#define ARP_REFRESH_MAX_PERIOD_S  (UINT32_MAX / 1000)

struct PACKED arp_periodic_params
{
    uint32_t refresh_period_s;
    ipv4_addr_t destination_ip;
    uint8_t  send_as_garp;
};

struct PACKED command_set_arp_periodic_refresh_req
{
    struct arp_periodic_params config;
};

static struct
{
    struct arg_int *arp_refresh_period_s;
    struct arg_str *destination_address;
    struct arg_lit *send_as_garp;
} args;


int arp_periodic_refresh_init(struct morsectrl *mors, struct mm_argtable *mm_args)
{
    MM_INIT_ARGTABLE(mm_args,
          "Configure the firmware to send a periodic ARP packet",
          args.arp_refresh_period_s = arg_int1("t", NULL, "<period>",
            "Period in seconds between ARP transmissions. A value of 0 disables the feature."),
          args.destination_address = arg_str0("d", NULL, "<dest IP>",
             "IP in dotted decimal notation - target protocol address field of the ARP request"),
          args.send_as_garp = arg_lit0("g", NULL,
                                  "Send as a Gratuitous ARP (GARP) instead of an ARP request"));
    return 0;
}


int arp_periodic_refresh(struct morsectrl *mors, int argc, char *argv[])
{
    struct command_set_arp_periodic_refresh_req *cmd_set;

    struct morsectrl_transport_buff *cmd_set_tbuff;
    struct morsectrl_transport_buff *rsp_set_tbuff;

    ipv4_addr_t temp_ip;

    cmd_set_tbuff = morsectrl_transport_cmd_alloc(mors->transport, sizeof(*cmd_set));
    rsp_set_tbuff = morsectrl_transport_resp_alloc(mors->transport, 0);

    int ret = 0;

    if (!cmd_set_tbuff || !rsp_set_tbuff)
        goto exit;

    cmd_set = TBUFF_TO_CMD(cmd_set_tbuff, struct command_set_arp_periodic_refresh_req);

    if (args.arp_refresh_period_s->count)
    {
        if (args.arp_refresh_period_s->ival[0] > ARP_REFRESH_MAX_PERIOD_S)
        {
            ret = EINVAL;
            mctrl_err("Max refresh period is %d\n", ARP_REFRESH_MAX_PERIOD_S);
            goto exit;
        }
        cmd_set->config.refresh_period_s = args.arp_refresh_period_s->ival[0];
    }
    else
    {
        ret = EINVAL;
        mctrl_err("ARP refresh period not entered\n");
        goto exit;
    }

    if (args.destination_address->count)
    {
       ret = str_to_ip(*args.destination_address->sval, &temp_ip);
       if (ret)
       {
          mctrl_err("Failed to parse IP address: %s\n", *args.destination_address->sval);
          goto exit;
       }
       cmd_set->config.destination_ip = temp_ip;
    }
    else if (cmd_set->config.refresh_period_s)
    {
        ret = EINVAL;
        mctrl_err("Destination IP address not entered\n");
        goto exit;
    }


    if (args.send_as_garp->count)
    {
        cmd_set->config.send_as_garp = 1;
    }
    else
    {
       cmd_set->config.send_as_garp = 0;
    }

    ret = morsectrl_send_command(mors->transport, MORSE_COMMAND_ARP_PERIODIC_REFRESH,
                                 cmd_set_tbuff, rsp_set_tbuff);
    if (ret < 0)
    {
        mctrl_err("Failed to set arp periodic refresh params: error(%d)\n", ret);
        goto exit;
    }

exit:
    morsectrl_transport_buff_free(cmd_set_tbuff);
    morsectrl_transport_buff_free(rsp_set_tbuff);
    return ret;
}

MM_CLI_HANDLER(arp_periodic_refresh, MM_INTF_REQUIRED, MM_DIRECT_CHIP_SUPPORTED);

