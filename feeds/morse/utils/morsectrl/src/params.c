/*
 * Copyright 2023 Morse Micro
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

enum morse_param_action
{
    MORSE_PARAM_ACTION_SET = 0,
    MORSE_PARAM_ACTION_GET = 1,

    MORSE_PARAM_ACTION_LAST,
    MORSE_PARAM_ACTION_MAX = UINT32_MAX,
};

enum morse_param_id
{
    MORSE_PARAM_ID_MAX_TRAFFIC_DELIVERY_WAIT_US     = 0,
    MORSE_PARAM_ID_EXTRA_ACK_TIMEOUT_ADJUST_US      = 1,
    MORSE_PARAM_ID_TX_STATUS_FLUSH_WATERMARK        = 2,
    MORSE_PARAM_ID_TX_STATUS_FLUSH_MIN_AMPDU_SIZE   = 3,
    MORSE_PARAM_ID_POWERSAVE_TYPE                   = 4,
    MORSE_PARAM_ID_SNOOZE_ADJUST                    = 5,
    MORSE_PARAM_ID_TX_BLOCK                         = 6,
    MORSE_PARAM_ID_FORCED_SNOOZE_PERIOD_US          = 7,
    MORSE_PARAM_ID_WAKE_ACTION_GPIO                 = 8,
    MORSE_PARAM_ID_WAKE_ACTION_GPIO_PULSE_MS        = 9,
    MORSE_PARAM_ID_CONNECTION_MONITOR_GPIO          = 10,
    MORSE_PARAM_ID_INPUT_TRIGGER_GPIO               = 11,
    MORSE_PARAM_ID_INPUT_TRIGGER_MODE               = 12,

    MORSE_PARAM_ID_LAST,
    MORSE_PARAM_ID_MAX = UINT32_MAX,
};

struct PACKED command_param_req
{
    /** The parameter to perform the action on [enum morse_param_id] */
    uint32_t param_id;
    /** The action to take on the the parameter [get | set] */
    uint32_t action;
    /** Any flags to modify the behaviour of the action (for forwards/backwards compatibility) */
    uint32_t flags;
    /** The value to set (only applicable for set actions) */
    uint32_t value;
};

struct PACKED command_param_cfm
{
    /** Any flags to signal change of interpretation of response
     *  (forwards/backwards compatibility) */
    uint32_t flags;
    /** The value returned (only applicable for get actions) */
    uint32_t value;
};

struct param_entry;

/**
 * @brief Callback to process user input for setting a parameter entry.
 *
 * @param entry The parameter entry to set.
 * @param value The value provided by the user on the CLI.
 * @param req On success of parsing, the value field of the req struct will be filled.
 *
 * @return 0 on success, else specific error code.
 */
typedef int (*param_process_t)(const struct param_entry* entry,
    char* value, struct command_param_req* req);

/**
 * @brief Callback that formats the response of a get operation, printing to stdout.
 *
 * @param entry The parameter entry to format.
 * @param resp Pointer to the response to format.
 *
 * @return 0 on success, else specific error code.
 */
typedef int (*param_format_t)(const struct param_entry* entry, struct command_param_cfm* resp);

struct param_entry {
    /** ID of the parameter */
    enum morse_param_id id;
    /** Name of the parameter (used to match on user CLI input) */
    const char *name;
    /** The help message to display for the parameter */
    const char *help;
    /** Minimum allowed value of the parameter (can be of a different int type,
     * but cast to uint32) */
    uint32_t min_val;
    /** Maximum allowed value of the parameter (can be of a different int type,
     * but cast to uint32) */
    uint32_t max_val;
    /** Function that processes user input for the set command */
    param_process_t set_fn;
    /** Function that formats response of the get command to stdout */
    param_format_t get_fn;
};

static int param_set_uint32(const struct param_entry* entry, char* value,
    struct command_param_req* req)
{
    int ret;
    uint32_t val;

    ret = str_to_uint32_range(value, &val, entry->min_val, entry->max_val);
    if (ret)
    {
        mctrl_err("Failed to parse value for '%s' [min:%u, max:%u]\n",
            entry->name, entry->min_val, entry->max_val);
        return ret;
    }

