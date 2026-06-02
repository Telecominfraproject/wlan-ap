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

struct PACKED set_enc_mode_command
{
    /* data of this message */
    uint8_t enc_mode;
};

static void usage(struct morsectrl *mors)
{
    mctrl_print("\tenc_mode <value>\tsets TIM enc_mode to driver\n");
}

int enc_mode(struct morsectrl *mors, int argc, char *argv[])
{
    int ret = -1;
    struct set_enc_mode_command *cmd;
    struct morsectrl_transport_buff *cmd_tbuff;
    struct morsectrl_transport_buff *rsp_tbuff;
    uint8_t encmode;

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

    cmd_tbuff = morsectrl_transport_cmd_alloc(mors->transport, sizeof(*cmd));
    rsp_tbuff = morsectrl_transport_resp_alloc(mors->transport, 0);

    if (!cmd_tbuff || !rsp_tbuff)
        goto exit;

    cmd = TBUFF_TO_CMD(cmd_tbuff, struct set_enc_mode_command);

    if (str_to_uint8(argv[1], &encmode))
    {
        mctrl_err("Invalid enc_mode\n");
        ret = -1;
        goto exit;
    }
    cmd->enc_mode = encmode;

    ret = morsectrl_send_command(mors->transport, MORSE_COMMAND_SET_ENC_MODE,
                                 cmd_tbuff, rsp_tbuff);
exit:
    if (ret < 0)
    {
        mctrl_err("Failed to set ifs\n");
    }

    morsectrl_transport_buff_free(cmd_tbuff);
    morsectrl_transport_buff_free(rsp_tbuff);
    return ret;
}

MM_CLI_HANDLER(enc_mode, MM_INTF_REQUIRED, MM_DIRECT_CHIP_NOT_SUPPORTED);
