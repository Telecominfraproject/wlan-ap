/*
 * Copyright 2020 Morse Micro
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

struct PACKED set_response_indication_command
{
    int8_t response_indication;
};


static void usage(struct morsectrl *mors) {
    mctrl_print("\tri <enable|disable> <value>\n");
    mctrl_print("\t\t\t\tforces specificed response indication if 'enable'\n");
    mctrl_print("\t\t\t\totherwise 'disable' force\n");
}

int ri(struct morsectrl *mors, int argc, char *argv[])
{
    int ret = -1;
    int8_t ri;
    struct set_response_indication_command *cmd;
    struct morsectrl_transport_buff *cmd_tbuff;
    struct morsectrl_transport_buff *rsp_tbuff;

    if (argc == 0)
    {
        usage(mors);
        return 0;
    }

    if (argc < 2)
    {
        mctrl_err("Invalid command parameters\n");
        usage(mors);
        return -1;
    }

    cmd_tbuff = morsectrl_transport_cmd_alloc(mors->transport, sizeof(*cmd));
    rsp_tbuff = morsectrl_transport_resp_alloc(mors->transport, 0);

    if (!cmd_tbuff || !rsp_tbuff)
        goto exit;

    cmd = TBUFF_TO_CMD(cmd_tbuff, struct set_response_indication_command);

    switch (expression_to_int(argv[1]))
    {
        case true:
            if (argc != 3)
            {
                mctrl_err("Invalid command parameters\n");
                usage(mors);
                ret = -1;
                goto exit;
            }

            if (str_to_int8_range(argv[2], &ri, 0, 3))
            {
                mctrl_err("Invalid value response indication must be between 0 and 3\n");
                usage(mors);
                ret = -1;
                goto exit;
            }
        break;

        case false:
            ri = -1;
        break;

        default:
            mctrl_err("Invalid command parameters\n");
            usage(mors);
            ret = -1;
            goto exit;
    }

    cmd->response_indication = ri;
    ret = morsectrl_send_command(mors->transport, MORSE_TEST_COMMAND_SET_RESPONSE_INDICATION,
                                 cmd_tbuff, rsp_tbuff);
exit:
    if (ret < 0)
    {
        mctrl_err("Failed to set ri\n");
    }

    morsectrl_transport_buff_free(cmd_tbuff);
    morsectrl_transport_buff_free(rsp_tbuff);
    return ret;
}

MM_CLI_HANDLER(ri, MM_INTF_REQUIRED, MM_DIRECT_CHIP_SUPPORTED);
