/*
*Copyright (c) 2017-2019 Qualcomm Technologies, Inc.
*
*All Rights Reserved.
*Confidential and Proprietary - Qualcomm Technologies, Inc.
*/

#ifndef __FTM_WLAN_WIN_H
#define __FTM_WLAN_WIN_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

#include <sys/ioctl.h>
#include <mtd/mtd-user.h>
#include "comdef.h"
#include "diagcmd.h"

#include "ftm_wlan.h"
#include "ftm_dbg.h"

#define MAC_XTAL_LENGTH 7
#define MAC_LENGTH_POS 103
#define MAC_POS 105
#define BT_TLV1_RESP_LEN 84
#define BT_RESP_LEN 100
#define FLASH_SECTOR_SIZE 0x10000
#define BD_LEN_EXPECTED 500
#define BD_SIZE_REQ_ID 106
#define BD_SIZE_REQ_POS 28
#define BD_SIZE_VAL 60

/* Identifier for first segment of
 * Board data response
 */
#define FIRST_SEG 48

/* Identifier for second segment of
 * board data response
 */
#define SECOND_SEG 49
#define THIRD_SEG 50
#define NO_ERROR 0

/* header length for first segment of
 * board data response
 */
#define FIRST_SEG_TLV_HDR 84
#define SECOND_SEG_TLV_HDR 28
#define THIRD_SEG_TLV_HDR 28

#define SEQUENCE_ID_POS 24

/* Position at which first parameter of
 * TLV request is located
 */
#define TLV_PAYLOAD_PARAM_1 80

/* Position at which second paramter of
 * TLV request is located
 */
#define TLV_PAYLOAD_PARAM_2 96

/* Parameter 1 value if request is for
 * board data capture
 */
#define BD_CAPTURE_REQ 101

/* Parameter 1 value if flash write request */
#define FLASH_WRITE_REQ 102

/* Parameter 1 value for device identify request */
#define DEVICE_IDENTIFY 103


/* Parameter 2 value for swift device identify */
#define QC9887_DEVICE_ID    0x50
#define QC9888_DEVICE_ID    0x3c
#define QC99xx_DEVICE_ID    0x46
#define QCN9000_DEVICE_ID   0x1104

#define TLV1_CMD_RESP_SIZE 118
#define TLV1_RESP_LEN 102

/* Offset at which BT_mac is to be stored in flash */
#define BT_MAC_OFFSET 0x40

#define FLASH_BASE_CALDATA_OFFSET_SOC_0 0x1000
#define FLASH_BASE_CALDATA_OFFSET_SOC_1 0x33000
#define REQ_SEG_SIZE 4096

#define FLASH_BASE_CALDATA_OFFSET_PCI_1 0x26800
#define FLASH_BASE_CALDATA_OFFSET_PCI_2 0x4C000

#define DIAG_HDR_LEN 16

#define FLASH_PARTITION "/dev/caldata"
#define VIRTUAL_FLASH_PARTITION "/tmp/virtual_art.bin"
#define WRITE_ART "/lib/compress_vart.sh write_caldata"

/* (0x33000-0x1000)=0x32000, Max available BDF size */
#define MAX_BDF_SIZE 200*1024

#define QC98XX_BLOCK_SIZE 512

#define BD_BLOCK_SIZE 256

/* Position of block size for the data */
#define QC98XX_BLOCK_SIZE_VAL 164

/* Position of block size for the data (radio != qc98XX) */
#define LEGACY_BLOCK_SIZE_VAL 100

#define M_EEEPROM_BLOCK_READ_ID_QC98XX 0xC8

#define M_EEEPROM_BLOCK_READ_ID_LEGACY 0xE9

/* Position where block data starts */
#define QC98XX_BLOCK_START_POS 200

#define LEGACY_BLOCK_START_POS 104

#define BD_READ_CMD_ID_POS 48

#define BD_READ_RESP_PARAM_POS 88

#define BD_READ_RESP_PARAM 0x7

/* Use of this parameter is not known */
#define LEGACY_RADIO_PARAM_POS 103

#define LEGACY_RADIO_PARAM_THRESHOLD 0x30

/* Valid caldata in each segment from FW */
#define CALDATA_SIZE_FIRST_SEG 1480
#define CALDATA_SIZE_SECOND_SEG 1536
#define CALDATA_SIZE_THIRD_SEG 1080


uint16_t TLV2_Specific_byte;

unsigned char BDbuffer[MAX_BDF_SIZE];
uint32_t BDbuffer_offset;
uint32_t resp_counter;
uint32_t bd_size;
uint8_t start_capture;

/* Deviceno is the instance id sent from
 * Qdart for the radio.
 */
int deviceno;

/* Device id received in the radio's
 * radio flash write requests, defaults to 0
 */
int deviceid = 0;

/* This is the remainder after whole 4096 size responses are sent */
uint32_t remaining_bytes ;
uint32_t total_4K_responses;

unsigned char BTsetmacResponse[] = {
    0x05, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x38, 0x00, 0x00, 0x00, 0x0F, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x09, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0xC6, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00,
    0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

/* Response sent for BDcapture and Flash write Requests */
unsigned char ftm_wlan_tlvRespMsg[] = {
    0x05, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x4A, 0x00, 0x00, 0x00,
    0x72, 0xD0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x09, 0x00, 0x00, 0x00,
    0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0xEA, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00,
    0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00,
    0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x02, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00,
    0x07, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x20, 0x2F
};

#endif /* __FTM_WLAN_WIN_H */

