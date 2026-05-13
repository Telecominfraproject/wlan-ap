/*
 * Copyright 2020 Morse Micro
 */

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <stdint.h>
#include <getopt.h>
#include <ctype.h>

#include "morsectrl.h"
#include "command.h"

#ifndef MORSE_CLIENT
char TOOL_NAME[] = "morsectrl";
#else
char TOOL_NAME[] = "morse_cli";
#endif

#ifndef MORSECTRL_VERSION_STRING
#define MORSECTRL_VERSION_STRING "Undefined"
#endif

static struct
{
    struct arg_lit *debug;
    struct arg_rex *trans_opts;
    struct arg_str *iface;
    struct arg_str *cfg_opts;
    struct arg_str *file_opts;
    struct arg_lit *version;
    struct arg_str *command;
} args;

extern struct command_handler __start_cli_handlers[];
extern struct command_handler __stop_cli_handlers[];

static void usage(struct morsectrl *mors)
{
    struct command_handler *handler;

    morsectrl_transport_list_available();

    mctrl_print("\nInterface Commands:\n");

    for (handler = __start_cli_handlers;
         handler < __stop_cli_handlers;
         handler++)
    {
        if (handler->is_intf_cmd == MM_INTF_REQUIRED)
        {
            if (handler->direct_chip_supported_cmd ||
                morsectrl_transport_driver_commands_supported())
            {
                if (handler->init)
                {
                    handler->init(mors, &handler->args);
                    mm_help_argtable(handler->name, &handler->args);
                }
                else
                {
                    handler->handler(mors, 0, NULL);
                }
            }
        }
    }

    mctrl_print("\nGeneral Commands (no interface required):\n");
    for (handler = __start_cli_handlers;
         handler < __stop_cli_handlers;
         handler++)
    {
        if (handler->is_intf_cmd == MM_INTF_NOT_REQUIRED)
        {
            if (handler->direct_chip_supported_cmd ||
                morsectrl_transport_driver_commands_supported())
            {
                if (handler->init)
                {
                    handler->init(mors, &handler->args);
                    mm_help_argtable(handler->name, &handler->args);
                }
                else
                {
                    handler->handler(mors, 0, NULL);
                }
            }
        }
    }
}

static void print_version(void)
{
    /* print Morsectrl Version: ... or Morse_cli Version: ...
    The leading captial letter is for backwards compatibility
    */
    mctrl_print("%c%s Version: %s\n",
        toupper(TOOL_NAME[0]),
        TOOL_NAME + 1,
        MORSECTRL_VERSION_STRING);
}

int cmdname_cmp(const void * a, const void * b) {
    return strcmp(((struct command_handler *)a)->name, ((struct command_handler *)b)->name);
}


