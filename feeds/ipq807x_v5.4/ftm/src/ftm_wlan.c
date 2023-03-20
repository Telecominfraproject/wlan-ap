/*==========================================================================

                     FTM WLAN Source File

============================================================================

# Copyright (c) 2011, 2013-2018 Qualcomm Technologies, Inc.
# All Rights Reserved.
# Confidential and Proprietary - Qualcomm Technologies, Inc.

===========================================================================*/

/*===========================================================================

                         Edit History


when       who       what, where, why
--------   ---       ----------------------------------------------------------
07/11/11   karthikm  Wrapper that contains routines for directing FTM commands
                     sent from host to the IOCTL calls of Atheros driver.
*/

/*
 * Copyright (c) 2006 Atheros Communications Inc.
 * All rights reserved.
 *
 *
// The software source and binaries included in this development package are
// licensed, not sold. You, or your company, received the package under one
// or more license agreements. The rights granted to you are specifically
// listed in these license agreement(s). All other rights remain with Atheros
// Communications, Inc., its subsidiaries, or the respective owner including
// those listed on the included copyright notices.  Distribution of any
// portion of this package must be in strict compliance with the license
// agreement(s) terms.
// </copyright>
//
// <summary>
//      FTM_WLAN_TCMD
//  Based on athtestcmd.c from AR6003 drop
// </summary>
//
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/param.h>
#include <net/if.h>
#include <linux/limits.h>
#include <endian.h>

#include "comdef.h"
#include "diagcmd.h"

#include "testcmd.h"
#include "libtcmd.h"
#include "ftm_wlan.h"
#include "ftm_dbg.h"

#define INVALID_FREQ    0
#define A_RATE_NUM      28
#define G_RATE_NUM      28
#define RATE_STR_LEN    20

int old_iface_id = -1;
#ifdef CONFIG_FTM_WLAN_AUTOLOAD
#define MODULE_FILE             "/proc/modules"
#define DRIVER_MODULE_TAG       "wlan"
#define WLAN_CFG_FILE           "/persist/wlan_mac.bin"
/* Offset for set mac address cmd */
#define MAIN_CMD_OFFSET         16
#define SUB_CMD_OFFSET          24
#define LENGTH_OFFSET           32
#define NMAC_OFFSET             48
#define PMAC_OFFSET             49

#if defined(ANDROID)
#if defined(BOARD_HAS_ATH_WLAN_AR6320)
#if BOARD_HAS_ATH_WLAN_AR6320
#define FTM_WLAN_LOAD_CMD       "/system/bin/insmod " \
                                "/system/lib/modules/wlan.ko con_mode=5"
#define FTM_WLAN_UNLOAD_CMD     "/system/bin/rmmod wlan"
#endif
#endif
#elif defined(MDM_LE)
#define FTM_WLAN_LOAD_CMD       "/etc/init.d/wlan start_ftm"
#define FTM_WLAN_UNLOAD_CMD     "/etc/init.d/wlan stop"
#else
#warning "Load and Unload driver may not work!"
#endif

typedef enum {
    SUBCMD_DRIVER_LOAD      = 'L',
    SUBCMD_DRIVER_UNLOAD    = 'U',
    SUBCMD_DRIVER_AUTO_MODE = 'A',
} sub_cmds;

static int load_wifi_driver_testmode(void);
static int unload_wifi_driver(void);
static bool is_wifi_driver_loaded(char *mod_tag);
static bool flag_driver_auto_load = false;
#endif /* CONFIG_FTM_WLAN_AUTOLOAD */
extern void diagpkt_free(void *pkt);

char bdf_file[128] = {'0'};
static void rxReport(void *buf);
#ifndef WIN_AP_HOST
char g_ifname[IFNAMSIZ] = "wlan";
#else
#ifdef WIN_AP_HOST_OPEN
char g_ifname[IFNAMSIZ] = "wlan";
#else
char g_ifname[IFNAMSIZ] = "wifi";
#endif
#endif

#ifdef WIN_AP_HOST

#define WIN_COMMON_OP_REQ 0xFE
#define BT_CMD_REQ 198
#define BTCMD_ID_POS 64
#define BT_MAC_LEN_POS 80
#define BT_CMD_LEN 0xC

#endif

void print_uchar_array(uint8_t *addr, int len)
{
   int i;
   for (i = 0;i< len; i++)
      DPRINTF(FTM_DBG_TRACE, "%02X ", addr[i]);
   DPRINTF(FTM_DBG_TRACE, "\n");
}

void print_uint16_array(uint16_t *addr, int len)
{
   int i;
   for (i = 0;i< len; i++)
      DPRINTF(FTM_DBG_TRACE, "%02X %02X ", addr[i]>>8, addr[i]&0xFF);
   DPRINTF(FTM_DBG_TRACE, "\n");
}

