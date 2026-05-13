/*
 * Copyright 2022 Morse Micro
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "command.h"
#include "utilities.h"

#define SIG_FIELD_ERROR_EVENT_DISABLED              (0)
#define SIG_FIELD_ERROR_EVENT_ENABLED_MONITOR_ONLY  (1)
#define SIG_FIELD_ERROR_EVENT_ENABLED_ANY_MODE      (2)

struct PACKED command_set_sig_field_error_event_config_req
{
    uint8_t config;
};


static void usage(struct morsectrl *mors)
{
    mctrl_print("\tsig_field_error_evt [enable|disable]\n");
    mctrl_print("\t\t\t\t'enable' will sig field error events when monitor mode is enabled.\n");
    mctrl_print("\t\t\t\t         These events will show up in sniffer traces as radiotap\n");
    mctrl_print("\t\t\t\t         headers with no payload.\n");
    mctrl_print("\t\t\t\t'disable' will disable sig field error events (default state).\n");
}


int sig_field_error_evt(struct morsectrl *mors, int argc, char *argv[])
{
    int ret = -1;
    int enabled;
    struct command_set_sig_field_error_event_config_req *cmd;
    struct morsectrl_transport_buff *cmd_tbuff;
    struct morsectrl_transport_buff *rsp_tbuff;

    if (argc < 2)
    {
        usage(mors);
        return 0;
    }

    enabled = expression_to_int(argv[1]);

    if (enabled == -1)
    {
        mctrl_err("Invalid command parameters\n");
        usage(mors);
        return -1;
    }

    cmd_tbuff = morsectrl_transport_cmd_alloc(mors->transport, sizeof(*cmd));
    rsp_tbuff = morsectrl_transport_resp_alloc(mors->transport, 0);

    if (!cmd_tbuff || !rsp_tbuff)
        goto exit;

    cmd = TBUFF_TO_CMD(cmd_tbuff, struct command_set_sig_field_error_event_config_req);

    if (enabled)
    {
        /*
         * As noted in the usage documentation, the "enable" option means enabled in monitor
         * mode. The firmware supports enabling this event in any mode (i.e.,
         * SIG_FIELD_ERROR_EVENT_ENABLED_ANY_MODE), but there is currently no use case for it.
         */
        cmd->config = SIG_FIELD_ERROR_EVENT_ENABLED_MONITOR_ONLY;
    }
    else
    {
        cmd->config = SIG_FIELD_ERROR_EVENT_DISABLED;
    }

    ret = morsectrl_send_command(mors->transport,
                                 MORSE_TEST_COMMAND_SET_SIG_FIELD_ERROR_EVENT_CONFIG,
                                 cmd_tbuff, rsp_tbuff);

exit:
    if (ret)
    {
        mctrl_err("Failed to set sig field error event config\n");
    }
    else
    {
        mctrl_print("\tSig field error event config: %s\n",
            (enabled) ? "enabled in monitor mode" : "disabled");
    }

    morsectrl_transport_buff_free(cmd_tbuff);
    morsectrl_transport_buff_free(rsp_tbuff);
    return ret;
}

MM_CLI_HANDLER(sig_field_error_evt, MM_INTF_REQUIRED, MM_DIRECT_CHIP_SUPPORTED);
