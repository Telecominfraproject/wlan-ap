#ifndef DIAGCMD_H
#define DIAGCMD_H
/*!
@ingroup packet_service
@file diagcmd.h

@brief

 Diagnostic Services Packet Processing Command Code Defintions


 @details
  This file contains packet id definitions and enumeration constants for subsystem identifiers (diagpkt_subsys_cmd_enum_type)
  for the serial interface to the dmss.  All packets must have unique identifiers (command codes). Once published, an identifier cannot
  be changed. Subsystem identifiers (SSIDs) allow each technology area to define, grow, and maintain a list of unique packet identifiers
  without coordinating with each other. It is required that all clients of the diagnostic dispatching service use the subsystem commands.
  Subsystem Identifiers 250 to 254 are reserved for OEMs use only .Please refer to the documentation of 80-V1294-1 for the packet request/response
  defintions of each packet id .

 @note
  DO NOT MODIFY THIS FILE WITHOUT PRIOR APPROVAL
  Diag commands, by design, are a tightly controlled set of values.
  Developers may not create command IDs at will.
  Request new commands using the following process:

  1. Send email to asw.diag.request requesting command ID assignments.
  2. Identify the command needed by name.
  3. Provide a brief description for the command.

*/

/*
Copyright (c) 1993-2015 by Qualcomm Technologies, Incorporated.  All Rights Reserved.
*/



