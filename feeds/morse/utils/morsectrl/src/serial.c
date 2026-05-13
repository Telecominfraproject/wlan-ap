/*
 * Copyright 2021 Morse Micro
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "command.h"

static void usage(struct morsectrl *mors)
{
    mctrl_print("\tserial\n");
    mctrl_print("\t\t\t\treads the chip serial number from OTP (zeroes if none blown)\n");
}

/**
 * This address should be the same for all boards but some boards will
 * have the bits not blown which will always show zeroes in that case,
 * please update the address if necessary
 */
#define SERIAL_OTP_ADDR "0x1005412c"

int io(struct morsectrl *mors, int argc, char **argv);

int serial(struct morsectrl *mors, int argc, char *argv[])
{
    int ret;

    char io_cmd_name[] = "io";
    char io_read_arg[] = "-r";
    char otp_addr[] = SERIAL_OTP_ADDR;

    char *io_argv[] = {io_cmd_name, io_read_arg, otp_addr};
    int io_argc = MORSE_ARRAY_SIZE(io_argv);

    if (argc == 0)
    {
        usage(mors);
        return 0;
    }

    if (argc != 1)
    {
        mctrl_err("This command doesn't expect arguments\n");
        usage(mors);
        return -1;
    }
    else
    {
        ret = io(mors, io_argc, io_argv);
    }

    return ret;
}

MM_CLI_HANDLER(serial, MM_INTF_NOT_REQUIRED, MM_DIRECT_CHIP_NOT_SUPPORTED);
