/*
 * Copyright 2021 Morse Micro
 */

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <stdint.h>
#include "portable_endian.h"

#include "command.h"

enum hartid
{
    HOST_HARTID = 0,
    MAC_HARTID = 1,
    UPHY_HARTID = 2,
    LPHY_HARTID = 3
};
struct PACKED command_force_assert_req
{
    /* Target hart to crash with an intended assert */
    uint32_t hart_id;
};


static void usage(struct morsectrl *mors) {
    mctrl_print("\tassert <core>\t\tforces the specified core to assert" \
           "- defaults to mac if no arg\n");
    mctrl_print("\t\t-a\t\tApp core\n");
    mctrl_print("\t\t-m\t\tMac core\n");
    mctrl_print("\t\t-u\t\tUphy core\n");
    mctrl_print("\t\t-l\t\tLphy core\n");
}

static int assert(struct morsectrl *mors, int argc, char *argv[])
{
    int ret = -1;
    int hart_id, option;
    struct command_force_assert_req *cmd;
    struct morsectrl_transport_buff *cmd_tbuff;
    struct morsectrl_transport_buff *rsp_tbuff;

    if (argc == 0)
    {
        usage(mors);
        return 0;
    }

    if (argc > 2)
    {
        mctrl_err("Invalid command parameters\n");
        usage(mors);
        return -1;
    }

    cmd_tbuff = morsectrl_transport_cmd_alloc(mors->transport, sizeof(*cmd));
    rsp_tbuff = morsectrl_transport_resp_alloc(mors->transport, 0);

    if (!cmd_tbuff || !rsp_tbuff)
        goto exit;

    cmd = TBUFF_TO_CMD(cmd_tbuff, struct command_force_assert_req);

    if (argc == 1)
    {
        hart_id = MAC_HARTID;
    }
    else
    {
        while ((option = getopt(argc, argv, "amul")) != -1)
        {
            switch (option)
            {
                case 'a' :
                    hart_id = HOST_HARTID;
                    break;
                case 'm' :
                    hart_id = MAC_HARTID;
                    break;
                case 'u' :
                    hart_id = UPHY_HARTID;
                    break;
                case 'l' :
                    hart_id = LPHY_HARTID;
                    break;
                default:
                    mctrl_err("Invalid hart\n");
                    usage(mors);
                    ret = -1;
                    goto exit;
            }
        }
    }
    cmd->hart_id = htole32(hart_id);

    ret = morsectrl_send_command(mors->transport, MORSE_TEST_COMMAND_FORCE_ASSERT,
                                 cmd_tbuff, rsp_tbuff);

exit:
    if (ret != -110)
    {
        mctrl_err("Chip didn't timeout. Command failed\n");
        if (ret)
            mctrl_err("Wrong error code returned: %d\n", ret);
        ret = -1;
    }
    else
    {
        ret = 0;
    }

    morsectrl_transport_buff_free(cmd_tbuff);
    morsectrl_transport_buff_free(rsp_tbuff);
    return ret;
}

MM_CLI_HANDLER(assert, MM_INTF_REQUIRED, MM_DIRECT_CHIP_SUPPORTED);