/*===========================================================================

                            Edit History

$Header: //source/qcom/qct/core/api/services/diag/main/latest/diagcmd.h#48 $

when       who     what, where, why
--------   ---     ----------------------------------------------------------
08/04/14   xy      Added DIAG_SUBSYS_AOSTLM_TEST
06/18/14   xy      Added DIAG_SUBSYS_STORAGE and DIAG_SUBSYS_WCI2
11/13/13   xy      Added DIAG_QSR4_EXT_MSG_TERSE_F
10/09/13   xy      Added DIAG_SUBSYS_IMS_QVP_RTP
09/25/13   xy      Added DIAG_SUBSYS_LWIP
08/26/13   sr      Added DIAG_SUBSYS_CNSS_POWER
06/26/13   sr      Added DIAG_SUBSYS_SYSTEM_OPERATIONS
05/31/13   sr      Removed the peek and poke cmds
05/08/13   sr      Added DIAG_SUBSYS_DS_IPA
03/22/13   sr      Added DIAG_SUBSYS_LIMITSMGR
10/23/12   sr      Added DIAG_SUBSYS_FTM_ANT
09/19/12   rh      Added DIAG_SUBSYS_TTLITE
09/17/12   rh      Added DIAG_SUBSYS_GNSS_SOC
08/13/12   rh      Added DIAG_SUBSYS_CXM
06/22/12   rh      Added subsystem ID for QDSS
06/06/12   rh      Added subsystem ID DIAG_SUBSYS_MPOWER
01/16/12   tbg     Added Secure Service Module error response
01/03/12   is      Add subsystem ID for TDSCDMA
10/21/11   rh      CMAPI subsystem id added
10/18/11   hm      New DCI commands added
07/01/11   hm      Allocated subsystem command codes for Flash
04/06/11   hm      Added subsystem id for 3GPP NAS team
02/25/11   hm      Added subsystem id for USCRIPT tool
01/18/11   hm      Added subsystem id for Q5 CORE
09/22/10   vg      Added subsystem id for STRIDE
07/15/10   sg      Added subsystem id for QNP
05/21/10   sg      Doxygenated the file
05/16/10   as      Added cmd_codes 101,102,105&106 for backward comparibility
04/21/10   sg      Added new SSID for Ulog Services
04/20/10   is    Remove support for DIAG_GET_PROPERTY_F, DIAG_PUT_PROPERTY_F,
                            DIAG_GET_PERM_PROPERTY_F, and DIAG_PUT_PERM_PROPERTY_F.
06/10/02   lcl/jwh FEATURE_HWTC changes.
05/23/02   sfh     Added DIAG_PROTOCOL_LOOPBACK_F  (123) command.
06/27/01   lad     Assigned equipment ID 0 to be for OEMs to use.
05/21/01   sfh     Added DIAG_TRACE_EVENT_REPORT_F for trace event support.
04/17/01   lad     Moved subsystem dispatch IDs from diagpkt.h.
04/06/01   lad     Changed the name of cmd code 111 from DUAG_TUNNEL_F to
                   DIAG_ROUTE_F.
02/23/01   lad     Cosmetic changes.
09/06/00   bgc     Added support for FEATURE_FACTORY_TESTMODE with
                   DIAG_FTM_CMD_F (set to 59, which is also DIAG_TMOB_F).
08/31/00   lad     Added command code for tunneling capability.
06/23/00   lad     Removed obsolete command codes and marked them "reserved".
06/19/00   lad     Added DIAG_PARM_RETRIEVE_F
05/31/00   jal     Added GPS statistics, session control, and grid support.
05/15/00   lad     Added streaming config support (nice).
02/28/00   lad     Added codes for event reporting service.
02/02/00   lad     Added commands used with FEATURE_DIAG_QCT_EXT.
09/17/99   lcc     Merged in RPC support from PLT archive.
08/17/99   tac     Merged in EFS changes from branch.
07/19/99    sk     Replacing reset_sup_fer with walsh code.
07/19/99    sk     Added walsh code display command.
03/30/99   lad     Added support for FEATURE_IS95B_MDR and FEATURE_SPECIAL_MDR.
11/04/98   lad     Added 1998/1999 copyright information.
10/29/98   jmk     Merged Module command changes into the mainline.
                   (Replaced MOD_GET_STATUS with MOD_EXTENDED_PKT cmd code 75)
10/27/98   jmk     Added cmd IDs for CSS command, and SMS message read/write.
09/11/98   grl     Added feature query command
10/06/97   jjn     Added new commands for the Module Phase 1 interface.  These
                   include Module Status Mask, AKEY and audio control packets.
04/23/97   jjn     Added new packet pair to allow System Unit to access
                   service option and caller ID information
03/25/97   jjn     Added new command (and packets) that allow writing to NV
                   without going offline (for the Module only)
02/28/97   jjn     Enabled RSSI packets for the Module, added a packet for
                   module status and sound reporting, and added a pcket for
                   retrieving SMS messages
06/25/96   jmk     Added cmd id for preferred roaming list read.
06/24/96   jmk     Added cmd id for preferred roaming list write.
04/09/96   jmk     Added cmd ids for sending security code, and return code
                   if phone is not unlocked for operations that require it.
03/06/96   jmk     Added command id for serial mode change (to AT cmd mode)
                   and command id for get rssi (for antenna aiming/WLL only)
08/16/95   jmk     Added command id for parm_get2 (includes MUX2 parms)
08/10/95   jmk     Added command id for Phone State, Pilot Sets and SPC reqs
01/28/95   ptw     Added command id to obtain System Time from the mobile.
12/07/94   jmk     Added command id for portable sleep on/off request.
11/07/94   jmk     Added command to request that seq_nums be used in pkts.
09/26/94   jmk     Put DIAG_ORIG_F and DIAG_END_F back in.
07/23/93   twp     Added DIAG_TMOB_F
01/14/93   twp     First release

===========================================================================*/

/*--------------------------------------------------------------------------

  Command Codes between the Diagnostic Monitor and the mobile. Packets
  travelling in each direction are defined here, while the packet templates
  for requests and responses are distinct.  Note that the same packet id
  value can be used for both a request and a response.  These values
  are used to index a dispatch table in diag.c, so

  DON'T CHANGE THE NUMBERS ( REPLACE UNUSED IDS WITH FILLERS ). NEW IDs
  MUST BE ASSIGNED AT THE END.

----------------------------------------------------------------------------*/

/*!
@cond DOXYGEN_BLOAT
*/
/* Version Number Request/Response            */
#define DIAG_VERNO_F    0

/* Mobile Station ESN Request/Response        */
#define DIAG_ESN_F      1

/* 2-11 Obsolete                 */


/* DMSS status Request/Response               */
#define DIAG_STATUS_F   12

/* 13-14 Reserved */

/* Set logging mask Request/Response          */
#define DIAG_LOGMASK_F  15

/* Log packet Request/Response                */
#define DIAG_LOG_F      16

/* Peek at NV memory Request/Response         */
#define DIAG_NV_PEEK_F  17

/* Poke at NV memory Request/Response         */
#define DIAG_NV_POKE_F  18

