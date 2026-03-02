#ifndef FTM_NFCQTI_H_
#define FTM_NFCQTI_H_
/*==========================================================================

                     nfc FTM header File

Description
   This file contains the decalarations used by ftm_nfc.c

Copyright (c) 2015 Qualcomm Technologies, Inc.
All Rights Reserved.
Confidential and Proprietary - Qualcomm Technologies, Inc.

===========================================================================*/

/*===========================================================================

                         Edit History


when        who              what,              where,     why
--------   ---     ----------------------------------------------------------
08/06/13                     NFC FTM layer
===========================================================================*/

#ifdef CONFIG_FTM_NFC

#include "stdio.h"
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <sys/types.h>
#include <hardware/nfc.h>
#include <hardware/hardware.h>
#include <malloc.h>
#include <string.h>
#include "msg.h"
#include "log.h"

#include "diag_lsm.h"
#include "diagpkt.h"
#include "diagcmd.h"
#include "diag.h"
#include "termios.h"

/*==========================================================================*
*                             Defnitions                                  *
*==========================================================================*/
#define FTM_MODE 1
#define TRUE                      1
#define FALSE                     0
#define FTM_MODE                  1
#define FTM_NFC_CMD_CODE          55
#define LOG_NFC_FTM               0x1802
#define FTM_NFC_LOG_HEADER_SIZE   12

#define FTM_NFC_I2C_SLAVE_WRITE   0x00
#define FTM_NFC_I2C_SLAVE_READ    0x01
#define FTM_NFC_NFCC_COMMAND      0x02
#define FTM_NFC_SEND_DATA         0x03

#define FTM_NFC_CMD_CMPL_TIMEOUT  15

#ifdef  ANDROID_M
#define NFC_NCI_HARDWARE_MODULE "nfc_nci.qc199x"
#else
#define NFC_NCI_HARDWARE_MODULE "nfc_nci"
#endif

enum
{
    NCI_HAL_INIT,
    NCI_HAL_WRITE,
    NCI_HAL_READ,
    NCI_HAL_DEINIT,
    NCI_HAL_ASYNC_LOG,
    NCI_HAL_ERROR
};
/*==========================================================================*
*                             Declarations                                  *
*==========================================================================*/
/* Reader thread handle */
pthread_t nfc_thread_handle;
sem_t semaphore_halcmd_complete;
sem_t semaphore_nfcftmcmd_complete;

/* structure that contains nfc cmd id and len
   part of the packet recieved from DIAG*/
PACKED struct ftm_nfc_cmd_header_type{
    uint16 nfc_cmd_id;
    uint16 nfc_cmd_len;
};

/* nfc FTM packet(for NCI cmd/rsp messages)*/
typedef PACKED struct{
    diagpkt_subsys_header_type diag_hdr;
    struct ftm_nfc_cmd_header_type ftm_nfc_hdr;
    uint16 nfc_nci_pkt_len;
    byte nci_data[258];
}ftm_nfc_pkt_type;

/* nfc FTM packet (for I2C write messgaes)*/
typedef PACKED struct{
    diagpkt_subsys_header_type diag_hdr;
    uint8 nfc_i2c_slave_status;
}ftm_nfc_i2c_write_rsp_pkt_type;

/* nfc FTM packet (for I2C read messgaes)*/
typedef PACKED struct{
    diagpkt_subsys_header_type diag_hdr;
    struct ftm_nfc_cmd_header_type ftm_nfc_hdr;
    uint8 nfc_i2c_slave_status;
    uint8 nfc_nb_reg_reads;
    byte i2c_reg_read_rsp[30];
}ftm_nfc_i2c_read_rsp_pkt_type;


typedef PACKED struct{
    diagpkt_subsys_header_type diag_hdr;
    struct ftm_nfc_cmd_header_type ftm_nfc_hdr;
}ftm_nfc_data_rsp_pkt_type;

typedef PACKED struct{
    log_hdr_type hdr;
    byte data[1];
}ftm_nfc_log_pkt_type;

/*Data buffer linked list*/
typedef struct asyncdata {
    uint8 *response_buff;
    uint8 async_datalen;
    struct asyncdata *next;
}asyncdata;

typedef void (tHAL_NFC_CBACK) (uint8 event, uint8 status);
typedef void (tHAL_NFC_DATA_CBACK) (uint16 data_len, uint8 *p_data);

void* ftm_nfc_dispatch_qti(ftm_nfc_pkt_type *ftm_nfc_pkt, uint16 pkt_len);

#endif /* CONFIG_FTM_NFC */
#endif /* FTM_NFCQTI_H_ */
