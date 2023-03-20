#ifndef MSGCFG_H
#define MSGCFG_H

#ifdef __cplusplus
extern "C" {
#endif

/*!
  @ingroup diag_message_service
  @file msgcfg.h
  @brief
  Configuration file for Diagnostic Message 2.0 Service
  @par
  MSG 2.0 offers the ability to utilize more build-time and run-time
  filtering of debug messages.
  @par
  This file contains definitions to identify subsystem IDs (SSIDs) and
  the ability to map legacy debug messages (MSG_LOW, MSG_MED, etc) to
  different SSIDs.  SSIDs are externally published and must not change
  once published.
  @par
  This file includes msgtgt.h, which is a target-specific file used to
  customize MSG 2.0's configuration.  It may specify SSID build masks,
  the default build mask (for undefined build masks), and other
  configurable features.  Defaults are listed in this file.
  @note
  All the SSIDs and default mask settings are not documented, for the sake of
  brevity.
*/

/*
Copyright (c) 2002-2016 by Qualcomm Technologies, Inc.
All Rights Reserved.
Qualcomm Technologies Confidential and Proprietary
*/

/*===========================================================================
                        EDIT HISTORY FOR FILE

  $Header: //source/qcom/qct/core/api/services/diag/main/latest/msgcfg.h#49 $

when       who     what, where, why
--------   ---     ----------------------------------------------------------
03/31/14   xy      Added new message SSIDs
01/29/14   xy      Added new message SSIDs
12/03/13   xy      Added new message SSIDs
09/25/13   xy      Added new message SSIDs
07/30/13   sr      Added new message SSIDs
06/26/13   sr      Added new message SSIDs
05/08/13   sr      Added new message SSIDs
03/22/13   sr      Added new message SSIDs
03/12/13   sr      Added new message SSIDs
01/22/13   sr      Added new message SSIDs
12/07/12   sr      Added new message SSIDs
11/27/12   sr      Added new message SSIDs
10/15/12   sr      Added new message SSIDs for QCNEA group
07/16/12   rh      Added new message SSIDs
06/22/12   rh      Added new message SSIDs
02/21/12   is      Add MSG_BUILD_MASK_LEGACY catergory to support F3 listener testing
02/29/12   rh      Added new message SSIDs
02/17/12   rs      Added MSG_SSID_SEC_WIDEVINE in Security category
01/05/12   rh      Added QCNEA SSIDs
12/08/11   rh      Added MSG_SSID_ADC
11/29/11   rh      Added SSID category for CTA
10/18/11   hm      Renamed reserved MCS SSIDs
09/01/11   hm      Added new WCDMA and TDSCDMA SSIDs
08/05/11   hm      Added new SSID
07/01/11   hm      New SSID added
04/26/11   is      Resolve modem compilation issue
04/25/11   hm      Added new SSID
04/05/11   hm      Added new SSID for PPM module
03/24/11   hm      Reverted Octopus Changes and added new QCHAT SSIDs
03/07/11   hm      Added new set of SSIDs for Octopus
07/27/10   sg      Added new SSID for Multimedia team
07/07/10   sg      Added new SSID for Data Services
07/06/10   sg      Changed MSG_SSID_MCS_RESERVED_1 to  MSG_SSID_FWS
06/10/10   mad     Doxygenated
05/05/10   sg      Added new SSID MSG_SSID_CFM
04/27/10   sg      Added new SSIDS for Sound Routing Driver , DAL
04/20/10   sg      Added new SSIDS for Audio Team
04/09/10   sg      Added new SSIDS for OMA device management
                   Secure Instant Wireless Access
04/02/10   sg      Cleaning up the msg.h inclusion
03/04/10   sg      Added new SSID for IMS team
02/22/10   sg      Added new SSIDs for Data Services
02/08/10   sg      Added new SSIDs for IMS team
01/13/10   sg      Added new SSID for CAD team
12/22/09   sg      Moved MSG_MASK_TBL_CNT to msgcfg.h
12/22/09   sg      Added New SSID for Connectivity Engine Team
10/28/09   sg      Added New SSID for Chaos CoOrdinator Service
10/22/09   sg      Added new SSID for  DS_MUX
09/29/09   mad     Removed MSG_TBL_GEN feature. Moved all array definitions
                   to an internal header file, msg_arrays_i.h
09/23/09   sg      Added new SSID  for the ECALL feature
09/02/09   JV      Added new SSIDs for the HDR team
08/04/09   JV      Support for compiling on C++
07/27/09   JV      New SSID for ANDROID data and DS apps
07/16/09   mad     Mainlined FEATUREs: IS2000, HDR, WCDMA, GSM, WLAN, DS,
                   DATA, HIT. msg_mask_tbl contents are now free of the above
                   external featurizations.
06/11/09   JV      New SSID for data services
05/11/09   JV      New SSIDs for Android QCRIL and A2 modules
04/23/09   JV      Added SSIDs for the WLAN libra module.
04/10/09   JV      Added SSIDs for MCS.
03/16/09   mad     Featurized inclusion of customer.h
09/11/08   sj      Included size for table and Fixed orphan file issue by FTM
12/15/06   as      Fixed compiler warnings.
12/14/04   as      Reallocated SSID's used by WinCE to L4LINUX.
03/07/03   lad     Initial SSID deployment.
12/03/02   lad     Created file.

===========================================================================*/
#include "comdef.h"

#include "msgtgt.h" /* Target-specific MSG config info, such as build masks */

/*!
@ingroup diag_message_service
@name Some constants used in MSG macro implementation
*/
/*@{*/ /* start group Some constants used in MSG macro implementation */
/*!
This constant specifies the default maximum file and format string length.
   It should be arbitrarily large as it is only used to protect against
   invalid string pointers. */
#ifndef MSG_MAX_STRLEN
  #define MSG_MAX_STRLEN 512
#endif

/*! Default build mask for all SSIDs may be specified in msgtgt.h.
    MSG_LEVEL is the minimum message priority which can be logged. If MSG_LEVEL
    is not defined, default is NONE.  If MSG_LEVEL is set to MSG_LVL_NONE, then
    there will be no calls to msg_send() etc.
*/
#ifndef MSG_BUILD_MASK_DFLT
  #ifdef MSG_LEVEL
    #define MSG_BUILD_MASK_DFLT MSG_LEVEL
  #else
    #define MSG_BUILD_MASK_DFLT 0
  #endif
#endif

#ifndef MSG_LEVEL
  #define MSG_LEVEL MSG_LVL_NONE
#endif

#ifndef MSG_BUILD_MASK_LEGACY
  #define MSG_BUILD_MASK_LEGACY 0
#endif

#ifndef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_DFLT
#endif

#ifndef MSG_BUILD_MASK_MSG_SSID_LEGACY
  #define MSG_BUILD_MASK_MSG_SSID_LEGACY MSG_BUILD_MASK_LEGACY
#endif


/*@}*/ /* start group Some constants used in MSG macro implementation */

/*!
@cond DOXYGEN_BLOAT
*/
/*---------------------------------------------------------------------------
  This section contains configuration entries for all SSIDs.

  Build-time filtering is accomplsihed using a name hash in a macro.  As a
  result, naming convention must be adhered to.

  SSIDs must be common and unique across all targets.  The SSIDs and the
  meaning of the mask bits for the SSID are to be published in an externalized
  interface.  They cannot be change once published.

  Not all targets support all SSIDs.

  MSG_SSID_DFLT is used to expand legacy macros (MSG_LOW, MSG_MED, e.t.c.).
  Legacy macros may be mapped to another SSID (listed here) via the build
  environment.  For each invocation of the compiler, MSG_BT_SSID_LEGACY may
  be specified to override the default SSID for legacy macros (0).

---------------------------------------------------------------------------*/

#define MSG_SSID_GEN_FIRST  0
  /* Legacy messages may be mapped to a different SSID
     by the build environment. */
  #define MSG_SSID_DFLT     0
  #define MSG_SSID_LEGACY   0
  #define MSG_SSID_AUDFMT   1
  #define MSG_SSID_AVS      2
  #define MSG_SSID_BOOT     3
  #define MSG_SSID_BT       4
  #define MSG_SSID_CM       5
  #define MSG_SSID_CMX      6
  #define MSG_SSID_DIAG     7
  #define MSG_SSID_DSM      8
  #define MSG_SSID_FS       9
  #define MSG_SSID_HS      10
  #define MSG_SSID_MDSP    11
  #define MSG_SSID_QDSP    12
  #define MSG_SSID_REX     13
  #define MSG_SSID_RF      14
  #define MSG_SSID_SD      15
  #define MSG_SSID_SIO     16
  #define MSG_SSID_VS      17
  #define MSG_SSID_WMS     18
  #define MSG_SSID_GPS     19
  #define MSG_SSID_MMOC    20
  #define MSG_SSID_RUIM    21
  #define MSG_SSID_TMC     22
  #define MSG_SSID_FTM     23
  #define MSG_SSID_MMGPS   24
  #define MSG_SSID_SLEEP   25
  #define MSG_SSID_SAM     26
  #define MSG_SSID_SRM     27
  #define MSG_SSID_SFAT    28
  #define MSG_SSID_JOYST   29
  #define MSG_SSID_MFLO    30
  #define MSG_SSID_DTV     31
  #define MSG_SSID_TCXOMGR 32
  #define MSG_SSID_EFS     33
  #define MSG_SSID_IRDA    34
  #define MSG_SSID_FM_RADIO 35
  #define MSG_SSID_AAM     36
  #define MSG_SSID_BM      37
  #define MSG_SSID_PE      38
  #define MSG_SSID_QIPCALL 39
  #define MSG_SSID_FLUTE   40
  #define MSG_SSID_CAMERA  41
  #define MSG_SSID_HSUSB   42
  #define MSG_SSID_FC      43
  #define MSG_SSID_USBHOST 44
  #define MSG_SSID_PROFILER 45
  #define MSG_SSID_MGP     46
  #define MSG_SSID_MGPME   47
  #define MSG_SSID_GPSOS   48
  #define MSG_SSID_MGPPE   49
  #define MSG_SSID_GPSSM   50
  #define MSG_SSID_IMS     51
  #define MSG_SSID_MBP_RF  52
  #define MSG_SSID_SNS     53
  #define MSG_SSID_WM      54
  #define MSG_SSID_LK      55
  #define MSG_SSID_PWRDB   56
  #define MSG_SSID_DCVS    57
  #define MSG_SSID_ANDROID_ADB    58
  #define MSG_SSID_VIDEO_ENCODER  59
  #define MSG_SSID_VENC_OMX       60
  #define MSG_SSID_GAN            61 /* Generic Access Network */
  #define MSG_SSID_KINETO_GAN     62
  #define MSG_SSID_ANDROID_QCRIL  63
  #define MSG_SSID_A2             64
  #define MSG_SSID_LINUX_DATA   65
  #define MSG_SSID_ECALL        66
  #define MSG_SSID_CHORD        67
  #define MSG_SSID_QCNE         68
  #define MSG_SSID_APPS_CAD_GENERAL 69
  #define MSG_SSID_OMADM     70 /* OMA device management */
  #define MSG_SSID_SIWA      71 /* Secure Instant Wireless Access */
  #define MSG_SSID_APR_MODEM 72 /* Audio Packet Router Modem */
  #define MSG_SSID_APR_APPS  73 /* Audio Packet Router Apps*/
  #define MSG_SSID_APR_ADSP  74 /* Audio Packet Router Adsp*/
  #define MSG_SSID_SRD_GENERAL 75
  #define MSG_SSID_ACDB_GENERAL 76
  #define MSG_SSID_DALTF  77  /* DAL Test Frame Work */
  #define MSG_SSID_CFM    78 /* Centralized Flow Control Manager */
  #define MSG_SSID_PMIC    79 /* PMIC SSID */
  #define MSG_SSID_GPS_SDP    80
  #define MSG_SSID_TLE    81
  #define MSG_SSID_TLE_XTM    82
  #define MSG_SSID_TLE_TLM    83
  #define MSG_SSID_TLE_TLM_MM 84
  #define MSG_SSID_WWAN_LOC   85
  #define MSG_SSID_GNSS_LOCMW 86
  #define MSG_SSID_QSET       87
  #define MSG_SSID_QBI        88
  #define MSG_SSID_ADC        89
  #define MSG_SSID_MMODE_QMI  90
  #define MSG_SSID_MCFG       91
  #define MSG_SSID_SSM        92
  #define MSG_SSID_MPOWER     93
  #define MSG_SSID_RMTS       94
  #define MSG_SSID_ADIE       95
  #define MSG_SSID_VT_VCEL    96
  #define MSG_SSID_FLASH_SCRUB 97
  #define MSG_SSID_STRIDE     98
  #define MSG_SSID_POLICYMAN  99
  #define MSG_SSID_TMS        100
  #define MSG_SSID_LWIP       101
  #define MSG_SSID_RFS        102
  #define MSG_SSID_RFS_ACCESS 103
  #define MSG_SSID_RLC        104
  #define MSG_SSID_MEMHEAP    105
  #define MSG_SSID_WCI2       106
  #define MSG_SSID_LOWI_TEST  107
  #define MSG_SSID_AOSTLM     108
  #define MSG_SSID_LOWI_AP    109
  #define MSG_SSID_LOWI_MP    110
  #define MSG_SSID_LOWI_LP    111
  #define MSG_SSID_MRE        112
  #define MSG_SSID_SLIM       113
  #define MSG_SSID_WLE        114
  #define MSG_SSID_WLM        115
  #define MSG_SSID_Q6ZIP      116
  #define MSG_SSID_RF_DEBUG   117
  #define MSG_SSID_NV         118

#define MSG_SSID_GEN_LAST   118


/* Messages arising from ONCRPC AMSS modules */
#define MSG_SSID_ONCRPC             500
#define MSG_SSID_ONCRPC_MISC_MODEM  501
#define MSG_SSID_ONCRPC_MISC_APPS   502
#define MSG_SSID_ONCRPC_CM_MODEM    503
#define MSG_SSID_ONCRPC_CM_APPS     504
#define MSG_SSID_ONCRPC_DB          505
#define MSG_SSID_ONCRPC_SND         506
#define MSG_SSID_ONCRPC_LAST        506

/* Default master category for 1X. */
#define MSG_SSID_1X             1000
  #define MSG_SSID_1X_ACP       1001
  #define MSG_SSID_1X_DCP       1002
  #define MSG_SSID_1X_DEC       1003
  #define MSG_SSID_1X_ENC       1004
  #define MSG_SSID_1X_GPSSRCH   1005
  #define MSG_SSID_1X_MUX       1006
  #define MSG_SSID_1X_SRCH      1007
#define MSG_SSID_1X_LAST        1007


/* Default master category for HDR. */
#define MSG_SSID_HDR_PROT      2000
  #define MSG_SSID_HDR_DATA    2001
  #define MSG_SSID_HDR_SRCH    2002
  #define MSG_SSID_HDR_DRIVERS 2003
  #define MSG_SSID_HDR_IS890   2004
  #define MSG_SSID_HDR_DEBUG   2005
  #define MSG_SSID_HDR_HIT     2006
  #define MSG_SSID_HDR_PCP     2007
  #define MSG_SSID_HDR_HEAPMEM 2008
#define MSG_SSID_HDR_LAST      2008


/* Default master category for UMTS. */
#define MSG_SSID_UMTS           3000
  #define MSG_SSID_WCDMA_L1     3001
  #define MSG_SSID_WCDMA_L2     3002
  #define MSG_SSID_WCDMA_MAC    3003
  #define MSG_SSID_WCDMA_RLC    3004
  #define MSG_SSID_WCDMA_RRC    3005
  #define MSG_SSID_NAS_CNM      3006
  #define MSG_SSID_NAS_MM       3007
  #define MSG_SSID_NAS_MN       3008
  #define MSG_SSID_NAS_RABM     3009
  #define MSG_SSID_NAS_REG      3010
  #define MSG_SSID_NAS_SM       3011
  #define MSG_SSID_NAS_TC       3012
  #define MSG_SSID_NAS_CB       3013
  #define MSG_SSID_WCDMA_LEVEL  3014
#define MSG_SSID_UMTS_LAST      3014


/* Default master category for GSM. */
#define MSG_SSID_GSM                4000
  #define MSG_SSID_GSM_L1           4001
  #define MSG_SSID_GSM_L2           4002
  #define MSG_SSID_GSM_RR           4003
  #define MSG_SSID_GSM_GPRS_GCOMMON 4004
  #define MSG_SSID_GSM_GPRS_GLLC    4005
  #define MSG_SSID_GSM_GPRS_GMAC    4006
  #define MSG_SSID_GSM_GPRS_GPL1    4007
  #define MSG_SSID_GSM_GPRS_GRLC    4008
  #define MSG_SSID_GSM_GPRS_GRR     4009
  #define MSG_SSID_GSM_GPRS_GSNDCP  4010
#define MSG_SSID_GSM_LAST           4010



#define MSG_SSID_WLAN           4500
  #define MSG_SSID_WLAN_ADP     4501
  #define MSG_SSID_WLAN_CP      4502
  #define MSG_SSID_WLAN_FTM     4503
  #define MSG_SSID_WLAN_OEM     4504
  #define MSG_SSID_WLAN_SEC     4505
  #define MSG_SSID_WLAN_TRP     4506
  #define MSG_SSID_WLAN_RESERVED_1  4507
  #define MSG_SSID_WLAN_RESERVED_2  4508
  #define MSG_SSID_WLAN_RESERVED_3  4509
  #define MSG_SSID_WLAN_RESERVED_4  4510
  #define MSG_SSID_WLAN_RESERVED_5  4511
  #define MSG_SSID_WLAN_RESERVED_6  4512
  #define MSG_SSID_WLAN_RESERVED_7  4513
  #define MSG_SSID_WLAN_RESERVED_8  4514
  #define MSG_SSID_WLAN_RESERVED_9  4515
  #define MSG_SSID_WLAN_RESERVED_10 4516
  #define MSG_SSID_WLAN_TL      4517
  #define MSG_SSID_WLAN_BAL     4518
  #define MSG_SSID_WLAN_SAL     4519
  #define MSG_SSID_WLAN_SSC     4520
  #define MSG_SSID_WLAN_HDD     4521
  #define MSG_SSID_WLAN_SME     4522
  #define MSG_SSID_WLAN_PE      4523
  #define MSG_SSID_WLAN_HAL     4524
  #define MSG_SSID_WLAN_SYS     4525
  #define MSG_SSID_WLAN_VOSS    4526
  #define MSG_SSID_WLAN_ATHOS       4527
  #define MSG_SSID_WLAN_WMI         4528
  #define MSG_SSID_WLAN_HTT         4529
  #define MSG_SSID_WLAN_PS_STA      4530
  #define MSG_SSID_WLAN_PS_IBSS     4531
  #define MSG_SSID_WLAN_PS_AP       4532
  #define MSG_SSID_WLAN_SMPS_STA    4533
  #define MSG_SSID_WLAN_WHAL        4534
  #define MSG_SSID_WLAN_COEX        4535
  #define MSG_SSID_WLAN_ROAM        4536
  #define MSG_SSID_WLAN_RESMGR      4537
  #define MSG_SSID_WLAN_PROTO       4538
  #define MSG_SSID_WLAN_SCAN        4539
  #define MSG_SSID_WLAN_BATCH_SCAN  4540
  #define MSG_SSID_WLAN_EXTSCAN     4541
  #define MSG_SSID_WLAN_RC          4542
  #define MSG_SSID_WLAN_BLOCKACK    4543
  #define MSG_SSID_WLAN_TXRX_DATA   4544
  #define MSG_SSID_WLAN_TXRX_MGMT   4545
  #define MSG_SSID_WLAN_BEACON      4546
  #define MSG_SSID_WLAN_OFFLOAD_MGR 4547
  #define MSG_SSID_WLAN_MACCORE     4548
  #define MSG_SSID_WLAN_PCIELP      4549
  #define MSG_SSID_WLAN_RTT         4550
  #define MSG_SSID_WLAN_DCS         4551
  #define MSG_SSID_WLAN_CACHEMGR    4552
  #define MSG_SSID_WLAN_ANI         4553
  #define MSG_SSID_WLAN_P2P         4554
  #define MSG_SSID_WLAN_CSA         4555
  #define MSG_SSID_WLAN_NLO         4556
  #define MSG_SSID_WLAN_CHATTER     4557
  #define MSG_SSID_WLAN_WOW         4558
  #define MSG_SSID_WLAN_WMMAC       4559
  #define MSG_SSID_WLAN_TDLS        4560
  #define MSG_SSID_WLAN_HB          4561
  #define MSG_SSID_WLAN_TXBF        4562
  #define MSG_SSID_WLAN_THERMAL     4563
  #define MSG_SSID_WLAN_DFS         4564
  #define MSG_SSID_WLAN_RMC         4565
  #define MSG_SSID_WLAN_STATS       4566
  #define MSG_SSID_WLAN_NAN         4567
  #define MSG_SSID_WLAN_HIF_UART    4568
  #define MSG_SSID_WLAN_LPI         4569
  #define MSG_SSID_WLAN_MLME        4570
  #define MSG_SSID_WLAN_SUPPL       4571
  #define MSG_SSID_WLAN_ERE         4572
  #define MSG_SSID_WLAN_OCB         4573
#define MSG_SSID_WLAN_LAST      4573


#define MSG_SSID_ATS            4600
  #define MSG_SSID_MSGR         4601
  #define MSG_SSID_APPMGR       4602
  #define MSG_SSID_QTF          4603
  #define MSG_SSID_FWS          4604
  #define MSG_SSID_SRCH4        4605
  #define MSG_SSID_CMAPI        4606
  #define MSG_SSID_MMAL         4607
  #define MSG_SSID_QRARB        4608
  #define MSG_SSID_LMTSMGR      4609
  #define MSG_SSID_MCS_RESERVED_7 4610
  #define MSG_SSID_MCS_RESERVED_8 4611
  #define MSG_SSID_IRATMAN      4612
  #define MSG_SSID_CXM          4613
  #define MSG_SSID_VSTMR        4614
  #define MSG_SSID_CFCM        4615
#define MSG_SSID_MCS_LAST     4615




/* Default master category for data services. */
#define MSG_SSID_DS             5000
  #define MSG_SSID_DS_RLP     5001
  #define MSG_SSID_DS_PPP     5002
  #define MSG_SSID_DS_TCPIP   5003
  #define MSG_SSID_DS_IS707   5004
  #define MSG_SSID_DS_3GMGR   5005
  #define MSG_SSID_DS_PS      5006
  #define MSG_SSID_DS_MIP     5007
  #define MSG_SSID_DS_UMTS    5008
  #define MSG_SSID_DS_GPRS    5009
  #define MSG_SSID_DS_GSM     5010
  #define MSG_SSID_DS_SOCKETS 5011
  #define MSG_SSID_DS_ATCOP   5012
  #define MSG_SSID_DS_SIO     5013
  #define MSG_SSID_DS_BCMCS   5014
  #define MSG_SSID_DS_MLRLP   5015
  #define MSG_SSID_DS_RTP     5016
  #define MSG_SSID_DS_SIPSTACK 5017
  #define MSG_SSID_DS_ROHC     5018
  #define MSG_SSID_DS_DOQOS    5019
  #define MSG_SSID_DS_IPC      5020
  #define MSG_SSID_DS_SHIM     5021
  #define MSG_SSID_DS_ACLPOLICY 5022
  #define MSG_SSID_DS_APPS     5023
  #define MSG_SSID_DS_MUX     5024
  #define MSG_SSID_DS_3GPP    5025
  #define MSG_SSID_DS_LTE     5026
  #define MSG_SSID_DS_WCDMA   5027
  #define MSG_SSID_DS_ACLPOLICY_APPS 5028 /* ACL POLICY */
  #define MSG_SSID_DS_HDR      5029
  #define MSG_SSID_DS_IPA      5030
  #define MSG_SSID_DS_EPC      5031
  #define MSG_SSID_DS_APPSRV   5032

#define MSG_SSID_DS_LAST       5032


/* Default master category for Security. */
#define MSG_SSID_SEC                5500
#define MSG_SSID_SEC_CRYPTO         5501  /* Cryptography */
#define MSG_SSID_SEC_SSL            5502  /* Secure Sockets Layer */
#define MSG_SSID_SEC_IPSEC          5503  /* Internet Protocol Security */
#define MSG_SSID_SEC_SFS            5504  /* Secure File System */
#define MSG_SSID_SEC_TEST           5505  /* Security Test Subsystem */
#define MSG_SSID_SEC_CNTAGENT       5506  /* Content Agent Interface */
#define MSG_SSID_SEC_RIGHTSMGR      5507  /* Rights Manager Interface */
#define MSG_SSID_SEC_ROAP           5508  /* Rights Object Aquisition Protocol */
#define MSG_SSID_SEC_MEDIAMGR       5509  /* Media Manager Interface */
#define MSG_SSID_SEC_IDSTORE        5510  /* ID Store Interface */
#define MSG_SSID_SEC_IXFILE         5511  /* File interface */
#define MSG_SSID_SEC_IXSQL          5512  /* SQL interface */
#define MSG_SSID_SEC_IXCOMMON       5513  /* Common Interface */
#define MSG_SSID_SEC_BCASTCNTAGENT  5514  /* Broadcast Content Agent Interface */
#define MSG_SSID_SEC_PLAYREADY      5515  /* Broadcast Content Agent Interface */
#define MSG_SSID_SEC_WIDEVINE       5516  /* Broadcast Content Agent Interface */
#define MSG_SSID_SEC_LAST           5516


/* Default master category for applications. */
#define MSG_SSID_APPS                    6000
  #define MSG_SSID_APPS_APPMGR           6001
  #define MSG_SSID_APPS_UI               6002
  #define MSG_SSID_APPS_QTV              6003
  #define MSG_SSID_APPS_QVP              6004
  #define MSG_SSID_APPS_QVP_STATISTICS   6005
  #define MSG_SSID_APPS_QVP_VENCODER     6006
  #define MSG_SSID_APPS_QVP_MODEM        6007
  #define MSG_SSID_APPS_QVP_UI           6008
  #define MSG_SSID_APPS_QVP_STACK        6009
  #define MSG_SSID_APPS_QVP_VDECODER     6010
  #define MSG_SSID_APPS_ACM              6011
  #define MSG_SSID_APPS_HEAP_PROFILE     6012
  #define MSG_SSID_APPS_QTV_GENERAL      6013
  #define MSG_SSID_APPS_QTV_DEBUG        6014
  #define MSG_SSID_APPS_QTV_STATISTICS   6015
  #define MSG_SSID_APPS_QTV_UI_TASK      6016
  #define MSG_SSID_APPS_QTV_MP4_PLAYER   6017
  #define MSG_SSID_APPS_QTV_AUDIO_TASK   6018
  #define MSG_SSID_APPS_QTV_VIDEO_TASK   6019
  #define MSG_SSID_APPS_QTV_STREAMING    6020
  #define MSG_SSID_APPS_QTV_MPEG4_TASK   6021
  #define MSG_SSID_APPS_QTV_FILE_OPS     6022
  #define MSG_SSID_APPS_QTV_RTP          6023
  #define MSG_SSID_APPS_QTV_RTCP         6024
  #define MSG_SSID_APPS_QTV_RTSP         6025
  #define MSG_SSID_APPS_QTV_SDP_PARSE    6026
  #define MSG_SSID_APPS_QTV_ATOM_PARSE   6027
  #define MSG_SSID_APPS_QTV_TEXT_TASK    6028
  #define MSG_SSID_APPS_QTV_DEC_DSP_IF   6029
  #define MSG_SSID_APPS_QTV_STREAM_RECORDING 6030
  #define MSG_SSID_APPS_QTV_CONFIGURATION    6031
  #define MSG_SSID_APPS_QCAMERA              6032
  #define MSG_SSID_APPS_QCAMCORDER           6033
  #define MSG_SSID_APPS_BREW                 6034
  #define MSG_SSID_APPS_QDJ                  6035
  #define MSG_SSID_APPS_QDTX                 6036
  #define MSG_SSID_APPS_QTV_BCAST_FLO        6037
  #define MSG_SSID_APPS_MDP_GENERAL          6038
  #define MSG_SSID_APPS_PBM                  6039
  #define MSG_SSID_APPS_GRAPHICS_GENERAL     6040
  #define MSG_SSID_APPS_GRAPHICS_EGL         6041
  #define MSG_SSID_APPS_GRAPHICS_OPENGL      6042
  #define MSG_SSID_APPS_GRAPHICS_DIRECT3D    6043
  #define MSG_SSID_APPS_GRAPHICS_SVG         6044
  #define MSG_SSID_APPS_GRAPHICS_OPENVG      6045
  #define MSG_SSID_APPS_GRAPHICS_2D          6046
  #define MSG_SSID_APPS_GRAPHICS_QXPROFILER  6047
  #define MSG_SSID_APPS_GRAPHICS_DSP         6048
  #define MSG_SSID_APPS_GRAPHICS_GRP         6049
  #define MSG_SSID_APPS_GRAPHICS_MDP         6050
  #define MSG_SSID_APPS_CAD                  6051
  #define MSG_SSID_APPS_IMS_DPL              6052
  #define MSG_SSID_APPS_IMS_FW               6053
  #define MSG_SSID_APPS_IMS_SIP              6054
  #define MSG_SSID_APPS_IMS_REGMGR           6055
  #define MSG_SSID_APPS_IMS_RTP              6056
  #define MSG_SSID_APPS_IMS_SDP              6057
  #define MSG_SSID_APPS_IMS_VS               6058
  #define MSG_SSID_APPS_IMS_XDM              6059
  #define MSG_SSID_APPS_IMS_HOM              6060
  #define MSG_SSID_APPS_IMS_IM_ENABLER       6061
  #define MSG_SSID_APPS_IMS_IMS_CORE         6062
  #define MSG_SSID_APPS_IMS_FWAPI            6063
  #define MSG_SSID_APPS_IMS_SERVICES         6064
  #define MSG_SSID_APPS_IMS_POLICYMGR        6065
  #define MSG_SSID_APPS_IMS_PRESENCE         6066
  #define MSG_SSID_APPS_IMS_QIPCALL          6067
  #define MSG_SSID_APPS_IMS_SIGCOMP          6068
  #define MSG_SSID_APPS_IMS_PSVT             6069
  #define MSG_SSID_APPS_IMS_UNKNOWN          6070
  #define MSG_SSID_APPS_IMS_SETTINGS         6071
  #define MSG_SSID_OMX_COMMON                6072
  #define MSG_SSID_APPS_IMS_RCS_CD           6073
  #define MSG_SSID_APPS_IMS_RCS_IM           6074
  #define MSG_SSID_APPS_IMS_RCS_FT           6075
  #define MSG_SSID_APPS_IMS_RCS_IS           6076
  #define MSG_SSID_APPS_IMS_RCS_AUTO_CONFIG  6077
  #define MSG_SSID_APPS_IMS_RCS_COMMON       6078
  #define MSG_SSID_APPS_IMS_UT               6079
  #define MSG_SSID_APPS_IMS_XML              6080
  #define MSG_SSID_APPS_IMS_COM              6081

#define MSG_SSID_APPS_LAST                   6081


/* Default master category for aDSP Tasks. */
#define MSG_SSID_ADSPTASKS                     6500
  #define MSG_SSID_ADSPTASKS_KERNEL            6501
  #define MSG_SSID_ADSPTASKS_AFETASK           6502
  #define MSG_SSID_ADSPTASKS_VOICEPROCTASK     6503
  #define MSG_SSID_ADSPTASKS_VOCDECTASK        6504
  #define MSG_SSID_ADSPTASKS_VOCENCTASK        6505
  #define MSG_SSID_ADSPTASKS_VIDEOTASK         6506
  #define MSG_SSID_ADSPTASKS_VFETASK           6507
  #define MSG_SSID_ADSPTASKS_VIDEOENCTASK      6508
  #define MSG_SSID_ADSPTASKS_JPEGTASK          6509
  #define MSG_SSID_ADSPTASKS_AUDPPTASK         6510
  #define MSG_SSID_ADSPTASKS_AUDPLAY0TASK      6511
  #define MSG_SSID_ADSPTASKS_AUDPLAY1TASK      6512
  #define MSG_SSID_ADSPTASKS_AUDPLAY2TASK      6513
  #define MSG_SSID_ADSPTASKS_AUDPLAY3TASK      6514
  #define MSG_SSID_ADSPTASKS_AUDPLAY4TASK      6515
  #define MSG_SSID_ADSPTASKS_LPMTASK           6516
  #define MSG_SSID_ADSPTASKS_DIAGTASK          6517
  #define MSG_SSID_ADSPTASKS_AUDRECTASK        6518
  #define MSG_SSID_ADSPTASKS_AUDPREPROCTASK    6519
  #define MSG_SSID_ADSPTASKS_MODMATHTASK       6520
  #define MSG_SSID_ADSPTASKS_GRAPHICSTASK      6521

#define MSG_SSID_ADSPTASKS_LAST                6521


/* Messages arising from Linux on L4, or its drivers or applications. */
#define MSG_SSID_L4LINUX_KERNEL          7000
#define MSG_SSID_L4LINUX_KEYPAD          7001
#define MSG_SSID_L4LINUX_APPS            7002
#define MSG_SSID_L4LINUX_QDDAEMON        7003
#define MSG_SSID_L4LINUX_LAST            MSG_SSID_L4LINUX_QDDAEMON

/* Messages arising from Iguana on L4, or its servers and drivers. */
#define MSG_SSID_L4IGUANA_IGUANASERVER   7100   /* Iguana Server itself */
#define MSG_SSID_L4IGUANA_EFS2           7101   /* platform/apps stuff */
#define MSG_SSID_L4IGUANA_QDMS           7102
#define MSG_SSID_L4IGUANA_REX            7103
#define MSG_SSID_L4IGUANA_SMMS           7104
#define MSG_SSID_L4IGUANA_FRAMEBUFFER    7105   /* platform/iguana stuff */
#define MSG_SSID_L4IGUANA_KEYPAD         7106
#define MSG_SSID_L4IGUANA_NAMING         7107
#define MSG_SSID_L4IGUANA_SDIO           7108
#define MSG_SSID_L4IGUANA_SERIAL         7109
#define MSG_SSID_L4IGUANA_TIMER          7110
#define MSG_SSID_L4IGUANA_TRAMP          7111
#define MSG_SSID_L4IGUANA_LAST           MSG_SSID_L4IGUANA_TRAMP

/* Messages arising from L4-specific AMSS modules */
#define MSG_SSID_L4AMSS_QDIAG            7200
#define MSG_SSID_L4AMSS_APS              7201
#define MSG_SSID_L4AMSS_LAST             MSG_SSID_L4AMSS_APS


/* Default master category for HIT. */
#define MSG_SSID_HIT         8000
#define MSG_SSID_HIT_LAST    8000


/* Default master category for Q6 */
#define MSG_SSID_QDSP6         8500
#define MSG_SSID_ADSP_AUD_SVC            8501  /* Audio Service */
#define MSG_SSID_ADSP_AUD_ENCDEC         8502  /* audio encoders/decoders */
#define MSG_SSID_ADSP_AUD_VOC            8503  /* voice encoders/decoders */
#define MSG_SSID_ADSP_AUD_VS             8504  /* voice services */
#define MSG_SSID_ADSP_AUD_MIDI           8505  /* MIDI-based file formats */
#define MSG_SSID_ADSP_AUD_POSTPROC       8506  /* e.g. Graph EQ, Spec Analyzer */
#define MSG_SSID_ADSP_AUD_PREPROC        8507  /* e.g. AGC-R */
#define MSG_SSID_ADSP_AUD_AFE            8508  /* audio front end */
#define MSG_SSID_ADSP_AUD_MSESSION       8509  /* media session */
#define MSG_SSID_ADSP_AUD_DSESSION       8510  /* device session */
#define MSG_SSID_ADSP_AUD_DCM            8511  /* device configuration */
#define MSG_SSID_ADSP_VID_ENC            8512  /* Video Encoder */
#define MSG_SSID_ADSP_VID_ENCRPC         8513  /* Video Encoder DAL driver */
#define MSG_SSID_ADSP_VID_DEC            8514  /* Video Decoder */
#define MSG_SSID_ADSP_VID_DECRPC         8515  /* Video Decoder DAL driver */
#define MSG_SSID_ADSP_VID_COMMONSW       8516  /* Video Common Software Units */
#define MSG_SSID_ADSP_VID_HWDRIVER       8517  /* Video Hardware */
#define MSG_SSID_ADSP_JPG_ENC            8518  /* JPEG Encoder */
#define MSG_SSID_ADSP_JPG_DEC            8519  /* JPEG Decoder */
#define MSG_SSID_ADSP_OMM                8520  /* openmm */
#define MSG_SSID_ADSP_PWRDEM             8521  /* Power or DEM messages */
#define MSG_SSID_ADSP_RESMGR             8522  /* Resource Manager */
#define MSG_SSID_ADSP_CORE               8523  /* General core (startup, heap stats, etc.) */
#define MSG_SSID_ADSP_RDA                8524
#define MSG_SSID_DSP_TOUCH_TAFE_HAL      8525
#define MSG_SSID_DSP_TOUCH_ALGORITHM     8526
#define MSG_SSID_DSP_TOUCH_FRAMEWORK     8527
#define MSG_SSID_DSP_TOUCH_SRE           8528
#define MSG_SSID_DSP_TOUCH_TAFE_DRIVER   8529

#define MSG_SSID_QDSP6_LAST              8529

/* Default master category for UMB. */
#define MSG_SSID_UMB         9000
#define MSG_SSID_UMB_APP     9001    /* UMB Application component */
#define MSG_SSID_UMB_DS      9002      /* UMB Data Services component */
#define MSG_SSID_UMB_CP      9003      /* UMB Call Processing component */
#define MSG_SSID_UMB_RLL     9004      /* UMB Radio Link Layer component */
#define MSG_SSID_UMB_MAC     9005    /* UMB MAC component */
#define MSG_SSID_UMB_SRCH    9006   /* UMB SRCH component */
#define MSG_SSID_UMB_FW      9007     /* UMB Firmware component */
#define MSG_SSID_UMB_PLT     9008     /* UMB PLT component */
#define MSG_SSID_UMB_LAST    9008

/* Default master category for LTE. */
#define MSG_SSID_LTE         9500
#define MSG_SSID_LTE_RRC       9501
#define MSG_SSID_LTE_MACUL     9502
#define MSG_SSID_LTE_MACDL     9503
#define MSG_SSID_LTE_MACCTRL   9504
#define MSG_SSID_LTE_RLCUL     9505
#define MSG_SSID_LTE_RLCDL     9506
#define MSG_SSID_LTE_PDCPUL    9507
#define MSG_SSID_LTE_PDCPDL    9508
#define MSG_SSID_LTE_ML1       9509
#define MSG_SSID_LTE_DISCOVERY 9510
#define MSG_SSID_LTE_LAST      9510

/*======================================================================================
 * SSIDs for Octopus Base Station Simulator. Although it needs only QXDM side changes
 * diag needs to be updated to keep it consistent with the QXDM database. OCTOPUS team
 * wanted the code changes reverted. They wanted to reserve the codes instead.
 * RESERVED 9700 TILL 10199
 *====================================================================================*/


/* Default master category for QCHAT */

#define MSG_SSID_QCHAT                                          10200
#define MSG_SSID_QCHAT_CAPP                                     10201
#define MSG_SSID_QCHAT_CENG                                     10202
#define MSG_SSID_QCHAT_CREG                                     10203
#define MSG_SSID_QCHAT_CMED                                     10204
#define MSG_SSID_QCHAT_CAUTH                                    10205
#define MSG_SSID_QCHAT_QBAL                                     10206
#define MSG_SSID_QCHAT_OSAL                                     10207
#define MSG_SSID_QCHAT_OEMCUST                                  10208
#define MSG_SSID_QCHAT_MULTI_PROC                               10209
#define MSG_SSID_QCHAT_UPK                                      10210
#define MSG_SSID_QCHAT_LAST                                     10210

/* Default master category for TDSCDMA */
#define MSG_SSID_TDSCDMA_L1                                     10251
#define MSG_SSID_TDSCDMA_L2                                     10252
#define MSG_SSID_TDSCDMA_MAC                                    10253
#define MSG_SSID_TDSCDMA_RLC                                    10254
#define MSG_SSID_TDSCDMA_RRC                                    10255
#define MSG_SSID_TDSCDMA_LAST                                   10255

/* Messages from the CTA framework */
#define MSG_SSID_CTA                                            10300
#define MSG_SSID_CTA_LAST                                       10300

/* QCNEA related SSIDs */
#define MSG_SSID_QCNEA                                          10350
#define MSG_SSID_QCNEA_CAC                                      10351
#define MSG_SSID_QCNEA_CORE                                     10352
#define MSG_SSID_QCNEA_CORE_CAS                                 10353
#define MSG_SSID_QCNEA_CORE_CDE                                 10354
#define MSG_SSID_QCNEA_CORE_COM                                 10355
#define MSG_SSID_QCNEA_CORE_LEE                                 10356
#define MSG_SSID_QCNEA_CORE_QMI                                 10357
#define MSG_SSID_QCNEA_CORE_SRM                                 10358
#define MSG_SSID_QCNEA_GENERIC                                  10359
#define MSG_SSID_QCNEA_NETLINK                                  10360
#define MSG_SSID_QCNEA_NIMS                                     10361
#define MSG_SSID_QCNEA_NSRM                                     10362
#define MSG_SSID_QCNEA_NSRM_CORE                                10363
#define MSG_SSID_QCNEA_NSRM_GATESM                              10364
#define MSG_SSID_QCNEA_NSRM_TRG                                 10365
#define MSG_SSID_QCNEA_PLCY                                     10366
#define MSG_SSID_QCNEA_PLCY_ANDSF                               10367
#define MSG_SSID_QCNEA_TEST                                     10368
#define MSG_SSID_QCNEA_WQE                                      10369
#define MSG_SSID_QCNEA_WQE_BQE                                  10370
#define MSG_SSID_QCNEA_WQE_CQE                                  10371
#define MSG_SSID_QCNEA_WQE_ICD                                  10372
#define MSG_SSID_QCNEA_WQE_IFSEL                                10373
#define MSG_SSID_QCNEA_WQE_IFSELRSM                             10374
#define MSG_SSID_QCNEA_ATP                                      10375
#define MSG_SSID_QCNEA_ATP_PLCY                                 10376
#define MSG_SSID_QCNEA_ATP_RPRT                                 10377

#define MSG_SSID_QCNEA_LAST                                     10377

/* DPM related SSIDs */
#define MSG_SSID_DPM                                            10400
#define MSG_SSID_DPM_COMMON                                     10401
#define MSG_SSID_DPM_COM                                        10402
#define MSG_SSID_DPM_QMI                                        10403
#define MSG_SSID_DPM_DSM                                        10404
#define MSG_SSID_DPM_CONFIG                                     10405
#define MSG_SSID_DPM_GENERIC                                    10406
#define MSG_SSID_DPM_NETLINK                                    10407
#define MSG_SSID_DPM_FD_MGR                                     10408
#define MSG_SSID_DPM_CT_MGR                                     10409
#define MSG_SSID_DPM_NSRM                                       10410
#define MSG_SSID_DPM_NSRM_CORE                                  10411
#define MSG_SSID_DPM_NSRM_GATESM                                10412
#define MSG_SSID_DPM_NSRM_TRG                                   10413
#define MSG_SSID_DPM_TEST                                       10414
#define MSG_SSID_DPM_TCM                                        10415

#define MSG_SSID_DPM_LAST                                       10415

/* EXAMPLE: Entry for an SSID */
#if 0

/* MSG_SSID_FOO */

/* Donot add this block of code when adding new ssids further */
/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_FOO
  #define MSG_BUILD_MASK_MSG_SSID_FOO    MSG_BUILD_MASK_DFLT
#endif

/* Donot add this block of code when adding new ssids further */
/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_FOO)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_LEGACY MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_FOO

