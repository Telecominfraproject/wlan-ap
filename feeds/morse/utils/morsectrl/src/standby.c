/*
 * Copyright 2022 Morse Micro
 */

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <stdint.h>
#include <dirent.h>
#include <sys/stat.h>
#include <ctype.h>

#include "command.h"
#include "utilities.h"
#include "transport/transport.h"
#include "channel.h"

/** The maximum allowed length of a user specified payload (bytes) in the status frames */
#define STANDBY_STATUS_FRAME_USER_PAYLOAD_MAX_LEN  (64)
/** The maximum allowed length of a user filter to apply to wake frames */
#define STANDBY_WAKE_FRAME_USER_FILTER_MAX_LEN     (64)

/** Max line length for a config file line */
#define MAX_LINE_LENGTH     (255)

typedef enum
{
    /** The external host is indicating that it's now awake */
    STANDBY_MODE_CMD_EXIT = 0x0,
    /** The external host is indicating that it's going into standby mode */
    STANDBY_MODE_CMD_ENTER,
    /** This version of the config command has since been deprecated */
    STANDBY_MODE_CMD_SET_CONFIG_V1_DEPRECATED,
    /** The external host provides a payload that gets appended to status frames */
    STANDBY_MODE_CMD_SET_STATUS_PAYLOAD,
    /** The external host provides a filter to be applied to incoming standby wake frames */
    STANDBY_MODE_CMD_SET_WAKE_FILTER,
    /** The external host sets a number of configuration options for standby mode */
    STANDBY_MODE_CMD_SET_CONFIG_V2,

    /** Force enum to UINT32 */
    STANDBY_MODE_CMD_MAX = UINT32_MAX,
} standby_mode_commands_t;

struct PACKED command_standby_set_config
{
    /** Interval for transmitting Standby status packets */
    uint32_t notify_period_s;
    /** Time for firmware to wait during beacon loss before entering deep sleep in seconds */
    uint32_t bss_inactivity_before_deep_sleep_s;
    /** Time for firmware to remain in deep sleep in seconds */
    uint32_t deep_sleep_period_s;
    /** Source IP address */
    ipv4_addr_t src_ip;
    /** Destination IP address */
    ipv4_addr_t dst_ip;
    /** Destination UDP Port */
    uint16_t dst_port;
    /** pad to word boundary */
    uint8_t pad[2];
    /** Time in seconds to increment each successive deep sleep */
    uint32_t deep_sleep_increment_s;
    /** Max time to deep sleep for */
    uint32_t deep_sleep_max_s;
};

struct PACKED command_standby_set_wake_filter
{
    /** The length of the wake filter */
    uint32_t len;
    /** The offset at which to apply the wake filter */
    uint32_t offset;
    /** The user-defined filter */
    uint8_t filter[STANDBY_WAKE_FRAME_USER_FILTER_MAX_LEN];
};

struct PACKED command_standby_set_status_payload
{
    /** The length of the payload */
    uint32_t len;
    /** The payload */
    uint8_t payload[STANDBY_STATUS_FRAME_USER_PAYLOAD_MAX_LEN];
};

struct PACKED command_standby_enter
{
    /** The BSSID to monitor for activity (or lack thereof) before entering deep sleep */
    uint8_t bssid[MAC_ADDR_LEN];
};

/**
 * Structure for Configuring MM standby mode
 */
struct PACKED command_standby_mode_req
{
    /** Standby Mode subcommands, see @ref standby_mode_commands_t */
    uint32_t cmd;
    union {
        uint8_t opaque[0];
        /** Valid for STANDBY_MODE_CMD_SET_CONFIG cmd */
        struct command_standby_set_config config;
        /** Valid for STANDBY_MODE_CMD_SET_STATUS_PAYLOAD cmd */
        struct command_standby_set_status_payload set_payload;
        /** Valid for STANDBY_MODE_CMD_ENTER cmd*/
        struct command_standby_enter enter;
        /** Valid for STANDBY_MODE_CMD_SET_WAKE_FILTER cmd */
        struct command_standby_set_wake_filter set_filter;
    };
};