/*===========================================================================
FUNCTION   rxReport

DESCRIPTION
  Quick debug routine that will print all the receive statistics

DEPENDENCIES
  NIL

RETURN VALUE
  NIL

SIDE EFFECTS
  NONE

===========================================================================*/
static void rxReport(void *buf)
{
    uint32 pkt;
    int  rssi;
    uint32 crcError;
    uint32 secErr;
    uint16 rateCnt[TCMD_MAX_RATES];
    uint16 rateCntShortGuard[TCMD_MAX_RATES];

    pkt = *(uint32 *)buf;
    rssi = (int)(*((uint32 *)buf + 1));
    crcError = *((uint32 *)buf + 2);
    secErr = *((uint32 *)buf + 3);

    DPRINTF(FTM_DBG_TRACE, "total pkt %u, crcError pkt %u, secErr pkt %u, average rssi %d\n",
            pkt, crcError, secErr, (int)( pkt ? (rssi / (int)pkt) : 0));


    memcpy(rateCnt, ((unsigned char *)buf) + (4 * sizeof(uint32)), sizeof(rateCnt));
    memcpy(rateCntShortGuard, ((unsigned char *)buf) + (4 * sizeof(uint32)) +
            (TCMD_MAX_RATES * sizeof(uint16)), sizeof(rateCntShortGuard));

    DPRINTF(FTM_DBG_INFO, "1Mbps     %d\n", rateCnt[0]);
    DPRINTF(FTM_DBG_INFO, "2Mbps     %d\n", rateCnt[1]);
    DPRINTF(FTM_DBG_INFO, "5.5Mbps   %d\n", rateCnt[2]);
    DPRINTF(FTM_DBG_INFO, "11Mbps    %d\n", rateCnt[3]);
    DPRINTF(FTM_DBG_INFO, "6Mbps     %d\n", rateCnt[4]);
    DPRINTF(FTM_DBG_INFO, "9Mbps     %d\n", rateCnt[5]);
    DPRINTF(FTM_DBG_INFO, "12Mbps    %d\n", rateCnt[6]);
    DPRINTF(FTM_DBG_INFO, "18Mbps    %d\n", rateCnt[7]);
    DPRINTF(FTM_DBG_INFO, "24Mbps    %d\n", rateCnt[8]);
    DPRINTF(FTM_DBG_INFO, "36Mbps    %d\n", rateCnt[9]);
    DPRINTF(FTM_DBG_INFO, "48Mbps    %d\n", rateCnt[10]);
    DPRINTF(FTM_DBG_INFO, "54Mbps    %d\n", rateCnt[11]);
    DPRINTF(FTM_DBG_INFO, "\n");
    DPRINTF(FTM_DBG_INFO, "HT20 MCS0 6.5Mbps   %d (SGI: %d)\n", rateCnt[12], rateCntShortGuard[12]);
    DPRINTF(FTM_DBG_INFO, "HT20 MCS1 13Mbps    %d (SGI: %d)\n", rateCnt[13], rateCntShortGuard[13]);
    DPRINTF(FTM_DBG_INFO, "HT20 MCS2 19.5Mbps  %d (SGI: %d)\n", rateCnt[14], rateCntShortGuard[14]);
    DPRINTF(FTM_DBG_INFO, "HT20 MCS3 26Mbps    %d (SGI: %d)\n", rateCnt[15], rateCntShortGuard[15]);
    DPRINTF(FTM_DBG_INFO, "HT20 MCS4 39Mbps    %d (SGI: %d)\n", rateCnt[16], rateCntShortGuard[16]);
    DPRINTF(FTM_DBG_INFO, "HT20 MCS5 52Mbps    %d (SGI: %d)\n", rateCnt[17], rateCntShortGuard[17]);
    DPRINTF(FTM_DBG_INFO, "HT20 MCS6 58.5Mbps  %d (SGI: %d)\n", rateCnt[18], rateCntShortGuard[18]);
    DPRINTF(FTM_DBG_INFO, "HT20 MCS7 65Mbps    %d (SGI: %d)\n", rateCnt[19], rateCntShortGuard[19]);
    DPRINTF(FTM_DBG_INFO, "\n");
    DPRINTF(FTM_DBG_INFO, "HT40 MCS0 13.5Mbps    %d (SGI: %d)\n", rateCnt[20], rateCntShortGuard[20]);
    DPRINTF(FTM_DBG_INFO, "HT40 MCS1 27.0Mbps    %d (SGI: %d)\n", rateCnt[21], rateCntShortGuard[21]);
    DPRINTF(FTM_DBG_INFO, "HT40 MCS2 40.5Mbps    %d (SGI: %d)\n", rateCnt[22], rateCntShortGuard[22]);
    DPRINTF(FTM_DBG_INFO, "HT40 MCS3 54Mbps      %d (SGI: %d)\n", rateCnt[23], rateCntShortGuard[23]);
    DPRINTF(FTM_DBG_INFO, "HT40 MCS4 81Mbps      %d (SGI: %d)\n", rateCnt[24], rateCntShortGuard[24]);
    DPRINTF(FTM_DBG_INFO, "HT40 MCS5 108Mbps     %d (SGI: %d)\n", rateCnt[25], rateCntShortGuard[25]);
    DPRINTF(FTM_DBG_INFO, "HT40 MCS6 121.5Mbps   %d (SGI: %d)\n", rateCnt[26], rateCntShortGuard[26]);
    DPRINTF(FTM_DBG_INFO, "HT40 MCS7 135Mbps     %d (SGI: %d)\n", rateCnt[27], rateCntShortGuard[27]);
}

ftm_wlan_rsp_pkt_type *g_rsp = NULL;
TCMD_ID tcmd = TCMD_CONT_RX_ID;
uint32 mode = 0;

/*===========================================================================
FUNCTION   ftm_wlan_tcmd_rx

DESCRIPTION
   Call back handler

DEPENDENCIES
  NIL

RETURN VALUE
  NONE

SIDE EFFECTS
  NONE

===========================================================================*/