#endif

#endif /* end example */


/* MSG_SSID_AUDFMT */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_AUDFMT
  #define MSG_BUILD_MASK_MSG_SSID_AUDFMT    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_AUDFMT)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_AUDFMT

#endif

/* MSG_SSID_AVS */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_AVS
  #define MSG_BUILD_MASK_MSG_SSID_AVS    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_AVS)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_AVS

#endif


/* MSG_SSID_BOOT */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_BOOT
  #define MSG_BUILD_MASK_MSG_SSID_BOOT    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_BOOT)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_BOOT

#endif

/* MSG_SSID_BT */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_BT
  #define MSG_BUILD_MASK_MSG_SSID_BT    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_BT)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_BT

#endif


/* MSG_SSID_CM */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_CM
  #define MSG_BUILD_MASK_MSG_SSID_CM    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_CM)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_CM

#endif


/* MSG_SSID_CMX */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_CMX
  #define MSG_BUILD_MASK_MSG_SSID_CMX    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_CMX)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_CMX

#endif

/* MSG_SSID_DIAG */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_DIAG
  #define MSG_BUILD_MASK_MSG_SSID_DIAG    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_DIAG)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_DIAG

#endif


/* MSG_SSID_DSM */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_DSM
  #define MSG_BUILD_MASK_MSG_SSID_DSM    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_DSM)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_DSM

