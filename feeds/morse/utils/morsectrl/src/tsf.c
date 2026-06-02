/*
 * Copyright 2022 Morse Micro
 */
#include <stdlib.h>
#include <stdio.h>

#include "command.h"
#include "utilities.h"

struct PACKED command_get_tsf_req
{
};

struct PACKED command_get_tsf_cfm
{
    /** The current TSF of the BSS the interface is associated on */
    uint64_t now_tsf;
};

static void usage(struct morsectrl *mors)
{
    mctrl_print("\ttsf\t\t\tretrieve the TSF (in hex)\n");
}

int tsf(struct morsectrl *mors, int argc, char *argv[])
{
    int ret = -1;
    struct command_get_tsf_req *cmd;
    struct command_get_tsf_cfm *rsp;
    struct morsectrl_transport_buff *cmd_tbuff;
    struct morsectrl_transport_buff *rsp_tbuff;

    if (argc == 0)
    {
        usage(mors);
        return 0;
    }

    if (argc != 1)
    {
        mctrl_err("Invalid command parameters\n");
        usage(mors);
        return -1;
    }

    cmd_tbuff = morsectrl_transport_cmd_alloc(mors->transport, sizeof(*cmd));
    rsp_tbuff = morsectrl_transport_resp_alloc(mors->transport, sizeof(*rsp));

    if (!cmd_tbuff || !rsp_tbuff)
        goto exit;

    cmd = TBUFF_TO_CMD(cmd_tbuff, struct command_get_tsf_req);
    rsp = TBUFF_TO_RSP(rsp_tbuff, struct command_get_tsf_cfm);

    ret = morsectrl_send_command(mors->transport, MORSE_COMMAND_GET_TSF,
                                 cmd_tbuff, rsp_tbuff);

exit:
    if (ret)
    {
        mctrl_err("TSF is not available\n");
    }
    else
    {
        mctrl_print("%" PRIx64 "\n", le64toh(rsp->now_tsf));
    }

    morsectrl_transport_buff_free(cmd_tbuff);
    morsectrl_transport_buff_free(rsp_tbuff);

    return ret;
}

MM_CLI_HANDLER(tsf, MM_INTF_REQUIRED, MM_DIRECT_CHIP_SUPPORTED);
