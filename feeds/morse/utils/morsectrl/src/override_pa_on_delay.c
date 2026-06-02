/*
 * Copyright 2022 Morse Micro
 */

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <stdint.h>

#include "portable_endian.h"
#include "command.h"
#include "utilities.h"

struct PACKED override_pa_on_delay_command
{
    /** The flags of this message. Bool, 1=enabled, 0=disabled */
    uint8_t enable;
    uint32_t delay_us;
};


static void usage(struct morsectrl *mors) {
    mctrl_print("\toverride_pa_on_delay [enable|disable] <delay us>\n");
    mctrl_print("\t\t\t\tenable overriding pa turn on delay with given value\n");
    mctrl_print("\t\t\t\tor disable overriding\n");
}

int override_pa_on_delay(struct morsectrl *mors, int argc, char *argv[])
{
    int ret = -1;
    bool enable = 0;
    uint32_t delay_us = 0;
    struct override_pa_on_delay_command *cmd;
    struct morsectrl_transport_buff *cmd_tbuff;
    struct morsectrl_transport_buff *rsp_tbuff;
    if (argc == 0)
    {
        usage(mors);
        return 0;
    }
    switch (expression_to_int(argv[1]))
    {
        case true:
            if (argc == 3)
            {
                enable = 1;

                if (check_string_is_int(argv[2]))
                {
                    if (str_to_uint32(argv[2], &delay_us))
                    {
                        mctrl_err("Invalid delay\n");
                        return -1;
                    }
                }
            }
            else
            {
                usage(mors);
                return -1;
            }
            break;

        case false:
            enable = 0;
            delay_us = 0;
            break;

        default:
            mctrl_err("Invalid command parameters\n");
            usage(mors);
            return -1;
    }

    cmd_tbuff = morsectrl_transport_cmd_alloc(mors->transport, sizeof(*cmd));
    rsp_tbuff = morsectrl_transport_resp_alloc(mors->transport, 0);

    if (!cmd_tbuff || !rsp_tbuff)
        goto exit;

    cmd = TBUFF_TO_CMD(cmd_tbuff, struct override_pa_on_delay_command);
    cmd->enable = htole32(enable);
    cmd->delay_us = htole32(delay_us);
    ret = morsectrl_send_command(mors->transport, MORSE_TEST_COMMAND_OVERRIDE_PA_ON_DELAY,
                                 cmd_tbuff, rsp_tbuff);

exit:
    if (ret < 0)
    {
        mctrl_err("Failed to execute command\n");
    }

    morsectrl_transport_buff_free(cmd_tbuff);
    morsectrl_transport_buff_free(rsp_tbuff);
    return ret;
}

MM_CLI_HANDLER(override_pa_on_delay, MM_INTF_REQUIRED, MM_DIRECT_CHIP_SUPPORTED);
