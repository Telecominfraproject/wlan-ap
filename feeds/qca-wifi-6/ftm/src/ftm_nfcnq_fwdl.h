/*
 * Copyright (c) 2016 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 *
 * Not a Contribution.
 * Apache license notifications and license are retained
 * for attribution purposes only.
 */

/*
 * Copyright (C) 2015 NXP Semiconductors
 * The original Work has been changed by NXP Semiconductors.
 *
 * Copyright (C) 2010 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/*=========================================================================
  FTM NFC NQ Firmware Download Header File
  Description
  This file contains the declarations of the functions and various
  definitions used to download firmware onto the NQ Chip.
===========================================================================*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>
#include <semaphore.h>
#include <pthread.h>

#define FALSE   ( 0 )
#define TRUE    ( !FALSE )

#define FIRMWARE_DOWNLOAD_MAX_PACKET_LEN            ( 0x100U )      // maximum length for a download packet
#define FIRMWARE_DOWNLOAD_PACKET_HEADER_LEN         ( 0x02U )       // length of the header
#define FIRMWARE_DOWNLOAD_PACKET_CRC16_LEN          ( 0x02U )       // length of CRC-16-CCITT value
#define FIRMWARE_DOWNLOAD_PACKET_MAX_PAYLOAD_LEN     FIRMWARE_DOWNLOAD_MAX_PACKET_LEN - \
                                                     FIRMWARE_DOWNLOAD_PACKET_HEADER_LEN - \
                                                     FIRMWARE_DOWNLOAD_PACKET_CRC16_LEN

/*  Values for the first byte of each packet, indicates if the packet is fragmented */
#define FIRMWARE_DOWNLOAD_PACKET_FRAG_FLAG_NONE     ( 0x00U )       // not fragmented
#define FIRMWARE_DOWNLOAD_PACKET_FRAG_FLAG_SET      ( 0x04U )       // fragmented packet, next packet is a part of this one

extern sem_t        sRspReady;                                      // semaphore used by reader thread
extern int          fdNfc;                                          // a handle to the kernel driver

typedef uint8_t     bool_t;

/* structure of the packet to be sent or received */
typedef struct firmware_download_packet
{
    uint8_t     fFragmentedPacket;                                          // flag to indicate if the packet is fragmented
    uint8_t     payloadLen;                                                 // length of payload
    uint8_t     payloadBuff[ FIRMWARE_DOWNLOAD_PACKET_MAX_PAYLOAD_LEN +
                             FIRMWARE_DOWNLOAD_PACKET_CRC16_LEN ];          // buffer containing the payload and CRC-16-CCITT value
} firmware_download_packet_t, *pfirmware_download_packet_t;

/* structure that contains all the other information about the packets */
typedef struct firmware_download_context
{
    const uint8_t              *pFirmwareImage;                     // pointer to the firmware image library
    uint16_t                    firmwareImageLen;                   // length of the firmware image

    uint8_t                     headerPlusPayloadLen;               // header and payload length of a packet for CRC calculation
    uint16_t                    readIndexFromLib;                   // index used to read from the firmware library
    uint16_t                    bytesLeftToSend;                    // number of bytes left to send when the chunk read is fragmented
    uint16_t                    totalPacketLen;                     // total length of packet to be sent or received
    bool_t                      fFirstPacket;                       // flag to indicate if it is the first packet
    bool_t                      fExitReadThread;                    // flag to indicate if reader thread is safe to exit
    firmware_download_packet_t  packetToSend;                       // contains information about packet to be sent
    firmware_download_packet_t  packetReceived;                     // contains information about packet from response received
} firmware_download_context_t, *pfirmware_download_context_t;



/**

    Firmware download packet format

    -----------------------------------------------------------------------------------------------------
    |               Header                  |           Payload                 |   CRC-16-CCITT value  |
    -----------------------------------------------------------------------------------------------------
    |   Fragment flag   |   Payload length  |   Command/Response    |   Data    |   CRC-16-CCITT value  |
    -----------------------------------------------------------------------------------------------------
    |       1 byte      |       1 byte      |         1 byte        |  n bytes  |         2 bytes       |
    -----------------------------------------------------------------------------------------------------


    Firmware library image format

    ---------------------------------------------------------------------------------          ----------------------------------
    |   0x00    | First chunk length | First chunk | Next chunk length | Next chunk |    ...   | Last chunk length | Last chunk |
    ---------------------------------------------------------------------------------          ----------------------------------
    |  1 byte   |      1 byte        |   n bytes   |      2 bytes      |   n bytes  |    ...   |      2 bytes      |   n bytes  |
    ---------------------------------------------------------------------------------          ----------------------------------

*/