void ftm_wlan_tcmd_rx(void *buf, int len)
{
    void *data = NULL;
    int data_len = 0;
    struct TCMD_CONT_RX_REPORT *report = NULL;
    TC_CMDS *tCmd = NULL;
    u_int32_t *src, *dest;
    int j;

#ifdef DEBUG
    current_time();
#endif
    DPRINTF(FTM_DBG_TRACE, "Rx call back received with len %d\n", len);

    /* Build the response to be sent */
    switch(tcmd) {
    case TCMD_CONT_RX_ID:
        report = &((TCMD_CONT_RX *) buf)->u.report;

        if (mode == TCMD_CONT_RX_REPORT) {
            rxReport((void*)report);
        } else if (mode == TCMD_CONT_RX_GETMAC) {
            tCmd = (TC_CMDS *)buf;

            DPRINTF(FTM_DBG_TRACE, "length %d version %d act %d\n",
                    tCmd->hdr.u.parm.length, tCmd->hdr.u.parm.version,
                    tCmd->hdr.act);

            DPRINTF(FTM_DBG_INFO,
                    "MAC address : %02x:%02x:%02x:%02x:%02x:%02x\n",
                    tCmd->buf[0], tCmd->buf[1], tCmd->buf[2],
                    tCmd->buf[3], tCmd->buf[4], tCmd->buf[5]);
        }

        data = report;
        data_len = sizeof(struct TCMD_CONT_RX_REPORT);
        break;

    case TC_CMDS_ID:
        tCmd = (TC_CMDS *)buf;

        DPRINTF(FTM_DBG_TRACE, "length %d version %d act %d\n",
                tCmd->hdr.u.parm.length, tCmd->hdr.u.parm.version,
                tCmd->hdr.act);
        data = buf;
        data_len = sizeof(TC_CMDS);

        if ( mode == TC_CMDS_READTHERMAL )
            DPRINTF(FTM_DBG_INFO, "Chip Thermal value: %d\n", tCmd->buf[0]);
        break;

    case TC_CMD_TLV_ID:
        data = buf;
        data_len = len;
        src = (u_int32_t *)buf;
        dest = (u_int32_t *)buf;
        for (j = 0; j < roundup(len, sizeof(u_int32_t))/4; j++) {
                *(dest+j) = le32toh(*(src+j));
        }
        print_uchar_array(buf, len);

        DPRINTF(FTM_DBG_TRACE, "tcmd_rx TC_CMD_TLV_ID length %d\n", len);
        break;

    default:
        data_len = 0;
        data = NULL;
        DPRINTF(FTM_DBG_TRACE, "Unknown TCMD response\n");
        break;
    }

    g_rsp = (ftm_wlan_rsp_pkt_type*)diagpkt_subsys_alloc( DIAG_SUBSYS_FTM,
            FTM_WLAN_CMD_CODE, (sizeof(g_rsp->common_header) +
                                sizeof(g_rsp->cmd.common_ops) + data_len));

    if (g_rsp == NULL) {
        DPRINTF(FTM_DBG_ERROR, "Failed to allocate diag packet! tcmd: %d",
                tcmd);
        return;
    }

#ifdef WIN_AP_HOST
    /* Handle win specific fw responses */
    if (data == NULL || data_len == 0)
        g_rsp->cmd.common_ops.result = htole32(FTM_ERR_CODE_IOCTL_FAIL);
    else
        win_host_handle_fw_resp(g_rsp, data, data_len);
    if (g_rsp == NULL)
	return;
#else

    if (data && data_len != 0)
        memcpy(g_rsp->cmd.common_ops.rx_and_therm.rxReport.data, data, data_len);

    g_rsp->cmd.common_ops.result = htole32(FTM_ERR_CODE_PASS);
#endif
}

/*===========================================================================
FUNCTION   isResponseNeeded

DESCRIPTION
   Do we need a response for the command

DEPENDENCIES
  NIL

RETURN VALUE
  boolean response required/not

SIDE EFFECTS
  NONE

===========================================================================*/
static bool isResponseNeeded(void *buf)
{
    bool respNeeded = false;

    tcmd = le32toh(*((uint32 *) buf));
    mode = le32toh(*((uint32 *) buf + 1));

    /// Insert commands which need response
    switch (tcmd)
    {
    case TC_CMD_TLV_ID:
        respNeeded = true;
        break;
    case TCMD_CONT_RX_ID:
        switch (mode)
        {
        case TCMD_CONT_RX_REPORT:
        case TCMD_CONT_RX_GETMAC:
            respNeeded = true;
            break;
        }
        break;
    case TC_CMDS_ID:
        switch (mode)
        {
        case TC_CMDS_READTHERMAL:
        case TC_CMDS_EFUSEDUMP:
        case TC_CMDS_EFUSEWRITE:
        case TC_CMDS_OTPSTREAMWRITE:
        case TC_CMDS_OTPDUMP:
            respNeeded = true; //TC_CMDS_EFUSEDUMP, TC_CMDS_EFUSEWRITE, TC_CMDS_OTPSTREAMWRITE, TC_CMDS_OTPDUMP, TC_CMDS_READTHERMAL
            break;
        }
        break;
    default:
        break;
    }

    if (respNeeded)
    {
        DPRINTF(FTM_DBG_TRACE, "cmdID %d response needed\n", tcmd);
    }
    else
    {
        DPRINTF(FTM_DBG_TRACE, "cmdID %d response not needed\n", tcmd);
    }

    return respNeeded;
}