#endif


/* MSG_SSID_FS */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_FS
  #define MSG_BUILD_MASK_MSG_SSID_FS    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_FS)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_FS

#endif

/* MSG_SSID_HS */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_HS
  #define MSG_BUILD_MASK_MSG_SSID_HS    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_HS)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_HS

#endif

/* MSG_SSID_MDSP */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_MDSP
  #define MSG_BUILD_MASK_MSG_SSID_MDSP    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_MDSP)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_MDSP

#endif


/* MSG_SSID_QDSP */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_QDSP
  #define MSG_BUILD_MASK_MSG_SSID_QDSP    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_QDSP)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_QDSP

#endif

/* MSG_SSID_REX */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_REX
  #define MSG_BUILD_MASK_MSG_SSID_REX    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_REX)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_REX

#endif

/* MSG_SSID_RF */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_RF
  #define MSG_BUILD_MASK_MSG_SSID_RF    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_RF)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_RF

#endif

/* MSG_SSID_SD */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_SD
  #define MSG_BUILD_MASK_MSG_SSID_SD    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_SD)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_SD

#endif


/* MSG_SSID_SIO */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_SIO
  #define MSG_BUILD_MASK_MSG_SSID_SIO    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_SIO)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_SIO

#endif


/* MSG_SSID_VS */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_VS
  #define MSG_BUILD_MASK_MSG_SSID_VS    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_VS)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_VS