    req->value = val;
    return 0;
}

static int param_get_uint32(const struct param_entry* entry, struct command_param_cfm* resp)
{
    mctrl_print("%u\n", resp->value);
    return 0;
}

static int param_set_int32(const struct param_entry* entry, char* value,
    struct command_param_req* req)
{
    int ret;
    int32_t val;

    ret = str_to_int32_range(value, &val, (int32_t)(entry->min_val), (int32_t)(entry->max_val));
    if (ret)
    {
        mctrl_err("Failed to parse value for '%s' [min:%d, max:%d]\n",
            entry->name, entry->min_val, entry->max_val);
        return ret;
    }

    req->value = (uint32_t)val;
    return 0;
}

static int param_get_int32(const struct param_entry* entry, struct command_param_cfm* resp)
{
    mctrl_print("%d\n", (int32_t)(resp->value));
    return 0;
}

/* Help strings for parameters should not have line control characters (e.g. '\n') embedded
 * within.
 */
struct param_entry params[] = {
    {
        .id = MORSE_PARAM_ID_MAX_TRAFFIC_DELIVERY_WAIT_US,
        .name = "traffic_delivery_wait",
        .help = "Time to wait for traffic delivery from the AP after the TIM "
                "is set in a busy BSS (us).",
        .min_val = 0,
        .max_val = UINT32_MAX,
        .set_fn = param_set_uint32,
        .get_fn = param_get_uint32,
    },
    {
        .id = MORSE_PARAM_ID_EXTRA_ACK_TIMEOUT_ADJUST_US,
        .name = "ack_timeout_adjust",
        .help = "Extra time to wait for 802.11 control response frames to be "
                "delivered (us).",
        .min_val = 0,
        .max_val = UINT32_MAX,
        .set_fn = param_set_uint32,
        .get_fn = param_get_uint32,
    },
    {
        .id = MORSE_PARAM_ID_WAKE_ACTION_GPIO,
        .name = "wake_action_gpio",
        .help = "Specify GPIO to pulse on reception of a Morse Micro "
                "wake action frame (-1 to disable).",
        .min_val = (uint32_t) -1,
        .max_val = INT32_MAX,
        .set_fn = param_set_int32,
        .get_fn = param_get_int32,
    },
    {
        .id = MORSE_PARAM_ID_WAKE_ACTION_GPIO_PULSE_MS,
        .name = "wake_action_gpio_pulse",
        .help = "Time to hold wake action GPIO high after reception of "
                "a morse micro wake action frame (ms).",
        .min_val = 50,
        .max_val = UINT32_MAX,
        .set_fn = param_set_uint32,
        .get_fn = param_get_uint32,
    },
    {
        .id = MORSE_PARAM_ID_CONNECTION_MONITOR_GPIO,
        .name = "connection_monitor_gpio",
        .help = "Specify GPIO that monitors and reflects device's "
                "802.11 connection status (-1 to disable).",
        .min_val = (uint32_t) -1,
        .max_val = INT32_MAX,
        .set_fn = param_set_int32,
        .get_fn = param_get_int32,
    },
    {
        .id = MORSE_PARAM_ID_INPUT_TRIGGER_GPIO,
        .name = "input_trigger_gpio",
        .help = "Specify GPIO that listens for an input signal to "
                "wake an external host (-1 to disable).",
        .min_val = (uint32_t) -1,
        .max_val = INT32_MAX,
        .set_fn = param_set_int32,
        .get_fn = param_get_int32,
    },
    {
        .id = MORSE_PARAM_ID_INPUT_TRIGGER_MODE,
        .name = "input_trigger_mode",
        .help = "Specify the active mode (high or low) for the trigger GPIO",
        .min_val = (uint32_t) -1,
        .max_val = INT32_MAX,
        .set_fn = param_set_int32,
        .get_fn = param_get_int32,
    },
#if !defined (MORSE_CLIENT)
    {
        .id = MORSE_PARAM_ID_TX_STATUS_FLUSH_WATERMARK,
        .name = "tx_status_flush_watermark",
        .help = "Number of pending tx statuses in the chip that will trigger a "
                "flush event back to the host.",
        .min_val = 1,
        .max_val = UINT32_MAX,
        .set_fn = param_set_uint32,
        .get_fn = param_get_uint32,
    },
    {
        .id = MORSE_PARAM_ID_TX_STATUS_FLUSH_MIN_AMPDU_SIZE,
        .name = "tx_status_flush_min_ampdu_size",
        .help = "Minimum number of mpdus in an AMPDU that will trigger an "
                "immediate flush of all pending tx statuses back to the host "
                "on tx completion.",
        .min_val = 0,
        .max_val = UINT32_MAX,
        .set_fn = param_set_uint32,
        .get_fn = param_get_uint32,
    },
    {
        .id = MORSE_PARAM_ID_POWERSAVE_TYPE,
        .name = "powersave_type",
        .help = "The type of powersave normal snooze: 0, "
                "superb snooze (testmode only): 1, "
                "super snooze: 2.",
        .min_val = 0,
        .max_val = 2,
        .set_fn = param_set_uint32,
        .get_fn = param_get_uint32,
    },
    {
        .id = MORSE_PARAM_ID_SNOOZE_ADJUST,
        .name = "snooze_adjust",
        .help = "Adjust the time to snooze for during power save (in usec) "
                "(+ve => longer snooze, -ve => shorter snooze)",
        .min_val = (uint32_t)(INT32_MIN),
        .max_val = (uint32_t)(INT32_MAX),
        .set_fn = param_set_int32,
        .get_fn = param_get_int32,
    },
    {
        .id = MORSE_PARAM_ID_TX_BLOCK,
        .name = "tx_block",
        .help = "Block the chip from transmitting",
        .min_val = 0,
        .max_val = 1,
        .set_fn = param_set_uint32,
        .get_fn = param_get_uint32,
    },
    {
        .id = MORSE_PARAM_ID_FORCED_SNOOZE_PERIOD_US,
        .name = "forced_snooze_period_us",
        .help = "Set a sleep period (uS) to force the chip to snooze even when not associated. "
                "To disable forced snooze test, set to 0.",
        .min_val = 0,
        .max_val = UINT32_MAX,
        .set_fn = param_set_uint32,
        .get_fn = param_get_uint32,
    },
#endif
};