/*===========================================================================
FUNCTION   ftm_wlan_common_op

DESCRIPTION
  Process ftm commands like load driver, Tx, Rx and few test commands

DEPENDENCIES
  NIL

RETURN VALUE
  Returns back buffer that is meant to be passed to the diag callback

SIDE EFFECTS
  NONE

===========================================================================*/
static void *ftm_wlan_common_op(ftm_wlan_req_pkt_type *wlan_ftm_pkt, int pkt_len)
{
   uint8_t *input_msg = (uint8_t*)wlan_ftm_pkt;
   uint32_t *cmd;
   ftm_wlan_rsp_pkt_type *rsp;
   int data_len = pkt_len - sizeof(diagpkt_subsys_header_v2_type) - 4;
   char ifname[IFNAMSIZ];
   bool resp = false;
   int curr_iface_id;

   snprintf(ifname, sizeof(ifname), "%s%d", g_ifname,
           wlan_ftm_pkt->cmd.common_ops.wlandeviceno);
   if (data_len <= 0) {
       DPRINTF(FTM_DBG_ERROR, "Invalid data_len: %d\n", data_len);
       return NULL;
   }

   cmd = (uint32_t*)(wlan_ftm_pkt->cmd.common_ops.data);
   DPRINTF(FTM_DBG_TRACE, "Command ID rec'd: 0x%X length %d\n", le32toh(*cmd), data_len);

   print_uchar_array((uint8_t*)(wlan_ftm_pkt->cmd.common_ops.data), data_len);

   g_rsp = NULL;

   rsp = (ftm_wlan_rsp_pkt_type*)diagpkt_subsys_alloc(DIAG_SUBSYS_FTM,
                                                      FTM_WLAN_CMD_CODE,
                                                      sizeof(rsp->common_header) +
                                                      sizeof(rsp->cmd.common_ops));

   if (rsp == NULL) {
           DPRINTF(FTM_DBG_ERROR, "Failed to allocate Diag packet: %p\n", rsp);
           goto err_out;
   }

   rsp->cmd.common_ops.result = htole32(FTM_ERR_CODE_PASS);

#ifdef CONFIG_FTM_WLAN_AUTOLOAD
   if (le32toh(*cmd) == TCMD_LOAD_DRIVER) {
       /* Get sub-cmd */
       unsigned int sub_cmd = 0;
       if (data_len >= (int)(2*sizeof(uint32)))
           sub_cmd = *((uint32*)wlan_ftm_pkt->cmd.common_ops.data + 1);

       /* execute sub_cmd */
       if (sub_cmd == SUBCMD_DRIVER_LOAD) {
           load_wifi_driver_testmode();
       } else if (sub_cmd == SUBCMD_DRIVER_UNLOAD) {
           unload_wifi_driver();
       } else if (sub_cmd == SUBCMD_DRIVER_AUTO_MODE) {
           flag_driver_auto_load = true;
       }

       /* These commands will not go to firmware */
       return rsp;
   }
   else if ((data_len > PMAC_OFFSET) && *(wlan_ftm_pkt->cmd.common_ops.data + MAIN_CMD_OFFSET) == OP_GENERIC_NART_CMD) {
       if (*(wlan_ftm_pkt->cmd.common_ops.data + SUB_CMD_OFFSET) == TCMD_SET_MAC_ADDR) {
           int i = 0;
           uint8_t length = 0, nMac = 0, *pMac = NULL;
           length = *(wlan_ftm_pkt->cmd.common_ops.data + LENGTH_OFFSET);
           nMac = *(wlan_ftm_pkt->cmd.common_ops.data + NMAC_OFFSET);
           pMac = (wlan_ftm_pkt->cmd.common_ops.data + PMAC_OFFSET);

           DPRINTF(FTM_DBG_TRACE, "Handling WLAN request of setting Mac address\n");

           if (!nMac || (length < (5 + 6*nMac))) {
               DPRINTF(FTM_DBG_ERROR, "invalid data, nMac = %u, length = %u\n",
                       nMac, length);
               rsp->cmd.common_ops.result = htole32(FTM_ERR_CODE_IOCTL_FAIL);
               goto err_out;
           }

           FILE *fp = fopen(WLAN_CFG_FILE, "wb");
           if (fp == NULL) {
               DPRINTF(FTM_DBG_INFO, "Can't open file %s\n", WLAN_CFG_FILE);
               rsp->cmd.common_ops.result = htole32(FTM_ERR_CODE_IOCTL_FAIL);
               goto err_out;
           }

           for (i = 0; i < nMac; i++) {
               fprintf(fp, "Intf%dMacAddress=%02X%02X%02X%02X%02X%02X\n", i,
                       pMac[0], pMac[1], pMac[2], pMac[3], pMac[4], pMac[5]);
               pMac = pMac + 6;
           }
           fprintf(fp, "END");
           fclose(fp);
           /* This command will not go to the firmware */
           return rsp;
       }
   }

   if ((flag_driver_auto_load == true) \
       && (!is_wifi_driver_loaded(DRIVER_MODULE_TAG))) {
       /* load the driver to testmode */
       load_wifi_driver_testmode();
   }
#endif

#ifdef WIN_AP_HOST
   /* 0xFE is the command id(first byte) for BD capture/Flash write requests */
    if (le32toh(*cmd) == WIN_COMMON_OP_REQ) {
        diagpkt_free(rsp);
        rsp = win_host_handle_bdf_req (wlan_ftm_pkt, pkt_len);
        return (void *)rsp;
    }
    if (data_len > BTCMD_ID_POS) {
        if((input_msg[BTCMD_ID_POS] == BT_CMD_REQ) && (input_msg[BT_MAC_LEN_POS] == BT_CMD_LEN)){
            diagpkt_free(rsp);
            rsp = win_bt_mac_flash_write(wlan_ftm_pkt, pkt_len);
            if (rsp)
                return (void *)rsp;
            else {
                DPRINTF(FTM_DBG_ERROR, "Response is NULL!\n");
                goto err_out;
            }
        }
    }
#endif
    curr_iface_id = wlan_ftm_pkt->cmd.common_ops.wlandeviceno;
    if (curr_iface_id != old_iface_id)
   {
       DPRINTF(FTM_DBG_TRACE, "Initializing Interface: %s\n", ifname);

       if (tcmd_tx_init(ifname, ftm_wlan_tcmd_rx))
       {
           DPRINTF(FTM_DBG_ERROR, "Couldn't init tcmd transport!\n");
           rsp->cmd.common_ops.result = htole32(FTM_ERR_CODE_IOCTL_FAIL);
           goto err_out;
       }

       DPRINTF(FTM_DBG_TRACE, "tcmd: Initialized Interface: %s\n", ifname);
#ifdef WIN_AP_HOST_OPEN
       tcmd_tx_start();
       DPRINTF(FTM_DBG_TRACE, "tcmd: tcmd_tx_start done\n");
#endif
       old_iface_id = curr_iface_id;
   }

   resp = isResponseNeeded( (void*)wlan_ftm_pkt->cmd.common_ops.data);

   if (tcmd_tx(wlan_ftm_pkt->cmd.common_ops.data, data_len, resp))
   {
        DPRINTF(FTM_DBG_ERROR, "TCMD timed out!\n");
        rsp->cmd.common_ops.result = htole32(FTM_ERR_CODE_IOCTL_FAIL);
        goto err_out;
   }

   if (resp)
   {
       if (g_rsp)
       {
          diagpkt_free(rsp);
          return (void *) g_rsp;
       }
       else
       {
           DPRINTF(FTM_DBG_ERROR, "No response got probably timing out.... \n");
           rsp->cmd.common_ops.result = htole32(FTM_ERR_CODE_IOCTL_FAIL);
       }
  } else {
       DPRINTF(FTM_DBG_ERROR, "Resp is false \n");
  }

err_out:
    return (void *) rsp;
}