#endif

/* MSG_SSID_WMS */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_WMS
  #define MSG_BUILD_MASK_MSG_SSID_WMS    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_WMS)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_WMS

#endif

/* MSG_SSID_GPS */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_GPS
  #define MSG_BUILD_MASK_MSG_SSID_GPS    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_GPS)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_GPS

#endif

/* MSG_SSID_MMOC */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_MMOC
  #define MSG_BUILD_MASK_MSG_SSID_MMOC    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_MMOC)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_MMOC

#endif

/* MSG_SSID_RUIM */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_RUIM
  #define MSG_BUILD_MASK_MSG_SSID_RUIM    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_RUIM)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_RUIM

#endif

/* MSG_SSID_TMC */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_TMC
  #define MSG_BUILD_MASK_MSG_SSID_TMC    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_TMC)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_TMC

#endif

/* MSG_SSID_FTM */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_FTM
  #define MSG_BUILD_MASK_MSG_SSID_FTM    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_FTM)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_FTM

#endif

/* MSG_SSID_MMGPS */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_MMGPS
  #define MSG_BUILD_MASK_MSG_SSID_MMGPS    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_MMGPS)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_MMGPS

#endif

/* MSG_SSID_SLEEP */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_SLEEP
  #define MSG_BUILD_MASK_MSG_SSID_SLEEP    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_SLEEP)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_SLEEP

#endif

/* MSG_SSID_SAM */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_SAM
  #define MSG_BUILD_MASK_MSG_SSID_SAM    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_SAM)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_SAM

#endif

/* MSG_SSID_SRM */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_SRM
  #define MSG_BUILD_MASK_MSG_SSID_SRM    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_SRM)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_SRM

#endif

/* MSG_SSID_SFAT */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_SFAT
  #define MSG_BUILD_MASK_MSG_SSID_SFAT    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_SFAT)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_SFAT

#endif

/* MSG_SSID_JOYST */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_JOYST
  #define MSG_BUILD_MASK_MSG_SSID_JOYST    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_JOYST)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_JOYST

#endif

/* MSG_SSID_MFLO */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_MFLO
  #define MSG_BUILD_MASK_MSG_SSID_MFLO    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_MFLO)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_MFLO

#endif

/* MSG_SSID_DTV */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_DTV
  #define MSG_BUILD_MASK_MSG_SSID_DTV    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_DTV)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_DTV

#endif

/* MSG_SSID_FLUTE */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_FLUTE
  #define MSG_BUILD_MASK_MSG_SSID_FLUTE    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_FLUTE)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_FLUTE

#endif

/* MSG_SSID_CAMERA */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_CAMERA
  #define MSG_BUILD_MASK_MSG_SSID_CAMERA    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_CAMERA)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_CAMERA

#endif

/* MSG_SSID_USBHOST */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_USBHOST
  #define MSG_BUILD_MASK_MSG_SSID_USBHOST    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_USBHOST)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_USBHOST

#endif

/* MSG_SSID_PROFILER */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_PROFILER
  #define MSG_BUILD_MASK_MSG_SSID_PROFILER    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_PROFILER)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_PROFILER

#endif

/* MSG_SSID_TCXOMGR */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_TCXOMGR
  #define MSG_BUILD_MASK_MSG_SSID_TCXOMGR    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_TCXOMGR)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_TCXOMGR

#endif

/* MSG_SSID_EFS */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_EFS
  #define MSG_BUILD_MASK_MSG_SSID_EFS    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_EFS)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_EFS

#endif

/* MSG_SSID_IRDA */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_IRDA
  #define MSG_BUILD_MASK_MSG_SSID_IRDA    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_IRDA)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_IRDA

#endif

/* MSG_SSID_FM_RADIO */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_FM_RADIO
  #define MSG_BUILD_MASK_MSG_SSID_FM_RADIO    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_FM_RADIO)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_FM_RADIO

#endif

/* MSG_SSID_AAM */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_AAM
  #define MSG_BUILD_MASK_MSG_SSID_AAM    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_AAM)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_AAM

#endif

/* MSG_SSID_BM */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_BM
  #define MSG_BUILD_MASK_MSG_SSID_BM    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_BM)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_BM

#endif

/* MSG_SSID_PE */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_PE
  #define MSG_BUILD_MASK_MSG_SSID_PE    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_PE)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_PE

#endif

/* MSG_SSID_QIPCALL */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_QIPCALL
  #define MSG_BUILD_MASK_MSG_SSID_QIPCALL    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_QIPCALL)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_QIPCALL

#endif

/* MSG_SSID_HSUSB */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_HSUSB
  #define MSG_BUILD_MASK_MSG_SSID_HSUSB    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_HSUSB)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_HSUSB

#endif

/* MSG_SSID_FC */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_FC
  #define MSG_BUILD_MASK_MSG_SSID_FC    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_FC)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_FC

#endif

/* MSG_SSID_MGP */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_MGP
  #define MSG_BUILD_MASK_MSG_SSID_MGP    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_MGP)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_MGP

#endif

/* MSG_SSID_MGPME */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_MGPME
  #define MSG_BUILD_MASK_MSG_SSID_MGPME    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_MGPME)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_MGPME

#endif

/* MSG_SSID_GPSOS */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_GPSOS
  #define MSG_BUILD_MASK_MSG_SSID_GPSOS    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_GPSOS)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_GPSOS

#endif

/* MSG_SSID_MGPPE */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_MGPPE
  #define MSG_BUILD_MASK_MSG_SSID_MGPPE    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_MGPPE)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_MGPPE

#endif

/* MSG_SSID_GPSSM */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_GPSSM
  #define MSG_BUILD_MASK_MSG_SSID_GPSSM    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_GPSSM)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_GPSSM

#endif

/* MSG_SSID_IMS */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_IMS
  #define MSG_BUILD_MASK_MSG_SSID_IMS    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_IMS)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_IMS

#endif

/* MSG_SSID_MBP_RF */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_MBP_RF
  #define MSG_BUILD_MASK_MSG_SSID_MBP_RF    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_MBP_RF)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_MBP_RF

#endif

/* MSG_SSID_SNS */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_SNS
  #define MSG_BUILD_MASK_MSG_SSID_SNS    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_SNS)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_SNS

#endif

/* MSG_SSID_WM */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_WM
  #define MSG_BUILD_MASK_MSG_SSID_WM    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_WM)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_WM

#endif

/* MSG_SSID_LK */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_LK
  #define MSG_BUILD_MASK_MSG_SSID_LK    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_LK)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_LK

#endif

/* MSG_SSID_ANDROID_ADB */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_ANDROID_ADB
  #define MSG_BUILD_MASK_MSG_SSID_ANDROID_ADB    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_ANDROID_ADB)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_ANDROID_ADB

#endif

/* MSG_SSID_PWRDB */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_PWRDB
  #define MSG_BUILD_MASK_MSG_SSID_PWRDB    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_PWRDB)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_PWRDB

#endif

/* MSG_SSID_DCVS */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_DCVS
  #define MSG_BUILD_MASK_MSG_SSID_DCVS    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_DCVS)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_DCVS

#endif

/* MSG_SSID_VIDEO_ENCODER */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_VIDEO_ENCODER
  #define MSG_BUILD_MASK_MSG_SSID_VIDEO_ENCODER    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_VIDEO_ENCODER)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_VIDEO_ENCODER

#endif

/* MSG_SSID_VENC_OMX */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_VENC_OMX
  #define MSG_BUILD_MASK_MSG_SSID_VENC_OMX    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_VENC_OMX)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_VENC_OMX

#endif

/* MSG_SSID_GAN */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_GAN
  #define MSG_BUILD_MASK_MSG_SSID_GAN    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_GAN)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_GAN

#endif

/* MSG_SSID_KINETO_GAN */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_KINETO_GAN
  #define MSG_BUILD_MASK_MSG_SSID_KINETO_GAN    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_KINETO_GAN)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_KINETO_GAN

#endif

/* MSG_SSID_ANDROID_QCRIL */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_ANDROID_QCRIL
  #define MSG_BUILD_MASK_MSG_SSID_ANDROID_QCRIL    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_ANDROID_QCRIL)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_ANDROID_QCRIL

#endif

/* MSG_SSID_A2 */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_A2
  #define MSG_BUILD_MASK_MSG_SSID_A2    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_A2)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_A2

#endif


/* MSG_SSID_LINUX_DATA */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_LINUX_DATA
  #define MSG_BUILD_MASK_MSG_SSID_LINUX_DATA    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_LINUX_DATA)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_LINUX_DATA

#endif

#ifndef MSG_BUILD_MASK_MSG_SSID_ECALL
  #define MSG_BUILD_MASK_MSG_SSID_ECALL    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_ECALL)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_ECALL

#endif

/* MSG_SSID_ONCRPC */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_ONCRPC
  #define MSG_BUILD_MASK_MSG_SSID_ONCRPC    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_ONCRPC)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_ONCRPC

#endif

/* MSG_SSID_ONCRPC_MISC_MODEM */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_ONCRPC_MISC_MODEM
  #define MSG_BUILD_MASK_MSG_SSID_ONCRPC_MISC_MODEM    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_ONCRPC_MISC_MODEM)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_ONCRPC_MISC_MODEM

#endif

/* MSG_SSID_ONCRPC_MISC_APPS */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_ONCRPC_MISC_APPS
  #define MSG_BUILD_MASK_MSG_SSID_ONCRPC_MISC_APPS    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_ONCRPC_MISC_APPS)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_ONCRPC_MISC_APPS

#endif

/* MSG_SSID_ONCRPC_CM_MODEM */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_ONCRPC_CM_MODEM
  #define MSG_BUILD_MASK_MSG_SSID_ONCRPC_CM_MODEM    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_ONCRPC_CM_MODEM)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_ONCRPC_CM_MODEM

#endif

/* MSG_SSID_ONCRPC_CM_APPS */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_ONCRPC_CM_APPS
  #define MSG_BUILD_MASK_MSG_SSID_ONCRPC_CM_APPS    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_ONCRPC_CM_APPS)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_ONCRPC_CM_APPS

#endif

/* MSG_SSID_ONCRPC_DB */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_ONCRPC_DB
  #define MSG_BUILD_MASK_MSG_SSID_ONCRPC_DB    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_ONCRPC_DB)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_ONCRPC_DB

#endif

/* MSG_SSID_ONCRPC_SND */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_ONCRPC_SND
  #define MSG_BUILD_MASK_MSG_SSID_ONCRPC_SND    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_ONCRPC_SND)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_ONCRPC_SND

#endif

/* 1X related SSIDs */

/* MSG_SSID_1X */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_1X
  #define MSG_BUILD_MASK_MSG_SSID_1X    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_1X)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_1X

#endif

/* MSG_SSID_1X_ACP */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_1X_ACP
  #define MSG_BUILD_MASK_MSG_SSID_1X_ACP    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_1X_ACP)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_1X_ACP

#endif

/* MSG_SSID_1X_DCP */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_1X_DCP
  #define MSG_BUILD_MASK_MSG_SSID_1X_DCP    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_1X_DCP)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_1X_DCP

#endif

/* MSG_SSID_1X_DEC */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_1X_DEC
  #define MSG_BUILD_MASK_MSG_SSID_1X_DEC    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_1X_DEC)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_1X_DEC

#endif

/* MSG_SSID_1X_ENC */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_1X_ENC
  #define MSG_BUILD_MASK_MSG_SSID_1X_ENC    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_1X_ENC)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_1X_ENC

#endif

/* MSG_SSID_1X_GPSSRCH */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_1X_GPSSRCH
  #define MSG_BUILD_MASK_MSG_SSID_1X_GPSSRCH    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_1X_GPSSRCH)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_1X_GPSSRCH

#endif

/* MSG_SSID_1X_MUX */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_1X_MUX
  #define MSG_BUILD_MASK_MSG_SSID_1X_MUX    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_1X_MUX)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_1X_MUX

#endif

/* MSG_SSID_1X_SRCH */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_1X_SRCH
  #define MSG_BUILD_MASK_MSG_SSID_1X_SRCH    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_1X_SRCH)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_1X_SRCH

#endif



/* HDR SSIDs  */


/* MSG_SSID_HDR_PROT */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_HDR_PROT
  #define MSG_BUILD_MASK_MSG_SSID_HDR_PROT    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_HDR_PROT)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_HDR_PROT

#endif

/* MSG_SSID_HDR_DATA */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_HDR_DATA
  #define MSG_BUILD_MASK_MSG_SSID_HDR_DATA    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_HDR_DATA)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_HDR_DATA

#endif

/* MSG_SSID_HDR_SRCH */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_HDR_SRCH
  #define MSG_BUILD_MASK_MSG_SSID_HDR_SRCH    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_HDR_SRCH)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_HDR_SRCH

#endif

/* MSG_SSID_HDR_DRIVERS */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_HDR_DRIVERS
  #define MSG_BUILD_MASK_MSG_SSID_HDR_DRIVERS    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_HDR_DRIVERS)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_HDR_DRIVERS

#endif

/* MSG_SSID_HDR_IS890 */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_HDR_IS890
  #define MSG_BUILD_MASK_MSG_SSID_HDR_IS890    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_HDR_IS890)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_HDR_IS890

#endif

/* MSG_SSID_HDR_DEBUG */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_HDR_DEBUG
  #define MSG_BUILD_MASK_MSG_SSID_HDR_DEBUG    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_HDR_DEBUG)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_HDR_DEBUG

#endif

/* MSG_SSID_HDR_HIT */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_HDR_HIT
  #define MSG_BUILD_MASK_MSG_SSID_HDR_HIT    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_HDR_HIT)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_HDR_HIT

#endif


/* MSG_SSID_HDR_PCP */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_HDR_PCP
  #define MSG_BUILD_MASK_MSG_SSID_HDR_PCP    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_HDR_PCP)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_HDR_PCP

#endif

/* MSG_SSID_HDR_HEAPMEM */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_HDR_HEAPMEM
  #define MSG_BUILD_MASK_MSG_SSID_HDR_HEAPMEM    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_HDR_HEAPMEM)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_HDR_HEAPMEM

#endif



/* UMTS SSIDs  */