static int get_line(const char **start, const char *end)
{
    const int max_len = 60; /* Max line length after leading tabs */
    int len = 0;
    int last_space = max_len + 1;
    const char *p;
    char *eol = MIN((char *)end, (char *)(*start) + max_len);

    /* Find first char to print on this line */
    while ((*start < eol) && ((**start == ' ') || (**start == '\n')))
    {
        (*start)++;
    }
    p = *start;

    /* Search up to the max characters that can be printed for a line
     * termination or end-of-text.
     * Keep track of the last space found, so the text is not split in the
     * middle of a word.
     */
    while (p <= eol)
    {
        if (*p == '\0' || *p == '\n')
        {
            return len;
        }
        if (*p == ' ')
        {
            last_space = len;
        }
        len++;
        p++;
    }

    return MIN(last_space, len);
}

static void print_param_help(const struct param_entry* param)
{
    const char *prefix = "\t\t\t";
    const char *start = param->help;
    const char *end = start + strlen(param->help);
    int len = 0;

    len = get_line(&start, end);
    while (start < end)
    {
        mctrl_print("%s%.*s\n", prefix, len, start);
        start += len;
        len = get_line(&start, end);
    }
}

static void set_help(struct morsectrl *mors)
{
    mctrl_print("\tset <param> <value>\n");

    for (unsigned int entry = 0; entry < MORSE_ARRAY_SIZE(params); entry++)
    {
        const struct param_entry* param = &params[entry];

        if (param->set_fn == NULL)
        {
            continue;
        }

        mctrl_print("\t\t%s\n", param->name);
        print_param_help(param);
    }
}

static void get_help(struct morsectrl *mors)
{
    mctrl_print("\tget <param>\n");

    for (unsigned int entry = 0; entry < MORSE_ARRAY_SIZE(params); entry++)
    {
        const struct param_entry* param = &params[entry];

        if (param->get_fn == NULL)
        {
            continue;
        }

        mctrl_print("\t\t%s\n", param->name);
        print_param_help(param);
    }
}