/* Invalid Command Response                   */
#define DIAG_BAD_CMD_F  19

/* Invalid parmaeter Response                 */
#define DIAG_BAD_PARM_F 20

/* Invalid packet length Response             */
#define DIAG_BAD_LEN_F  21

/* 22-23 Reserved */

/* Packet not allowed in this mode
   ( online vs offline )                      */
#define DIAG_BAD_MODE_F     24

/* info for TA power and voice graphs         */
#define DIAG_TAGRAPH_F      25

/* Markov statistics                          */
#define DIAG_MARKOV_F       26

/* Reset of Markov statistics                 */
#define DIAG_MARKOV_RESET_F 27

/* Return diag version for comparison to
   detect incompatabilities                   */
#define DIAG_DIAG_VER_F     28

/* Return a timestamp                         */
#define DIAG_TS_F           29

/* Set TA parameters                          */
#define DIAG_TA_PARM_F      30

/* Request for msg report                     */
#define DIAG_MSG_F          31

/* Handset Emulation -- keypress              */
#define DIAG_HS_KEY_F       32

/* Handset Emulation -- lock or unlock        */
#define DIAG_HS_LOCK_F      33

/* Handset Emulation -- display request       */
#define DIAG_HS_SCREEN_F    34

/* 35 Reserved */

/* Parameter Download                         */
#define DIAG_PARM_SET_F     36

/* 37 Reserved */

/* Read NV item                               */
#define DIAG_NV_READ_F  38
/* Write NV item                              */
#define DIAG_NV_WRITE_F 39
/* 40 Reserved */

/* Mode change request                        */
#define DIAG_CONTROL_F    41

/* Error record retreival                     */
#define DIAG_ERR_READ_F   42

/* Error record clear                         */
#define DIAG_ERR_CLEAR_F  43

/* Symbol error rate counter reset            */
#define DIAG_SER_RESET_F  44

/* Symbol error rate counter report           */
#define DIAG_SER_REPORT_F 45

/* Run a specified test                       */
#define DIAG_TEST_F       46

/* Retreive the current dip switch setting    */
#define DIAG_GET_DIPSW_F  47

/* Write new dip switch setting               */
#define DIAG_SET_DIPSW_F  48

/* Start/Stop Vocoder PCM loopback            */
#define DIAG_VOC_PCM_LB_F 49

/* Start/Stop Vocoder PKT loopback            */
#define DIAG_VOC_PKT_LB_F 50

/* 51-52 Reserved */

/* Originate a call                           */
#define DIAG_ORIG_F 53
/* End a call                                 */
#define DIAG_END_F  54
/* 55-57 Reserved */

/* Switch to downloader                       */
#define DIAG_DLOAD_F 58
/* Test Mode Commands and FTM commands        */
#define DIAG_TMOB_F  59
/* Test Mode Commands and FTM commands        */
#define DIAG_FTM_CMD_F  59
/* 60-62 Reserved */

/* Featurization Removal requested by CMI
#ifdef FEATURE_HWTC
*/

#define DIAG_TEST_STATE_F 61
/*
#endif
*/

/* Return the current state of the phone      */
#define DIAG_STATE_F        63

/* Return all current sets of pilots          */
#define DIAG_PILOT_SETS_F   64

/* Send the Service Prog. Code to allow SP    */
#define DIAG_SPC_F          65

/* Invalid nv_read/write because SP is locked */
#define DIAG_BAD_SPC_MODE_F 66

/* get parms obsoletes PARM_GET               */
#define DIAG_PARM_GET2_F    67

/* Serial mode change Request/Response        */
#define DIAG_SERIAL_CHG_F   68

/* 69 Reserved */

/* Send password to unlock secure operations
   the phone to be in a security state that
   is wasn't - like unlocked.                 */
#define DIAG_PASSWORD_F     70

/* An operation was attempted which required  */
#define DIAG_BAD_SEC_MODE_F 71

/* Write Preferred Roaming list to the phone. */
#define DIAG_PR_LIST_WR_F   72

/* Read Preferred Roaming list from the phone.*/
#define DIAG_PR_LIST_RD_F   73

/* 74 Reserved */

/* Subssytem dispatcher (extended diag cmd)   */
#define DIAG_SUBSYS_CMD_F   75

/* 76-80 Reserved */

