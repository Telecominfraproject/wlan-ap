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
#include "utilities.h"

/* Limits on duty cycle, expressed in  percent */
#define FSG_DUTY_CYCLE_MIN      (0.01)
#define FSG_DUTY_CYCLE_MAX      (99.99)
#define FSG_DEFAULT_IFS_US      (160)

struct PACKED command_set_fsg_req
{
    /**
     * The number of rounds that the FSG needs to execute,
     * (-1) infintie, (0) disable, (> 0) finite
     */
    int32_t n_iterations;
    /**
     * The duty cycle to maintain given the previously set transmission parameters.
     * Scaled by 100, (e.g. 98.5% -> 9850)
     */
    uint32_t duty_cycle_scaled;
    /**
     * The nominal inter-frame spacing between PSDUs. Must be (!= 0). A value of (-1)
     * will default to SIFS.
     */
    int32_t ifs_us;
};


static void usage(struct morsectrl *mors)
{
    mctrl_print("\tFast Symbol Generator (FSG) <command>\n");
    mctrl_print("\t\tenable <iterations> <duty cycle> [ifs]\n");
    mctrl_print("\t\t\titerations\tnumber of transmissions, set to -1 for infinite\n");
    mctrl_print(
            "\t\t\tduty cycle\tthe duty cycle to maintain between transmissions %%(%.2f-%.2f)\n",
            FSG_DUTY_CYCLE_MIN, FSG_DUTY_CYCLE_MAX);
    mctrl_print(
            "\t\t\tifs       \tinter-frame spacing between PSDUs in microseconds (default:%dus)\n",
            FSG_DEFAULT_IFS_US);
    mctrl_print("\t\tdisable\n");
}

int fsg(struct morsectrl *mors, int argc, char *argv[])
{
    int ret = -1;
    struct command_set_fsg_req *cmd;
    struct morsectrl_transport_buff *cmd_tbuff;
    struct morsectrl_transport_buff *rsp_tbuff;
    int32_t n_iterations, ifs_us;

    if (argc < 2)
    {
        usage(mors);
        return 0;
    }

    cmd_tbuff = morsectrl_transport_cmd_alloc(mors->transport, sizeof(*cmd));
    rsp_tbuff = morsectrl_transport_resp_alloc(mors->transport, 0);

    if (!cmd_tbuff || !rsp_tbuff)
        goto exit;

    cmd = TBUFF_TO_CMD(cmd_tbuff, struct command_set_fsg_req);

    switch (expression_to_int(argv[1]))
    {
        case true:
        {
            if (argc < 4)
            {
                usage(mors);
                ret = -1;
                goto exit;
            }

            if (str_to_int32(argv[2], &n_iterations) || n_iterations == 0)
            {
                mctrl_err("Invalid iteration value (must be non-zero).\n");
                usage(mors);
                ret = -1;
                goto exit;
            }

            cmd->n_iterations = htole32(n_iterations);

            float duty_cycle = strtof(argv[3], NULL);
            if ((duty_cycle < FSG_DUTY_CYCLE_MIN) || (duty_cycle > FSG_DUTY_CYCLE_MAX))
            {
                mctrl_err("Invalid duty cycle %.2f (%.2f-%.2f).\n",
                       duty_cycle, FSG_DUTY_CYCLE_MIN, FSG_DUTY_CYCLE_MAX);
                usage(mors);
                ret = -1;
                goto exit;
            }
            cmd->duty_cycle_scaled = (uint32_t)(duty_cycle * 100);
            cmd->ifs_us = FSG_DEFAULT_IFS_US;
            if (argc == 5)
            {
                if (str_to_int32(argv[4], &ifs_us))
                {
                    mctrl_err("Invalid interframe spacing (us)\n");
                    ret = -1;
                    goto exit;
                }
                cmd->ifs_us = htole32(ifs_us);
            }
            break;
        }
        case false:
        {
            cmd->n_iterations = 0; /* 0 means disable */
            break;
        }
        default:
        {
            mctrl_err("Invalid command parameters\n");
            usage(mors);
            ret = -1;
            goto exit;
        }
    }

    ret = morsectrl_send_command(mors->transport, MORSE_TEST_COMMAND_SET_FSG,
                                 cmd_tbuff, rsp_tbuff);

exit:
    if (ret < 0)
    {
        mctrl_err("Failed to set fsg\n");
    }

    morsectrl_transport_buff_free(cmd_tbuff);
    morsectrl_transport_buff_free(rsp_tbuff);
    return ret;
}

MM_CLI_HANDLER(fsg, MM_INTF_REQUIRED, MM_DIRECT_CHIP_SUPPORTED);
