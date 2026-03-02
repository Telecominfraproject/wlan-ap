/*=========================================================================
                       NQ NFC FTM Header File
Description
   This file contains the declarations of the functions
   used to communicate with the NQ Chip and various definitions.

Copyright (c) 2015-2017 Qualcomm Technologies, Inc.
All Rights Reserved.
Confidential and Proprietary - Qualcomm Technologies, Inc.

===========================================================================*/
/*===========================================================================
                         Edit History
when        who              what,              where,     why
--------   ---     ----------------------------------------------------------
===========================================================================*/

#ifndef _FTM_NFCNQ
#define _FTM_NFCNQ

#include "msg.h"
#include "diagpkt.h"
#include "diagcmd.h"
#include "errno.h"
#include <linux/ioctl.h>
#include <pthread.h>
#include <semaphore.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "log.h"

#define LOG_ERROR( ... )             printf( __VA_ARGS__ )
#define LOG_INFORMATION( ... )       printf( __VA_ARGS__ )

#ifdef NFC_FTM_DEBUG
#define LOG_MESSAGE( ... )           printf( __VA_ARGS__ )
#else
#define LOG_MESSAGE( ... )           do{ } while ( FALSE )
#endif

typedef PACKED struct _ftm_nfc_cmd_header{
    uint16                          nfc_cmd_id;
    uint16                          nfc_cmd_len;
} ftm_nfc_cmd_header, *pftm_nfc_cmd_header;

typedef PACKED struct{
    diagpkt_subsys_header_type      diag_hdr;
    ftm_nfc_cmd_header              ftm_nfc_hdr;
    uint16                          nfc_nci_pkt_len;
    byte                            nci_data[258];
}ftm_nfc_pkt_type, *pftm_nfc_pkt_type;

typedef PACKED struct{
    diagpkt_subsys_header_type      diag_hdr;
    uint16                          nfc_fwdl_cmd_id;
    byte                            nfc_fwdl_pkt_len;
    byte                            nfc_fwdl_pkt_data;
}ftm_nfc_fwdl_pkt_type, *pftm_nfc_fwdl_pkt_type;

typedef PACKED struct{
    diagpkt_subsys_header_type      diag_hdr;
    uint16                          nfc_chip_type_cmd_id;
    byte                            nfc_chip_type_pkt_len;
    byte                            nfc_chip_type_pkt_data;
}ftm_nfc_chip_type_pkt_type, *pftm_nfc_chip_type_pkt_type;

typedef PACKED struct{
    log_hdr_type                    hdr;
    byte                            data[1];
} ftm_nfc_log_pkt_type, *pftm_nfc_log_pkt_type;

typedef PACKED struct _NCI_MESSAGE
{
            byte                    gid;            // Group ID
            byte                    oid;            // Operation ID
            byte                    len;            // payload length in bytes
            byte                    buf[ 252 ];     // Payload Buffer
} NCI_MESSAGE, *PNCI_MESSAGE;

typedef enum
{
  NCIMT_DATA                      = 0x00, /**< DATA packet. */
  NCIMT_CMD                       = 0x20, /**< Control packet - Command. */
  NCIMT_RSP                       = 0x40, /**< Control packet - Response. */
  NCIMT_NTF                       = 0x60, /**< Control packet - Notification. */

  NCIMT_INVALID_VALUE             = 0xFF, /**< Invalid packet type. */

  NCIMT_BITMASK                   = 0xE0, /**< Most significant three bits. */
  NCIMT_BITSHIFT                  = 5

} NCIMT;

typedef enum
{
    UNKNOWN_NQ_CHIP_TYPE =  0,
    SKIP_CHIP_CHECK      =  1,
    NQ_110               = 11,
    NQ_120               = 12,
    NQ_210               = 21,
    NQ_220               = 22,
    NQ_310               = 31,
    NQ_330               = 33,
    MAXIMUM_NQ_CHIP_TYPE
} NQ_CHIP_TYPE;

struct nqx_devinfo
{
    unsigned char chip_type;
    unsigned char rom_version;
    unsigned char fw_major;
    unsigned char fw_minor;
};

union nqx_uinfo
{
    unsigned int i;
    struct nqx_devinfo info;
};

int     ftm_nq_nfc_open( void );
int     ftm_nq_nfc_close( void );
int     ftm_nfc_hw_reset( void );
int     ProcessCommand( uint8_t *nci_data );
void   *PrepareRsp( ftm_nfc_pkt_type *nfc_ftm_pkt );
void   *ftm_nfc_dispatch_nq( ftm_nfc_pkt_type *nfc_ftm_pkt, uint16 pkt_len);
void   *nfc_read_thread( void *arg );
extern sem_t               sRfNtf;
extern int          ese_dwp_test;
extern void printTecnologyDetails(char technology, char protocol);

#define FTM_NFC_CMD_CODE                            55
#define FTM_NFC_NFCC_COMMAND                        0x02
#define FTM_NFC_SEND_DATA                           0x03
#define FTM_NFC_REQ_CHIP_TYPE                       0x04
#define FTM_NFC_FWPIN_CTRL                          0x05
#define FTM_NFC_CMD_CMPL_TIMEOUT                    3

#define FTM_NFC_QTI_CHIP                            0x00
#define FTM_NFC_NQ_CHIP                             0x01
#define FTM_NFC_FWDL_SUCCESS                        0x01

#define MIN_CMD_PKT_LEN                             4           // Minimum length for a valid FTM packet, 2 bytes for Diag header, 2 bytes for command ID

#define LOG_NFC_FTM                                 0x1802
#define LOG_HEADER_LENGTH                           12

#define NFC_SET_PWR                                _IOW(0xE9, 0x01, unsigned int)
#define NFCC_GET_INFO                              _IOW(0xE9, 0x09, unsigned int)
#define POWER_OFF                                    0
#define POWER_ON                                     1
#define FIRMWARE_MODE                                2

#define EXPECTED_CORE_INIT_RSP_LEN                  29
#define CHIP_ID                                     24

#define SKIP_NQ_HARDWARE_CHECK                      "SkipNQHardwareCheck"
#define HARDWARE_TYPE_TIMEOUT                        2

#define UNUSED_PARAMETER( x )      ( void )( x )

#endif // _FTM_NFCNQ