/* MSG_SSID_UMTS */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_UMTS
  #define MSG_BUILD_MASK_MSG_SSID_UMTS    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_UMTS)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_UMTS

#endif


/* MSG_SSID_WCDMA_L1 */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_WCDMA_L1
  #define MSG_BUILD_MASK_MSG_SSID_WCDMA_L1    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_WCDMA_L1)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_WCDMA_L1

#endif

/* MSG_SSID_WCDMA_L2 */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_WCDMA_L2
  #define MSG_BUILD_MASK_MSG_SSID_WCDMA_L2    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_WCDMA_L2)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_WCDMA_L2

#endif

/* MSG_SSID_WCDMA_MAC */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_WCDMA_MAC
  #define MSG_BUILD_MASK_MSG_SSID_WCDMA_MAC    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_WCDMA_MAC)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_WCDMA_MAC

#endif

/* MSG_SSID_WCDMA_RLC */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_WCDMA_RLC
  #define MSG_BUILD_MASK_MSG_SSID_WCDMA_RLC    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_WCDMA_RLC)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_WCDMA_RLC

#endif

/* MSG_SSID_WCDMA_RRC */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_WCDMA_RRC
  #define MSG_BUILD_MASK_MSG_SSID_WCDMA_RRC    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_WCDMA_RRC)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_WCDMA_RRC

#endif

/* MSG_SSID_NAS_CNM */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_NAS_CNM
  #define MSG_BUILD_MASK_MSG_SSID_NAS_CNM    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_NAS_CNM)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_NAS_CNM

#endif

/* MSG_SSID_NAS_MM */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_NAS_MM
  #define MSG_BUILD_MASK_MSG_SSID_NAS_MM    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_NAS_MM)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_NAS_MM

#endif

/* MSG_SSID_NAS_MN */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_NAS_MN
  #define MSG_BUILD_MASK_MSG_SSID_NAS_MN    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_NAS_MN)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_NAS_MN

#endif

/* MSG_SSID_NAS_RABM */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_NAS_RABM
  #define MSG_BUILD_MASK_MSG_SSID_NAS_RABM    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_NAS_RABM)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_NAS_RABM

#endif

/* MSG_SSID_NAS_REG */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_NAS_REG
  #define MSG_BUILD_MASK_MSG_SSID_NAS_REG    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_NAS_REG)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_NAS_REG

#endif

/* MSG_SSID_NAS_SM */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_NAS_SM
  #define MSG_BUILD_MASK_MSG_SSID_NAS_SM    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_NAS_SM)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_NAS_SM

#endif

#ifndef MSG_BUILD_MASK_MSG_SSID_NAS_CB
  #define MSG_BUILD_MASK_MSG_SSID_NAS_CB    MSG_BUILD_MASK_DFLT
#endif
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_NAS_CB)
  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT
  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_NAS_CB
#endif
/* MSG_SSID_NAS_TC */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_NAS_TC
  #define MSG_BUILD_MASK_MSG_SSID_NAS_TC    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_NAS_TC)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_NAS_TC

#endif



/* GSM SSIDs  */


/* MSG_SSID_GSM */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_GSM
  #define MSG_BUILD_MASK_MSG_SSID_GSM    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_GSM)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_GSM

#endif

/* MSG_SSID_GSM_L1 */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_GSM_L1
  #define MSG_BUILD_MASK_MSG_SSID_GSM_L1    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_GSM_L1)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_GSM_L1

#endif

/* MSG_SSID_GSM_L2 */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_GSM_L2
  #define MSG_BUILD_MASK_MSG_SSID_GSM_L2    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_GSM_L2)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_GSM_L2

#endif

/* MSG_SSID_GSM_RR */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_GSM_RR
  #define MSG_BUILD_MASK_MSG_SSID_GSM_RR    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_GSM_RR)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_GSM_RR

#endif

/* MSG_SSID_GSM_GPRS_GCOMMON */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_GSM_GPRS_GCOMMON
  #define MSG_BUILD_MASK_MSG_SSID_GSM_GPRS_GCOMMON    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_GSM_GPRS_GCOMMON)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_GSM_GPRS_GCOMMON

#endif

/* MSG_SSID_GSM_GPRS_GLLC */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_GSM_GPRS_GLLC
  #define MSG_BUILD_MASK_MSG_SSID_GSM_GPRS_GLLC    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_GSM_GPRS_GLLC)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_GSM_GPRS_GLLC

#endif

/* MSG_SSID_GSM_GPRS_GMAC */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_GSM_GPRS_GMAC
  #define MSG_BUILD_MASK_MSG_SSID_GSM_GPRS_GMAC    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_GSM_GPRS_GMAC)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_GSM_GPRS_GMAC

#endif

/* MSG_SSID_GSM_GPRS_GPL1 */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_GSM_GPRS_GPL1
  #define MSG_BUILD_MASK_MSG_SSID_GSM_GPRS_GPL1    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_GSM_GPRS_GPL1)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_GSM_GPRS_GPL1

#endif

/* MSG_SSID_GSM_GPRS_GRLC */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_GSM_GPRS_GRLC
  #define MSG_BUILD_MASK_MSG_SSID_GSM_GPRS_GRLC    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_GSM_GPRS_GRLC)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_GSM_GPRS_GRLC

#endif

/* MSG_SSID_GSM_GPRS_GRR */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_GSM_GPRS_GRR
  #define MSG_BUILD_MASK_MSG_SSID_GSM_GPRS_GRR    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_GSM_GPRS_GRR)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_GSM_GPRS_GRR

#endif

/* MSG_SSID_GSM_GPRS_GSNDCP */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_GSM_GPRS_GSNDCP
  #define MSG_BUILD_MASK_MSG_SSID_GSM_GPRS_GSNDCP    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_GSM_GPRS_GSNDCP)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_GSM_GPRS_GSNDCP

#endif


/* MSG_SSID_WLAN */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_WLAN
  #define MSG_BUILD_MASK_MSG_SSID_WLAN    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_WLAN)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_WLAN

#endif

/* MSG_SSID_WLAN_ADP */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_WLAN_ADP
  #define MSG_BUILD_MASK_MSG_SSID_WLAN_ADP    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_WLAN_ADP)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_WLAN_ADP

#endif

/* MSG_SSID_WLAN_CP */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_WLAN_CP
  #define MSG_BUILD_MASK_MSG_SSID_WLAN_CP    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_WLAN_CP)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_WLAN_CP

#endif

/* MSG_SSID_WLAN_FTM */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_WLAN_FTM
  #define MSG_BUILD_MASK_MSG_SSID_WLAN_FTM    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_WLAN_FTM)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_WLAN_FTM

#endif

/* MSG_SSID_WLAN_OEM */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_WLAN_OEM
  #define MSG_BUILD_MASK_MSG_SSID_WLAN_OEM    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_WLAN_OEM)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_WLAN_OEM

#endif

/* MSG_SSID_WLAN_SEC */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_WLAN_SEC
  #define MSG_BUILD_MASK_MSG_SSID_WLAN_SEC    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_WLAN_SEC)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_WLAN_SEC

#endif

/* MSG_SSID_WLAN_TRP */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_WLAN_TRP
  #define MSG_BUILD_MASK_MSG_SSID_WLAN_TRP    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_WLAN_TRP)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_WLAN_TRP

#endif

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_WLAN_RESERVED_1
  #define MSG_BUILD_MASK_MSG_SSID_WLAN_RESERVED_1    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_WLAN_RESERVED_1)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_WLAN_RESERVED_1

#endif

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_WLAN_RESERVED_2
  #define MSG_BUILD_MASK_MSG_SSID_WLAN_RESERVED_2    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_WLAN_RESERVED_2)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_WLAN_RESERVED_2

#endif

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_WLAN_RESERVED_3
  #define MSG_BUILD_MASK_MSG_SSID_WLAN_RESERVED_3    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_WLAN_RESERVED_3)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_WLAN_RESERVED_3

#endif

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_WLAN_RESERVED_4
  #define MSG_BUILD_MASK_MSG_SSID_WLAN_RESERVED_4    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_WLAN_RESERVED_4)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_WLAN_RESERVED_4

#endif

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_WLAN_RESERVED_5
  #define MSG_BUILD_MASK_MSG_SSID_WLAN_RESERVED_5    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_WLAN_RESERVED_5)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_WLAN_RESERVED_5

#endif

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_WLAN_RESERVED_6
  #define MSG_BUILD_MASK_MSG_SSID_WLAN_RESERVED_6    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_WLAN_RESERVED_6)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_WLAN_RESERVED_6

#endif

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_WLAN_RESERVED_7
  #define MSG_BUILD_MASK_MSG_SSID_WLAN_RESERVED_7    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_WLAN_RESERVED_7)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_WLAN_RESERVED_7

#endif

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_WLAN_RESERVED_8
  #define MSG_BUILD_MASK_MSG_SSID_WLAN_RESERVED_8    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_WLAN_RESERVED_8)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_WLAN_RESERVED_8

#endif

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_WLAN_RESERVED_9
  #define MSG_BUILD_MASK_MSG_SSID_WLAN_RESERVED_9    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_WLAN_RESERVED_9)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_WLAN_RESERVED_9

#endif

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_WLAN_RESERVED_10
  #define MSG_BUILD_MASK_MSG_SSID_WLAN_RESERVED_10    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_WLAN_RESERVED_10)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_WLAN_RESERVED_10

#endif

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_WLAN_TL
  #define MSG_BUILD_MASK_MSG_SSID_WLAN_TL    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_WLAN_TL)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_WLAN_TL

#endif

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_WLAN_BAL
  #define MSG_BUILD_MASK_MSG_SSID_WLAN_BAL    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_WLAN_BAL)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_WLAN_BAL

#endif

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_WLAN_SAL
  #define MSG_BUILD_MASK_MSG_SSID_WLAN_SAL    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_WLAN_SAL)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_WLAN_SAL

#endif

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_WLAN_SSC
  #define MSG_BUILD_MASK_MSG_SSID_WLAN_SSC    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_WLAN_SSC)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_WLAN_SSC

#endif

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_WLAN_HDD
  #define MSG_BUILD_MASK_MSG_SSID_WLAN_HDD    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_WLAN_HDD)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_WLAN_HDD

#endif

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_WLAN_SME
  #define MSG_BUILD_MASK_MSG_SSID_WLAN_SME    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_WLAN_SME)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_WLAN_SME

#endif

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_WLAN_PE
  #define MSG_BUILD_MASK_MSG_SSID_WLAN_PE    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_WLAN_PE)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_WLAN_PE

#endif

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_WLAN_HAL
  #define MSG_BUILD_MASK_MSG_SSID_WLAN_HAL    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_WLAN_HAL)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_WLAN_HAL

#endif

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_WLAN_SYS
  #define MSG_BUILD_MASK_MSG_SSID_WLAN_SYS    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_WLAN_SYS)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_WLAN_SYS

#endif

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_WLAN_VOSS
  #define MSG_BUILD_MASK_MSG_SSID_WLAN_VOSS    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_WLAN_VOSS)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_WLAN_VOSS

#endif


/* MSG_SSID_ATS */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_ATS
  #define MSG_BUILD_MASK_MSG_SSID_ATS    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_ATS)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_ATS

#endif

/* MSG_SSID_MSGR */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_MSGR
  #define MSG_BUILD_MASK_MSG_SSID_MSGR    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_MSGR)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_MSGR

#endif

/* MSG_SSID_APPMGR */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_APPMGR
  #define MSG_BUILD_MASK_MSG_SSID_APPMGR    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_APPMGR)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_APPMGR

#endif

/* MSG_SSID_QTF */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_QTF
  #define MSG_BUILD_MASK_MSG_SSID_QTF    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_QTF)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_QTF

#endif

/* MSG_SSID_MCS_RESERVED_5 */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_MCS_RESERVED_5
  #define MSG_BUILD_MASK_MSG_SSID_MCS_RESERVED_5    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_MCS_RESERVED_5)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_MCS_RESERVED_5

#endif

/* MSG_SSID_MCS_RESERVED_6 */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_MCS_RESERVED_6
  #define MSG_BUILD_MASK_MSG_SSID_MCS_RESERVED_6    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_MCS_RESERVED_6)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_MCS_RESERVED_6

#endif

/* MSG_SSID_MCS_RESERVED_7 */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_MCS_RESERVED_7
  #define MSG_BUILD_MASK_MSG_SSID_MCS_RESERVED_7    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_MCS_RESERVED_7)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_MCS_RESERVED_7

#endif

/* MSG_SSID_MCS_RESERVED_8*/

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_MCS_RESERVED_8
  #define MSG_BUILD_MASK_MSG_SSID_MCS_RESERVED_8    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_MCS_RESERVED_8)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_MCS_RESERVED_8

#endif


/* DS SSIDs  */

/* MSG_SSID_DS */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_DS
  #define MSG_BUILD_MASK_MSG_SSID_DS    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_DS)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_DS

#endif

/* MSG_SSID_DS_RLP */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_DS_RLP
  #define MSG_BUILD_MASK_MSG_SSID_DS_RLP    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_DS_RLP)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_DS_RLP

#endif

/* MSG_SSID_DS_PPP */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_DS_PPP
  #define MSG_BUILD_MASK_MSG_SSID_DS_PPP    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_DS_PPP)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_DS_PPP

#endif

/* MSG_SSID_DS_TCPIP */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_DS_TCPIP
  #define MSG_BUILD_MASK_MSG_SSID_DS_TCPIP    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_DS_TCPIP)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_DS_TCPIP

#endif

/* MSG_SSID_DS_IS707 */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_DS_IS707
  #define MSG_BUILD_MASK_MSG_SSID_DS_IS707    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_DS_IS707)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_DS_IS707

#endif

/* MSG_SSID_DS_3GMGR */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_DS_3GMGR
  #define MSG_BUILD_MASK_MSG_SSID_DS_3GMGR    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_DS_3GMGR)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_DS_3GMGR

#endif

/* MSG_SSID_DS_PS */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_DS_PS
  #define MSG_BUILD_MASK_MSG_SSID_DS_PS    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_DS_PS)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_DS_PS

#endif

/* MSG_SSID_DS_MIP */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_DS_MIP
  #define MSG_BUILD_MASK_MSG_SSID_DS_MIP    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_DS_MIP)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_DS_MIP

#endif

/* MSG_SSID_DS_UMTS */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_DS_UMTS
  #define MSG_BUILD_MASK_MSG_SSID_DS_UMTS    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_DS_UMTS)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_DS_UMTS

#endif

/* MSG_SSID_DS_GPRS */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_DS_GPRS
  #define MSG_BUILD_MASK_MSG_SSID_DS_GPRS    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_DS_GPRS)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_DS_GPRS

#endif

/* MSG_SSID_DS_GSM */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_DS_GSM
  #define MSG_BUILD_MASK_MSG_SSID_DS_GSM    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_DS_GSM)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_DS_GSM

#endif

/* MSG_SSID_DS_SOCKETS */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_DS_SOCKETS
  #define MSG_BUILD_MASK_MSG_SSID_DS_SOCKETS    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_DS_SOCKETS)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_DS_SOCKETS

#endif

/* MSG_SSID_DS_ATCOP */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_DS_ATCOP
  #define MSG_BUILD_MASK_MSG_SSID_DS_ATCOP    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_DS_ATCOP)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_DS_ATCOP

#endif

