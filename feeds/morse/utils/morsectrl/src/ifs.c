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

#define IF_SPACING_MIN_US 160

struct PACKED set_ifs_command
{
    /** The flags of this message */
    uint32_t ifs;
};


static void usage(struct morsectrl *mors) {
    mctrl_print("\tifs <value>\t\tsets interframe spacing in us (min: %uus)\n",
                    IF_SPACING_MIN_US);
}

int ifs(struct morsectrl *mors, int argc, char *argv[])
{
    int ret = -1;
    uint32_t ifs;
    struct set_ifs_command *cmd;
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

    cmd_tbuff = morsectrl_transport_cmd_alloc(mors->transport, sizeof(*cmd));
    rsp_tbuff = morsectrl_transport_resp_alloc(mors->transport, 0);

    if (!cmd_tbuff || !rsp_tbuff)
        goto exit;

    cmd = TBUFF_TO_CMD(cmd_tbuff, struct set_ifs_command);
    if (str_to_uint32_range(argv[1], &ifs, IF_SPACING_MIN_US, UINT32_MAX))
    {
        mctrl_err("Invalid interframe spacing value must be greater than %d\n",
                    IF_SPACING_MIN_US);
        usage(mors);
        ret = -1;
        goto exit;
    }

    cmd->ifs = htole32(ifs);
    ret = morsectrl_send_command(mors->transport, MORSE_COMMAND_SET_IFS,
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

MM_CLI_HANDLER(ifs, MM_INTF_REQUIRED, MM_DIRECT_CHIP_SUPPORTED);
