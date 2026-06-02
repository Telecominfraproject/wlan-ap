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
#include "stats_format.h"
#include "transport/transport.h"
#include "utilities.h"
#define RAW_CMD_MAX_3BIT_SLOTS          (0b111)
#define RAW_CMD_MIN_SLOT_DUR_US         (500)
#define RAW_CMD_MAX_SLOT_DUR_US         (RAW_CMD_MIN_SLOT_DUR_US + (200 * (1 << 11) - 1))
#define RAW_CMD_MAX_START_TIME_US       (UINT8_MAX * 2 * 1024)
#define RAW_CMD_MAX_AID                 (2007) /* set by linux */

enum morse_cmd_raw_tlv_tag {
    MORSE_RAW_CMD_TAG_SLOT_DEF = 0,
    MORSE_RAW_CMD_TAG_GROUP = 1,
    MORSE_RAW_CMD_TAG_START_TIME = 2,
    MORSE_RAW_CMD_TAG_PRAW = 3,
    MORSE_RAW_CMD_TAG_BCN_SPREAD = 4,

    NUM_MORSE_RAW_CMD_TAGS
};

/* slot definition is required for new raw configs */
struct PACKED morse_cmd_raw_tlv_slot_def {
    uint8_t tag;
    /** Total length of the RAW window. This / num_slots = slot_duration */
    uint32_t raw_duration_us;
    /** Number of individual "slots" within the RAW window */
    uint8_t num_slots;
    /** Allow STAs to transmit into the next slot */
    uint8_t cross_slot_bleed;
};

struct PACKED morse_cmd_raw_tlv_group {
    uint8_t tag;

    /** Start AID for this raw config */
    uint16_t aid_start;

    /** End AID for this config (inclusive) */
    uint16_t aid_end;
};

struct PACKED morse_cmd_raw_tlv_start_time {
    uint8_t tag;
    /** Time the RAW window starts, measured from the end of the frame carrying RPS IE.*/
    uint32_t start_time_us;
};

struct PACKED morse_cmd_raw_tlv_praw {
    uint8_t tag;
    uint8_t periodicity;
    uint8_t validity;
    uint8_t start_offset;
    uint8_t refresh_on_expiry;
};

struct PACKED morse_cmd_raw_tlv_bcn_spread {
    uint8_t tag;
    /** Maximum number of beacons to spread across*/
    uint16_t max_spread;
    /** Nominal stations per beacon */
    uint16_t nominal_sta_per_bcn;
};

union PACKED morse_cmd_raw_tlvs {
    uint8_t tag;
    struct morse_cmd_raw_tlv_slot_def slot_def;
    struct morse_cmd_raw_tlv_group group;
    struct morse_cmd_raw_tlv_start_time start_time;
    struct morse_cmd_raw_tlv_praw praw;
    struct morse_cmd_raw_tlv_bcn_spread bcn_spread;
};

enum morse_cmd_raw_config_flags {
    /** Set bit to enable RAW */
    RAW_CMD_FLAG_ENABLE = BIT(0),
    /** Set bit to delete RAW at this index */
    RAW_CMD_FLAG_DELETE = BIT(1),
    /** Set bit to update RAW config state */
    RAW_CMD_FLAG_UPDATE = BIT(2),
};

struct PACKED morse_cmd_raw_cfg {
    /** Flags for this RAW config */
    uint32_t flags;

    /** ID to reference this RAW config.
     * If ID already exists, existing config will be overwritten
     */
    uint16_t id;

    uint8_t variable[];
};

static struct {
    struct arg_csi *slot_def;
    struct arg_lit *cross_slot;
    struct arg_csi *aid_group;
    struct arg_int *start_time;
    struct arg_csi *bcn_spread;
    struct arg_csi *praw;
    struct arg_rex *enable;
    struct arg_int *id;
} args;

int raw_init(struct morsectrl *mors, struct mm_argtable *mm_args)
{
    MM_INIT_ARGTABLE(mm_args, "Configure Restricted Access Window parameters",
        args.slot_def = arg_csi0("s", "slot_def", "<RAW duration (usec)>,<number of slots>", 2,
            "Slot definition of RAW assignment. Required for new configs."),
        args.cross_slot = arg_lit0("x", "cross_slot",
            "Enable cross slot bleed (requires --slot_def)"),
        args.aid_group = arg_csi0("a", "aid_group", "<start AID>,<end AID>", 2,
            "AID range for the config"),
        args.start_time = arg_int0("t", "start_time", "<start time (usec)>",
            "Start time for the RAW window from the end of the frame"),
        args.bcn_spread = arg_csi0("b", "bcn_spread",
            "<max beacons to spread over>,<nominal STAs per beacon>", 2, "Use beacon spreading"),
        args.praw = arg_csi0("p", "praw", "<periodicity>,<validity (-1 for persistent)>,<offset>",
            3, "Use Periodic RAW"),
        args.enable = arg_rex1(NULL, NULL, "(enable|disable|delete)", "{enable|disable|delete}", 0,
            "enable/disable or delete RAW configs. If <id> is 0, globally enable/disable/delete"),
        args.id = arg_int1(NULL, NULL, "<id>", "ID for the RAW config. 0 is reserved as 'global'"));
    return 0;
}