int main(int argc, char *argv[])
{
    struct command_handler *handler = NULL;
    int ret = MORSE_OK;
    char *trans_opts = NULL;
    char *iface_opts = NULL;
    char *cfg_opts = NULL;
    char *file_opts = NULL;
    char *transport_regex;
    struct mm_argtable main_args = {};

    struct morsectrl mors = {
        .debug = false,
    };

    transport_regex = morsectrl_transport_get_regex();
    if (transport_regex == NULL)
    {
        return 1;
    }

    qsort(__start_cli_handlers, (__stop_cli_handlers - __start_cli_handlers),
          sizeof(struct command_handler), cmdname_cmp);

    MM_INIT_ARGTABLE((&main_args), NULL,
                     args.debug = arg_lit0("d", "debug", "show debug messages for given command"),
                     args.iface = arg_str0("i", "interface", NULL,
                                           "specify the interface for the transport "
                                           "(default " DEFAULT_INTERFACE_NAME ")"),
                     args.file_opts = arg_str0("f", "configfile", NULL,
                                               "specify config file with transport/interface/config"
                                               " (command line will override file contents)"),
                     args.trans_opts = arg_rex0("t", "transport", transport_regex,
                                                transport_regex, 0,
                                                "specify the transport to use"),
                     args.cfg_opts = arg_str0("c", "config", NULL,
                                              "specify the config for the transport"),
                     args.version = arg_lit0("v", NULL, "print the version"),
                     args.command = arg_str1(NULL, NULL, "command", "sub-command to run"));

    args.iface->sval[0] = DEFAULT_INTERFACE_NAME;

    args.command->hdr.flag |= ARG_STOPPARSE;

    ret = mm_parse_argtable_noerror(NULL, &main_args, argc, argv);

    if (args.debug->count)
    {
        mors.debug = true;
    }

    if (args.iface->count)
    {
        iface_opts = (char *)args.iface->sval[0];
    }

    if (args.file_opts->count)
    {
        file_opts = (char *)args.file_opts->sval[0];
    }

    if (args.trans_opts->count)
    {
        trans_opts = (char *)args.trans_opts->sval[0];
    }

    if (args.cfg_opts->count)
    {
        cfg_opts = (char *)args.cfg_opts->sval[0];
    }

    if (ret)
    {
        if (main_args.help->count)
        {
            morsectrl_transport_parse(&mors.transport, mors.debug,
                                      trans_opts, iface_opts, cfg_opts);
            usage(&mors);
        }
        else if (args.version->count)
        {
            print_version();
            ret = 0;
        }
        else
        {
            arg_print_errors(stdout, main_args.end, TOOL_NAME);
            mctrl_err("Try %s --help for more information\n", TOOL_NAME);
        }
        goto exit;
    }

    if (file_opts)
    {
        ret = morsectrl_config_file_parse(file_opts,
                                          &trans_opts,
                                          &iface_opts,
                                          &cfg_opts,
                                          mors.debug);

        if (ret)
            goto exit;
    }

    ret = morsectrl_transport_parse(&mors.transport, mors.debug, trans_opts, iface_opts, cfg_opts);
    if (ret)
        goto exit;

    for (handler = __start_cli_handlers;
         handler < __stop_cli_handlers;
         handler++)
    {
        if (!strcmp(args.command->sval[0], handler->name))
        {
            if (mors.debug)
            {
                mctrl_print("Calling: %s ", handler->name);
                for (int j = 1; j < argc; j++)
                {
                    mctrl_print("%s ", argv[j]);
                }
                mctrl_print("\n");
            }

            if (handler->init)
            {
                if (handler->init(&mors, &handler->args))
                {
                    ret = MORSE_ARG_ERR;
                    goto exit;
                }

                if ((ret = mm_parse_argtable(handler->name, &handler->args,
                                             argc - args.command->hdr.idx,
                                             argv + args.command->hdr.idx)) != 0)
                {
                    mm_free_argtable(&handler->args);
                    if (ret > 0)
                        ret = MORSE_ARG_ERR;
                    else
                        ret = 0;

                    goto exit;
                }
            }
            else
            {
                /* Legacy handlers need argc and argv updated, and also optind resset */
                argc -= args.command->hdr.idx;
                argv += args.command->hdr.idx;
                optind = 1;
            }

            if (handler->direct_chip_supported_cmd != MM_DIRECT_CHIP_SUPPORTED &&
                !morsectrl_transport_has_driver(mors.transport))
            {
                mctrl_err("Command '%s' cannot be used with transport %s\n", handler->name,
                          trans_opts);
                mctrl_err("To check valid commands run 'morsectrl -t %s -h'\n", trans_opts);
                ret = ETRANSFTDISPIERR;
                goto exit;
            }

            if (!strcmp(handler->name, "version"))
                print_version();

            if (handler->is_intf_cmd == MM_INTF_REQUIRED ||
                (!strncmp(handler->name, "reset", strlen(handler->name)) &&
                 morsectrl_transport_has_reset(mors.transport)))
            {
                ret = morsectrl_transport_init(mors.transport);
                if (ret)
                {
                    mctrl_err("Transport init failed\n");
                    goto exit;
                }
            }

            ret = handler->handler(&mors, argc, argv);
            goto transport_exit;
        }
    }

    ret = MORSE_CMD_ERR;
    mctrl_err("Invalid command '%s'\n", args.command->sval[0]);
    mctrl_err("Try %s --help for more information\n", TOOL_NAME);

transport_exit:
    if (handler != NULL && (handler < __stop_cli_handlers) &&
        handler->is_intf_cmd == MM_INTF_REQUIRED)
        morsectrl_transport_deinit(mors.transport);
exit:
    /**
     * For return codes less than 0, or greater than 255 (i.e. the nix return code error range)
     * remap error to MORSE_CMD_ERR. The return code 255 (-1) is avoided as ssh uses this to
     * indicate an ssh error.
     */
    if ((ret < 0) || (ret > 254))
    {
        ret = MORSE_CMD_ERR;
    }
    return ret;
}
