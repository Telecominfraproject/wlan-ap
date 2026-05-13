/*
 * Copyright 2022 Morse Micro
 */

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <stdint.h>
#include <math.h>

#include "portable_endian.h"
#include "command.h"
#include "utilities.h"

/** TX power adjustments available in the system, values can be or'ed together to create a mask */
enum tx_pwr_adj_type {
    /** MCS based adjustment */
    TX_PWR_ADJ_MCS             = (1 << 0),
    /** Sub band adjustment */
    TX_PWR_ADJ_SUBBAND         = (1 << 1),
    /** Temperature adjustment */
    TX_PWR_ADJ_TEMPERATURE     = (1 << 2),
    /** Channel based adjustment */
    TX_PWR_ADJ_CHANNEL         = (1 << 3),
    /** MAX */
    TX_PWR_ADJ_MAX = UINT32_MAX
};

/** Structure to send power adjustment commands to chip */
struct PACKED tx_pwr_adj_command
{
    /** The flags to se/get power adjustments */
    uint8_t flag;
    /** Bitmask to enable/disable power adujstments */
    uint32_t en_tx_pwr_adj_mask;
};

/** Structure to store response from chip */
struct PACKED tx_pwr_adj_cfm
{
    /** Bit mask variable to specify enable/disable of tx power adjustment */
    uint32_t en_tx_pwr_adj_mask;
    /** Sub-band 1in8 scale values in qdB */
    int32_t sb_1in_8[4];
    /** Sub-band 2in8 scale values in qdB */
    int32_t sb_2in_8[2];
    /** Teperature based scaler value in linear scale */
    int32_t temp_power_scaler;
    /** Current sub-band scale value in linear scale */
    int32_t subband_scale;
    /** txscaler value in linear scale */
    int32_t tx_linear_power_scaler;
    /** Transmit power control */
    uint32_t tx_power_drift_lin;
    /** Chip's base power in qdBm */
    int8_t base_power_qdbm;
    /** Chip's max power in qdBm */
    int8_t max_tx_power_qdbm;
    /** Current transmit power in qdBm */
    int8_t current_tx_power_qdbm;
    /** Scale values of MCS 0-10 */
    int8_t scale_value_db[11];
    /** Board's antenna gain in dBi */
    int8_t tx_antenna_gain_dbi;
    /** Regulatory domain's transmit power limit in dBm */
    int8_t regulatory_limit_dbm;
};

static void usage(struct morsectrl *mors) {
    mctrl_print("\ttx_pwr_adj <adjustment options> <value>\n");
    mctrl_print("\t\t\tget chip's TX power state if none of [-m|-s|-t|-c] given\n");
    mctrl_print("\t\t\t-m <1/0> enable/disable mcs based adjustment\n");
    mctrl_print("\t\t\t-s <1/0> enable/disable subband based adjustment\n");
    mctrl_print("\t\t\t-t <1/0> enable/disable temperature based adjustment\n");
    mctrl_print("\t\t\t-c <1/0> enable/disable channel based adjustment\n");
}

static void process_power_state(struct tx_pwr_adj_cfm* cfm)
{
    mctrl_print("TX power state information\n");
    mctrl_print("\tBase power: %.3f dBm\n\tMax power: %.3f dBm\n\tCurrent power: %.3f dBm\n",
            ((float) cfm->base_power_qdbm)/4.0,
            ((float) cfm->max_tx_power_qdbm)/4.0,
            ((float) cfm->current_tx_power_qdbm)/4.0);
    mctrl_print("\tRegulatory limit: %.3f dBm\n",
            ((float) cfm->regulatory_limit_dbm)/4.0);
    mctrl_print("\tSubband power adjustment: %.3f dB\n",
            (float) 10*log10((cfm->subband_scale/65536.0)));
    mctrl_print("\tTemperature power adjustment: %.3f dB\n",
            (float) 10*log10((cfm->temp_power_scaler/65536.0)));
    mctrl_print("\tArbitrary txscaler: %.3f dB\n",
            (float) 20*log10((cfm->tx_linear_power_scaler/65536.0)));
    mctrl_print("\tTx power drift: %.3f dB\n",
            (float) 10*log10((cfm->tx_power_drift_lin/65536.0)));
    mctrl_print("\tTX antenna gain: %d dBi\n",
            cfm->tx_antenna_gain_dbi);
    mctrl_print("\tTX power adjustement mask: %d\n",
            cfm->en_tx_pwr_adj_mask);
    mctrl_print("\tEnable MCS based adjustment: %d\n",
            (cfm->en_tx_pwr_adj_mask & TX_PWR_ADJ_MCS) && 1);
    mctrl_print("\tEnable sub-band based adjustment: %d\n",
            (cfm->en_tx_pwr_adj_mask & TX_PWR_ADJ_SUBBAND) && 1);
    mctrl_print("\tEnable temperature based adjustment: %d\n",
            (cfm->en_tx_pwr_adj_mask & TX_PWR_ADJ_TEMPERATURE) && 1);
    mctrl_print("\tEnable channel based adjustment: %d\n",
            (cfm->en_tx_pwr_adj_mask & TX_PWR_ADJ_CHANNEL) && 1);
    for (int i = 0; i < 10 ; i++)
    {
        mctrl_print("\tMCS%d: %.3f dBm\n", i,
                ((float) (cfm->scale_value_db[i]+cfm->base_power_qdbm))/4.0);
    }
}