int raw(struct morsectrl *mors, int argc, char *argv[])
{
    int ret = -1;
    struct morse_cmd_raw_cfg *cmd;
    union morse_cmd_raw_tlvs *tlv;
    struct morsectrl_transport_buff *cmd_tbuff;
    struct morsectrl_transport_buff *rsp_tbuff;

    static const size_t cmd_max_size = sizeof(*cmd) + (sizeof(*tlv) * NUM_MORSE_RAW_CMD_TAGS);

    /* allocate for largest possible size */
    cmd_tbuff = morsectrl_transport_cmd_alloc(mors->transport, cmd_max_size);
    rsp_tbuff = morsectrl_transport_resp_alloc(mors->transport, 0);

    if (!cmd_tbuff || !rsp_tbuff)
        goto exit;

    cmd = TBUFF_TO_CMD(cmd_tbuff, struct morse_cmd_raw_cfg);
    tlv = (union morse_cmd_raw_tlvs *) &cmd->variable[0];

    memset(cmd, 0 , cmd_max_size);

    if (args.id->count != 0)
    {
        if (args.id->ival[0] > UINT16_MAX)
        {
            mctrl_err("Invalid RAW ID, must be 1 - %u (or 0 for global)\n", UINT16_MAX);
            ret = -1;
            goto exit;
        }
        cmd->id = args.id->ival[0];
    }

    if (args.enable->count)
    {
        if (strcmp("enable", args.enable->sval[0]) == 0)
        {
            cmd->flags |= RAW_CMD_FLAG_ENABLE;
        }
        if (strcmp("delete", args.enable->sval[0]) == 0)
        {
            cmd->flags |= RAW_CMD_FLAG_DELETE;
            goto done;
        }
        /* else, disable */
    }

    if (args.slot_def->count)
    {
        uint8_t num_slots = args.slot_def->ival[0][1];

        tlv->slot_def.tag = MORSE_RAW_CMD_TAG_SLOT_DEF;
        tlv->slot_def.raw_duration_us = args.slot_def->ival[0][0];
        tlv->slot_def.num_slots = num_slots;
        tlv->slot_def.cross_slot_bleed = (args.cross_slot->count != 0);

        if (num_slots < 1 || num_slots > 63)
        {
            mctrl_err("Invalid number of slots, must be 1-63\n");
            ret = -1;
            goto exit;
        }


        if (tlv->slot_def.raw_duration_us < (num_slots * RAW_CMD_MIN_SLOT_DUR_US) ||
                tlv->slot_def.raw_duration_us > (num_slots * RAW_CMD_MAX_SLOT_DUR_US))
        {
            mctrl_err("Invalid RAW duration. min: %u, max: %u\n"
                    "Try reducing the number of slots\n",
                    num_slots * RAW_CMD_MIN_SLOT_DUR_US,
                    num_slots * RAW_CMD_MAX_SLOT_DUR_US);
            ret = -1;
            goto exit;
        }

        tlv = (union morse_cmd_raw_tlvs *)(((uint8_t*)tlv) + sizeof(tlv->slot_def));
    }
    else
    {
        if (args.cross_slot->count)
        {
            mctrl_err("Cross slot is ignored without a slot_def\n");
            /* dont need to exit here. Warning the user is enough */
        }
    }

    if (args.aid_group->count)
    {
        tlv->group.tag = MORSE_RAW_CMD_TAG_GROUP;
        tlv->group.aid_start = args.aid_group->ival[0][0];
        tlv->group.aid_end = args.aid_group->ival[0][1];

        if (tlv->group.aid_start > tlv->group.aid_end)
        {
            mctrl_err("AID start (%u) should be less than AID end (%u)\n",
                    tlv->group.aid_start, tlv->group.aid_end);
            ret = -1;
            goto exit;
        }

        if (tlv->group.aid_start < 1 || tlv->group.aid_end > RAW_CMD_MAX_AID)
        {
            mctrl_err("AID range is invalid (min: 1, max: %u)\n", RAW_CMD_MAX_AID);
            ret = -1;
            goto exit;
        }

        tlv = (union morse_cmd_raw_tlvs *)(((uint8_t*)tlv) + sizeof(tlv->group));
    }

    if (args.start_time->count)
    {
        tlv->start_time.tag = MORSE_RAW_CMD_TAG_START_TIME;
        tlv->start_time.start_time_us = args.start_time->ival[0];

        if (tlv->start_time.start_time_us > RAW_CMD_MAX_START_TIME_US) {
            mctrl_err("Invalid start time, must be 0-%u\n", RAW_CMD_MAX_START_TIME_US);
            ret = -1;
            goto exit;
        }

        tlv = (union morse_cmd_raw_tlvs *)(((uint8_t*)tlv) + sizeof(tlv->start_time));
    }

    if (args.praw->count && args.bcn_spread->count)
    {
        mctrl_err("Beacon spreading and PRAW are not supported together\n");
        ret = -1;
        goto exit;
    }

    if (args.praw->count)
    {
        tlv->praw.tag = MORSE_RAW_CMD_TAG_PRAW;

        if (args.praw->ival[0][0] > UINT8_MAX || args.praw->ival[0][0] < 1) {
            mctrl_err("Invalid periodicity, must be 1-%u\n", UINT8_MAX);
            ret = -1;
            goto exit;
        }
        tlv->praw.periodicity = args.praw->ival[0][0];

        if (args.praw->ival[0][1] != -1 &&
                (args.praw->ival[0][1] > UINT8_MAX || args.praw->ival[0][0] < 1)) {
            mctrl_err("Invalid validity, must be 1-%u, or -1 for persistent\n", UINT8_MAX);
            ret = -1;
            goto exit;
        }
        if (args.praw->ival[0][1] == -1) {
            tlv->praw.validity = 255;
            tlv->praw.refresh_on_expiry = 1;
        } else {
            tlv->praw.validity = args.praw->ival[0][1];
        }

        if (args.praw->ival[0][2] > UINT8_MAX || args.praw->ival[0][2] < 0) {
            mctrl_err("Invalid start offset, must be 0-%u\n", UINT8_MAX);
            ret = -1;
            goto exit;
        }
        tlv->praw.start_offset = args.praw->ival[0][2];

        if (tlv->praw.start_offset >= tlv->praw.periodicity) {
            mctrl_err("Start offset (%u) must be less than periodicity (%u)\n",
                    tlv->praw.start_offset, tlv->praw.periodicity);
            ret = -1;
            goto exit;
        }

        tlv = (union morse_cmd_raw_tlvs *)(((uint8_t*)tlv) + sizeof(tlv->praw));
    }

    if (args.bcn_spread->count)
    {
        tlv->bcn_spread.tag = MORSE_RAW_CMD_TAG_BCN_SPREAD;
        tlv->bcn_spread.max_spread = args.bcn_spread->ival[0][0];
        tlv->bcn_spread.nominal_sta_per_bcn = args.bcn_spread->ival[0][1];

        if (tlv->bcn_spread.max_spread > UINT16_MAX)
        {
            mctrl_err("Invalid max beacon spread (max: %u)\n", UINT16_MAX);
            ret = -1;
            goto exit;
        }

        if (tlv->bcn_spread.nominal_sta_per_bcn > UINT16_MAX)
        {
            mctrl_err("Invalid nominal STAs per beacon (max: %u)\n", UINT16_MAX);
            ret = -1;
            goto exit;
        }

        tlv = (union morse_cmd_raw_tlvs *)(((uint8_t*)tlv) + sizeof(tlv->bcn_spread));
    }

    if ((uint8_t*)tlv > cmd->variable)
    {
        if (cmd->id == 0) {
            mctrl_err("Can't set options when configuring global RAW\n");
            ret = -1;
            goto exit;
        }
        cmd->flags |= RAW_CMD_FLAG_UPDATE;
    }

    morsectrl_transport_set_cmd_data_length(cmd_tbuff, (uint8_t*)tlv - (uint8_t*)cmd);

done:
    ret = morsectrl_send_command(mors->transport, MORSE_COMMAND_CONFIG_RAW,
                                 cmd_tbuff, rsp_tbuff);

exit:
    if (ret < 0)
    {
        mctrl_err("Failed to set RAW config\n");
    }
    else
    {
        mctrl_print("RAW config sent for ID:%u (%s)\n", cmd->id,
            cmd->flags & RAW_CMD_FLAG_ENABLE ? "enable" : "disable");
    }

    morsectrl_transport_buff_free(cmd_tbuff);
    morsectrl_transport_buff_free(rsp_tbuff);
    return ret;
}

MM_CLI_HANDLER(raw, MM_INTF_REQUIRED, MM_DIRECT_CHIP_NOT_SUPPORTED);
