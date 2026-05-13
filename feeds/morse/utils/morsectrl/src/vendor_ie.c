/*
 * Copyright 2022 Morse Micro
 */
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <math.h>

#include "portable_endian.h"
#include "command.h"
#include "utilities.h"

#define MAX_VENDOR_IE_LENGTH                (255)
#define NUM_OUI_BYTES                       (3)

enum command_vendor_ie_opcode
{
    MORSE_VENDOR_IE_OP_ADD_ELEMENT = 0,
    MORSE_VENDOR_IE_OP_CLEAR_ELEMENTS,
    MORSE_VENDOR_IE_OP_ADD_FILTER,
    MORSE_VENDOR_IE_OP_CLEAR_FILTERS,

    MORSE_VENDOR_IE_OP_MAX = UINT16_MAX,
    MORSE_VENDOR_IE_OP_INVALID = MORSE_VENDOR_IE_OP_MAX
};

enum vendor_ie_mgmt_type_flags {
    MORSE_VENDOR_IE_TYPE_BEACON     = BIT(0),
    MORSE_VENDOR_IE_TYPE_PROBE_REQ  = BIT(1),
    MORSE_VENDOR_IE_TYPE_PROBE_RESP = BIT(2),
    MORSE_VENDOR_IE_TYPE_ASSOC_REQ  = BIT(3),
    MORSE_VENDOR_IE_TYPE_ASSOC_RESP = BIT(4),
    /* ... etc. */

    MORSE_VENDOR_IE_TYPE_ALL        = UINT16_MAX
};

struct PACKED command_vendor_ie_req
{
    uint16_t opcode;
    uint16_t mgmt_type_mask;
    uint8_t data[MAX_VENDOR_IE_LENGTH];
};

struct PACKED command_vendor_ie_cfm
{
    /* empty */
};


static void usage(struct morsectrl *mors)
{
    mctrl_print("\tvendor_ie [-a <bytes> | -c | -o <oui> | -r ] [ -b | -p ]\t");
    mctrl_print("Manipulate vendor information elements\n");
    mctrl_print("\t\t-a <bytes>\tadd a vendor element (hex string)\n");
    mctrl_print("\t\t-c\tclear previously added vendor elements\n");
    mctrl_print("\t\t-o <oui>\tadd an OUI to the vendor IE whitelist (hex string)\n");
    mctrl_print("\t\t-r\treset configured OUI whitelist\n");
    mctrl_print("\t\t-b\tapply to beacons\n");
    mctrl_print("\t\t-p\tapply to probes\n");
    mctrl_print("\t\t-s\tapply to assocs\n");
}

static int check_cmd_opcode_not_set(struct command_vendor_ie_req *cmd)
{
    if (cmd->opcode != MORSE_VENDOR_IE_OP_INVALID)
    {
        mctrl_err("Specify only one of [a,o,r,c]\n");
        return -1;
    }
    return 0;
}