/* Asks the phone what it supports            */
#define DIAG_FEATURE_QUERY_F   81

/* 82 Reserved */

/* Read SMS message out of NV                 */
#define DIAG_SMS_READ_F        83

/* Write SMS message into NV                  */
#define DIAG_SMS_WRITE_F       84

/* info for Frame Error Rate
   on multiple channels                       */
#define DIAG_SUP_FER_F         85

/* Supplemental channel walsh codes           */
#define DIAG_SUP_WALSH_CODES_F 86

/* Sets the maximum # supplemental
   channels                                   */
#define DIAG_SET_MAX_SUP_CH_F  87

/* get parms including SUPP and MUX2:
   obsoletes PARM_GET and PARM_GET_2          */
#define DIAG_PARM_GET_IS95B_F  88

/* Performs an Embedded File System
   (EFS) operation.                           */
#define DIAG_FS_OP_F           89

/* AKEY Verification.                         */
#define DIAG_AKEY_VERIFY_F     90

/* Handset emulation - Bitmap screen          */
#define DIAG_BMP_HS_SCREEN_F   91

/* Configure communications                   */
#define DIAG_CONFIG_COMM_F        92

/* Extended logmask for > 32 bits.            */
#define DIAG_EXT_LOGMASK_F        93

/* 94-95 reserved */

/* Static Event reporting.                    */
#define DIAG_EVENT_REPORT_F       96

/* Load balancing and more!                   */
#define DIAG_STREAMING_CONFIG_F   97

/* Parameter retrieval                        */
#define DIAG_PARM_RETRIEVE_F      98

 /* A state/status snapshot of the DMSS.      */
#define DIAG_STATUS_SNAPSHOT_F    99

/* 100 obsolete  */

/* Get_property requests                      */
#define DIAG_GET_PROPERTY_F      101

/* Put_property requests                      */
#define DIAG_PUT_PROPERTY_F      102

/* Get_guid requests                          */
#define DIAG_GET_GUID_F          103

/* Invocation of user callbacks               */
#define DIAG_USER_CMD_F          104

/* Get permanent properties                   */
#define DIAG_GET_PERM_PROPERTY_F 105

/* Put permanent properties                   */
#define DIAG_PUT_PERM_PROPERTY_F 106

/* Permanent user callbacks                   */
#define DIAG_PERM_USER_CMD_F     107

/* GPS Session Control                        */
#define DIAG_GPS_SESS_CTRL_F     108

/* GPS search grid                            */
#define DIAG_GPS_GRID_F          109

/* GPS Statistics                             */
#define DIAG_GPS_STATISTICS_F    110

/* Packet routing for multiple instances of diag */
#define DIAG_ROUTE_F             111

/* IS2000 status                              */
#define DIAG_IS2000_STATUS_F     112

/* RLP statistics reset                       */
#define DIAG_RLP_STAT_RESET_F    113

/* (S)TDSO statistics reset                   */
#define DIAG_TDSO_STAT_RESET_F   114

/* Logging configuration packet               */
#define DIAG_LOG_CONFIG_F        115

/* Static Trace Event reporting */
#define DIAG_TRACE_EVENT_REPORT_F 116

/* SBI Read */
#define DIAG_SBI_READ_F           117

/* SBI Write */
#define DIAG_SBI_WRITE_F          118

/* SSD Verify */
#define DIAG_SSD_VERIFY_F         119

/* Log on Request */
#define DIAG_LOG_ON_DEMAND_F      120

/* Request for extended msg report */
#define DIAG_EXT_MSG_F            121

/* ONCRPC diag packet */
#define DIAG_ONCRPC_F             122

/* Diagnostics protocol loopback. */
#define DIAG_PROTOCOL_LOOPBACK_F  123

/* Extended build ID text */
#define DIAG_EXT_BUILD_ID_F       124

/* Request for extended msg report */
#define DIAG_EXT_MSG_CONFIG_F     125

/* Extended messages in terse format */
#define DIAG_EXT_MSG_TERSE_F      126

/* Translate terse format message identifier */
#define DIAG_EXT_MSG_TERSE_XLATE_F 127

/* Subssytem dispatcher Version 2 (delayed response capable) */
#define DIAG_SUBSYS_CMD_VER_2_F    128

