/*
 * Copyright 2021 Morse Micro
 */

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <stdint.h>
#include "command.h"
#include "utilities.h"

#define GAINCODE_MIN 0
#define GAINCODE_MAX 20

struct PACKED command_set_agc_gaincode
{
    /* set gain code */
    uint32_t agc_gain_code;
};

static void usage(struct morsectrl *mors)
{
    mctrl_print("\tset_agc_gaincode [auto | <value>]\n");
    mctrl_print("\t\tset gain code for testing (value)\n");
    mctrl_print("\t\tuse 'auto' to enable AGC\n");
    mctrl_print("\t\tuse 'mask_rssi3' to mask agc rssi counter 3\n");
}

int set_agc_gaincode(struct morsectrl *mors, int argc, char *argv[])
{
    int ret = -1;
    uint32_t gain_code;
    struct command_set_agc_gaincode *cmd;
    struct morsectrl_transport_buff *cmd_tbuff;
    struct morsectrl_transport_buff *rsp_tbuff;

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

    if (!strcmp(argv[1], "auto"))
    {
       gain_code = UINT8_MAX;
    }
    else if (!strcmp(argv[1], "mask_rssi3"))
    {
        gain_code = UINT8_MAX - 1;
    }
    else if (str_to_uint32_range(argv[1], &gain_code, GAINCODE_MIN, GAINCODE_MAX))
    {
        mctrl_err("Invalid gain code value '%s', expected range [%d, %d]\n",
                argv[1], GAINCODE_MIN, GAINCODE_MAX);
        usage(mors);
        return -1;
    }

    cmd_tbuff = morsectrl_transport_cmd_alloc(mors->transport, sizeof(*cmd));
    rsp_tbuff = morsectrl_transport_resp_alloc(mors->transport, 0);

    if (!cmd_tbuff || !rsp_tbuff)
        goto exit;

    cmd = TBUFF_TO_CMD(cmd_tbuff, struct command_set_agc_gaincode);
    cmd->agc_gain_code = gain_code;
    ret = morsectrl_send_command(mors->transport, MORSE_TEST_COMMAND_SET_AGC_GAIN_CODE,
                                 cmd_tbuff, rsp_tbuff);
exit:
    if (ret < 0)
    {
        mctrl_err("Failed to set agc gain code \n");
    }

    morsectrl_transport_buff_free(cmd_tbuff);
    morsectrl_transport_buff_free(rsp_tbuff);
    return ret;
}

MM_CLI_HANDLER(set_agc_gaincode, MM_INTF_REQUIRED, MM_DIRECT_CHIP_SUPPORTED);