/* MSG_SSID_DS_SIO */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_DS_SIO
  #define MSG_BUILD_MASK_MSG_SSID_DS_SIO    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_DS_SIO)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_DS_SIO

#endif

/* MSG_SSID_DS_BCMCS */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_DS_BCMCS
  #define MSG_BUILD_MASK_MSG_SSID_DS_BCMCS    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_DS_BCMCS)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_DS_BCMCS

#endif

/* MSG_SSID_DS_MLRLP */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_DS_MLRLP
  #define MSG_BUILD_MASK_MSG_SSID_DS_MLRLP    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_DS_MLRLP)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_DS_MLRLP

#endif

/* MSG_SSID_DS_RTP */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_DS_RTP
  #define MSG_BUILD_MASK_MSG_SSID_DS_RTP    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_DS_RTP)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_DS_RTP

#endif

/* MSG_SSID_DS_SIPSTACK */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_DS_SIPSTACK
  #define MSG_BUILD_MASK_MSG_SSID_DS_SIPSTACK    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_DS_SIPSTACK)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_DS_SIPSTACK

#endif

/* MSG_SSID_DS_ROHC */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_DS_ROHC
  #define MSG_BUILD_MASK_MSG_SSID_DS_ROHC    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_DS_ROHC)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_DS_ROHC

#endif

/* MSG_SSID_DS_DOQOS */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_DS_DOQOS
  #define MSG_BUILD_MASK_MSG_SSID_DS_DOQOS    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_DS_DOQOS)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_DS_DOQOS

#endif

/* MSG_SSID_DS_IPC */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_DS_IPC
  #define MSG_BUILD_MASK_MSG_SSID_DS_IPC    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_DS_IPC)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_DS_IPC

#endif

/* MSG_SSID_DS_SHIM */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_DS_SHIM
  #define MSG_BUILD_MASK_MSG_SSID_DS_SHIM    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_DS_SHIM)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_DS_SHIM

#endif

/* MSG_SSID_DS_ACLPOLICY */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_DS_ACLPOLICY
  #define MSG_BUILD_MASK_MSG_SSID_DS_ACLPOLICY    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_DS_ACLPOLICY)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_DS_ACLPOLICY

#endif

/* MSG_SSID_DS_APPS */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_DS_APPS
  #define MSG_BUILD_MASK_MSG_SSID_DS_APPS    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_DS_APPS)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_DS_APPS

#endif

#ifndef MSG_BUILD_MASK_MSG_SSID_DS_MUX
  #define MSG_BUILD_MASK_MSG_SSID_DS_MUX    MSG_BUILD_MASK_DFLT
#endif



/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_DS_MUX)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_DS_MUX

#endif


/* MSG_SSID_SEC */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_SEC
  #define MSG_BUILD_MASK_MSG_SSID_SEC    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_SEC)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_SEC

#endif

/* MSG_SSID_SEC_CRYPTO */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_SEC_CRYPTO
  #define MSG_BUILD_MASK_MSG_SSID_SEC_CRYPTO    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_SEC_CRYPTO)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_SEC_CRYPTO

#endif

/* MSG_SSID_SEC_SSL */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_SEC_SSL
  #define MSG_BUILD_MASK_MSG_SSID_SEC_SSL    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_SEC_SSL)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_SEC_SSL

#endif

/* MSG_SSID_SEC_IPSEC */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_SEC_IPSEC
  #define MSG_BUILD_MASK_MSG_SSID_SEC_IPSEC    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_SEC_IPSEC)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_SEC_IPSEC

#endif

/* MSG_SSID_SEC_SFS */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_SEC_SFS
  #define MSG_BUILD_MASK_MSG_SSID_SEC_SFS    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_SEC_SFS)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_SEC_SFS

#endif

/* MSG_SSID_SEC_TEST */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_SEC_TEST
  #define MSG_BUILD_MASK_MSG_SSID_SEC_TEST    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_SEC_TEST)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_SEC_TEST

#endif

/* MSG_SSID_SEC_CNTAGENT */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_SEC_CNTAGENT
  #define MSG_BUILD_MASK_MSG_SSID_SEC_CNTAGENT    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_SEC_CNTAGENT)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_SEC_CNTAGENT

#endif

/* MSG_SSID_SEC_RIGHTSMGR */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_SEC_RIGHTSMGR
  #define MSG_BUILD_MASK_MSG_SSID_SEC_RIGHTSMGR    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_SEC_RIGHTSMGR)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_SEC_RIGHTSMGR

#endif

/* MSG_SSID_SEC_ROAP */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_SEC_ROAP
  #define MSG_BUILD_MASK_MSG_SSID_SEC_ROAP    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_SEC_ROAP)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_SEC_ROAP

#endif

/* MSG_SSID_SEC_MEDIAMGR */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_SEC_MEDIAMGR
  #define MSG_BUILD_MASK_MSG_SSID_SEC_MEDIAMGR    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_SEC_MEDIAMGR)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_SEC_MEDIAMGR

#endif

/* MSG_SSID_SEC_IDSTORE */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_SEC_IDSTORE
  #define MSG_BUILD_MASK_MSG_SSID_SEC_IDSTORE    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_SEC_IDSTORE)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_SEC_IDSTORE

#endif

/* MSG_SSID_SEC_IXFILE */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_SEC_IXFILE
  #define MSG_BUILD_MASK_MSG_SSID_SEC_IXFILE    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_SEC_IXFILE)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_SEC_IXFILE

#endif

/* MSG_SSID_SEC_IXSQL */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_SEC_IXSQL
  #define MSG_BUILD_MASK_MSG_SSID_SEC_IXSQL    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_SEC_IXSQL)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_SEC_IXSQL

#endif

/* MSG_SSID_SEC_IXCOMMON */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_SEC_IXCOMMON
  #define MSG_BUILD_MASK_MSG_SSID_SEC_IXCOMMON    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_SEC_IXCOMMON)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_SEC_IXCOMMON

#endif

/* MSG_SSID_SEC_BCASTCNTAGENT */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_SEC_BCASTCNTAGENT
  #define MSG_BUILD_MASK_MSG_SSID_SEC_BCASTCNTAGENT    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_SEC_BCASTCNTAGENT)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_SEC_BCASTCNTAGENT

#endif

/* APPS SSIDs  */

/* MSG_SSID_APPS */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_APPS
  #define MSG_BUILD_MASK_MSG_SSID_APPS    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_APPS)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_APPS

#endif


/* MSG_SSID_APPS_APPMGR */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_APPS_APPMGR
  #define MSG_BUILD_MASK_MSG_SSID_APPS_APPMGR    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_APPS_APPMGR)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_APPS_APPMGR

#endif

/* MSG_SSID_APPS_UI */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_APPS_UI
  #define MSG_BUILD_MASK_MSG_SSID_APPS_UI    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_APPS_UI)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_APPS_UI

#endif

/* MSG_SSID_APPS_QTV */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_APPS_QTV
  #define MSG_BUILD_MASK_MSG_SSID_APPS_QTV    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_APPS_QTV)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_APPS_QTV

#endif

/* MSG_SSID_APPS_QVP */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_APPS_QVP
  #define MSG_BUILD_MASK_MSG_SSID_APPS_QVP    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_APPS_QVP)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_APPS_QVP

#endif

/* MSG_SSID_APPS_QVP_STATISTICS */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_APPS_QVP_STATISTICS
  #define MSG_BUILD_MASK_MSG_SSID_APPS_QVP_STATISTICS    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_APPS_QVP_STATISTICS)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_APPS_QVP_STATISTICS

#endif

/* MSG_SSID_APPS_QVP_VENCODER */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_APPS_QVP_VENCODER
  #define MSG_BUILD_MASK_MSG_SSID_APPS_QVP_VENCODER    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_APPS_QVP_VENCODER)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_APPS_QVP_VENCODER

#endif

/* MSG_SSID_APPS_QVP_MODEM */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_APPS_QVP_MODEM
  #define MSG_BUILD_MASK_MSG_SSID_APPS_QVP_MODEM    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_APPS_QVP_MODEM)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_APPS_QVP_MODEM

#endif

/* MSG_SSID_APPS_QVP_UI */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_APPS_QVP_UI
  #define MSG_BUILD_MASK_MSG_SSID_APPS_QVP_UI    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_APPS_QVP_UI)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_APPS_QVP_UI

#endif

/* MSG_SSID_APPS_QVP_STACK */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_APPS_QVP_STACK
  #define MSG_BUILD_MASK_MSG_SSID_APPS_QVP_STACK    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_APPS_QVP_STACK)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_APPS_QVP_STACK

#endif

/* MSG_SSID_APPS_QVP_VDECODER */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_APPS_QVP_VDECODER
  #define MSG_BUILD_MASK_MSG_SSID_APPS_QVP_VDECODER    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_APPS_QVP_VDECODER)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_APPS_QVP_VDECODER

#endif

/* MSG_SSID_APPS_ACM */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_APPS_ACM
  #define MSG_BUILD_MASK_MSG_SSID_APPS_ACM    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_APPS_ACM)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_APPS_ACM

#endif

/* MSG_SSID_APPS_HEAP_PROFILE */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_APPS_HEAP_PROFILE
  #define MSG_BUILD_MASK_MSG_SSID_APPS_HEAP_PROFILE    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_APPS_HEAP_PROFILE)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_APPS_HEAP_PROFILE

#endif

/* MSG_SSID_APPS_QTV_GENERAL */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_APPS_QTV_GENERAL
  #define MSG_BUILD_MASK_MSG_SSID_APPS_QTV_GENERAL    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_APPS_QTV_GENERAL)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_APPS_QTV_GENERAL

#endif

/* MSG_SSID_APPS_QTV_DEBUG */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_APPS_QTV_DEBUG
  #define MSG_BUILD_MASK_MSG_SSID_APPS_QTV_DEBUG    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_APPS_QTV_DEBUG)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_APPS_QTV_DEBUG

#endif

/* MSG_SSID_APPS_QTV_STATISTICS */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_APPS_QTV_STATISTICS
  #define MSG_BUILD_MASK_MSG_SSID_APPS_QTV_STATISTICS    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_APPS_QTV_STATISTICS)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_APPS_QTV_STATISTICS

#endif

/* MSG_SSID_APPS_QTV_UI_TASK */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_APPS_QTV_UI_TASK
  #define MSG_BUILD_MASK_MSG_SSID_APPS_QTV_UI_TASK    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_APPS_QTV_UI_TASK)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_APPS_QTV_UI_TASK

#endif

/* MSG_SSID_APPS_QTV_MP4_PLAYER */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_APPS_QTV_MP4_PLAYER
  #define MSG_BUILD_MASK_MSG_SSID_APPS_QTV_MP4_PLAYER    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_APPS_QTV_MP4_PLAYER)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_APPS_QTV_MP4_PLAYER

#endif

/* MSG_SSID_APPS_QTV_AUDIO_TASK */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_APPS_QTV_AUDIO_TASK
  #define MSG_BUILD_MASK_MSG_SSID_APPS_QTV_AUDIO_TASK    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_APPS_QTV_AUDIO_TASK)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_APPS_QTV_AUDIO_TASK

#endif

/* MSG_SSID_APPS_QTV_VIDEO_TASK */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_APPS_QTV_VIDEO_TASK
  #define MSG_BUILD_MASK_MSG_SSID_APPS_QTV_VIDEO_TASK    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_APPS_QTV_VIDEO_TASK)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_APPS_QTV_VIDEO_TASK

#endif

/* MSG_SSID_APPS_QTV_STREAMING */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_APPS_QTV_STREAMING
  #define MSG_BUILD_MASK_MSG_SSID_APPS_QTV_STREAMING    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_APPS_QTV_STREAMING)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_APPS_QTV_STREAMING

#endif

/* MSG_SSID_APPS_QTV_MPEG4_TASK */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_APPS_QTV_MPEG4_TASK
  #define MSG_BUILD_MASK_MSG_SSID_APPS_QTV_MPEG4_TASK    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_APPS_QTV_MPEG4_TASK)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_APPS_QTV_MPEG4_TASK

#endif

/* MSG_SSID_APPS_QTV_FILE_OPS  */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_APPS_QTV_FILE_OPS
  #define MSG_BUILD_MASK_MSG_SSID_APPS_QTV_FILE_OPS     MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_APPS_QTV_FILE_OPS )

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_APPS_QTV_FILE_OPS

#endif

/* MSG_SSID_APPS_QTV_RTP */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_APPS_QTV_RTP
  #define MSG_BUILD_MASK_MSG_SSID_APPS_QTV_RTP    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_APPS_QTV_RTP)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_APPS_QTV_RTP

#endif

/* MSG_SSID_APPS_QTV_RTCP */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_APPS_QTV_RTCP
  #define MSG_BUILD_MASK_MSG_SSID_APPS_QTV_RTCP    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_APPS_QTV_RTCP)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_APPS_QTV_RTCP

#endif

/* MSG_SSID_APPS_QTV_RTSP */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_APPS_QTV_RTSP
  #define MSG_BUILD_MASK_MSG_SSID_APPS_QTV_RTSP    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_APPS_QTV_RTSP)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_APPS_QTV_RTSP

#endif

/* MSG_SSID_APPS_QTV_SDP_PARSE */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_APPS_QTV_SDP_PARSE
  #define MSG_BUILD_MASK_MSG_SSID_APPS_QTV_SDP_PARSE    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_APPS_QTV_SDP_PARSE)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_APPS_QTV_SDP_PARSE

#endif

/* MSG_SSID_APPS_QTV_ATOM_PARSE */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_APPS_QTV_ATOM_PARSE
  #define MSG_BUILD_MASK_MSG_SSID_APPS_QTV_ATOM_PARSE    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_APPS_QTV_ATOM_PARSE)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_APPS_QTV_ATOM_PARSE

#endif

/* MSG_SSID_APPS_QTV_TEXT_TASK */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_APPS_QTV_TEXT_TASK
  #define MSG_BUILD_MASK_MSG_SSID_APPS_QTV_TEXT_TASK    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_APPS_QTV_TEXT_TASK)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_APPS_QTV_TEXT_TASK

#endif

/* MSG_SSID_APPS_QTV_DEC_DSP_IF */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_APPS_QTV_DEC_DSP_IF
  #define MSG_BUILD_MASK_MSG_SSID_APPS_QTV_DEC_DSP_IF    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_APPS_QTV_DEC_DSP_IF)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_APPS_QTV_DEC_DSP_IF

#endif

/* MSG_SSID_APPS_QTV_STREAM_RECORDING */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_APPS_QTV_STREAM_RECORDING
  #define MSG_BUILD_MASK_MSG_SSID_APPS_QTV_STREAM_RECORDING    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_APPS_QTV_STREAM_RECORDING)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_APPS_QTV_STREAM_RECORDING

#endif

/* MSG_SSID_APPS_QTV_CONFIGURATION */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_APPS_QTV_CONFIGURATION
  #define MSG_BUILD_MASK_MSG_SSID_APPS_QTV_CONFIGURATION    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_APPS_QTV_CONFIGURATION)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_APPS_QTV_CONFIGURATION

#endif

/* MSG_SSID_APPS_QCAMERA */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_APPS_QCAMERA
  #define MSG_BUILD_MASK_MSG_SSID_APPS_QCAMERA    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_APPS_QCAMERA)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_APPS_QCAMERA

#endif