static const struct param_entry* match_str_to_param(char *str, enum morse_param_action action)
{
    for (unsigned int entry = 0; entry < MORSE_ARRAY_SIZE(params); entry++)
    {
        const struct param_entry* param = &params[entry];

        if (strncmp(str, param->name, strlen(str)) == 0)
        {
            if ((action == MORSE_PARAM_ACTION_SET) && (param->set_fn == NULL))
            {
                break;
            }

            if ((action == MORSE_PARAM_ACTION_GET) && (param->get_fn == NULL))
            {
                break;
            }

            return param;
        }
    }

    return NULL;
}

static void param_help(struct morsectrl *mors, enum morse_param_action action)
{
    if (action == MORSE_PARAM_ACTION_SET)
    {
        set_help(mors);
    }
    else if (action == MORSE_PARAM_ACTION_GET)
    {
        get_help(mors);
    }
}

static int param_get_set(struct morsectrl *mors,
    enum morse_param_action action, int argc, char *argv[])
{
    int ret = MORSE_ARG_ERR;
    const struct param_entry *param;
    struct command_param_req *cmd;
    struct command_param_cfm *rsp;
    struct morsectrl_transport_buff *cmd_tbuff;
    struct morsectrl_transport_buff *rsp_tbuff;

    param = match_str_to_param(argv[1], action);
    if (param == NULL)
    {
        mctrl_err("Invalid parameter: '%s'\n", argv[1]);
        param_help(mors, action);
        return ret;
    }

    cmd_tbuff = morsectrl_transport_cmd_alloc(mors->transport, sizeof(*cmd));
    rsp_tbuff = morsectrl_transport_resp_alloc(mors->transport, sizeof(*rsp));

    if (!cmd_tbuff || !rsp_tbuff)
        goto exit;

    cmd = TBUFF_TO_CMD(cmd_tbuff, struct command_param_req);
    rsp = TBUFF_TO_RSP(rsp_tbuff, struct command_param_cfm);

    cmd->param_id = param->id;
    cmd->action = action;
    cmd->flags = 0;

    if (action == MORSE_PARAM_ACTION_SET)
    {
        ret = param->set_fn(param, argv[2], cmd);
        if (ret < 0)
        {
            goto exit;
        }
    }

    ret = morsectrl_send_command(mors->transport,
        MORSE_COMMAND_GET_SET_GENERIC_PARAM, cmd_tbuff, rsp_tbuff);
exit:
    if (ret == 0)
    {
        if (action == MORSE_PARAM_ACTION_GET)
        {
            param->get_fn(param, rsp);
        }
    }
    else
    {
        mctrl_err("Failed to %s parameter: '%s'\n",
            (action == MORSE_PARAM_ACTION_SET) ? "set" : "get",
            param->name);
        ret = MORSE_CMD_ERR;
    }

    morsectrl_transport_buff_free(cmd_tbuff);
    morsectrl_transport_buff_free(rsp_tbuff);
    return ret;
}

int get(struct morsectrl *mors, int argc, char *argv[])
{
    if (argc == 0)
    {
        get_help(mors);
        return 0;
    }

    if (argc != 2)
    {
        mctrl_err("Invalid command parameters\n");
        get_help(mors);
        return MORSE_ARG_ERR;
    }

    return param_get_set(mors, MORSE_PARAM_ACTION_GET, argc, argv);
}

int set(struct morsectrl *mors, int argc, char *argv[])
{
    if (argc == 0)
    {
        set_help(mors);
        return 0;
    }

    if (argc < 3)
    {
        mctrl_err("Invalid command parameters\n");
        set_help(mors);
        return MORSE_ARG_ERR;
    }

    return param_get_set(mors, MORSE_PARAM_ACTION_SET, argc, argv);
}

MM_CLI_HANDLER(get, MM_INTF_REQUIRED, MM_DIRECT_CHIP_SUPPORTED);
MM_CLI_HANDLER(set, MM_INTF_REQUIRED, MM_DIRECT_CHIP_SUPPORTED);