typedef enum
{
    /** No specific reason for exiting standby mode */
    STANDBY_MODE_EXIT_REASON_NONE,
    /** The STA has received the wakeup frame */
    STANDBY_MODE_EXIT_REASON_WAKEUP_FRAME,
    /** The STA needs to associate */
    STANDBY_MODE_EXIT_REASON_ASSOCIATE,

    /** Force to UINT8_MAX */
    STANDBY_MODE_EXIT_REASON_MAX = UINT8_MAX,
} standby_mode_exit_reason_t;

/**
 * Response structure for MM standby mode command
 */
struct PACKED command_standby_mode_cfm
{
    union {
        uint8_t opaque[0];
        /** Valid for a response to STANDBY_MODE_CMD_EXIT, see @ref standby_mode_exit_reason_t */
        uint8_t reason;
    };
};

struct standby_config_parse_context
{
    struct command_standby_set_config *set_cfg;
    struct command_standby_set_wake_filter *filter_cfg;
};

struct standby_session_parse_context
{
    uint8_t *bssid;
    struct command_set_channel_req *req;
};

static int standby_get_cmd(char str[])
{
    if (strcmp("enter", str) == 0) return STANDBY_MODE_CMD_ENTER;
    else if (strcmp("exit", str) == 0) return STANDBY_MODE_CMD_EXIT;
    else if (strcmp("config", str) == 0) return STANDBY_MODE_CMD_SET_CONFIG_V2;
    else if (strcmp("payload", str) == 0) return STANDBY_MODE_CMD_SET_STATUS_PAYLOAD;
    else
    {
        return -1;
    }
}

static void usage(struct morsectrl *mors)
{
    mctrl_print("\tstandby <command> [options]\n");
    mctrl_print("\t\tenter [<session dir>]\n"
                "\t\t\tPut the STA FW into standby mode.\n");
    mctrl_print(
           "\t\t<session dir>\tThe full directory path for storing persistent sessions,\n"
           "\t\t\twhich should be obtained from the wpa_supplicant standby_config_dir\n"
           "\t\t\tconfiguration parameter. This parameter is no longer required\n"
           "\t\t\tand is retained for backwards compatibility.\n");
    mctrl_print("\t\texit \tTell the STA FW that the external host is awake.\n");
    mctrl_print("\t\tpayload\t<hex string of user data to append to standby status frames>\n");
    mctrl_print("\t\tconfig\t <config file>\t Configure standby mode\n"
            "\t\t\t <config file> Path to file containing Standby mode configuration parameters\n");
    if (mors->debug)
    {
        mctrl_print("\t\tstore -b <bssid> -d <dir>\t"
                   "Store session information when associated (internal use only)\n"
               "\t\t\t-b <bssid>\t\tthe association BSSID\n"
               "\t\t\t-d <dir>\t\tthe full directory path for storing persistent sessions\n");
    }
}