/* MSG_SSID_APPS_QCAMCORDER */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_APPS_QCAMCORDER
  #define MSG_BUILD_MASK_MSG_SSID_APPS_QCAMCORDER    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_APPS_QCAMCORDER)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_APPS_QCAMCORDER

#endif

/* MSG_SSID_APPS_BREW */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_APPS_BREW
  #define MSG_BUILD_MASK_MSG_SSID_APPS_BREW    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_APPS_BREW)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_APPS_BREW

#endif

/* MSG_SSID_APPS_QDJ */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_APPS_QDJ
  #define MSG_BUILD_MASK_MSG_SSID_APPS_QDJ    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_APPS_QDJ)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_APPS_QDJ

#endif

/* MSG_SSID_APPS_QDTX */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_APPS_QDTX
  #define MSG_BUILD_MASK_MSG_SSID_APPS_QDTX    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_APPS_QDTX)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_APPS_QDTX

#endif

/* MSG_SSID_APPS_QTV_BCAST_FLO */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_APPS_QTV_BCAST_FLO
  #define MSG_BUILD_MASK_MSG_SSID_APPS_QTV_BCAST_FLO    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_APPS_QTV_BCAST_FLO)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_APPS_QTV_BCAST_FLO

#endif

/* MSG_SSID_APPS_MDP_GENERAL */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_APPS_MDP_GENERAL
  #define MSG_BUILD_MASK_MSG_SSID_APPS_MDP_GENERAL    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_APPS_MDP_GENERAL)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_APPS_MDP_GENERAL

#endif

/* MSG_SSID_APPS_PBM */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_APPS_PBM
  #define MSG_BUILD_MASK_MSG_SSID_APPS_PBM    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_APPS_PBM)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_APPS_PBM

#endif

/* MSG_SSID_APPS_GRAPHICS_GENERAL */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_APPS_GRAPHICS_GENERAL
  #define MSG_BUILD_MASK_MSG_SSID_APPS_GRAPHICS_GENERAL    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_APPS_GRAPHICS_GENERAL)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_APPS_GRAPHICS_GENERAL

#endif

/* MSG_SSID_APPS_GRAPHICS_EGL */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_APPS_GRAPHICS_EGL
  #define MSG_BUILD_MASK_MSG_SSID_APPS_GRAPHICS_EGL    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_APPS_GRAPHICS_EGL)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_APPS_GRAPHICS_EGL

#endif

/* MSG_SSID_APPS_GRAPHICS_OPENGL */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_APPS_GRAPHICS_OPENGL
  #define MSG_BUILD_MASK_MSG_SSID_APPS_GRAPHICS_OPENGL    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_APPS_GRAPHICS_OPENGL)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_APPS_GRAPHICS_OPENGL

#endif

/* MSG_SSID_APPS_GRAPHICS_DIRECT3D */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_APPS_GRAPHICS_DIRECT3D
  #define MSG_BUILD_MASK_MSG_SSID_APPS_GRAPHICS_DIRECT3D    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_APPS_GRAPHICS_DIRECT3D)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_APPS_GRAPHICS_DIRECT3D

#endif

/* MSG_SSID_APPS_GRAPHICS_SVG */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_APPS_GRAPHICS_SVG
  #define MSG_BUILD_MASK_MSG_SSID_APPS_GRAPHICS_SVG    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_APPS_GRAPHICS_SVG)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_APPS_GRAPHICS_SVG

#endif

/* MSG_SSID_APPS_GRAPHICS_OPENVG */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_APPS_GRAPHICS_OPENVG
  #define MSG_BUILD_MASK_MSG_SSID_APPS_GRAPHICS_OPENVG    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_APPS_GRAPHICS_OPENVG)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_APPS_GRAPHICS_OPENVG

#endif

/* MSG_SSID_APPS_GRAPHICS_2D */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_APPS_GRAPHICS_2D
  #define MSG_BUILD_MASK_MSG_SSID_APPS_GRAPHICS_2D    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_APPS_GRAPHICS_2D)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_APPS_GRAPHICS_2D

#endif

/* MSG_SSID_APPS_GRAPHICS_QXPROFILER */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_APPS_GRAPHICS_QXPROFILER
  #define MSG_BUILD_MASK_MSG_SSID_APPS_GRAPHICS_QXPROFILER    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_APPS_GRAPHICS_QXPROFILER)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_APPS_GRAPHICS_QXPROFILER

#endif

/* MSG_SSID_APPS_GRAPHICS_DSP */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_APPS_GRAPHICS_DSP
  #define MSG_BUILD_MASK_MSG_SSID_APPS_GRAPHICS_DSP    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_APPS_GRAPHICS_DSP)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_APPS_GRAPHICS_DSP

#endif

/* MSG_SSID_APPS_GRAPHICS_GRP */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_APPS_GRAPHICS_GRP
  #define MSG_BUILD_MASK_MSG_SSID_APPS_GRAPHICS_GRP    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_APPS_GRAPHICS_GRP)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_APPS_GRAPHICS_GRP

#endif

/* MSG_SSID_APPS_GRAPHICS_MDP */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_APPS_GRAPHICS_MDP
  #define MSG_BUILD_MASK_MSG_SSID_APPS_GRAPHICS_MDP    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_APPS_GRAPHICS_MDP)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_APPS_GRAPHICS_MDP

#endif

/* MSG_SSID_ADSPTASKS */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_ADSPTASKS
  #define MSG_BUILD_MASK_MSG_SSID_ADSPTASKS    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_ADSPTASKS)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_ADSPTASKS

#endif

/* MSG_SSID_ADSPTASKS_KERNEL */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_ADSPTASKS_KERNEL
  #define MSG_BUILD_MASK_MSG_SSID_ADSPTASKS_KERNEL    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_ADSPTASKS_KERNEL)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_ADSPTASKS_KERNEL

#endif

/* MSG_SSID_ADSPTASKS_AFETASK */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_ADSPTASKS_AFETASK
  #define MSG_BUILD_MASK_MSG_SSID_ADSPTASKS_AFETASK    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_ADSPTASKS_AFETASK)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_ADSPTASKS_AFETASK

#endif

/* MSG_SSID_ADSPTASKS_VOICEPROCTASK */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_ADSPTASKS_VOICEPROCTASK
  #define MSG_BUILD_MASK_MSG_SSID_ADSPTASKS_VOICEPROCTASK    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_ADSPTASKS_VOICEPROCTASK)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_ADSPTASKS_VOICEPROCTASK

#endif

/* MSG_SSID_ADSPTASKS_VOCDECTASK */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_ADSPTASKS_VOCDECTASK
  #define MSG_BUILD_MASK_MSG_SSID_ADSPTASKS_VOCDECTASK    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_ADSPTASKS_VOCDECTASK)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_ADSPTASKS_VOCDECTASK

#endif

/* MSG_SSID_ADSPTASKS_VOCENCTASK     */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_ADSPTASKS_VOCENCTASK
  #define MSG_BUILD_MASK_MSG_SSID_ADSPTASKS_VOCENCTASK    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_ADSPTASKS_VOCENCTASK)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_ADSPTASKS_VOCENCTASK

#endif

/* MSG_SSID_ADSPTASKS_VIDEOTASK */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_ADSPTASKS_VIDEOTASK
  #define MSG_BUILD_MASK_MSG_SSID_ADSPTASKS_VIDEOTASK    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_ADSPTASKS_VIDEOTASK)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_ADSPTASKS_VIDEOTASK

#endif

/* MSG_SSID_ADSPTASKS_VFETASK */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_ADSPTASKS_VFETASK
  #define MSG_BUILD_MASK_MSG_SSID_ADSPTASKS_VFETASK    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_ADSPTASKS_VFETASK)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_ADSPTASKS_VFETASK

#endif

/* MSG_SSID_ADSPTASKS_VIDEOENCTASK */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_ADSPTASKS_VIDEOENCTASK
  #define MSG_BUILD_MASK_MSG_SSID_ADSPTASKS_VIDEOENCTASK    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_ADSPTASKS_VIDEOENCTASK)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_ADSPTASKS_VIDEOENCTASK

#endif

/* MSG_SSID_ADSPTASKS_JPEGTASK */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_ADSPTASKS_JPEGTASK
  #define MSG_BUILD_MASK_MSG_SSID_ADSPTASKS_JPEGTASK    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_ADSPTASKS_JPEGTASK)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_ADSPTASKS_JPEGTASK

#endif

/* MSG_SSID_ADSPTASKS_AUDPPTASK */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_ADSPTASKS_AUDPPTASK
  #define MSG_BUILD_MASK_MSG_SSID_ADSPTASKS_AUDPPTASK    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_ADSPTASKS_AUDPPTASK)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_ADSPTASKS_AUDPPTASK

#endif

/* MSG_SSID_ADSPTASKS_AUDPLAY0TASK */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_ADSPTASKS_AUDPLAY0TASK
  #define MSG_BUILD_MASK_MSG_SSID_ADSPTASKS_AUDPLAY0TASK    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_ADSPTASKS_AUDPLAY0TASK)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_ADSPTASKS_AUDPLAY0TASK

#endif

/* MSG_SSID_ADSPTASKS_AUDPLAY1TASK */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_ADSPTASKS_AUDPLAY1TASK
  #define MSG_BUILD_MASK_MSG_SSID_ADSPTASKS_AUDPLAY1TASK    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_ADSPTASKS_AUDPLAY1TASK)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_ADSPTASKS_AUDPLAY1TASK

#endif

/* MSG_SSID_ADSPTASKS_AUDPLAY2TASK */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_ADSPTASKS_AUDPLAY2TASK
  #define MSG_BUILD_MASK_MSG_SSID_ADSPTASKS_AUDPLAY2TASK    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_ADSPTASKS_AUDPLAY2TASK)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_ADSPTASKS_AUDPLAY2TASK

#endif

/* MSG_SSID_ADSPTASKS_AUDPLAY3TASK */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_ADSPTASKS_AUDPLAY3TASK
  #define MSG_BUILD_MASK_MSG_SSID_ADSPTASKS_AUDPLAY3TASK    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_ADSPTASKS_AUDPLAY3TASK)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_ADSPTASKS_AUDPLAY3TASK

#endif

/* MSG_SSID_ADSPTASKS_AUDPLAY4TASK */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_ADSPTASKS_AUDPLAY4TASK
  #define MSG_BUILD_MASK_MSG_SSID_ADSPTASKS_AUDPLAY4TASK    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_ADSPTASKS_AUDPLAY4TASK)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_ADSPTASKS_AUDPLAY4TASK

#endif

/* MSG_SSID_ADSPTASKS_LPMTASK */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_ADSPTASKS_LPMTASK
  #define MSG_BUILD_MASK_MSG_SSID_ADSPTASKS_LPMTASK    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_ADSPTASKS_LPMTASK)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_ADSPTASKS_LPMTASK

#endif

/* MSG_SSID_ADSPTASKS_DIAGTASK */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_ADSPTASKS_DIAGTASK
  #define MSG_BUILD_MASK_MSG_SSID_ADSPTASKS_DIAGTASK    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_ADSPTASKS_DIAGTASK)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_ADSPTASKS_DIAGTASK

#endif

/* MSG_SSID_ADSPTASKS_AUDRECTASK */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_ADSPTASKS_AUDRECTASK
  #define MSG_BUILD_MASK_MSG_SSID_ADSPTASKS_AUDRECTASK    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_ADSPTASKS_AUDRECTASK)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_ADSPTASKS_AUDRECTASK

#endif

/* MSG_SSID_ADSPTASKS_AUDPREPROCTASK */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_ADSPTASKS_AUDPREPROCTASK
  #define MSG_BUILD_MASK_MSG_SSID_ADSPTASKS_AUDPREPROCTASK    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_ADSPTASKS_AUDPREPROCTASK)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_ADSPTASKS_AUDPREPROCTASK

#endif

/* MSG_SSID_ADSPTASKS_MODMATHTASK */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_ADSPTASKS_MODMATHTASK
  #define MSG_BUILD_MASK_MSG_SSID_ADSPTASKS_MODMATHTASK    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_ADSPTASKS_MODMATHTASK)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_ADSPTASKS_MODMATHTASK

#endif

/* MSG_SSID_ADSPTASKS_GRAPHICSTASK */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_ADSPTASKS_GRAPHICSTASK
  #define MSG_BUILD_MASK_MSG_SSID_ADSPTASKS_GRAPHICSTASK    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_ADSPTASKS_GRAPHICSTASK)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_ADSPTASKS_GRAPHICSTASK

#endif

/* OS WINCE SSIDs  */

/* MSG_SSID_L4LINUX_KERNEL */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_L4LINUX_KERNEL
  #define MSG_BUILD_MASK_MSG_SSID_L4LINUX_KERNEL    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_L4LINUX_KERNEL)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_L4LINUX_KERNEL

#endif

/* MSG_SSID_L4LINUX_KEYPAD */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_L4LINUX_KEYPAD
  #define MSG_BUILD_MASK_MSG_SSID_L4LINUX_KEYPAD    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_L4LINUX_KEYPAD)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_L4LINUX_KEYPAD

#endif

/* MSG_SSID_L4LINUX_APPS */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_L4LINUX_APPS
  #define MSG_BUILD_MASK_MSG_SSID_L4LINUX_APPS    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_L4LINUX_APPS)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_L4LINUX_APPS

#endif

/* MSG_SSID_L4LINUX_QDDAEMON */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_L4LINUX_QDDAEMON
  #define MSG_BUILD_MASK_MSG_SSID_L4LINUX_QDDAEMON    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_L4LINUX_QDDAEMON)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_L4LINUX_QDDAEMON

#endif

/* MSG_SSID_L4IGUANA_IGUANASERVER */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_L4IGUANA_IGUANASERVER
  #define MSG_BUILD_MASK_MSG_SSID_L4IGUANA_IGUANASERVER    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_L4IGUANA_IGUANASERVER)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_L4IGUANA_IGUANASERVER

#endif

/* MSG_SSID_L4IGUANA_EFS2 */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_L4IGUANA_EFS2
  #define MSG_BUILD_MASK_MSG_SSID_L4IGUANA_EFS2    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_L4IGUANA_EFS2)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_L4IGUANA_EFS2

#endif

/* MSG_SSID_L4IGUANA_QDMS */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_L4IGUANA_QDMS
  #define MSG_BUILD_MASK_MSG_SSID_L4IGUANA_QDMS    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_L4IGUANA_QDMS)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_L4IGUANA_QDMS

#endif

/* MSG_SSID_L4IGUANA_REX */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_L4IGUANA_REX
  #define MSG_BUILD_MASK_MSG_SSID_L4IGUANA_REX    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_L4IGUANA_REX)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_L4IGUANA_REX

#endif

/* MSG_SSID_L4IGUANA_SMMS */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_L4IGUANA_SMMS
  #define MSG_BUILD_MASK_MSG_SSID_L4IGUANA_SMMS    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_L4IGUANA_SMMS)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_L4IGUANA_SMMS

#endif

/* MSG_SSID_L4IGUANA_FRAMEBUFFER */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_L4IGUANA_FRAMEBUFFER
  #define MSG_BUILD_MASK_MSG_SSID_L4IGUANA_FRAMEBUFFER    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_L4IGUANA_FRAMEBUFFER)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_L4IGUANA_FRAMEBUFFER

#endif

