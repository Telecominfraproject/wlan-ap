/*
 * Copyright 2020 Morse Micro
 */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/ioctl.h>

#include "command.h"

char *MORSE_IO_DEV_NAMES[] =
{
    "/dev/morse_io",
    "/dev/morsef2", /* Legacy */
    "/dev/morsef1"  /* Legacy */
};

static void usage()
{
    mctrl_print("\tio [-rh] [-s size] [-f filename] <address> [value]\n");
    mctrl_print("\t\t-h\t\tprint this message\n"
           "\t\t-v\t\tenable verbose\n"
           "\t\t-r\t\tread from chip [default is write]\n"
           "\t\t-s <size>\tsize of file read/write [default w:filesize r:4]\n"
           "\t\t-f\t\tfilename read/write to file\n");
}

static size_t file_size(FILE *fptr)
{
    size_t size;
    fseek(fptr, 0L, SEEK_END);
    size = ftell(fptr);
    fseek(fptr, 0L, SEEK_SET);
    return size;
}

int io(struct morsectrl *mors, int argc, char *argv[])
{
    int fd;
    int ret  = 0;
    int tx_bytes = 0;
    int opt_index, option;
    int count = 0, size = 0;
    int dir_write = 1, func = 2, verbose = 0;
    uint32_t value = 0, address = 0, *data = &value, *buffer = NULL;
    char *endptr, *filename = NULL;
    FILE *fptr = NULL;

    /* Reading options */
    while ((option = getopt(argc, argv, "rhvFs:f:")) != -1)
    {
        switch (option)
        {
            case 'r' :
                dir_write = 0;
                break;
            case 's' :
                if (str_to_int32(optarg, &size))
                {
                    mctrl_err("Invalid size argument\n");
                    return -1;
                }
                break;
            case 'f' :
                filename = optarg;
                break;
            case 'v' :
                verbose = 1;
                break;
            case 'h':
                usage();
                return 0;
            case -1:
                break;
            default:
                mctrl_err("Invalid option\n");
                usage();
                return -1;
        }
    }
    opt_index = optind;

    /* Reading address */
    if (opt_index >= argc)
    {
        usage();
        return -1;
    }
    address = strtoul(argv[opt_index++], &endptr, 0);
    if (*endptr != '\0')
    {
        mctrl_err("Invalid address\n");
        usage();
        return -1;
    }

    /* Open file */
    if (filename != NULL)
    {
        if (dir_write == 1)
        {
            fptr = fopen(filename, "rb");
        }
        else
        {
            fptr = fopen(filename, "wb");
        }
        if (!fptr)
        {
            mctrl_err("Couldn't open file\n");
            return -1;
        }
    }

    /* calculate size */
    if (fptr)
    {
        /* if size set use it, else write ? filesize : 4 */
        size = size ? size : dir_write ? file_size(fptr) : 4;
    }
    else
    {
        if (size)
        {
            mctrl_err("Invalid size. Only set with file access\n");
            usage();
            return -1;
        }
        size = 4;
    }

    /* Allocate buffer if required (i.e > 4)*/
    if (size > 4)
    {
        buffer = malloc(size);
        if (!buffer)
        {
            mctrl_err("Failed to allocate memory\n");
            fclose(fptr);
            return (-ENOMEM);
        }
        data = buffer;
    }

    /* Read input */
    if (dir_write == 1)
    {
        if (fptr)
        {
            /* From input file */
            fread(data, 1, size, fptr);
        }
        else
        {
            /* Read from command line */
            if (opt_index >= argc)
            {
                usage();
                return -1;
            }
            value = strtoul(argv[opt_index], &endptr, 0);
            if (*endptr != '\0')
            {
                mctrl_err("Invalid value\n");
                usage();
                return -1;
            }
        }
    }

    /* open device */
    int retries = 0;
    while (retries < MORSE_ARRAY_SIZE(MORSE_IO_DEV_NAMES))
    {
        if ((fd = open(MORSE_IO_DEV_NAMES[retries], O_RDWR)) >= 0)
        {
            break;
        }
        retries++;
    }
    if (retries == MORSE_ARRAY_SIZE(MORSE_IO_DEV_NAMES))
    {
        mctrl_err("Failed to open device file\n");
        return -1;
    }

    if (verbose)
    {
        mctrl_print("%s %d bytes %s Func%d (%s %s)\n",
           dir_write ? "writing" : "reading",
           size,
           dir_write ? "to": "from" ,
           func,
           dir_write ? "from" : "to",
           fptr ? filename : "command line");
    }

    /* write/read */
    while (count < size)
    {
        ioctl(fd, _IO('k', 1), address + count);
        tx_bytes = dir_write ? write(fd, data + count, size) : read(fd, data + count, size);
        if (tx_bytes < 0)
        {
            mctrl_err("Failed to %s device\n", dir_write ? "write to" : "read from");
            return -1;
        }
        if (!dir_write)
        {
            /* write data */
            if (fptr)
            {
                fwrite(data, 1, tx_bytes, fptr);
            }
            else
            {
                mctrl_print("0x%08X\n", value);
            }
        }
        count += tx_bytes;
    }

    return ret;
}

MM_CLI_HANDLER(io, MM_INTF_NOT_REQUIRED, MM_DIRECT_CHIP_NOT_SUPPORTED);