/* Get the event mask */
#define DIAG_EVENT_MASK_GET_F      129

/* Set the event mask */
#define DIAG_EVENT_MASK_SET_F      130

/* RESERVED CODES: 131-139 */

/* Command Code for Changing Port Settings */
#define DIAG_CHANGE_PORT_SETTINGS  140

/* Country network information for assisted dialing */
#define DIAG_CNTRY_INFO_F          141

/* Send a Supplementary Service Request */
#define DIAG_SUPS_REQ_F            142

/* Originate SMS request for MMS */
#define DIAG_MMS_ORIG_SMS_REQUEST_F 143

/* Change measurement mode*/
#define DIAG_MEAS_MODE_F           144

/* Request measurements for HDR channels */
#define DIAG_MEAS_REQ_F            145

/* Send Optimized F3 messages */
#define DIAG_QSR_EXT_MSG_TERSE_F   146

/* Packet ID for command/responses sent over DCI */
#define DIAG_DCI_CMD_REQ 147

/* Packet ID for delayed responses sent over DCI */
#define DIAG_DCI_DELAYED_RSP 148

/* Error response code on DCI (only APSS side) */
#define DIAG_BAD_TRANS_F 149

/* Error response code for cmomands disallowed by SSM */
#define DIAG_SSM_DISALLOWED_CMD_F 150

/* Log on extended Request */
#define DIAG_LOG_ON_DEMAND_EXT_F      151

/* Packet ID for extended event/log/F3 pkt */
#define DIAG_CMD_EXT_F      152

/*Qshrink4 command code for Qshrink 4 packet*/
#define DIAG_QSR4_EXT_MSG_TERSE_F   153

/*DCI command code for dci control packet*/
#define DIAG_DCI_CONTROL_PACKET   154

/*Compressed diag data which is sent out by the DMSS to the host.*/
#define DIAG_COMPRESSED_PKT   155


/* Number of packets defined. */
#define DIAG_MAX_F                 155



