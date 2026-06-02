/*
 * Copyright 2022 Morse Micro
 */

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include "portable_endian.h"
#include "utilities.h"

#include "command.h"

#define MORSECTRL_CMD_REQ_FLAG      (BIT(0))

int morsectrl_send_command(struct morsectrl_transport *transport,
                           int message_id,
                           struct morsectrl_transport_buff *cmd,
                           struct morsectrl_transport_buff *resp)
{
    int ret = 0;
    struct command *command;
    struct response *response;

    if (!cmd || !resp)
    {
        ret = -ENOMEM;
        goto exit;
    }

    command = (struct command *)cmd->data;
    memset(&command->hdr, 0, sizeof(command->hdr));
    command->hdr.message_id = htole16(message_id);
    command->hdr.len = htole16(cmd->data_len - sizeof(struct command));
    command->hdr.flags = MORSECTRL_CMD_REQ_FLAG;
    response = (struct response *)resp->data;

    ret = morsectrl_transport_send(transport, cmd, resp);

    if (ret < 0)
    {
        morsectrl_transport_debug(transport, "Message failed %d\n", ret);
        goto exit;
    }

    ret = le32toh(response->status);
    if (ret)
    {
        if (ret != 110)
        {
            morsectrl_transport_debug(transport, "Command failed\n");
        }

        goto exit;
    }

exit:
    return ret;
} // NOLINT - checkstyle.py seems to think this brace is in the wrong place.