static int parse_standby_config_keyval(struct morsectrl *mors, void *context, const char *key,
                                    const char *val)
{
    struct standby_config_parse_context *config = context;
    uint32_t temp;
    ipv4_addr_t temp_ip;

    if (mors->debug)
    {
        mctrl_print("standby_config: %s - %s\n", key, val);
    }

    if (strcmp("notify_period_s", key) == 0)
    {
        if (str_to_uint32(val, &temp) < 0)
        {
            goto error;
        }
        config->set_cfg->notify_period_s = htole32(temp);
        return 0;
    }
    else if (strcmp("bss_inactivity_before_deep_sleep_s", key) == 0)
    {
        if (str_to_uint32(val, &temp) < 0)
        {
            goto error;
        }
        config->set_cfg->bss_inactivity_before_deep_sleep_s = htole32(temp);
        return 0;
    }
    else if (strcmp("deep_sleep_period_s", key) == 0)
    {
        if (str_to_uint32(val, &temp) < 0)
        {
            goto error;
        }
        config->set_cfg->deep_sleep_period_s = htole32(temp);
        return 0;
    }
    else if (strcmp("src_ip", key) == 0)
    {
        if (str_to_ip(val, &temp_ip) < 0)
        {
            goto error;
        }
        memcpy(&config->set_cfg->src_ip, &temp_ip, sizeof(config->set_cfg->src_ip));
        return 0;
    }
    else if (strcmp("dest_ip", key) == 0)
    {
        if (str_to_ip(val, &temp_ip) < 0)
        {
            goto error;
        }
        memcpy(&config->set_cfg->dst_ip, &temp_ip, sizeof(config->set_cfg->dst_ip));
        return 0;
    }
    else if (strcmp("dest_port", key) == 0)
    {
        if (str_to_uint32(val, &temp) < 0)
        {
            goto error;
        }
        config->set_cfg->dst_port = htole16((uint16_t) temp);
        return 0;
    }
    else if (strcmp("deep_sleep_increment_s", key) == 0)
    {
        if (str_to_uint32(val, &temp) < 0)
        {
            goto error;
        }
        config->set_cfg->deep_sleep_increment_s = htole32(temp);
        return 0;
    }
    else if (strcmp("deep_sleep_max_s", key) == 0)
    {
        if (str_to_uint32(val, &temp) < 0)
        {
            goto error;
        }
        config->set_cfg->deep_sleep_max_s = htole32(temp);
        return 0;
    }
    else if (strcmp("wake_packet_filter", key) == 0)
    {
        uint32_t len = MIN(strlen(val) / 2, MORSE_ARRAY_SIZE(config->filter_cfg->filter));
        hexstr2bin(val, config->filter_cfg->filter, len);
        config->filter_cfg->len = htole32(len);
        return 0;
    }
    else if (strcmp("wake_packet_filter_offset", key) == 0)
    {
        if (str_to_uint32(val, &temp) < 0)
        {
            goto error;
        }
        config->filter_cfg->offset = htole32(temp);
        return 0;
    }
    mctrl_err("Key is not a recognised parameter: %s\n", key);
    return 0;

error:
    mctrl_err("Failed to parse value for %s (val: %s)\n", key, val);
    return -1;
}

static int parse_standby_session_keyval(struct morsectrl *mors, void *context, const char *key,
                                            const char *val)
{
    struct standby_session_parse_context *ctx = context;
    uint32_t temp;

    if (mors->debug)
    {
        mctrl_print("standby_session: %s - %s\n", key, val);
    }

    if (strcmp("bssid", key) == 0)
    {
        if (str_to_mac_addr(ctx->bssid, val) < 0)
        {
            goto error;
        }
        return 0;
    }
    else if (strcmp("op_chan_freq", key) == 0)
    {
        if (str_to_uint32(val, &temp) < 0)
        {
            goto error;
        }
        ctx->req->operating_channel_freq_hz = htole32(temp);
        return 0;
    }
    else if (strcmp("op_chan_bw", key) == 0)
    {
        if (str_to_uint32(val, &temp) < 0)
        {
            goto error;
        }
        ctx->req->operating_channel_bw_mhz = (uint8_t) temp;
        return 0;
    }
    else if (strcmp("pri_chan_bw", key) == 0)
    {
        if (str_to_uint32(val, &temp) < 0)
        {
            goto error;
        }
        ctx->req->primary_channel_bw_mhz = (uint8_t) temp;
        return 0;
    }
    else if (strcmp("pri_1mhz_chan", key) == 0)
    {
        if (str_to_uint32(val, &temp) < 0)
        {
            goto error;
        }
        ctx->req->primary_1mhz_channel_index = (uint8_t) temp;
        return 0;
    }

    mctrl_err("Key is not a recognised parameter: %s\n", key);
    return 0;

error:
    mctrl_err("Failed to parse value for %s (val: %s)\n", key, val);
    return -1;
}

/**
 * @brief Generic config file parser.
 * Scans lines and calls a handler function for each `key=value` it finds.
 * Lines starting with a `#` will be interpreted as a comment and ignored.
 *
 * @param mors Morsectrl object
 * @param conf_file File to parse
 * @param keyval_process Callback function called for each key value
 * @param context Context pointer for callback function
 * @return int 0 if successfully parsed, otherwise negative value
 */