typedef enum {
  DIAG_SUBSYS_OEM                = 0,       /* Reserved for OEM use */
  DIAG_SUBSYS_ZREX               = 1,       /* ZREX */
  DIAG_SUBSYS_SD                 = 2,       /* System Determination */
  DIAG_SUBSYS_BT                 = 3,       /* Bluetooth */
  DIAG_SUBSYS_WCDMA              = 4,       /* WCDMA */
  DIAG_SUBSYS_HDR                = 5,       /* 1xEvDO */
  DIAG_SUBSYS_DIABLO             = 6,       /* DIABLO */
  DIAG_SUBSYS_TREX               = 7,       /* TREX - Off-target testing environments */
  DIAG_SUBSYS_GSM                = 8,       /* GSM */
  DIAG_SUBSYS_UMTS               = 9,       /* UMTS */
  DIAG_SUBSYS_HWTC               = 10,      /* HWTC */
  DIAG_SUBSYS_FTM                = 11,      /* Factory Test Mode */
  DIAG_SUBSYS_REX                = 12,      /* Rex */
  DIAG_SUBSYS_OS                 = DIAG_SUBSYS_REX,
  DIAG_SUBSYS_GPS                = 13,      /* Global Positioning System */
  DIAG_SUBSYS_WMS                = 14,      /* Wireless Messaging Service (WMS, SMS) */
  DIAG_SUBSYS_CM                 = 15,      /* Call Manager */
  DIAG_SUBSYS_HS                 = 16,      /* Handset */
  DIAG_SUBSYS_AUDIO_SETTINGS     = 17,      /* Audio Settings */
  DIAG_SUBSYS_DIAG_SERV          = 18,      /* DIAG Services */
  DIAG_SUBSYS_FS                 = 19,      /* File System - EFS2 */
  DIAG_SUBSYS_PORT_MAP_SETTINGS  = 20,      /* Port Map Settings */
  DIAG_SUBSYS_MEDIAPLAYER        = 21,      /* QCT Mediaplayer */
  DIAG_SUBSYS_QCAMERA            = 22,      /* QCT QCamera */
  DIAG_SUBSYS_MOBIMON            = 23,      /* QCT MobiMon */
  DIAG_SUBSYS_GUNIMON            = 24,      /* QCT GuniMon */
  DIAG_SUBSYS_LSM                = 25,      /* Location Services Manager */
  DIAG_SUBSYS_QCAMCORDER         = 26,      /* QCT QCamcorder */
  DIAG_SUBSYS_MUX1X              = 27,      /* Multiplexer */
  DIAG_SUBSYS_DATA1X             = 28,      /* Data */
  DIAG_SUBSYS_SRCH1X             = 29,      /* Searcher */
  DIAG_SUBSYS_CALLP1X            = 30,      /* Call Processor */
  DIAG_SUBSYS_APPS               = 31,      /* Applications */
  DIAG_SUBSYS_SETTINGS           = 32,      /* Settings */
  DIAG_SUBSYS_GSDI               = 33,      /* Generic SIM Driver Interface */
  DIAG_SUBSYS_UIMDIAG            = DIAG_SUBSYS_GSDI,
  DIAG_SUBSYS_TMC                = 34,      /* Task Main Controller */
  DIAG_SUBSYS_USB                = 35,      /* Universal Serial Bus */
  DIAG_SUBSYS_PM                 = 36,      /* Power Management */
  DIAG_SUBSYS_DEBUG              = 37,
  DIAG_SUBSYS_QTV                = 38,
  DIAG_SUBSYS_CLKRGM             = 39,      /* Clock Regime */
  DIAG_SUBSYS_DEVICES            = 40,
  DIAG_SUBSYS_WLAN               = 41,      /* 802.11 Technology */
  DIAG_SUBSYS_PS_DATA_LOGGING    = 42,      /* Data Path Logging */
  DIAG_SUBSYS_PS                 = DIAG_SUBSYS_PS_DATA_LOGGING,
  DIAG_SUBSYS_MFLO               = 43,      /* MediaFLO */
  DIAG_SUBSYS_DTV                = 44,      /* Digital TV */
  DIAG_SUBSYS_RRC                = 45,      /* WCDMA Radio Resource Control state */
  DIAG_SUBSYS_PROF               = 46,      /* Miscellaneous Profiling Related */
  DIAG_SUBSYS_TCXOMGR            = 47,
  DIAG_SUBSYS_NV                 = 48,      /* Non Volatile Memory */
  DIAG_SUBSYS_AUTOCONFIG         = 49,
  DIAG_SUBSYS_PARAMS             = 50,      /* Parameters required for debugging subsystems */
  DIAG_SUBSYS_MDDI               = 51,      /* Mobile Display Digital Interface */
  DIAG_SUBSYS_DS_ATCOP           = 52,
  DIAG_SUBSYS_L4LINUX            = 53,      /* L4/Linux */
  DIAG_SUBSYS_MVS                = 54,      /* Multimode Voice Services */
  DIAG_SUBSYS_CNV                = 55,      /* Compact NV */
  DIAG_SUBSYS_APIONE_PROGRAM     = 56,      /* apiOne */
  DIAG_SUBSYS_HIT                = 57,      /* Hardware Integration Test */
  DIAG_SUBSYS_DRM                = 58,      /* Digital Rights Management */
  DIAG_SUBSYS_DM                 = 59,      /* Device Management */
  DIAG_SUBSYS_FC                 = 60,      /* Flow Controller */
  DIAG_SUBSYS_MEMORY             = 61,      /* Malloc Manager */
  DIAG_SUBSYS_FS_ALTERNATE       = 62,      /* Alternate File System */
  DIAG_SUBSYS_REGRESSION         = 63,      /* Regression Test Commands */
  DIAG_SUBSYS_SENSORS            = 64,      /* The sensors subsystem */
  DIAG_SUBSYS_FLUTE              = 65,      /* FLUTE */
  DIAG_SUBSYS_ANALOG             = 66,      /* Analog die subsystem */
  DIAG_SUBSYS_APIONE_PROGRAM_MODEM = 67,    /* apiOne Program On Modem Processor */
  DIAG_SUBSYS_LTE                = 68,      /* LTE */
  DIAG_SUBSYS_BREW               = 69,      /* BREW */
  DIAG_SUBSYS_PWRDB              = 70,      /* Power Debug Tool */
  DIAG_SUBSYS_CHORD              = 71,      /* Chaos Coordinator */
  DIAG_SUBSYS_SEC                = 72,      /* Security */
  DIAG_SUBSYS_TIME               = 73,      /* Time Services */
  DIAG_SUBSYS_Q6_CORE            = 74,      /* Q6 core services */
  DIAG_SUBSYS_COREBSP	         = 75,      /* CoreBSP */
                                            /* Command code allocation:
                                                [0 - 2047]	- HWENGINES
                                                [2048 - 2147]	- MPROC
                                                [2148 - 2247]	- BUSES
                                                [2248 - 2347]	- USB
                                                [2348 - 2447]   - FLASH
                                                [2448 - 3447]   - UART
                                                [3448 - 3547]   - PRODUCTS
                                                [3547 - 65535]	- Reserved
                                            */

  DIAG_SUBSYS_MFLO2              = 76,      /* Media Flow */
                                            /* Command code allocation:
                                                [0 - 1023]       - APPs
                                                [1024 - 65535]   - Reserved
                                            */
  DIAG_SUBSYS_ULOG               = 77,  /* ULog Services */
  DIAG_SUBSYS_APR              = 78,  /* Asynchronous Packet Router (Yu, Andy)*/
  DIAG_SUBSYS_QNP    = 79 , /*QNP (Ravinder Are , Arun Harnoor)*/
  DIAG_SUBSYS_STRIDE    = 80 , /* Ivailo Petrov */
  DIAG_SUBSYS_OEMDPP    = 81 , /* to read/write calibration to DPP partition */
  DIAG_SUBSYS_Q5_CORE   = 82 , /* Requested by ADSP team */
  DIAG_SUBSYS_USCRIPT   = 83 , /* core/power team USCRIPT tool */
  DIAG_SUBSYS_NAS       = 84 , /* Requested by 3GPP NAS team */
  DIAG_SUBSYS_CMAPI     = 85 , /* Requested by CMAPI */
  DIAG_SUBSYS_SSM       = 86,
  DIAG_SUBSYS_TDSCDMA   = 87,  /* Requested by TDSCDMA team */
  DIAG_SUBSYS_SSM_TEST  = 88,
  DIAG_SUBSYS_MPOWER    = 89,  /* Requested by MPOWER team */
  DIAG_SUBSYS_QDSS      = 90,  /* For QDSS STM commands */
  DIAG_SUBSYS_CXM       = 91,
  DIAG_SUBSYS_GNSS_SOC  = 92,  /* Secondary GNSS system */
  DIAG_SUBSYS_TTLITE    = 93,
  DIAG_SUBSYS_FTM_ANT   = 94,
  DIAG_SUBSYS_MLOG      = 95,
  DIAG_SUBSYS_LIMITSMGR = 96,
  DIAG_SUBSYS_EFSMONITOR = 97,
  DIAG_SUBSYS_DISPLAY_CALIBRATION = 98,
  DIAG_SUBSYS_VERSION_REPORT = 99,
  DIAG_SUBSYS_DS_IPA = 100,
  DIAG_SUBSYS_SYSTEM_OPERATIONS   = 101,
  DIAG_SUBSYS_CNSS_POWER          = 102,
  DIAG_SUBSYS_LWIP                = 103,
  DIAG_SUBSYS_IMS_QVP_RTP         = 104,
  DIAG_SUBSYS_STORAGE             = 105,
  DIAG_SUBSYS_WCI2                = 106,
  DIAG_SUBSYS_AOSTLM_TEST         = 107,
  DIAG_SUBSYS_CFCM                = 108,
  DIAG_SUBSYS_CORE_SERVICES       = 109,
  DIAG_SUBSYS_CVD                 = 110,
  DIAG_SUBSYS_MCFG                = 111,
  DIAG_SUBSYS_MODEM_STRESSFW      = 112,
  DIAG_SUBSYS_LAST,

  /* Subsystem IDs reserved for OEM use */
  DIAG_SUBSYS_RESERVED_OEM_0     = 250,
  DIAG_SUBSYS_RESERVED_OEM_1     = 251,
  DIAG_SUBSYS_RESERVED_OEM_2     = 252,
  DIAG_SUBSYS_RESERVED_OEM_3     = 253,
  DIAG_SUBSYS_RESERVED_OEM_4     = 254,
  DIAG_SUBSYS_LEGACY             = 255
} diagpkt_subsys_cmd_enum_type;
/*!
@endcond
*/
#endif  /* DIAGCMD_H */



