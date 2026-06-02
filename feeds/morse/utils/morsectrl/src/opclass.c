/*
 * Copyright 2022 Morse Micro
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

#define GLOBAL_OP_CLASS_MIN 64
#define GLOBAL_OP_CLASS_MAX 77

struct PACKED set_opclass_command
{
    uint8_t opclass;
    uint8_t prim_opclass;
};

static void usage(struct morsectrl *mors)
{
    mctrl_print("\topclass [s1g_operating_class] -l <s1g_prim_chan_global_op_class>\n");
    mctrl_print("\t\t\t\tSet s1g_operating_class for S1G Operation element in\n");
    mctrl_print("\t\t\t\tbeacon and global operating class of primary channel\n");
    mctrl_print("\t\t\t\tfor country ie in probe response\n");
    mctrl_print("\t\t-l <value>\tGlobal Operating class for primary channel\n");
}

int opclass(struct morsectrl *mors, int argc, char *argv[])
{
    int ret = -1;
    int option;
    uint8_t tmp;
    struct set_opclass_command *cmd;
    struct morsectrl_transport_buff *cmd_tbuff;
    struct morsectrl_transport_buff *rsp_tbuff;

    if (argc == 0)
    {
        usage(mors);
        return 0;
    }

    if (argc < 2 || argc > 5)
    {
        usage(mors);
        return -1;
    }

    cmd_tbuff = morsectrl_transport_cmd_alloc(mors->transport, sizeof(*cmd));
    rsp_tbuff = morsectrl_transport_resp_alloc(mors->transport, 0);

    if (!cmd_tbuff || !rsp_tbuff)
        goto exit;

    cmd = TBUFF_TO_CMD(cmd_tbuff, struct set_opclass_command);

    if (str_to_uint8(argv[1], &tmp))
    {
        mctrl_err("Invalid op class\n");
        ret = -1;
        goto exit;
    }
    cmd->opclass = tmp;

    while ((option = getopt(argc-1, argv+1, "l:")) != -1)
    {
        switch (option) {
        case 'l' :
        {
            if (str_to_uint8_range(optarg, &tmp,
                    GLOBAL_OP_CLASS_MIN, GLOBAL_OP_CLASS_MAX) < 0)
            {
                mctrl_err("Global operating class %u must be within range min %u : max %u\n",
                                    tmp, GLOBAL_OP_CLASS_MIN, GLOBAL_OP_CLASS_MAX);
                usage(mors);
                ret = -1;
                goto exit;
            }
            cmd->prim_opclass = tmp;
            break;
        }
        case '?' :
            usage(mors);
            goto exit;
        default :
            mctrl_err("Invalid argument\n");
            usage(mors);
            goto exit;
        }
    }

    ret = morsectrl_send_command(mors->transport, MORSE_COMMAND_SET_S1G_OP_CLASS,
                                 cmd_tbuff, rsp_tbuff);

exit:
    if (ret)
    {
        mctrl_err("Failed to set opclass\n");
    }

    morsectrl_transport_buff_free(cmd_tbuff);
    morsectrl_transport_buff_free(rsp_tbuff);
    return ret;
}

MM_CLI_HANDLER(opclass, MM_INTF_REQUIRED, MM_DIRECT_CHIP_NOT_SUPPORTED);
