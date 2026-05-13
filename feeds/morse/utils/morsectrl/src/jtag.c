/*
 * Copyright 2020 Morse Micro
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "command.h"
#include "gpioctrl.h"
#include "utilities.h"

static void usage(struct morsectrl *mors)
{
    mctrl_print("\tjtag <disable|enable> [GPIO]\n");
    mctrl_print("\t\t\t\tdisable|enable jtag through RPi GPIO pin\n");
}

int morsectrl_jtag(int enable, int jtag_gpio)
{
    int ret;
    const char *direction = enable ? "out" : "in";

    ret = gpio_export(jtag_gpio);
    if (ret)
    {
        goto exit;
    }

    ret = gpio_set_dir(jtag_gpio, direction);
    if (ret)
    {
        goto exit;
    }

    if (enable)
    {
        ret = gpio_set_val(jtag_gpio, 1);
        if (ret)
        {
             goto exit;
        }
        sleep_ms(5);
    }

exit:
    return ret;
}

int jtag(struct morsectrl *mors, int argc, char *argv[])
{
    int ret, jtag_gpio, enable;

    if (argc == 0)
    {
        usage(mors);
        return 0;
    }

    if (argc != 3)
    {
        if (argc == 2)
        {
            jtag_gpio = gpio_get_env(JTAG_GPIO);
            if (jtag_gpio == -1)
            {
                mctrl_err("Couldn't identify GPIO\n"
                "Try entering GPIO manually or export %s to your env var\n", JTAG_GPIO);
                usage(mors);
                return -1;
            }
        }
        else
        {
            mctrl_err("Invalid command parameters\n");
            usage(mors);
            return -1;
        }
    }
    else
    {
        if (str_to_int32(argv[2], &jtag_gpio))
        {
            mctrl_err("Invalid JTAG gpio\n");
            usage(mors);
            return -1;
        }
    }

    enable = expression_to_int(argv[1]);
    if (enable == -1)
    {
        mctrl_err("Invalid option.\n");
        usage(mors);
        return -1;
    }

    ret = morsectrl_jtag(enable, jtag_gpio);
    if (ret < 0)
    {
        mctrl_err("Failed to enable jtag\n");
    }

    return ret;
}

MM_CLI_HANDLER(jtag, MM_INTF_NOT_REQUIRED, MM_DIRECT_CHIP_SUPPORTED);