/*===========================================================================
FUNCTION  ftm_wlan_bdf_read

DESCRIPTION
  Read the data from bdf_file

DEPENDENCIES
  NIL

RETURN VALUE
  Returns back buffer that is meant to be passed to the diag callback

SIDE EFFECTS
  NONE

===========================================================================*/
static void *ftm_wlan_bdf_read(ftm_wlan_req_pkt_type *wlan_ftm_read_pkt)
{
    FILE *fp;
    byte err_code;
    int ret;
    unsigned long file_size;
    unsigned int size;  /*actual number of bytes transferred in resp pkt*/
    char *buf = NULL;
    struct stat st;
    int byte_rem;
    uint16 rsp_pkt_size;
    ftm_wlan_rsp_pkt_type *ftm_read_resp = NULL;

    rsp_pkt_size = sizeof(ftm_read_resp->common_header) + sizeof(ftm_read_resp->cmd.read_file);

    fp = fopen(bdf_file, "r");
    if (fp == NULL) {
        DPRINTF(FTM_DBG_ERROR, " failed to open file: %s\n", bdf_file);
        err_code = WLAN_BDF_FILE_OPEN_FAIL;
        goto ftm_read_out;
    }
    st.st_size = 0;
    /*get size of the file*/
    if (stat(bdf_file, &st) == 0)
        file_size = st.st_size;
    else {
        DPRINTF(FTM_DBG_ERROR, "Failed to get file size \n");
        err_code = WLAN_BDF_FILE_STAT_FAIL;
        goto ftm_read_out;
    }
    DPRINTF(FTM_DBG_TRACE, "ftm_daemon: %s file size is: %lu\n", bdf_file, file_size);
    if (file_size <= wlan_ftm_read_pkt->cmd.read_file.offset) {
        DPRINTF(FTM_DBG_ERROR, " Bad offset \n");
        err_code = WLAN_BDF_BAD_OFFSET;
        goto ftm_read_out;
    }

    DPRINTF(FTM_DBG_TRACE, "ftm_daemon: offset: %d\n", wlan_ftm_read_pkt->cmd.read_file.offset);
    ret = fseek(fp, wlan_ftm_read_pkt->cmd.read_file.offset, SEEK_SET);
    if (ret == -1) {
        DPRINTF(FTM_DBG_ERROR, "fseek failed \n");
        err_code = WLAN_BDF_FILE_SEEK_FAIL;
        goto ftm_read_out;
    }

    /*validate the size(number of bytes) to be read from file */
    if ((wlan_ftm_read_pkt->cmd_rsp_pkt_size - rsp_pkt_size) > (file_size - wlan_ftm_read_pkt->cmd.read_file.offset)) {
        size = file_size - wlan_ftm_read_pkt->cmd.read_file.offset;
    } else
        size = wlan_ftm_read_pkt->cmd_rsp_pkt_size - rsp_pkt_size;
    DPRINTF(FTM_DBG_TRACE, "ftm_daemon: number of bytes to be read: %d\n", size);

    buf = (char *)malloc(size);
    if(!buf) {
        DPRINTF(FTM_DBG_ERROR, " failed to allocate buf memory \n");
        err_code = WLAN_BDF_READ_FAILED;
        goto ftm_read_out;
    }
    ret = fread(buf, 1, size, fp);
    if (ret ==(signed)size || feof(fp)) {
        size = ret;
        err_code = WLAN_BDF_READ_SUCCESS;
        rsp_pkt_size += size;
    } else {
        DPRINTF(FTM_DBG_ERROR, "ftm_daemon: fread failed\n");
        err_code = WLAN_BDF_READ_FAILED;
    }

ftm_read_out:
    if (err_code != WLAN_BDF_READ_SUCCESS)
        rsp_pkt_size += 1;         //need to send atleast 1 byte of data to diag in case of read failure

    ftm_read_resp = (ftm_wlan_rsp_pkt_type *)diagpkt_subsys_alloc(DIAG_SUBSYS_FTM,
                                                                  FTM_WLAN_CMD_CODE,
                                                                  rsp_pkt_size
                                                                 );
    if (ftm_read_resp == NULL) {
        DPRINTF(FTM_DBG_ERROR, "Failed to allocate Diag resp packet\n");
        if (fp)
            fclose(fp);
        if (buf)
            free(buf);
        return NULL;
    }
    ftm_read_resp->common_header.cmd_id = htole16(FTM_WLAN_BDF_READ);
    ftm_read_resp->common_header.cmd_rsp_pkt_size = htole16(rsp_pkt_size);
    ftm_read_resp->cmd.read_file.result = err_code;
    if (err_code == WLAN_BDF_READ_SUCCESS) {
        ftm_read_resp->cmd.read_file.size = htole16(size);
        byte_rem = file_size - (size + wlan_ftm_read_pkt->cmd.read_file.offset);
        DPRINTF(FTM_DBG_TRACE, "ftm_daemon: byte_rem: %d\n", byte_rem);
        memcpy(ftm_read_resp->cmd.read_file.bytes_remaining, &byte_rem, sizeof(ftm_read_resp->cmd.read_file.bytes_remaining));
        memcpy(ftm_read_resp->cmd.read_file.data, buf, size);
    } else
        memset(ftm_read_resp->cmd.read_file.data, 0, 1);

    DPRINTF(FTM_DBG_TRACE, "ftm_daemon: read resp msg dump:\n");
    print_uchar_array((uint8_t *)ftm_read_resp, (int)rsp_pkt_size);

    if (fp)
        fclose(fp);
    if (buf)
        free(buf);
    return (void *)ftm_read_resp;
}

