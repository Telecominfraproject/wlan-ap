/*==========================================================================

                     FTM BT Task Header File

Description
  Global Data declarations of the ftm bt component.

# Copyright (c) 2010-2011, 2013-2014 by Qualcomm Technologies, Inc.
# All Rights Reserved.
# Qualcomm Technologies Proprietary and Confidential.

===========================================================================*/

/*===========================================================================

                         Edit History


when       who     what, where, why
--------   ---     ----------------------------------------------------------
09/28/11   rrr      Moved peristent NV item related APIs to CPP,
                    for having BD address being programmed twice if previous
                    BD address was random generated.
09/03/11   agaja    Added support for NV_READ and NV_WRITE Commands to write
                    onto Persist File system
02/08/11   braghave Changes to read the HCI commands from a binary file for
		    non-Android case
06/18/10   rakeshk  Created a header file to hold the definitons for ftm bt
                    task
===========================================================================*/

#ifdef  CONFIG_FTM_BT

#include "diagpkt.h"
#include <sys/types.h>
#ifdef USE_LIBSOCCFG
#include "btqsocnvm.h"
#include "btqsocnvmutils.h"
#endif

/* -------------------------------------------------------------------------
** Definitions and Declarations
** ------------------------------------------------------------------------- */

#define FTM_BT_CMD_CODE 4          /* BT FTM Command code */
#define FTM_FM_CMD_CODE 28         /* FM FTM Command code */
#define HCI_EVT_HDR_SIZE  3
#define HCI_ACL_HDR_SIZE  5
#define PROTOCOL_BYTE_SIZE 1
#define HC_VS_MAX_CMD_EVENT  260
#define HC_VS_MAX_ACL  1200
#define FTM_BT_HCI_USER_CMD 0
#define BT_FTM_CMD_RSP_LEN 1100
#define FTM_BT_DRV_START_TEST 0xA

/* MACROS for pin connectivty test*/
#define BT_CMD_SLIM_TEST          0xBFAC
#define LOOP_BACK_EVT_OGF         0x02
#define LOOP_BACK_EVT_OCF         0x18
#define LOOP_BACK_EVT_STATUS      0x00
#define LOOP_BACK_EVT_OGF_BIT     0x04
#define LOOP_BACK_EVT_OCF_BIT     0x05
#define LOOP_BACK_EVT_STATUS_BIT  0x06


#define FTM_BT_LOG_HEADER_SIZE (sizeof(ftm_bt_log_pkt_type) - 1)


/* Vendor Specific command codes */
#define BT_QSOC_EDL_CMD_OPCODE             (0xFC00)
#define BT_QSOC_NVM_ACCESS_OPCODE          (0xFC0B)

#define BT_QSOC_EDL_CMD_CODE             (0x00)
#define BT_QSOC_NVM_ACCESS_CODE          (0x0B)
#define BT_QSOC_VS_EDL_APPVER_RESP   	(0x02)

#ifndef HC_VS_MAX_CMD_EVENT
#define HC_VS_MAX_CMD_EVENT  260
#endif /* HC_VS_MAX_CMD_EVENT */

#define BT_QSOC_MAX_NVM_CMD_SIZE     0x64  /* Maximum size config (NVM) cmd  */
#define BT_QSOC_MAX_BD_ADDRESS_SIZE  0x06  /**< Length of BT Address */

#ifndef HCI_CMD_HDR_SIZE
#define HCI_CMD_HDR_SIZE  4
#endif /* HCI_CMD_HDR_SIZE */

#ifndef HCI_EVT_HDR_SIZE
#define HCI_EVT_HDR_SIZE  3
#endif /* HCI_EVT_HDR_SIZE */

#define FTM_BT_LOG_PKT_ID 0x01


#define BT_HCI_CMD_PKT 0x01
#define BT_HCI_ACL_PKT 0x02
#define BT_HCI_EVT_PKT 0x04

#define BT_HCI_CMD_CMPLT_EVT 0x0E
#define FM_HCI_EVT_PKT 0x14
#define FM_HCI_CMD_PKT 0x11

extern int boardtype;

/* VS command structure */
typedef struct
{
  uint8  vs_cmd_len;
  uint8  vs_cmd_data[BT_QSOC_MAX_NVM_CMD_SIZE];
} bt_qsoc_cfg_tbl_struct_type;

/* First Commamd structure - Used to store the First command for later
*  processing
*/
struct first_cmd
{
  uint8 *cmd_buf;
  int cmd_len;
};