int tx_pwr_adj(struct morsectrl *mors, int argc, char *argv[])
{
    int ret = -1, option;
    uint8_t flag = 0;
    uint32_t en_tx_pwr_adj_mask;
    struct tx_pwr_adj_command *cmd = {0};
    struct tx_pwr_adj_cfm *cfm = {0};
    struct morsectrl_transport_buff *cmd_tbuff;
    struct morsectrl_transport_buff *rsp_tbuff;
    int32_t tmp;

    if (argc == 0)
    {
        usage(mors);
        return 0;
    }

    cmd_tbuff = morsectrl_transport_cmd_alloc(mors->transport, sizeof(*cmd));
    rsp_tbuff = morsectrl_transport_resp_alloc(mors->transport, sizeof(*cfm));

    if (!cmd_tbuff || !rsp_tbuff)
        goto exit;

    cmd = TBUFF_TO_CMD(cmd_tbuff, struct tx_pwr_adj_command);
    cfm = TBUFF_TO_RSP(rsp_tbuff, struct tx_pwr_adj_cfm);

    if (argc == 1)
    {
        /** Get the tx power adjustments */
        flag = 1;
        en_tx_pwr_adj_mask = 0;
    }
    else if (argc > 1)
    {
        /** Set the tx power adjustments */
        flag = 2;
        /** Read the cuurent mask and update with user input */
        cmd->flag = htole32(1);
        cmd->en_tx_pwr_adj_mask = 0;
        ret = morsectrl_send_command(mors->transport, MORSE_TEST_COMMAND_TX_PWR_ADJ,
                                 cmd_tbuff, rsp_tbuff);
        if (ret < 0)
        {
            mctrl_err("Failed to execute command\n");
            goto exit;
        }
        en_tx_pwr_adj_mask = cfm->en_tx_pwr_adj_mask;
        while ((option = getopt(argc, argv, "m:s:t:c:")) != -1)
        {
            switch (option)
            {
                case 'm':
                    if (str_to_int32(optarg, &tmp))
                    {
                        mctrl_err("Invalid MCS based power adjustment\n");
                        ret = -1;
                        goto exit;
                    }
                    en_tx_pwr_adj_mask = (en_tx_pwr_adj_mask & (~TX_PWR_ADJ_MCS))
                        | (tmp & 0x01);
                    mctrl_print("MCS based power adjustment: %d\n", tmp);
                    break;
                case 's':
                    if (str_to_int32(optarg, &tmp))
                    {
                        mctrl_err("Invalid Subband based power adjustment\n");
                        ret = -1;
                        goto exit;
                    }

                    en_tx_pwr_adj_mask = (en_tx_pwr_adj_mask & (~TX_PWR_ADJ_SUBBAND))
                        | ((tmp & 0x01) << 1);
                    mctrl_print("Subband based power adjustment: %d\n", tmp);
                    break;
                case 't':
                    if (str_to_int32(optarg, &tmp))
                    {
                        mctrl_err("Invalid tmperature based power adjustment\n");
                        ret = -1;
                        goto exit;
                    }

                    en_tx_pwr_adj_mask = (en_tx_pwr_adj_mask & (~TX_PWR_ADJ_TEMPERATURE))
                        | ((tmp & 0x01) << 2);
                    mctrl_print("Temperature based power adjustment: %d\n", tmp);
                    break;
                case 'c':
                    if (str_to_int32(optarg, &tmp))
                    {
                        mctrl_err("Invalid channel based power adjustment\n");
                        ret = -1;
                        goto exit;
                    }

                    en_tx_pwr_adj_mask = (en_tx_pwr_adj_mask & (~TX_PWR_ADJ_CHANNEL))
                        | ((tmp & 0x01) << 3);
                    mctrl_print("Channel based power adjustment: %d\n", tmp);
                    break;
                default:
                    usage(mors);
                    goto exit;
                    break;
            }
        }
    }
    else
    {
        mctrl_err("Invalid command parameters\n");
        usage(mors);
        ret = -1;
        goto exit;
    }

    cmd->flag = htole32(flag);
    cmd->en_tx_pwr_adj_mask = htole32(en_tx_pwr_adj_mask);
    ret = morsectrl_send_command(mors->transport, MORSE_TEST_COMMAND_TX_PWR_ADJ,
                                 cmd_tbuff, rsp_tbuff);
    if (flag == 1)
    {
        process_power_state(cfm);
    }
exit:

    morsectrl_transport_buff_free(cmd_tbuff);
    morsectrl_transport_buff_free(rsp_tbuff);
    return ret;
}

MM_CLI_HANDLER(tx_pwr_adj, MM_INTF_REQUIRED, MM_DIRECT_CHIP_SUPPORTED);