/*===========================================================================
FUNCTION  ftm_wlan_bdf_write

DESCRIPTION
  Write the data received from application to bdf_file

DEPENDENCIES
  NIL

RETURN VALUE
  Returns back buffer that is meant to be passed to the diag callback

SIDE EFFECTS
  NONE

===========================================================================*/
static void *ftm_wlan_bdf_write(ftm_wlan_req_pkt_type *wlan_ftm_write_pkt)
{
    FILE *fp;
    byte err_code;
    ftm_wlan_rsp_pkt_type *ftm_write_resp = NULL;
    uint16 rsp_pkt_len;

    rsp_pkt_len = sizeof(ftm_write_resp->common_header) + sizeof(ftm_write_resp->cmd.write_file) + 1;
    ftm_write_resp = (ftm_wlan_rsp_pkt_type *)diagpkt_subsys_alloc(DIAG_SUBSYS_FTM,
                                                                   FTM_WLAN_CMD_CODE,
                                                                   rsp_pkt_len
                                                                  );
    if (ftm_write_resp == NULL) {
        DPRINTF(FTM_DBG_ERROR, "Failed to allocate Diag resp packet\n");
        return NULL;
    }

    if (!wlan_ftm_write_pkt->cmd.write_file.append_flag) {
        DPRINTF(FTM_DBG_TRACE, "ftm_daemon: bdf_file = %s open in write mode\n", bdf_file);
        fp = fopen(bdf_file, "w");
        if (fp == NULL) {
            DPRINTF(FTM_DBG_ERROR, " failed to open file: %s\n", bdf_file);
            err_code = WLAN_BDF_FILE_OPEN_FAIL;
            goto ftm_write_out;
        }
    } else {
        DPRINTF(FTM_DBG_TRACE, "ftm_daemon: bdf_file = %s open in append mode\n", bdf_file);
        fp = fopen(bdf_file, "a");
        if (fp == NULL) {
            DPRINTF(FTM_DBG_ERROR, " failed to open file: %s\n", bdf_file);
            err_code = WLAN_BDF_FILE_OPEN_FAIL;
            goto ftm_write_out;
        }
    }

    fwrite(&(wlan_ftm_write_pkt->cmd.write_file.data), 1, wlan_ftm_write_pkt->cmd.write_file.size, fp);
    if (ferror(fp)) {
        DPRINTF(FTM_DBG_ERROR, " failed to write\n");
        err_code = WLAN_BDF_WRITE_FAILED;
    } else
        err_code = WLAN_BDF_WRITE_SUCCESS;

ftm_write_out:
    if (fp)
        fclose(fp);
    ftm_write_resp->common_header.cmd_id = htole16(FTM_WLAN_BDF_WRITE);
    ftm_write_resp->common_header.cmd_rsp_pkt_size = htole16(rsp_pkt_len);
    ftm_write_resp->cmd.write_file.result = err_code;
    memset(ftm_write_resp->cmd.write_file.rsvd, 0,
           sizeof(ftm_write_resp->cmd.write_file.rsvd));
    memset(ftm_write_resp->cmd.write_file.data, 0, 1);
    DPRINTF(FTM_DBG_TRACE, "ftm_daemon: write resp msg dump:\n");
    print_uchar_array((uint8_t *)ftm_write_resp, (int)rsp_pkt_len);

    return (void *)ftm_write_resp;
}

/*===========================================================================
FUNCTION  ftm_wlan_bdf_get_filename

DESCRIPTION
  Get bdf_file path

DEPENDENCIES
  NIL

RETURN VALUE
  Returns back buffer that is meant to be passed to the diag callback

SIDE EFFECTS
  NONE

===========================================================================*/
static void *ftm_wlan_bdf_get_filename()
{
    ftm_wlan_rsp_pkt_type *ftm_get_fname_resp = NULL;
    uint16 rsp_pkt_len;
    int fname_len = 1;  //allocate 1 byte of data in case of error
    byte err_code;

    if (bdf_file[0])
        fname_len = strlen(bdf_file);

    rsp_pkt_len = sizeof(ftm_get_fname_resp->common_header) +  sizeof(ftm_get_fname_resp->cmd.get_fname) + fname_len;
    ftm_get_fname_resp = (ftm_wlan_rsp_pkt_type *)diagpkt_subsys_alloc(DIAG_SUBSYS_FTM,
                                                                       FTM_WLAN_CMD_CODE,
                                                                       rsp_pkt_len
                                                                      );
    if (ftm_get_fname_resp == NULL) {
         DPRINTF(FTM_DBG_ERROR, "Failed to allocate Diag resp packet\n");
         return NULL;
    }

    if (!bdf_file[0] || strlen(bdf_file) > 128) {
        DPRINTF(FTM_DBG_TRACE, "ftm_daemon: get_filename failed: bdf_file:%s fname_len: %d\n", bdf_file, fname_len);
        err_code = WLAN_BDF_PATH_GET_FAILED;
        memset(ftm_get_fname_resp->cmd.get_fname.data, 0, 1);
    } else {
        DPRINTF(FTM_DBG_TRACE, "ftm_daemon: bdf_file is  %s\n", bdf_file);
        memcpy(ftm_get_fname_resp->cmd.get_fname.data, bdf_file, fname_len);
        err_code = WLAN_BDF_PATH_GET_SUCCESS;
    }

    ftm_get_fname_resp->common_header.cmd_id = htole16(FTM_WLAN_BDF_GET_FNAMEPATH);
    ftm_get_fname_resp->common_header.cmd_rsp_pkt_size = htole16(rsp_pkt_len);
    memset(ftm_get_fname_resp->cmd.get_fname.rsvd, 0, sizeof(ftm_get_fname_resp->cmd.get_fname.rsvd));
    ftm_get_fname_resp->cmd.get_fname.result = err_code;
    DPRINTF(FTM_DBG_TRACE, "ftm_daemon: get_filename resp msg dump:\n");
    print_uchar_array((uint8_t *)ftm_get_fname_resp, (int)rsp_pkt_len);

    return (void *)ftm_get_fname_resp;
}