/* FTM Global State - Enum defines the various states of the FTM
* module
*/
typedef enum ftm_state
{
  FTM_SOC_NOT_INITIALISED,
  FTM_SOC_READ_APP_VER,
  FTM_SOC_READ_HW_VER,
  FTM_SOC_POKE8_TBL_INIT,
  FTM_SOC_DOWNLOAD_NVM,
  FTM_SOC_DOWNLOAD_NVM_EFS,
  FTM_SOC_SLEEP_DISABLE,
  FTM_SOC_RESET,
  FTM_SOC_INITIALISED
}ftm_state;
/* FTM CMD status */
typedef enum ftm_log_packet_type
{
  FTM_USER_CMD_PASS,
  FTM_USER_CMD_FAIL,
  FTM_HCI_EVENT
}ftm_log_packet_type;

/* FTM Log Packet - Used to send back the event of a HCI Command */
typedef PACKED struct
{
  log_hdr_type hdr;
  byte         data[1];         /* Variable length payload,
                                   look at FTM log id for contents */
} ftm_bt_log_pkt_type;


/* FTM (BT) PKT Header */
typedef PACKED struct
{
  word cmd_id;            /* command id (required) */
  word cmd_data_len;      /* request pkt data length, excluding the diag and ftm headers
                             (optional, set to 0 if not used)*/
  word cmd_rsp_pkt_size;  /* rsp pkt size, size of response pkt if different then req pkt
                             (optional, set to 0 if not used)*/
} ftm_bt_cmd_header_type;

/* Bluetooth FTM packet */
typedef PACKED struct
{
  diagpkt_subsys_header_type diag_hdr;
  ftm_bt_cmd_header_type     ftm_hdr;
  byte                       data[1];
} ftm_bt_pkt_type;

/* SoC Cfg open Struct*/
#ifdef USE_LIBSOCCFG
typedef struct
{
   bt_qsoc_config_params_struct_type run_time_params;
   bt_qsoc_enum_nvm_mode nvm_mode;
   bt_qsoc_enum_type soc_type;
}ftm_bt_soc_runtime_cfg_type;
#endif

/*===========================================================================
FUNCTION   ftm_bt_err_timedout

DESCRIPTION
  This routine triggers the shutdown of the HCI and Power resources in case
  a HCI command previously sent times out.

DEPENDENCIES
  NIL

RETURN VALUE
  RETURN NIL

SIDE EFFECTS
  NONE

===========================================================================*/
void ftm_bt_err_timedout();

/*===========================================================================
FUNCTION   ftm_bt_dispatch

DESCRIPTION
  Processes the BT FTM packet and dispatches the command to FTM HCI driver

DEPENDENCIES
  NIL

RETURN VALUE
  NIL,The error in the Command Processing is sent to the DIAG App on PC via
  log packets

SIDE EFFECTS
  None

===========================================================================*/
void ftm_bt_dispatch(void *ftm_bt_pkt ,int cmd_len );

/*===========================================================================
FUNCTION   bt_hci_send_ftm_cmd

DESCRIPTION
 Helper Routine to process the HCI cmd and invokes the sub routines to intialise
 the SoC if needed based on the state of the FTM module

DEPENDENCIES
  NIL

RETURN VALUE
  RETURN VALUE
  FALSE = failure, else TRUE

SIDE EFFECTS
  None

===========================================================================*/

boolean ftm_bt_hci_send_cmd
(
  uint8 * cmd_buf,   /* pointer to Cmd */
  uint16 cmd_len     /* Cmd length */
);

/*===========================================================================
FUNCTION   bt_hci_hal_vs_sendcmd

DESCRIPTION
 Helper Routine to process the VS HCI cmd and constucts the HCI packet before
 calling bt_hci_send_ftm_cmd routine

DEPENDENCIES
  NIL

RETURN VALUE
  RETURN VALUE
  FALSE = failure, else TRUE

SIDE EFFECTS
  None

===========================================================================*/
boolean ftm_bt_hci_hal_vs_sendcmd
(
uint16 opcode,       /* Opcode */
uint8  *pCmdBuffer,  /* Pointer to Payload*/
uint8  nSize         /* Cmd Size */
);

/*===========================================================================
FUNCTION   isLatestTarget

DESCRIPTION
For all the target/solution which has Bluedroid as stack and libbt-vendor as
vendor initialization component considered as latest target

DEPENDENCIES
  NIL

RETURN VALUE
  RETURN VALUE
  FALSE = failure, else TRUE

SIDE EFFECTS
  None

===========================================================================*/
boolean isLatestTarget();
char *get_current_time(void);
#endif /* CONFIG_FTM_BT */