static int config_parse(struct morsectrl *mors, const char *conf_file,
                        int (*keyval_process)(
                                    struct morsectrl *mors,
                                    void *context,
                                    const char *key,
                                    const char *val),
                        void *context)
{
    int ret;
    FILE *cfg_file;
    char line[MAX_LINE_LENGTH];
    uint16_t line_num = 0;

    if (!conf_file || !keyval_process || is_dir(conf_file))
        return -1;

    cfg_file = fopen(conf_file, "r");

    if (cfg_file == NULL)
        return -1;

    while (fgets(line, MAX_LINE_LENGTH, cfg_file))
    {
        char *key, *val;

        line_num++;

        if (line[0] == '#' || line[0] == '\n' || line[0] == '\r')
            continue;

        key = strip(line);
        val = strchr(line, '=');

        if (val)
            *val++ = '\0';

        if (key && key[0] && val && val[0])
        {
            ret = keyval_process(mors, context, key, val);
            if (ret)
            {
                fclose(cfg_file);
                return ret;
            }
        }
        else
        {
            mctrl_err("No key=value on line %d\n", line_num);
            fclose(cfg_file);
            return -1;
        }
    }

    fclose(cfg_file);
    return 0;
}

static int standby_session_store(struct morsectrl *mors, const char *ifname, const uint8_t *bssid,
    const char *standby_session_dir, struct command_get_channel_cfm *rsp)
{
    FILE *f;
    char dir[MORSE_FILENAME_LEN_MAX];
    char fname[MORSE_FILENAME_LEN_MAX];
    struct stat buf;

    if (snprintf(dir, sizeof(dir), "%s", standby_session_dir) < 0)
    {
        mctrl_err("%s: Failed to set dir name (%d)\n",
                dir, errno);
        return -1;
    }

    if (stat(dir, &buf) != 0) {
        if (mkdir_path(dir) < 0)
        {
            mctrl_err("%s: Failed to create %s (%d)\n",
                    ifname, dir, errno);
            return -1;
        }
    }

    if (snprintf(fname, sizeof(fname), "%s/%s", dir, ifname) < 0) {
        mctrl_err("%s: Failed to set file name (%d)\n",
                ifname, errno);
        return -1;
    }

    f = fopen(fname, "w");
    if (!f) {
        mctrl_err("%s: Failed to open %s\n",
                ifname, fname);
        return -1;
    }

    fprintf(f, "bssid=" MACSTR "\n", MAC2STR(bssid));
    fprintf(f, "op_chan_freq=%u\n", rsp->operating_channel_freq_hz);
    fprintf(f, "op_chan_bw=%u\n", rsp->operating_channel_bw_mhz);
    fprintf(f, "pri_chan_bw=%u\n", rsp->primary_channel_bw_mhz);
    fprintf(f, "pri_1mhz_chan=%u\n", rsp->primary_1mhz_channel_index);

    fclose(f);

    if (mors->debug)
    {
        mctrl_print("%s: Created %s\n", ifname, fname);
    }

    return 0;
}

static int standby_session_load(struct morsectrl *mors, const char *standby_session_dir,
        uint8_t *bssid, struct command_set_channel_req *req)
{
    const char *ifname = morsectrl_transport_get_ifname(mors->transport);
    struct standby_session_parse_context context = {
        .bssid = bssid,
        .req = req
    };

    char session_path[MORSE_FILENAME_LEN_MAX];

    if (snprintf(session_path, sizeof(session_path),
                "%s/%s", standby_session_dir, ifname) < 0)
    {
        mctrl_err("%s: Failed to set session path name (%d)\n",
                ifname, errno);
        return -1;
    }

    if (config_parse(mors, session_path, parse_standby_session_keyval, &context))
    {
        goto err_parse;
    }

    return 0;

err_parse:
    mctrl_err("%s: Failed to parse %s\n", ifname, standby_session_dir);
    return -1;
}