/*===========================================================================
FUNCTION  ftm_wlan_bdf_set_filename

DESCRIPTION
  Set bdf_file path for further bdf file read/write operation

DEPENDENCIES
  NIL

RETURN VALUE
  Returns back buffer that is meant to be passed to the diag callback

SIDE EFFECTS
  NONE

===========================================================================*/
static void *ftm_wlan_bdf_set_filename(ftm_wlan_req_pkt_type *wlan_ftm_set_fname_pkt)
{
    ftm_wlan_rsp_pkt_type *ftm_set_fname_resp = NULL;
    uint16 rsp_pkt_len;
    uint16 size;
    byte err_code;

    rsp_pkt_len = sizeof(ftm_set_fname_resp->common_header) + sizeof(ftm_set_fname_resp->cmd.set_fname);
    size =  wlan_ftm_set_fname_pkt->cmd_data_len - rsp_pkt_len;
    rsp_pkt_len += 1;   //for 1 byte of reserved data
    ftm_set_fname_resp = (ftm_wlan_rsp_pkt_type *)diagpkt_subsys_alloc(DIAG_SUBSYS_FTM,
                                                                       FTM_WLAN_CMD_CODE,
                                                                       rsp_pkt_len
                                                                      );
    if (ftm_set_fname_resp == NULL) {
        DPRINTF(FTM_DBG_ERROR, "Failed to allocate Diag resp packet\n");
        return NULL;
    }
    ftm_set_fname_resp->common_header.cmd_id = htole16(FTM_WLAN_BDF_SET_FNAMEPATH);

    if (size == 0 || size >= 128) {
        DPRINTF(FTM_DBG_ERROR, "ftm_daemon: set_filename failed: fname_length %d\n", size);
        err_code = WLAN_BDF_PATH_SET_FAILED;
    } else {
        memcpy(bdf_file, wlan_ftm_set_fname_pkt->cmd.set_fname.data, size);
        bdf_file[size] = '\0';
        if (strncmp(bdf_file, "/firmware", 9) == 0) {
            DPRINTF(FTM_DBG_TRACE, "ftm_daemon: remount /firmware partition as rw\n");
            system("mount -o rw,remount /firmware /firmware");
        }
        DPRINTF(FTM_DBG_TRACE, "ftm_daemon: bdf_file set to %s\n", bdf_file);
        err_code = WLAN_BDF_PATH_SET_SUCCESS;
    }

    ftm_set_fname_resp->common_header.cmd_rsp_pkt_size = htole16(rsp_pkt_len);
    ftm_set_fname_resp->cmd.set_fname.result = err_code;
    memset(ftm_set_fname_resp->cmd.set_fname.rsvd, 0, sizeof(ftm_set_fname_resp->cmd.set_fname.rsvd));
    memset(ftm_set_fname_resp->cmd.set_fname.data, 0, 1);
    DPRINTF(FTM_DBG_TRACE, "ftm_daemon: set_filename resp msg dump:\n");
    print_uchar_array((uint8_t *)ftm_set_fname_resp, (int)rsp_pkt_len);
    return (void *)ftm_set_fname_resp;
}

/*===========================================================================
FUNCTION   ftm_wlan_bdf_get_max_transfer_size

DESCRIPTION
  Get maximum transfer size(in bytes) for further bdf file read/write operation

DEPENDENCIES
  NIL

RETURN VALUE
  Returns back buffer that is meant to be passed to the diag callback

SIDE EFFECTS
  NONE

===========================================================================*/
static void *ftm_wlan_bdf_get_max_transfer_size()
{
    ftm_wlan_rsp_pkt_type *ftm_get_max_size_resp = NULL;
    uint16 rsp_pkt_len;

    rsp_pkt_len = sizeof(ftm_get_max_size_resp->common_header) + sizeof(ftm_get_max_size_resp->cmd.get_max_transfer_size);
    ftm_get_max_size_resp = (ftm_wlan_rsp_pkt_type *)diagpkt_subsys_alloc(DIAG_SUBSYS_FTM,
                                                                          FTM_WLAN_CMD_CODE,
                                                                          rsp_pkt_len
                                                                          );
    if (ftm_get_max_size_resp == NULL) {
        DPRINTF(FTM_DBG_ERROR, "Failed to allocate Diag resp packet\n");
        return NULL;
     }
    ftm_get_max_size_resp->common_header.cmd_id = htole16(FTM_WLAN_BDF_GET_MAX_TRANSFER_SIZE);
    ftm_get_max_size_resp->common_header.cmd_rsp_pkt_size = htole16(rsp_pkt_len);
    ftm_get_max_size_resp->cmd.get_max_transfer_size.max_size = 1024;
    ftm_get_max_size_resp->cmd.get_max_transfer_size.result = 0;
    memset(ftm_get_max_size_resp->cmd.get_max_transfer_size.rsvd, 0,
           sizeof(ftm_get_max_size_resp->cmd.get_max_transfer_size.rsvd));
    DPRINTF(FTM_DBG_TRACE, "ftm_daemon: get_max_transfer_size resp msg dump:\n");
    print_uchar_array((uint8_t *)ftm_get_max_size_resp, (int)rsp_pkt_len);
    return (void *)ftm_get_max_size_resp;
 }

