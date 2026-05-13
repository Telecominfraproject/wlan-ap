/*
 * Copyright 2023 Morse Micro
 */

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <stdint.h>
#ifndef MORSE_WIN_BUILD
#include <net/if.h>
#endif
#include "portable_endian.h"

#include "command.h"
#include "utilities.h"

#define AUTO_TRIGGER_DISABLED           ((uint8_t)0)
#define AUTO_TRIGGER_ENABLED            ((uint8_t)1)
#define AUTO_TRIGGER_FLAG_DEFAULT       ((uint8_t)0xFF)
#define AUTO_TRIGGER_TIMEOUT_MIN        (100U)
#define AUTO_TRIGGER_TIMEOUT_MAX        (10000U)
#define AUTO_TRIGGER_TIMEOUT_DEFAULT    (0U)


struct PACKED set_uapsd
{
    /** Auto trigger enabled/disabled flag */
    uint8_t auto_trigger_enabled;

    /** Timeout(ms) at which frame is triggered */
    uint32_t auto_trigger_timeout;
};

struct PACKED uapsd_cfm
{
    /** Confirm auto trigger enabled/disabled */
    uint8_t auto_trigger_enabled;
};

static void usage(struct morsectrl *mors)
{
    mctrl_print("\tuapsd -a <enable/disable> -t <timeout in ms>\n");
    mctrl_print("\t\tU-APSD auto trigger frame control\n");
    mctrl_print("\t\t-a <value>\tEnable/Disable auto trigger frame\n");
    mctrl_print("\t\t-t <value>\tTimeout at which trigger frame is send when enabled\n");
}

int uapsd(struct morsectrl *mors, int argc, char *argv[])
{
    int ret = -1;
    int option;
    uint8_t is_auto_trigger_enabled = AUTO_TRIGGER_FLAG_DEFAULT;
    uint32_t timeout_in_ms = AUTO_TRIGGER_TIMEOUT_DEFAULT;
    struct set_uapsd *cmd;
    struct uapsd_cfm *rsp;
    struct morsectrl_transport_buff *cmd_tbuff;
    struct morsectrl_transport_buff *rsp_tbuff;

    if (argc == 0)
    {
        usage(mors);
        return 0;
    }

    if (argc < 2 || argc > 7)
    {
        usage(mors);
        return -1;
    }

    cmd_tbuff = morsectrl_transport_cmd_alloc(mors->transport, sizeof(*cmd));
    rsp_tbuff = morsectrl_transport_resp_alloc(mors->transport, sizeof(*rsp));

    if (!cmd_tbuff || !rsp_tbuff)
        goto exit;

    cmd = TBUFF_TO_CMD(cmd_tbuff, struct set_uapsd);

    memset(cmd, 0, sizeof(*cmd));
    while ((option = getopt(argc, argv, "a:t:")) != -1)
    {
        switch (option) {
        case 'a' :
            if (str_to_uint8_range(optarg, &is_auto_trigger_enabled,
                AUTO_TRIGGER_DISABLED, AUTO_TRIGGER_ENABLED) < 0)
            {
                mctrl_err("Auto trigger enable flag %u must be either disabled %u : enabled %u\n",
                        is_auto_trigger_enabled, AUTO_TRIGGER_DISABLED, AUTO_TRIGGER_ENABLED);
                usage(mors);
                ret = -1;
                goto exit;
            }
            cmd->auto_trigger_enabled = is_auto_trigger_enabled;
            break;
        case 't' :
            if (str_to_uint32_range(optarg, &timeout_in_ms,
                AUTO_TRIGGER_TIMEOUT_MIN, AUTO_TRIGGER_TIMEOUT_MAX) < 0)
            {
                mctrl_err("Auto trigger timeout %u must be between min %u : max %u\n",
                        timeout_in_ms, AUTO_TRIGGER_TIMEOUT_MIN, AUTO_TRIGGER_TIMEOUT_MAX);
                usage(mors);
                ret = -1;
                goto exit;
            }
            cmd->auto_trigger_timeout = timeout_in_ms;
            break;
        case '?' :
            usage(mors);
            goto exit;
        default :
            mctrl_err("Invalid argument\n");
            usage(mors);
            goto exit;
        }
    }

    if (is_auto_trigger_enabled == AUTO_TRIGGER_FLAG_DEFAULT)
    {
        mctrl_err("Invalid is_auto_trigger_enabled %d\n", is_auto_trigger_enabled);
        usage(mors);
        goto exit;
    }

    if ((is_auto_trigger_enabled == AUTO_TRIGGER_ENABLED &&
        timeout_in_ms == AUTO_TRIGGER_TIMEOUT_DEFAULT) ||
        (is_auto_trigger_enabled == AUTO_TRIGGER_DISABLED &&
        timeout_in_ms != AUTO_TRIGGER_TIMEOUT_DEFAULT))
    {
        mctrl_err("Invalid timeout_in_ms %d\n", timeout_in_ms);
        usage(mors);
        goto exit;
    }

    ret = morsectrl_send_command(mors->transport, MORSE_COMMAND_UAPSD_CONFIG,
                                 cmd_tbuff, rsp_tbuff);

exit:
    if (ret)
    {
        mctrl_err("Failed to set U-APSD config with error %d\n", ret);
    }

    morsectrl_transport_buff_free(cmd_tbuff);
    morsectrl_transport_buff_free(rsp_tbuff);
    return ret;
}

MM_CLI_HANDLER(uapsd, MM_INTF_REQUIRED, MM_DIRECT_CHIP_NOT_SUPPORTED);