static int process_standby_enter(struct morsectrl *mors,
                                    struct command_standby_mode_req *standby_cmd,
                                    int argc, char *argv[])
{
    const char *standby_session_dir = NULL;
    int ret;
    struct command_set_channel_req *ch_cmd;
    struct morsectrl_transport_buff *cmd_tbuff;
    struct morsectrl_transport_buff *rsp_tbuff;

    if (argc < 2 || argc > 3)
    {
        mctrl_err("Invalid number of arguments\n");
        return -1;
    }

    if (argc == 2)
    {
        return 0;
    }

    standby_session_dir = argv[2];

    cmd_tbuff = morsectrl_transport_cmd_alloc(mors->transport, sizeof(*ch_cmd));
    rsp_tbuff = morsectrl_transport_resp_alloc(mors->transport, 0);

    if (!cmd_tbuff || !rsp_tbuff)
    {
        mctrl_err("Alloc failure\n");
        ret = -1;
        goto exit;
    }

    ch_cmd = TBUFF_TO_CMD(cmd_tbuff, struct command_set_channel_req);

    /* load the saved standby parameters */
    ret = standby_session_load(mors, standby_session_dir, standby_cmd->enter.bssid, ch_cmd);
    if (ret < 0)
    {
        mctrl_err("Failed to load session info\n");
        goto exit;
    }

    if (mors->debug)
    {
        mctrl_print("Loaded session info:\n");
        mctrl_print("bssid " MACSTR "\n", MAC2STR(standby_cmd->enter.bssid));
        mctrl_print("op ch freq %d\n", ch_cmd->operating_channel_freq_hz);
        mctrl_print("op ch bw %d\n", ch_cmd->operating_channel_bw_mhz);
        mctrl_print("pri ch bw %d\n", ch_cmd->primary_channel_bw_mhz);
        mctrl_print("pri 1mhz idx %d\n", ch_cmd->primary_1mhz_channel_index);
    }

    /* Set the channel before we go to sleep */
    ret = morsectrl_send_command(mors->transport, MORSE_COMMAND_SET_CHANNEL,
                                 cmd_tbuff, rsp_tbuff);
    if (ret < 0)
    {
        mctrl_err("failed to set channel info %d\n", ret);
        goto exit;
    }

exit:
    morsectrl_transport_buff_free(cmd_tbuff);
    morsectrl_transport_buff_free(rsp_tbuff);

    return ret;
}

/* Standby store commands are invoked from wpa_supplicant, so provide some context for error
 * messages.
 */
static void standby_store_print_msg(const char *msg)
{
    mctrl_err("morsectrl standby store failed - %s\n", msg);
}

static int standby_store_session_cmd(struct morsectrl *mors, int argc, char *argv[])
{
    int option;
    uint8_t bssid[MAC_ADDR_LEN] = { 0 };
    bool have_bssid = false;
    char *standby_session_dir = NULL;
    int ret;
    const char *ifname = morsectrl_transport_get_ifname(mors->transport);
    struct command_set_channel_req *cmd;
    struct command_get_channel_cfm *rsp;
    struct morsectrl_transport_buff *cmd_tbuff;
    struct morsectrl_transport_buff *rsp_tbuff;

    cmd_tbuff = morsectrl_transport_cmd_alloc(mors->transport, sizeof(*cmd));
    rsp_tbuff = morsectrl_transport_resp_alloc(mors->transport, sizeof(*rsp));

    if (ifname == NULL)
    {
        standby_store_print_msg("no interface - transport not supported");
        ret = -1;
        goto exit;
    }

    if (!cmd_tbuff || !rsp_tbuff)
    {
        standby_store_print_msg("alloc failure");
        ret = -1;
        goto exit;
    }

    cmd = TBUFF_TO_CMD(cmd_tbuff, struct command_set_channel_req);
    rsp = TBUFF_TO_RSP(rsp_tbuff, struct command_get_channel_cfm);

    if (argc < 1)
    {
        standby_store_print_msg("not enough arguments to store command");
        ret = -1;
        goto exit;
    }

    while ((option = getopt(argc, argv, "b:d:")) != -1)
    {
        switch (option)
        {
            case 'b':
            {
                if (str_to_mac_addr(bssid, optarg) < 0)
                {
                    standby_store_print_msg("invalid BSSID");
                    ret = -1;
                    goto exit;
                }
                have_bssid = true;
                break;
            }
            case 'd':
            {
                standby_session_dir = optarg;
                break;
            }
            default:
                standby_store_print_msg("invalid argument");
                ret = -1;
                goto exit;
        }
    }

    if (!have_bssid || !standby_session_dir)
    {
        standby_store_print_msg("BSSID or session directory not supplied");
        ret = -1;
        goto exit;
    }

    ret = morsectrl_send_command(mors->transport, MORSE_COMMAND_GET_FULL_CHANNEL,
                                 cmd_tbuff, rsp_tbuff);
    if (ret < 0)
    {
        standby_store_print_msg("failed to get channel info");
        ret = -1;
        goto exit;
    }

    ret = standby_session_store(mors, ifname, bssid, standby_session_dir, rsp);

exit:
    morsectrl_transport_buff_free(cmd_tbuff);
    morsectrl_transport_buff_free(rsp_tbuff);

    return ret;
}

