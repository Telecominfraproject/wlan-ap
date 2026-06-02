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

/** Calibration types available in the system, values can be or'ed together to create a mask */
typedef enum
{
    /** Temperature calibration */
    CALIBRATION_TEMPERATURE = (1 << 0),
    /** VBAT calibration */
    CALIBRATION_VBAT = (1 << 1),
    /** AON clock calibration */
    CALIBRATION_AON_CLK = (1 << 2),
    /** DC calibration */
    CALIBRATION_DC = (1 << 3),
    /** I/Q calibration */
    CALIBRATION_IQ = (1 << 4),

    /** Special calibration type for testing purposes */
    CALIBRATION_SPOOF_TEST = (1 << 30),

    /** Last / MAX */
    CALIBRATION_LAST = (CALIBRATION_TEMPERATURE | CALIBRATION_VBAT |
                        CALIBRATION_AON_CLK | CALIBRATION_DC |
                        CALIBRATION_IQ | CALIBRATION_SPOOF_TEST),
    CALIBRATION_MAX = UINT32_MAX
} calibration_type_t;

struct PACKED set_periodic_cal_cmd
{
    calibration_type_t periodic_cal_enabled;
};

static void usage(struct morsectrl *mors)
{
    mctrl_print("\tperiodic_cal <enable_mask>\n");
    mctrl_print("\t\trespective bit position is 1 will enable the respective cal on-chip\n");
    mctrl_print("\t\trespective bit position is 0 will disable the respective cal on-chip\n");
    mctrl_print(
        "\t\tone-shot enable/disable for all cals,careful not to overwrite current config\n");
    mctrl_print("\t\t0x10 - IQ\n");
    mctrl_print("\t\t0x08 - DC\n");
    mctrl_print("\t\t0x04 - AON_CLK\n");
    mctrl_print("\t\t0x02 - VBAT\n");
    mctrl_print("\t\t0x01 - TEMP\n");
}

int periodic_cal(struct morsectrl *mors, int argc, char *argv[])
{
    int ret = -1;
    int enable_mask;
    char *ptr;
    struct set_periodic_cal_cmd *cmd;
    struct morsectrl_transport_buff *cmd_tbuff;
    struct morsectrl_transport_buff *rsp_tbuff;

    if (argc < 2)
    {
        usage(mors);
        return 0;
    }

    enable_mask = strtol(argv[1] + 2, &ptr, 16);

    if (enable_mask == -1)
    {
        mctrl_err("Invalid command parameters\n");
        usage(mors);
        return -1;
    }

    cmd_tbuff = morsectrl_transport_cmd_alloc(mors->transport, sizeof(*cmd));
    rsp_tbuff = morsectrl_transport_resp_alloc(mors->transport, 0);

    if (!cmd_tbuff || !rsp_tbuff)
        goto exit;

    cmd = TBUFF_TO_CMD(cmd_tbuff, struct set_periodic_cal_cmd);
    cmd->periodic_cal_enabled = enable_mask;

    ret = morsectrl_send_command(mors->transport, MORSE_COMMAND_SET_PERIODIC_CAL,
                                 cmd_tbuff, rsp_tbuff);

exit:
    if (ret)
    {
        mctrl_err("Failed to set periodic cal\n");
    }
    else
    {
        mctrl_print("\tCalibration type: 0x%x\n", cmd->periodic_cal_enabled);
    }

    morsectrl_transport_buff_free(cmd_tbuff);
    morsectrl_transport_buff_free(rsp_tbuff);
    return ret;
}

MM_CLI_HANDLER(periodic_cal, MM_INTF_REQUIRED, MM_DIRECT_CHIP_SUPPORTED);
