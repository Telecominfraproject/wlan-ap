/*
 *Copyright (c) 2018-2020 Qualcomm Technologies, Inc.
 *
 *All Rights Reserved.
 *Confidential and Proprietary - Qualcomm Technologies, Inc.
 */

/* IPQ-QCA402X specific file */
#ifdef IPQ_AP_HOST_IOT

#include <semaphore.h>
#include <time.h>
#include "diagpkt.h"
#include "log.h"

#define MFG_CMD_ID_BLE_HCI 4
#define MFG_CMD_ID_I15P4_HMI 5
#define MFG_CMD_ID_OTP_INVALID 256
#define MFG_CMD_ID_OTP_SET_BITS 257
#define MFG_CMD_ID_OTP_WRITE_BYTE 258
#define MFG_CMD_ID_OTP_READ_BYTE 259
#define MFG_CMD_ID_OTP_TLV_INIT 260
#define MFG_CMD_ID_OTP_TLV_READ 261
#define MFG_CMD_ID_OTP_TLV_WRITE 262
#define MFG_CMD_ID_OTP_TLV_STATUS 263
#define MFG_CMD_ID_OTP_TLV_DELETE 264

#define MFG_CMD_ID_RAWFLASH_INVALID 288
#define MFG_CMD_ID_RAWFLASH_CLEAR_BITS 289
#define MFG_CMD_ID_RAWFLASH_WRITE 290
#define MFG_CMD_ID_RAWFLASH_READ  291
#define MFG_CMD_ID_RAWFLASH_ERASE 292
#define MFG_CMD_ID_RAWFLASH_DISABLE_MFG 293

#define MFG_CMD_ID_FS_INVALID 304
#define MFG_CMD_ID_FS_READ 305
#define MFG_CMD_ID_FS_WRITE 306
#define MFG_CMD_ID_FS_DELETE 307
#define MFG_CMD_ID_FS_LIST_SETUP 308
#define MFG_CMD_ID_FS_LIST_NEXT 309
#define MFG_CMD_ID_FS_MOUNT 310
#define MFG_CMD_ID_FS_UNMOUNT 311

/* Add more MFG tool commands for QCA402x. These
command are interpreted internally within QCA402x */
#define MFG_CMD_ID_MISC_REBOOT 352
#define MFG_CMD_ID_MISC_ADDR_READ 353
#define MFG_CMD_ID_MISC_ADDR_WRITE 354
#define MFG_CMD_ID_MISC_HWSS_DONE 355
#define MFG_CMD_ID_MISC_XTAL_CAP_SET 356
#define MFG_CMD_ID_MISC_PART_SZ_GET 357

/* Add MFG tool command to enable flashing of QCA402x
by putting QCA402x in EDL mode and selecting USB mux
select option to tie USB port 81 on IPQ402x to QCA402x */
#define MFG_CMD_ID_MISC_PROG_MODE 358

/*Command to invalidate specified QCA402x Imageset */
#define MFG_CMD_ID_MISC_FWUP 359
/* Add MFG tool PROG_MODE subcommands to enable flashing
of QCA402x on IPQ807x. Interpretation of sub-commands is as
follows:

MFG_FLASH_ON - Put QCA402x into reset state, Put QCA402x in
EDL mode and enable USB port to be tied to QCA402x

MFG_FLASH_OFF - Pull QCA402x out of EDL mode and Pull QCA402x
out of reset

MFG_EDL_ON - Put QCA402x in EDL mode

MFG_FLASH_OFF - Pull QCA402x out of EDL mode

MFG_USB_ON - Enable USB port to be tied to QCA402x

MFG_USB_OFF - Enable USB port to be tied to IPQ807x

MFG_PROG_RESP - Expected response field
*/

enum flash_state {
    MFG_PROG_RESP,
    MFG_FLASH_ON,
    MFG_FLASH_OFF,
    MFG_EDL_ON,
    MFG_EDL_OFF,
    MFG_USB_ON,
    MFG_USB_OFF
};

typedef struct
{
    uint8 start;
    uint8 version;
    uint16 length;
} PACKED_STRUCT diag_nonhdlc_hdr_t;

typedef struct
{
    diag_nonhdlc_hdr_t hdr;
    byte payload[0];
} PACKED_STRUCT ftm_iot_req_pkt_type;

typedef struct
{
    log_hdr_type hdr;
    byte buf[1];
} PACKED_STRUCT ftm_bt_rsp_pkt_type;

/* Two semaphores are used to handle sequencing of requests, ack responses
and multiple asynchronous data responses from QCA402x */

sem_t iot_sem;
sem_t iot_sem_async;

int ftm_iot_cmd_code;
int ftm_iot_dut_interface_code;
int ftm_iot_reserved_code;
int interface;
int thread_stop;

void *ftm_iot_dispatch(void *iot_ftm_pkt, int pkt_len, void *hdl);
#ifdef IPQ_AP_HOST_IOT_QCA402X
void *ftm_iot_dispatch_qca402x(void *iot_ftm_pkt, int pkt_len, void *hdl);
void *iot_thr_func_qca402x(void *hdl);
#endif
#ifdef IPQ_AP_HOST_IOT_IPQ50XX
void *ftm_iot_dispatch_ipq50xx(void *iot_ftm_pkt, int pkt_len, int *hdl);
void *iot_thr_func_ipq50xx(void *hdl);
#endif
#endif /*ifdef IPQ_AP_HOST_IOT*/