/* MSG_SSID_L4IGUANA_KEYPAD */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_L4IGUANA_KEYPAD
  #define MSG_BUILD_MASK_MSG_SSID_L4IGUANA_KEYPAD    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_L4IGUANA_KEYPAD)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_L4IGUANA_KEYPAD

#endif

/* MSG_SSID_L4IGUANA_NAMING */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_L4IGUANA_NAMING
  #define MSG_BUILD_MASK_MSG_SSID_L4IGUANA_NAMING    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_L4IGUANA_NAMING)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_L4IGUANA_NAMING

#endif

/* MSG_SSID_L4IGUANA_SDIO */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_L4IGUANA_SDIO
  #define MSG_BUILD_MASK_MSG_SSID_L4IGUANA_SDIO    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_L4IGUANA_SDIO)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_L4IGUANA_SDIO

#endif

/* MSG_SSID_L4IGUANA_SERIAL */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_L4IGUANA_SERIAL
  #define MSG_BUILD_MASK_MSG_SSID_L4IGUANA_SERIAL    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_L4IGUANA_SERIAL)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_L4IGUANA_SERIAL

#endif

/* MSG_SSID_L4IGUANA_TIMER */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_L4IGUANA_TIMER
  #define MSG_BUILD_MASK_MSG_SSID_L4IGUANA_TIMER    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_L4IGUANA_TIMER)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_L4IGUANA_TIMER

#endif

/* MSG_SSID_L4IGUANA_TRAMP */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_L4IGUANA_TRAMP
  #define MSG_BUILD_MASK_MSG_SSID_L4IGUANA_TRAMP    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_L4IGUANA_TRAMP)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_L4IGUANA_TRAMP

#endif

/* MSG_SSID_L4AMSS_QDIAG */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_L4AMSS_QDIAG
  #define MSG_BUILD_MASK_MSG_SSID_L4AMSS_QDIAG    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_L4AMSS_QDIAG)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_L4AMSS_QDIAG

#endif

/* MSG_SSID_L4AMSS_APS */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_L4AMSS_APS
  #define MSG_BUILD_MASK_MSG_SSID_L4AMSS_APS    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_L4AMSS_APS)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_L4AMSS_APS

#endif



/* MSG_SSID_HIT */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_HIT
  #define MSG_BUILD_MASK_MSG_SSID_HIT    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_HIT)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_HIT

#endif


/* MSG_SSID_QDSP6 */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_QDSP6
  #define MSG_BUILD_MASK_MSG_SSID_QDSP6    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_QDSP6)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_QDSP6

#endif

/* MSG_SSID_ADSP_AUD_SVC */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_ADSP_AUD_SVC
  #define MSG_BUILD_MASK_MSG_SSID_ADSP_AUD_SVC    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_ADSP_AUD_SVC)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_ADSP_AUD_SVC

#endif /* MSG_SSID_ADSP_AUD_SVC */

/* MSG_SSID_ADSP_AUD_ENCDEC */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_ADSP_AUD_ENCDEC
  #define MSG_BUILD_MASK_MSG_SSID_ADSP_AUD_ENCDEC    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_ADSP_AUD_ENCDEC)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_ADSP_AUD_ENCDEC

#endif /* MSG_SSID_ADSP_AUD_ENCDEC */

/* MSG_SSID_ADSP_AUD_VOC */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_ADSP_AUD_VOC
  #define MSG_BUILD_MASK_MSG_SSID_ADSP_AUD_VOC    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_ADSP_AUD_VOC)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_ADSP_AUD_VOC

#endif /* MSG_SSID_ADSP_AUD_VOC */

/* MSG_SSID_ADSP_AUD_VS */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_ADSP_AUD_VS
  #define MSG_BUILD_MASK_MSG_SSID_ADSP_AUD_VS    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_ADSP_AUD_VS)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_ADSP_AUD_VS

#endif /* MSG_SSID_ADSP_AUD_VS */

/* MSG_SSID_ADSP_AUD_MIDI */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_ADSP_AUD_MIDI
  #define MSG_BUILD_MASK_MSG_SSID_ADSP_AUD_MIDI   MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_ADSP_AUD_MIDI)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_ADSP_AUD_MIDI

#endif /* MSG_SSID_ADSP_AUD_MIDI */

/* MSG_SSID_ADSP_AUD_POSTPROC */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_ADSP_AUD_POSTPROC
  #define MSG_BUILD_MASK_MSG_SSID_ADSP_AUD_POSTPROC   MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_ADSP_AUD_POSTPROC)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_ADSP_AUD_POSTPROC

#endif /* MSG_SSID_ADSP_AUD_POSTPROC */

/* MSG_SSID_ADSP_AUD_PREPROC */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_ADSP_AUD_PREPROC
  #define MSG_BUILD_MASK_MSG_SSID_ADSP_AUD_PREPROC   MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_ADSP_AUD_PREPROC)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_ADSP_AUD_PREPROC

#endif /* MSG_SSID_ADSP_AUD_PREPROC */

/* MSG_SSID_ADSP_AUD_AFE  */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_ADSP_AUD_AFE
  #define MSG_BUILD_MASK_MSG_SSID_ADSP_AUD_AFE   MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_ADSP_AUD_AFE)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_ADSP_AUD_AFE

#endif /* MSG_SSID_ADSP_AUD_AFE  */

/* MSG_SSID_ADSP_AUD_MSESSION  */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_ADSP_AUD_MSESSION
  #define MSG_BUILD_MASK_MSG_SSID_ADSP_AUD_MSESSION    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_ADSP_AUD_MSESSION)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_ADSP_AUD_MSESSION

#endif /* MSG_SSID_ADSP_AUD_MSESSION   */

/* MSG_SSID_ADSP_AUD_DSESSION   */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_ADSP_AUD_DSESSION
  #define MSG_BUILD_MASK_MSG_SSID_ADSP_AUD_DSESSION    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_ADSP_AUD_DSESSION)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_ADSP_AUD_DSESSION

#endif /* MSG_SSID_ADSP_AUD_DSESSION   */

/* MSG_SSID_ADSP_AUD_DCM   */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_ADSP_AUD_DCM
  #define MSG_BUILD_MASK_MSG_SSID_ADSP_AUD_DCM    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_ADSP_AUD_DCM)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_ADSP_AUD_DCM

#endif /* MSG_SSID_ADSP_AUD_DCM  */

/* MSG_SSID_ADSP_VID_ENC    */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_ADSP_VID_ENC
  #define MSG_BUILD_MASK_MSG_SSID_ADSP_VID_ENC     MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_ADSP_VID_ENC)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_ADSP_VID_ENC

#endif /* MSG_SSID_ADSP_VID_ENC  */

/* MSG_SSID_ADSP_VID_ENCRPC  */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_ADSP_VID_ENCRPC
  #define MSG_BUILD_MASK_MSG_SSID_ADSP_VID_ENCRPC     MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_ADSP_VID_ENCRPC)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_ADSP_VID_ENCRPC

#endif /* MSG_SSID_ADSP_VID_ENCRPC  */

/* MSG_SSID_ADSP_VID_DEC  */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_ADSP_VID_DEC
  #define MSG_BUILD_MASK_MSG_SSID_ADSP_VID_DEC     MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_ADSP_VID_DEC)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_ADSP_VID_DEC

#endif /* MSG_SSID_ADSP_VID_DEC  */

/* MSG_SSID_ADSP_VID_DECRPC  */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_ADSP_VID_DECRPC
  #define MSG_BUILD_MASK_MSG_SSID_ADSP_VID_DECRPC     MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_ADSP_VID_DECRPC)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_ADSP_VID_DECRPC

#endif /* MSG_SSID_ADSP_VID_DECRPC  */

/* MSG_SSID_ADSP_VID_COMMONSW  */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_ADSP_VID_COMMONSW
  #define MSG_BUILD_MASK_MSG_SSID_ADSP_VID_COMMONSW     MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_ADSP_VID_COMMONSW)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_ADSP_VID_COMMONSW

#endif /* MSG_SSID_ADSP_VID_COMMONSW  */

/* MSG_SSID_ADSP_VID_HWDRIVER  */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_ADSP_VID_HWDRIVER
  #define MSG_BUILD_MASK_MSG_SSID_ADSP_VID_HWDRIVER    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_ADSP_VID_HWDRIVER)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_ADSP_VID_HWDRIVER

#endif /* MSG_SSID_ADSP_VID_HWDRIVER  */

/* MSG_SSID_ADSP_JPG_ENC  */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_ADSP_JPG_ENC
  #define MSG_BUILD_MASK_MSG_SSID_ADSP_JPG_ENC   MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_ADSP_JPG_ENC)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_ADSP_JPG_ENC

#endif /* MSG_SSID_ADSP_JPG_ENC  */

/* MSG_SSID_ADSP_JPG_DEC  */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_ADSP_JPG_DEC
  #define MSG_BUILD_MASK_MSG_SSID_ADSP_JPG_DEC   MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_ADSP_JPG_DEC)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_ADSP_JPG_DEC

#endif /* MSG_SSID_ADSP_JPG_DEC  */

/* MSG_SSID_ADSP_OMM  */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_ADSP_OMM
  #define MSG_BUILD_MASK_MSG_SSID_ADSP_OMM    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_ADSP_OMM)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_ADSP_OMM

#endif /* MSG_SSID_ADSP_OMM   */

/* MSG_SSID_ADSP_PWRDEM  */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_ADSP_PWRDEM
  #define MSG_BUILD_MASK_MSG_SSID_ADSP_PWRDEM    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_ADSP_PWRDEM)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_ADSP_PWRDEM

#endif /* MSG_SSID_ADSP_PWRDEM  */

/* MSG_SSID_ADSP_RESMGR  */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_ADSP_RESMGR
  #define MSG_BUILD_MASK_MSG_SSID_ADSP_RESMGR    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_ADSP_RESMGR)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_ADSP_RESMGR

#endif /* MSG_SSID_ADSP_RESMGR  */

/* MSG_SSID_ADSP_CORE  */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_ADSP_CORE
  #define MSG_BUILD_MASK_MSG_SSID_ADSP_CORE    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_ADSP_CORE)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_ADSP_CORE

#endif /* MSG_SSID_ADSP_CORE  */

/* MSG_SSID_LTE */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_LTE
  #define MSG_BUILD_MASK_MSG_SSID_LTE    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_LTE)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_LTE

#endif

/* MSG_SSID_LTE_RRC */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_LTE_RRC
  #define MSG_BUILD_MASK_MSG_SSID_LTE_RRC    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_LTE_RRC)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_LTE_RRC

#endif

/* MSG_SSID_LTE_MACUL */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_LTE_MACUL
  #define MSG_BUILD_MASK_MSG_SSID_LTE_MACUL    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_LTE_MACUL)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_LTE_MACUL

#endif

/* MSG_SSID_LTE_MACDL */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_LTE_MACDL
  #define MSG_BUILD_MASK_MSG_SSID_LTE_MACDL    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_LTE_MACDL)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_LTE_MACDL

#endif

/* MSG_SSID_LTE_MACCTRL */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_LTE_MACCTRL
  #define MSG_BUILD_MASK_MSG_SSID_LTE_MACCTRL    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_LTE_MACCTRL)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_LTE_MACCTRL

#endif

/* MSG_SSID_LTE_RLCUL */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_LTE_RLCUL
  #define MSG_BUILD_MASK_MSG_SSID_LTE_RLCUL    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_LTE_RLCUL)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_LTE_RLCUL

#endif

/* MSG_SSID_LTE_RLCDL */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_LTE_RLCDL
  #define MSG_BUILD_MASK_MSG_SSID_LTE_RLCDL   MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_LTE_RLCDL)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_LTE_RLCDL

#endif

/* MSG_SSID_LTE_PDCPUL */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_LTE_PDCPUL
  #define MSG_BUILD_MASK_MSG_SSID_LTE_PDCPUL   MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_LTE_PDCPUL)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_LTE_PDCPUL

#endif

/* MSG_SSID_LTE_PDCPDL */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_LTE_PDCPDL
  #define MSG_BUILD_MASK_MSG_SSID_LTE_PDCPDL   MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_LTE_PDCPDL)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_LTE_PDCPDL

#endif

/* MSG_SSID_LTE_ML1 */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_LTE_ML1
  #define MSG_BUILD_MASK_MSG_SSID_LTE_ML1   MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_LTE_ML1)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_LTE_ML1

#endif

/* MSG_SSID_UMB */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_UMB
  #define MSG_BUILD_MASK_MSG_SSID_UMB    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_UMB)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_UMB

#endif

/* MSG_SSID_UMB_APP */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_UMB_APP
  #define MSG_BUILD_MASK_MSG_SSID_UMB_APP    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_UMB_APP)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_UMB_APP

#endif

/* MSG_SSID_UMB_DS */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_UMB_DS
  #define MSG_BUILD_MASK_MSG_SSID_UMB_DS    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_UMB_DS)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_UMB_DS

#endif

/* MSG_SSID_UMB_CP */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_UMB_CP
  #define MSG_BUILD_MASK_MSG_SSID_UMB_CP    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_UMB_CP)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_UMB_CP

#endif

/* MSG_SSID_UMB_RLL */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_UMB_RLL
  #define MSG_BUILD_MASK_MSG_SSID_UMB_RLL    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_UMB_RLL)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_UMB_RLL

#endif

/* MSG_SSID_UMB_MAC */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_UMB_MAC
  #define MSG_BUILD_MASK_MSG_SSID_UMB_MAC    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_UMB_MAC)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_UMB_MAC

#endif

/* MSG_SSID_UMB_SRCH */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_UMB_SRCH
  #define MSG_BUILD_MASK_MSG_SSID_UMB_SRCH    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_UMB_SRCH)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_UMB_SRCH

#endif

/* MSG_SSID_UMB_FW */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_UMB_FW
  #define MSG_BUILD_MASK_MSG_SSID_UMB_FW    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_UMB_FW)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_UMB_FW

#endif

/* MSG_SSID_UMB_PLT */

/* In case it is not defined in msgtgt.h */
#ifndef MSG_BUILD_MASK_MSG_SSID_UMB_PLT
  #define MSG_BUILD_MASK_MSG_SSID_UMB_PLT    MSG_BUILD_MASK_DFLT
#endif

/* If the build environment has specified this file to translate legacy
   messages to the this SSID. */
#if defined (MSG_BT_SSID_DFLT) && (MSG_BT_SSID_DFLT==MSG_SSID_UMB_PLT)

  #undef MSG_SSID_DFLT
  #define MSG_SSID_DFLT MSG_BT_SSID_DFLT

  #undef MSG_BUILD_MASK_MSG_SSID_DFLT
  #define MSG_BUILD_MASK_MSG_SSID_DFLT MSG_BUILD_MASK_MSG_SSID_UMB_PLT

#endif

/* These SSIDs are reserved for OEM (customer) use only. These IDs will
   never be used by Qualcomm. */
#define MSG_FIRST_TARGET_OEM_SSID (0xC000)
#define MSG_LAST_TARGET_OEM_SSID (0xCFFF)

/* Macros to generate the Build Mask and Run Time tables */

/* Important Note: This needs to be modified manually now,
when we add a new RANGE of SSIDs to the msg_mask_tbl */
#define MSG_MASK_TBL_CNT      24
/*!
@endcond  DOXYGEN_BLOAT
*/
#ifdef __cplusplus
}
#endif

#endif         /* MSGCFG_H */