int vendor_ie(struct morsectrl *mors, int argc, char *argv[])
{
    int ret = 0;
    int option;
    size_t length = 0;

    struct command_vendor_ie_req *cmd_vie;
    struct command_vendor_ie_cfm *rsp_vie;
    struct morsectrl_transport_buff *cmd_tbuff = NULL;
    struct morsectrl_transport_buff *rsp_tbuff = NULL;

    if (argc == 0)
    {
        usage(mors);
        return 0;
    }

    if (argc < 2)
    {
        mctrl_err("Invalid command parameters\n");
        usage(mors);
        return -1;
    }

    cmd_tbuff = morsectrl_transport_cmd_alloc(mors->transport, sizeof(*cmd_vie));
    rsp_tbuff = morsectrl_transport_resp_alloc(mors->transport, sizeof(*rsp_vie));
    if (!cmd_tbuff || !rsp_tbuff)
    {
        ret = -1;
        goto exit;
    }

    cmd_vie = TBUFF_TO_CMD(cmd_tbuff, struct command_vendor_ie_req);
    rsp_vie = TBUFF_TO_RSP(rsp_tbuff, struct command_vendor_ie_cfm);

    if (cmd_vie == NULL ||
        rsp_vie == NULL)
    {
        ret = -1;
        goto exit;
    }

    memset(cmd_vie, 0, sizeof(*cmd_vie));
    cmd_vie->opcode = MORSE_VENDOR_IE_OP_INVALID;
    cmd_vie->mgmt_type_mask = 0;

    while ((option = getopt(argc, argv, "a:o:crhpsb")) != -1)
    {
        switch (option)
        {
            case 'a':
            {
                ret = check_cmd_opcode_not_set(cmd_vie);
                if (ret)
                    goto exit;

                length = strlen(optarg);

                if (length & 1)
                {
                    mctrl_err("Odd number of characters in data bytestring\n");
                    ret = -1;
                    goto exit;
                }
                length = length / 2;

                if (length > sizeof(cmd_vie->data))
                {
                    mctrl_err("Vendor IE has too many bytes %zu\n", length);
                    ret = -1;
                    goto exit;
                }

                cmd_vie->opcode = MORSE_VENDOR_IE_OP_ADD_ELEMENT;
                if (hexstr2bin(optarg, cmd_vie->data, length))
                {
                    mctrl_err("Invalid hex string\n");
                    ret = -1;
                    goto exit;
                }
                break;
            }
            case 'p':
            {
                cmd_vie->mgmt_type_mask |= (MORSE_VENDOR_IE_TYPE_PROBE_REQ |
                                            MORSE_VENDOR_IE_TYPE_PROBE_RESP);
                break;
            }
            case 's':
            {
                cmd_vie->mgmt_type_mask |= (MORSE_VENDOR_IE_TYPE_ASSOC_REQ |
                                            MORSE_VENDOR_IE_TYPE_ASSOC_RESP);
                break;
            }
            case 'b':
            {
                cmd_vie->mgmt_type_mask |= MORSE_VENDOR_IE_TYPE_BEACON;
                break;
            }
            case 'c':
            {
                ret = check_cmd_opcode_not_set(cmd_vie);
                if (ret)
                    goto exit;


                cmd_vie->opcode = MORSE_VENDOR_IE_OP_CLEAR_ELEMENTS;
                break;
            }
            case 'o':
            {
                ret = check_cmd_opcode_not_set(cmd_vie);
                if (ret)
                    goto exit;


                length = strlen(optarg) / 2;
                if (length != NUM_OUI_BYTES)
                {
                    ret = -1;
                    mctrl_err("invalid oui length\n");
                    goto exit;
                }
                cmd_vie->opcode = MORSE_VENDOR_IE_OP_ADD_FILTER;
                hexstr2bin(optarg, cmd_vie->data, length);
                break;
            }
            case 'r':
            {
                ret = check_cmd_opcode_not_set(cmd_vie);
                if (ret)
                    goto exit;


                cmd_vie->opcode = MORSE_VENDOR_IE_OP_CLEAR_FILTERS;
                break;
            }
            case 'h':
            {
                ret = 0;
                usage(mors);
                goto exit;
            }
            default:
                ret = -1;
                mctrl_err("Unrecognised command parameters\n");
                usage(mors);
                goto exit;
        }
    }

    /* set the length used in command */
    morsectrl_transport_set_cmd_data_length(cmd_tbuff,
                                length + sizeof(*cmd_vie) - sizeof(cmd_vie->data));

    if (cmd_vie->opcode == MORSE_VENDOR_IE_OP_INVALID)
    {
        mctrl_err("No command specified\n");
        usage(mors);
        goto exit;
    }

    if (cmd_vie->mgmt_type_mask == 0)
    {
        mctrl_err("No frame type specified\n");
        usage(mors);
        goto exit;
    }

    ret = morsectrl_send_command(mors->transport,
                                 MORSE_COMMAND_VENDOR_IE_CONFIG,
                                 cmd_tbuff,
                                 rsp_tbuff);

    if (ret < 0)
    {
        mctrl_err("Command error (%d)\n", ret);
        goto exit;
    }

exit:
    morsectrl_transport_buff_free(cmd_tbuff);
    morsectrl_transport_buff_free(rsp_tbuff);

    return ret;
}

MM_CLI_HANDLER(vendor_ie, MM_INTF_REQUIRED, MM_DIRECT_CHIP_NOT_SUPPORTED);