#ifdef CONFIG_FTM_WLAN_AUTOLOAD
/*===========================================================================
FUNCTION
  load_wifi_driver_testmode

DESCRIPTION
  Use system call to load driver

DEPENDENCIES
  NIL

RETURN VALUE
  NONE

SIDE EFFECTS
  NONE
===========================================================================*/

static int load_wifi_driver_testmode(void)
{
    int ret = 0;

    /* clean-up the driver state */
    if ((ret = unload_wifi_driver())) {
        DPRINTF(FTM_DBG_ERROR, "%s: Unload driver failed: %d\n", __func__, ret);
        return ret;
    }

#ifdef FTM_WLAN_LOAD_CMD
    if ((ret = system(FTM_WLAN_LOAD_CMD))) {
        DPRINTF(FTM_DBG_ERROR, "WLAN driver load failed!\n");
        return ret;
    }
#else
#error "FTM_WLAN_LOAD_CMD is not defined!"
#endif

    DPRINTF(FTM_DBG_TRACE, "WLAN driver loaded in FTM mode successfully!\n");

    return ret;
}
/*===========================================================================
FUNCTION
  Unload_wifi_driver if the drvier is detected existing already

DESCRIPTION
  Use system call to unload driver

DEPENDENCIES
  NIL

RETURN VALUE
  NONE

SIDE EFFECTS
  NONE
===========================================================================*/
static int unload_wifi_driver(void)
{
    int ret = 0;

    if (is_wifi_driver_loaded(DRIVER_MODULE_TAG)) {
#ifdef FTM_WLAN_UNLOAD_CMD
        if ((ret = system(FTM_WLAN_UNLOAD_CMD))) {
            DPRINTF(FTM_DBG_ERROR, "WLAN driver unload failed!\n");
            return ret;
        }
#else
#error "FTM_WLAN_UNLOAD_CMD is not defined!"
#endif
    }

    DPRINTF(FTM_DBG_TRACE, "WLAN driver unloaded successfully!\n");

    return ret;
}

/*===========================================================================
FUNCTION
  is_wifi_driver_loaded

DESCRIPTION
  Check if WLAN driver is loaded or not

DEPENDENCIES
  NIL

RETURN VALUE
  Returns true if driver already loaded, false if driver not loaded

SIDE EFFECTS
  NONE
===========================================================================*/
static bool is_wifi_driver_loaded(char *mod_tag)
{
    FILE *proc = NULL;
    char cmd[NAME_MAX + 64]; /* File name max + room for command */
    bool ret = false;

    snprintf(cmd, sizeof(cmd), "cat /proc/modules | grep %s", mod_tag);

    proc = popen(cmd, "r");

    if (proc == NULL) {
        DPRINTF(FTM_DBG_ERROR, "%s failed!\n", __func__);
        return ret;
    }

    if (fread (cmd, 1, sizeof(cmd), proc) > 0)
        ret = true;

    fclose(proc);

    return ret;
}
#endif /* CONFIG_FTM_WLAN_AUTOLOAD */

/*===========================================================================
FUNCTION   ftm_wlan_dispatch

DESCRIPTION
  WLAN FTM dispatch routine. Main entry point routine for WLAN FTM for
  AR6003

DEPENDENCIES
  NIL

RETURN VALUE
  Returns back buffer that is meant to be passed to the diag callback

SIDE EFFECTS
  NONE

===========================================================================*/
void* ftm_wlan_dispatch(ftm_wlan_req_pkt_type *wlan_ftm_pkt, int pkt_len)
{
    if (!wlan_ftm_pkt || !pkt_len) {
        DPRINTF(FTM_DBG_ERROR, "Invalid ftm wlan Requst Packet\n");
        return NULL;
    }
#ifdef DEBUG
    current_time();
#endif
    DPRINTF(FTM_DBG_TRACE, "ftm_daemon: cmd: %d\n", wlan_ftm_pkt->cmd_id);
    DPRINTF(FTM_DBG_TRACE, "ftm_daemon: Request Packet Dump:\n");
    print_uchar_array((uint8_t *)wlan_ftm_pkt, pkt_len);

    switch (le16toh(wlan_ftm_pkt->cmd_id)) {
    case FTM_WLAN_COMMON_OP:
        return ftm_wlan_common_op(wlan_ftm_pkt, pkt_len);
    case FTM_WLAN_BDF_GET_MAX_TRANSFER_SIZE:
        return ftm_wlan_bdf_get_max_transfer_size();
    case FTM_WLAN_BDF_READ:
        return ftm_wlan_bdf_read(wlan_ftm_pkt);
    case FTM_WLAN_BDF_WRITE:
        return ftm_wlan_bdf_write(wlan_ftm_pkt);
    case FTM_WLAN_BDF_GET_FNAMEPATH:
        return ftm_wlan_bdf_get_filename();
    case FTM_WLAN_BDF_SET_FNAMEPATH:
        return ftm_wlan_bdf_set_filename(wlan_ftm_pkt);
    default:
        DPRINTF(FTM_DBG_ERROR, " Unknown Command\n");
        return NULL;
    }
}



