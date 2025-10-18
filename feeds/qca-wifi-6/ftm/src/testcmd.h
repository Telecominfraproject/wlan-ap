/*==========================================================================

                     TCMD header File

# Copyright (c) 2011, 2013-2014 by Qualcomm Technologies, Inc.
# All Rights Reserved.
# Qualcomm Technologies Proprietary and Confidential.

===========================================================================*/

/*===========================================================================

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
//
 *
 */

#ifndef  TESTCMD_H_
#define  TESTCMD_H_

#include <stdint.h>

#ifdef AR6002_REV2
#define TCMD_MAX_RATES 12
#else
#define TCMD_MAX_RATES 28
#endif

#define PREPACK
#define POSTPACK __attribute__ ((packed))

#define ATH_MAC_LEN 6
#define TC_CMDS_SIZE_MAX  256

/* Continuous Rx
 act: TCMD_CONT_RX_PROMIS - promiscuous mode (accept all incoming frames)
      TCMD_CONT_RX_FILTER - filter mode (accept only frames with dest
                                             address equal specified
                                             mac address (set via act =3)
      TCMD_CONT_RX_REPORT  off mode  (disable cont rx mode and get the
                                          report from the last cont
                                          Rx test)

     TCMD_CONT_RX_SETMAC - set MacAddr mode (sets the MAC address for the
                                                 target. This Overrides
                                                 the default MAC address.)

*/
typedef enum {
	TCMD_CONT_RX_PROMIS = 0,
	TCMD_CONT_RX_FILTER,
	TCMD_CONT_RX_REPORT,
	TCMD_CONT_RX_SETMAC,
	TCMD_CONT_RX_SET_ANT_SWITCH_TABLE,
	TC_CMD_RESP,
	TCMD_CONT_RX_GETMAC,
} TCMD_CONT_RX_ACT;

typedef PREPACK struct {
	uint32_t testCmdId;
	uint32_t act;
	uint32_t enANI;
	PREPACK union {
		struct PREPACK TCMD_CONT_RX_PARA {
			uint32_t freq;
			uint32_t antenna;
			uint32_t wlanMode;
		} POSTPACK para;
		struct PREPACK TCMD_CONT_RX_REPORT {
			uint32_t totalPkt;
			int32_t rssiInDBm;
			uint32_t crcErrPkt;
			uint32_t secErrPkt;
			uint16_t rateCnt[TCMD_MAX_RATES];
			uint16_t rateCntShortGuard[TCMD_MAX_RATES];
		} POSTPACK report;
		struct PREPACK TCMD_CONT_RX_MAC {
			char addr[ATH_MAC_LEN];
			char btaddr[ATH_MAC_LEN];
                        uint16_t regDmn[2];
                        uint32_t otpWriteFlag;
		} POSTPACK mac;
		struct PREPACK TCMD_CONT_RX_ANT_SWITCH_TABLE {
			uint32_t antswitch1;
			uint32_t antswitch2;
		} POSTPACK antswitchtable;
	} POSTPACK u;
} POSTPACK TCMD_CONT_RX;

typedef enum {
    TC_CMDS_TS =0,
    TC_CMDS_CAL,
    TC_CMDS_TPCCAL = TC_CMDS_CAL,
    TC_CMDS_TPCCAL_WITH_OTPWRITE,
    TC_CMDS_OTPDUMP,
    TC_CMDS_OTPSTREAMWRITE,
    TC_CMDS_EFUSEDUMP,
    TC_CMDS_EFUSEWRITE,
    TC_CMDS_READTHERMAL,
} TC_CMDS_ACT;

typedef PREPACK struct {
    uint32_t   testCmdId;
    uint32_t   act;
    PREPACK union {
        uint32_t  enANI;    // to be identical to CONT_RX struct
        struct PREPACK {
            uint16_t   length;
            uint8_t    version;
            uint8_t    bufLen;
        } POSTPACK parm;
    } POSTPACK u;
} POSTPACK TC_CMDS_HDR;

typedef PREPACK struct {
    TC_CMDS_HDR  hdr;
    char buf[TC_CMDS_SIZE_MAX];
} POSTPACK TC_CMDS;

typedef enum {
	TCMD_CONT_TX_ID,
	TCMD_CONT_RX_ID,
	TCMD_PM_ID,
	TC_CMDS_ID,
	TCMD_SET_REG_ID,
	TC_CMD_TLV_ID,
	OP_GENERIC_NART_CMD = 8,

	/*For synergy purpose we added the following tcmd id but these
	tcmd's will not go to the firmware instead we will write values
	to the NV area */

	TCMD_NIC_MAC = 100,
	TCMD_CAL_FILE_INDEX = 101,
	TCMD_LOAD_DRIVER = 102,
	TCMD_SET_MAC_ADDR = 198,
} TCMD_ID;

#ifdef __cplusplus
}
#endif

#endif /* TESTCMD_H_ */