static int send_wake_filter_cmd(struct morsectrl *mors,
                                    struct command_standby_set_wake_filter *wake_cmd)
{
    int ret = -1;
    struct morsectrl_transport_buff *cmd_tbuff;
    struct morsectrl_transport_buff *rsp_tbuff;
    struct command_standby_mode_req *cmd;
    struct command_standby_mode_cfm *rsp = NULL;

    cmd_tbuff = morsectrl_transport_cmd_alloc(mors->transport, sizeof(*cmd));
    rsp_tbuff = morsectrl_transport_resp_alloc(mors->transport, sizeof(*rsp));

    if (!cmd_tbuff || !rsp_tbuff)
    {
        goto exit;
    }

    cmd = TBUFF_TO_CMD(cmd_tbuff, struct command_standby_mode_req);
    rsp = TBUFF_TO_RSP(rsp_tbuff, struct command_standby_mode_cfm);

    cmd->cmd = STANDBY_MODE_CMD_SET_WAKE_FILTER;

    memcpy(&cmd->set_filter, wake_cmd, sizeof(*wake_cmd));

    ret = morsectrl_send_command(mors->transport, MORSE_COMMAND_STANDBY_MODE,
        cmd_tbuff, rsp_tbuff);

exit:
    if (ret < 0)
    {
        mctrl_err("Failed to send standby command %d\n", ret);
    }

    morsectrl_transport_buff_free(cmd_tbuff);
    morsectrl_transport_buff_free(rsp_tbuff);
    return ret;
}

static int process_set_config_cmd(struct morsectrl *mors, struct command_standby_mode_req* cmd,
                                    int argc, char *argv[])
{
    if (argc != 3)
    {
        mctrl_err("Invalid number of arguments %d\n", argc);
        return -1;
    }

    struct command_standby_set_wake_filter wake_filter = {0};
    static const struct command_standby_set_config config_default = {
        .bss_inactivity_before_deep_sleep_s = 60,
        .deep_sleep_period_s = 120,
        .notify_period_s = 15,
        .dst_ip.as_u32 = 0,
        .dst_port = 22000,
        .src_ip.as_u32 = 0,
        .deep_sleep_increment_s = 0, /* Default no increment */
        .deep_sleep_max_s = UINT32_MAX, /* Default max int / no max */
    };

    memcpy(&cmd->config, &config_default, sizeof(config_default));

    struct standby_config_parse_context context = {
        .filter_cfg = &wake_filter,
        .set_cfg = &cmd->config
    };

    if (config_parse(mors, argv[2], parse_standby_config_keyval, &context))
    {
        mctrl_err("Failed to parse config file\n");
        return -1;
    }

    /* If the wake filter has been configured, send the command to set it here. */
    if (wake_filter.len != 0)
    {
        return send_wake_filter_cmd(mors, &wake_filter);
    }

    return 0;
}

static int process_set_status_payload(struct command_standby_mode_req* cmd, int argc, char *argv[])
{
    uint32_t payload_len;

    if (argc != 3)
    {
        mctrl_err("Invalid number of arguments\n");
        return -1;
    }

    argc -= 1;
    argv += 1;

    payload_len = strlen(argv[1]);
    if ((payload_len % 2) != 0)
    {
        mctrl_err("Invalid hex string, length must be a multiple of 2\n");
        return -1;
    }

    payload_len = (payload_len / 2);
    if (payload_len > STANDBY_STATUS_FRAME_USER_PAYLOAD_MAX_LEN)
    {
        mctrl_err("Supplied payload is too large: %d > %d\n", payload_len,
            STANDBY_STATUS_FRAME_USER_PAYLOAD_MAX_LEN);
        return -1;
    }

    cmd->set_payload.len = payload_len;
    for (int i = 0; i < payload_len; i++)
    {
        int ret = sscanf(&(argv[1][i * 2]), "%2hhx", &cmd->set_payload.payload[i]);
        if (ret != 1)
        {
            mctrl_err("Invalid hex string\n");
            return -1;
        }
    }

    return 0;
}

int standby(struct morsectrl *mors, int argc, char *argv[])
{
    int ret = -1;
    struct morsectrl_transport_buff *cmd_tbuff;
    struct morsectrl_transport_buff *rsp_tbuff;
    struct command_standby_mode_req *cmd;
    struct command_standby_mode_cfm *rsp = NULL;

    if (argc == 0)
    {
        usage(mors);
        return 0;
    }

    if (argc < 2)
    {
        usage(mors);
        return -1;
    }

    /* Local-only command - not sent to firmware */
    if (strcmp("store", argv[1]) == 0)
    {
        return standby_store_session_cmd(mors, argc - 1, &argv[1]);
    }

    ret = standby_get_cmd(argv[1]);
    if (ret < 0)
    {
        mctrl_err("Invalid standby command '%s'\n", argv[1]);
        usage(mors);
        return -1;
    }

    cmd_tbuff = morsectrl_transport_cmd_alloc(mors->transport, sizeof(*cmd));
    rsp_tbuff = morsectrl_transport_resp_alloc(mors->transport, sizeof(*rsp));

    if (!cmd_tbuff || !rsp_tbuff)
    {
        goto exit;
    }

    cmd = TBUFF_TO_CMD(cmd_tbuff, struct command_standby_mode_req);
    rsp = TBUFF_TO_RSP(rsp_tbuff, struct command_standby_mode_cfm);

    cmd->cmd = standby_get_cmd(argv[1]);

    switch (cmd->cmd)
    {
        case STANDBY_MODE_CMD_SET_CONFIG_V2:
        {
            if (process_set_config_cmd(mors, cmd, argc, argv))
            {
                ret = -1;
                usage(mors);
                goto exit;
            }

            if (mors->debug)
            {
                mctrl_print("Setting standby configuration:\n");
                mctrl_print("deep sleep inactivity period: %d\n",
                        cmd->config.bss_inactivity_before_deep_sleep_s);
                mctrl_print("deep_sleep period: %d\n", cmd->config.deep_sleep_period_s);
                mctrl_print("notify period : %d\n", cmd->config.notify_period_s);
                mctrl_print("dst port: %d\n", cmd->config.dst_port);
                mctrl_print("dst ip: " IPSTR "\n", IP2STR(cmd->config.dst_ip.octet));
                mctrl_print("src ip: " IPSTR "\n", IP2STR(cmd->config.src_ip.octet));
            }

            break;
        }
        case STANDBY_MODE_CMD_SET_STATUS_PAYLOAD:
        {
            if (process_set_status_payload(cmd, argc, argv))
            {
                ret = -1;
                usage(mors);
                goto exit;
            }

            break;
        }
        case STANDBY_MODE_CMD_ENTER:
        {
            if (process_standby_enter(mors, cmd, argc, argv))
            {
                ret = -1;
                usage(mors);
                goto exit;
            }
            mctrl_print("Enter standby\n");
            break;
        }
        case STANDBY_MODE_CMD_EXIT:
        default:
            break;
    }

    ret = morsectrl_send_command(mors->transport, MORSE_COMMAND_STANDBY_MODE,
        cmd_tbuff, rsp_tbuff);
exit:
    if (ret < 0)
    {
        mctrl_err("Failed to send standby command %d\n", ret);
    }

    morsectrl_transport_buff_free(cmd_tbuff);
    morsectrl_transport_buff_free(rsp_tbuff);
    return ret;
}

MM_CLI_HANDLER(standby, MM_INTF_REQUIRED, MM_DIRECT_CHIP_SUPPORTED);
