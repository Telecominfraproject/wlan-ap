/*==========================================================================

                     FTM FM Common Header File

Description
  Global Data declarations of the ftm fm component.

# Copyright (c) 2010-2012, 2014 by Qualcomm Technologies, Inc.  All Rights Reserved.
# Qualcomm Technologies Proprietary and Confidential.

===========================================================================*/

/*===========================================================================

                         Edit History


when       who     what, where, why
--------   ---     ----------------------------------------------------------
08/03/2011 uppalas  Adding support for new ftm commands
06/18/10   rakeshk  Created a header file to hold the definitons for ftm fm
                    task
07/06/10   rakeshk  Clean roomed the data structures and defined data
                    structures to be passed to the PFAL layers
01/11/11   rakeshk  Added support for new FTM APIS
02/09/11   rakeshk  Added support for BLER FTM APIs
04/03/11   ananthk  Added support for FM FTM Transmit APIs
===========================================================================*/
#ifdef CONFIG_FTM_FM

#include "diagpkt.h"
#include "log.h"
#include <sys/types.h>

#define FTM_FM_LOG_PKT_ID 65
#define FTM_FM_CMD_CODE 28
#define LOG_FTM_FM_C  ((uint16) 0x14CC)
#define FEATURE_FTM_FM_DEBUG
#define DEFAULT_DATA_SIZE 249

/* FM6500 A0 chip version.
 **/
#define FM6500_A0_VERSION                     (0x01010013)
/**
 *  * FM6500 2.0 chip version.
 **/
#define FMQSOCCOM_FM6500_20_VERSION           (0x01010010)
/**
 *  * FM6500 2.1 chip version.
 **/
#define FMQSOCCOM_FM6500_21_VERSION           (0x02010204)
/**
 *  WCN 2243 1.0's FM chip version.
 */
#define FMQSOCCOM_FM6500_WCN2243_10_VERSION   (0x0302010A)
/**
 *  WCN 2243 2.0's FM chip version.
 */
#define FMQSOCCOM_FM6500_WCN2243_20_VERSION   (0x04020205)

extern int chipVersion;

/* RDS Group processing parameters */
#define FM_RX_RDS_GRP_RT_EBL         1
#define FM_RX_RDS_GRP_PS_EBL         2
#define FM_RX_RDS_GRP_AF_EBL         4
#ifdef FM_SOC_TYPE_CHEROKEE
#define FM_RX_RDS_GRP_PS_SIMPLE_EBL  8
#define FM_RX_RDS_GRP_ECC_EBL        32
#define FM_RX_RDS_GRP_PTYN_EBL       64
#define FM_RX_RDS_GRP_RT_PLUS_EBL    128
#else
#define FM_RX_RDS_GRP_PS_SIMPLE_EBL  16
#endif


/* lower and upper band limits of regions */
#define REGION_US_EU_BAND_LOW              87500
#define REGION_US_EU_BAND_HIGH             107900
#define REGION_JAPAN_STANDARD_BAND_LOW     76000
#define REGION_JAPAN_STANDARD_BAND_HIGH    90000
#define REGION_JAPAN_WIDE_BAND_LOW         90000
#define REGION_JAPAN_WIDE_BAND_HIGH        108000
#define V4L2_CID_PRIVATE_BASE 0x08000000
#define MAX_RDS_PS_LENGTH 108
#define MAX_RDS_RT_LENGTH  64
#define V4L2_CID_PRIVATE_IRIS_RDS_GRP_COUNTERS_EXT    0x08000042

typedef enum  {
V4L2_CID_PRIVATE_IRIS_HLSI = (V4L2_CID_PRIVATE_BASE + 0x1d),
V4L2_CID_PRIVATE_IRIS_SOFT_MUTE,
V4L2_CID_PRIVATE_IRIS_RIVA_ACCS_ADDR,
V4L2_CID_PRIVATE_IRIS_RIVA_ACCS_LEN,
V4L2_CID_PRIVATE_IRIS_RIVA_PEEK,
V4L2_CID_PRIVATE_IRIS_RIVA_POKE,
V4L2_CID_PRIVATE_IRIS_SSBI_ACCS_ADDR,
V4L2_CID_PRIVATE_IRIS_SSBI_PEEK,
V4L2_CID_PRIVATE_IRIS_SSBI_POKE,
V4L2_CID_PRIVATE_IRIS_TX_TONE,
V4L2_CID_PRIVATE_IRIS_RDS_GRP_COUNTERS,
V4L2_CID_PRIVATE_IRIS_SET_NOTCH_FILTER,
V4L2_CID_PRIVATE_IRIS_AGC_CTRL = 0x08000043,
V4L2_CID_PRIVATE_IRIS_AGC_STATE,
V4L2_CID_PRIVATE_IRIS_READ_DEFAULT = 0x00980928,//using private CIDs under userclass
V4L2_CID_PRIVATE_IRIS_WRITE_DEFAULT,
}v4l2_cid_private_iris_t_copy;
typedef enum
{
  /* Total no. of PS names that can be transmitted       : 12
     Width of each transmitted PS name is                : 8
     Total no. of PS characters  that can be transmitted : (12*8 = 96)
  */
  MAX_TX_PS_LEN         = 96,
  MAX_TX_PS_RPT_CNT     = 15,
}FmTxPSFeatures;

/* FTM FM command IDs */
typedef enum
{
#ifdef FEATURE_FTM_FM_DEBUG
  FTM_FM_RX_SET_POWER_MODE         = 13,
  FTM_FM_RX_SET_SIGNAL_THRESHOLD   = 14,
  FTM_FM_RX_GET_RSSI_LIMIT         = 16,
  FTM_FM_RX_GET_PS_INFO            = 17,
  FTM_FM_RX_GET_RT_INFO            = 18,
  FTM_FM_RX_GET_AF_INFO            = 19,
  FTM_FM_RX_SEARCH_STATIONS        = 20,
  FTM_FM_RX_SEARCH_RDS_STATIONS    = 21,
  FTM_FM_RX_SEARCH_STATIONS_LIST   = 22,
  FTM_FM_RX_CANCEL_SEARCH          = 23,
  FTM_FM_RX_RDS_GROUP_PROC_OPTIONS = 25,
  FTM_FM_RX_RDS_PI_MATCH_OPTIONS   = 26,
  FTM_FM_TX_GET_PS_FEATURES        = 36,
  FTM_FM_TX_TX_PS_INFO             = 38,
  FTM_FM_TX_STOP_PS_INFO_TX        = 39,
  FTM_FM_TX_TX_RT_INFO             = 40,
  FTM_FM_TX_STOP_RT_INFO_TX        = 41,
  FTM_FM_RX_GET_SIGNAL_THRESHOLD   = 46,
  FTM_FM_FMWAN_REG_RD              = 51,
  FTM_FM_RX_GET_DEFAULTS           = 62,
  FTM_FM_RX_SET_DEFAULTS           = 63,
  FTM_FM_RX_GET_SINR_SAMPLES       = 64,
  FTM_FM_RX_SET_SINR_SAMPLES       = 65,
  FTM_FM_RX_GET_SINR_THRESHOLD     = 66,
  FTM_FM_RX_SET_SINR_THRESHOLD     = 67,
  FTM_FM_RX_GET_ONCHANNEL_TH       = 68,
  FTM_FM_RX_SET_ONCHANNEL_TH       = 69,
  FTM_FM_RX_GET_OFFCHANNEL_TH      = 70,
  FTM_FM_RX_SET_OFFCHANNEL_TH      = 71,
  FTM_FM_TX_PWR_LVL_CFG            = 72,
#endif /* FEATURE_FTM_FM_DEBUG */

  FTM_FM_RX_ENABLE_RECEIVER        = 7,
  FTM_FM_RX_DISABLE_RECEIVER       = 8,
  FTM_FM_RX_CONFIGURE_RECEIVER     = 9,
  FTM_FM_RX_SET_MUTE_MODE          = 10,
  FTM_FM_RX_SET_STEREO_MODE        = 11,
  FTM_FM_RX_SET_STATION            = 12,
  FTM_FM_RX_GET_STATION_PARAMETERS = 15,
  FTM_FM_RX_RDS_GROUP_OPTIONS      = 24,
  FTM_FM_TX_ENABLE_TRANSMITTER     = 33,
  FTM_FM_TX_DISABLE_TRANSMITTER    = 34,
  FTM_FM_TX_CONFIGURE_TRANSMITTER  = 35,
  FTM_FM_TX_SET_STATION            = 37,
  FTM_FM_TX_TX_RDS_GROUPS          = 42,
  FTM_FM_TX_TX_CONT_RDS_GROUPS     = 43,
  FTM_FM_TX_TX_RDS_CTRL            = 44,
  FTM_FM_TX_GET_RDS_GROUP_BUF_SIZE = 45,
  FTM_FM_BUS_WRITE                 = 47,
  FTM_FM_BUS_READ                  = 48,
  FTM_FM_NOTIFY_WAN                = 49,
  FTM_FM_NOTIFY_FM                 = 50,
  FTM_FM_ROUTE_AUDIO               = 52,
  FTM_FM_RX_SET_AF_THRESHOLD       = 53,
  FTM_FM_RX_SET_RSSI_CHECK_TIMER   = 54,
  FTM_FM_RX_SET_RDS_PI_TIMER       = 55,
  FTM_FM_RX_GET_AF_THRESHOLD       = 56,
  FTM_FM_RX_GET_RSSI_CHECK_TIMER   = 57,
  FTM_FM_RX_GET_RDS_PI_TIMER       = 58,
  FTM_FM_RX_GET_RDS_ERR_COUNT      = 59,
  FTM_FM_RX_RESET_RDS_ERR_COUNT    = 60,
  FTM_FM_TX_SEARCH_STATIONS        = 61,
  FTM_FM_SET_HLSI                  = 100,
  FTM_FM_SET_SOFT_MUTE             = 101,
  FTM_FM_SET_ANTENNA               = 102,
  FTM_FM_SET_NOTCH_FILTER          = 103,
  FTM_FM_READ_RDS_GRP_CNTRS        = 104,
  FTM_FM_SET_TONE_GENERATION       = 105,
  FTM_FM_PEEK_SSBI                 = 106,
  FTM_FM_POKE_SSBI                 = 107,
  FTM_FM_PEEK_RIVA_WORD            = 108,
  FTM_FM_POKE_RIVA_WORD            = 109,
  FTM_FM_ENABLE_AUDIO              = 111,
  FTM_FM_DISABLE_AUDIO             = 112,
  FTM_FM_VOLUME_SETTING            = 113,
  FTM_FM_READ_RDS_GRP_CNTRS_EXT    = 114,
  FTM_FM_SET_GET_RESET_AGC         = 115,
  FTM_FM_MAX
} ftm_fm_sub_cmd_type;

#define XFR_CTRL_OFFSET 0x1F
/* Wait time for ensuring XFR is generated */
#define WAIT_ON_ISR_DELAY 15000 //15 ms
#define AFTH_OFFSET 0x2E
#define CHCOND_OFFSET 0x22
#define RDSTIMEOUT_OFFSET 0x25
#define FM_SLAVE_ADDR 0x2A
#define RDSERR_OFFSET 0x24
#define RDSRESET_OFFSET 0x20
#define BLOCKS_PER_GROUP 0x04
#define FTM_FM_RDS_COUNT 0x11


#define MAX_RIVA_DATA_LEN  245
#define MAX_RIVA_PEEK_RSP_SIZE 251
#define SSBI_PEEK_DATA_SIZE 1

#define IRIS_BUF_PEEK  6
#define IRIS_BUF_SSBI_PEEK IRIS_BUF_PEEK+1
#define IRIS_BUF_RDS_CNTRS  IRIS_BUF_SSBI_PEEK+1
#define IRIS_BUF_RD_DEFAULT IRIS_BUF_RDS_CNTRS+1
#ifdef FM_SOC_TYPE_CHEROKEE
#define RDS_GRP_CNTRS_SIZE 48
#else
#define RDS_GRP_CNTRS_SIZE 36
#endif
/* Generic result, used for any command that only returns an error code */
typedef enum
{
  FTM_FM_SUCCESS,
  FTM_FAIL,
  FTM_FILE_DOES_NOT_EXIST,
  FTM_MMC_ERROR,
  FTM_FM_UNRECOGNIZED_CMD,
  FTM_NO_RESOURCES,
  FTM_FM_PENDING,
  FTM_INVALID_PARAM,
  FTM_FM_DISALLOWED,
  FTM_TEST_NOT_IMPLEMENTED,
  FTM_CUST_HW_ID_UNKNOWN,
  FTM_FM_BUS_WRITE_ERROR,
  FTM_FM_BUS_READ_ERROR,
  FTM_FM_CLIENT_MAX,

} ftm_fm_api_result_type;

/* FM power state enum */
typedef enum
{
  FM_POWER_OFF,
  FM_POWER_TRANSITION,
  FM_RX_ON,
  FM_TX_ON
}fm_power_state;

/* FM command status enum */
typedef enum
{
  FM_CMD_SUCCESS,
  FM_CMD_PENDING,
  FM_CMD_NO_RESOURCES,
  FM_CMD_INVALID_PARAM,
  FM_CMD_DISALLOWED,
  FM_CMD_UNRECOGNIZED_CMD,
  FM_CMD_FAILURE
}fm_cmd_status_type;

/**
*  FM event result.
*/
typedef enum
{
   FM_EV_SUCCESS  = 0,
   /**<  Event indicates success. */

   FM_EV_FAILURE  = 1,
   /**<  Event is a response to a command that failed */

   FM_EV_CMD_DISALLOWED = 2,
   /**<  Event is a response to a command that was disallowed. */

   FM_EV_CMD_INVALID_PARAM = 3
   /**<  Event is a response to a command that contained an invalid parameter. */

} FmEvResultType;

/**
*  FM Receiver event names.
*/
typedef enum
{
   /* -----------------------------------------------
         1 -> FM Receiver initialization events
      ----------------------------------------------- */

   FM_RX_EV_ENABLE_RECEIVER = 0,

   FM_RX_EV_DISABLE_RECEIVER,

   FM_RX_EV_CFG_RECEIVER,

   /* -----------------------------------------------
         2 ->       FM receiver control events
      ----------------------------------------------- */

   FM_RX_EV_MUTE_MODE_SET,

   FM_RX_EV_STEREO_MODE_SET,

   FM_RX_EV_RADIO_STATION_SET,

   FM_RX_EV_PWR_MODE_SET,

   FM_RX_EV_SET_SIGNAL_THRESHOLD,

   /* -----------------------------------------------
         3 ->       FM receiver status events
      ----------------------------------------------- */

   FM_RX_EV_RADIO_TUNE_STATUS,

   FM_RX_EV_STATION_PARAMETERS,

   FM_RX_EV_RDS_LOCK_STATUS,

   FM_RX_EV_STEREO_STATUS,

   FM_RX_EV_SERVICE_AVAILABLE,

   FM_RX_EV_GET_SIGNAL_THRESHOLD,

   /* -----------------------------------------------
         4 ->       FM search status events
      ----------------------------------------------- */

   FM_RX_EV_SEARCH_IN_PROGRESS,

   FM_RX_EV_SEARCH_RDS_IN_PROGRESS,

   FM_RX_EV_SEARCH_LIST_IN_PROGRESS,

   FM_RX_EV_SEARCH_COMPLETE,

   FM_RX_EV_SEARCH_RDS_COMPLETE,

   FM_RX_EV_SEARCH_LIST_COMPLETE,

   FM_RX_EV_SEARCH_CANCELLED,

   /* -----------------------------------------------
         5 ->       FM RDS status events
      ----------------------------------------------- */

   FM_RX_EV_RDS_GROUP_DATA,

   FM_RX_EV_RDS_PS_INFO,

   FM_RX_EV_RDS_RT_INFO,

   FM_RX_EV_RDS_AF_INFO,

   FM_RX_EV_RDS_PI_MATCH_AVAILABLE,

   /* -----------------------------------------------
         6 ->   FM RDS control events
      ----------------------------------------------- */

   FM_RX_EV_RDS_GROUP_OPTIONS_SET,

   FM_RX_EV_RDS_PROC_REG_DONE,

   FM_RX_EV_RDS_PI_MATCH_REG_DONE,

   FM_RX_EV_MAX_EVENT

} FmRxEventType;

typedef enum radio_band_type
{
  FM_US_EU = 0x0,
  FM_JAPAN_STANDARD = 0x1,
  FM_JAPAN_WIDE = 0x2,
  FM_USER_DEFINED = 0x4
}radio_band_type;

typedef enum emphasis_type
{
  FM_RX_EMP75 = 0x0,
  FM_RX_EMP50 = 0x1
}emphasis_type;

typedef enum channel_space_type
{
  FM_RX_SPACE_200KHZ = 0x0,
  FM_RX_SPACE_100KHZ = 0x1,
  FM_RX_SPACE_50KHZ = 0x2
}channel_space_type;

typedef enum rds_system_type
{
 FM_RX_RDBS_SYSTEM = 0x0,
 FM_RX_RDS_SYSTEM = 0x1,
 FM_RX_NO_RDS_SYSTEM = 0x2
}rds_sytem_type;

typedef struct band_limit_freq
{
  uint32 lower_limit;
  uint32 upper_limit;
}band_limit_freq;


typedef enum rds_sync_type
{
 FM_RDS_NOT_SYNCED = 0x0,
 FM_RDS_SYNCED = 0x1
}rds_sync_type;

typedef enum stereo_type
{
 FM_RX_MONO = 0x0,
 FM_RX_STEREO = 0x1
}stereo_type;

typedef enum fm_service_available
{
  FM_SERVICE_NOT_AVAILABLE = 0x0,
  FM_SERVICE_AVAILABLE = 0x1
}fm_service_available;

typedef enum mute_type
{
  FM_RX_NO_MUTE = 0x00,
  FM_RX_MUTE_RIGHT = 0x01,
  FM_RX_MUTE_LEFT = 0x02,
  FM_RX_MUTE_BOTH = 0x03
}mute_type;

typedef enum antenna_type
{
  WIRED_HS,
  PWB_ANT
}antenna_type;

typedef enum audio_output
{
  HEADSET,
  SPEAKER,
} audio_output;
/**
*  RDS/RBDS Program Type type.
*/
typedef uint8  fm_prgm_type;

/**
*   RDS/RBDS Program Identification type.
*/
typedef uint16 fm_prgmid_type;
/**
*  RDS/RBDS Program Services type.
*/
typedef char fm_prm_services;
/**
*  RDS/RBDS Radio Text type.
*/
/*
* FM RX RIVA peek request
*/
typedef  struct fm_riva_peek_word
{
  uint8   subOpcode;
  uint32 startaddress;
  uint8 payload_length;/*In Bytes*/
  uint8 data[MAX_RIVA_DATA_LEN];
}__attribute__((packed))fm_riva_peek_word;

/*
* FM RX RIVA poke request
*/
typedef struct fm_riva_poke_word
{
 uint8   subOpcode;
 uint32 startaddress;
 uint8 payload_length;/*In Bytes*/
 uint8 data[MAX_RIVA_DATA_LEN];
}__attribute__((packed))fm_riva_poke_word ;


/*
* FM RX SSBI peek/poke request
*/
typedef struct fm_ssbi_poke_reg
{
  uint16 startaddress;
  uint8 data;
}__attribute__((packed))fm_ssbi_poke_reg;

/*
* fm Set Get Reset AGC request
*/
typedef struct fm_set_get_reset_agc_req
{
  uint8 ucCtrl;
  uint8 ucGainState;
}__attribute__((packed))fm_set_get_reset_agc_req;

typedef struct fm_set_get_reset_agc_params
{
  uint8 ucCurrentGainState;
  uint8 ucGainStateChange1;
  uint8 ucGainStateChange2;
  uint8 ucGainStateChange3;
}__attribute__((packed))fm_set_get_reset_agc_params;

typedef PACKED struct
{
  uint8  status ;
  uint8  data_length ;
  uint8  data[DEFAULT_DATA_SIZE];
}__attribute__((packed)) readDefaults_data;

typedef PACKED struct
{
  diagpkt_subsys_header_type  header ; /*Diag header*/
  uint8  status ;
  uint8  data_length ;
  uint8  data[DEFAULT_DATA_SIZE];
}__attribute__((packed)) default_read_rsp;

/*RDS Group counters*/
typedef  struct fm_rds_grp_cntrsparams
{
  uint32 totalRdsSBlockErrors;
  uint32 totalRdsGroups;
  uint32 totalRdsGroup0;
  uint32 totalRdsGroup2;
  uint32 totalRdsBlockB;
  uint32 totalRdsProcessedGroup0;
  uint32 totalRdsProcessedGroup2;
  uint32 totalRdsGroupFiltered;
  uint32 totalRdsChangeFiltered;
}__attribute__((packed)) fm_rds_grp_cntrsparams;

/*RDS Group counters extended */
typedef  struct fm_rds_grpcntrs_extendedparams
{
  uint32 totalRdsSyncLoss;
  uint32 totalRdsNotSync;
  uint32 totalRdsSyncInt;
}__attribute__((packed)) fm_rds_grpcntrs_extendedparams;

typedef char fm_radiotext_info;
/**
*  FM Global Paramaters struct.
*/
typedef struct
{
  uint32  current_station_freq;/*a frequency in kHz the band range*/
  uint8 service_available;
  uint8 rssi; /* rssi range from 0-100*/
  uint8 stype;
  uint8 rds_sync_status;
  uint8 mute_status;
  uint8 ssbi_peek_data;
  fm_prgmid_type pgm_id; /* Program Id */
  fm_prgm_type pgm_type; /* Program type */
  fm_prm_services  pgm_services[MAX_RDS_PS_LENGTH];
  fm_radiotext_info  radio_text[MAX_RDS_RT_LENGTH];/* RT maximum is 64 bytes */
  fm_riva_poke_word  riva_data_access_params;
  fm_set_get_reset_agc_params set_get_reset_agc_params;
  fm_rds_grp_cntrsparams rds_group_counters;
  fm_rds_grpcntrs_extendedparams rds_group_counters_extended;
  readDefaults_data  default_read_data;
  uint8 fm_ps_length;
  uint8 fm_rt_length;
  uint8 sinr_samples;
  char sinr_threshold;
  uint8 On_channel_threshold;
  uint8 Off_channel_threshold;
}fm_station_params_available;
/**
*  FM Config Request structure.
*/
typedef struct fm_config_data
{
  uint8 band;
  uint8 emphasis;
  uint8 spacing;
  uint8 rds_system;
  band_limit_freq bandlimits;
  uint8 is_fm_tx_on;
}fm_config_data;

/*
* FM RDS Options Config Request
*/
typedef struct fm_rds_options
{
  uint32 rds_group_mask;
  uint32 rds_group_buffer_size;
  uint8 rds_change_filter;
}fm_rds_options;
/*
* FM RX Search stations request
*/
typedef struct fm_search_stations
{
  uint8 search_mode;
  uint8 dwell_period;
  uint8 search_dir;
}fm_search_stations;

/*
* FM RX Search DDS stations request
*/
typedef struct fm_search_rds_stations
{
  uint8 search_mode;
  uint8 dwell_period;
  uint8 search_dir;
  uint8 program_type;
  uint16 program_id;
}fm_search_rds_stations;

/*
* FM RX Search station lists request
*/
typedef struct fm_search_list_stations
{
  uint8 search_mode;
  uint8 search_dir;
  uint32 srch_list_max;
  /**< Maximum number of stations that can be returned from a search. */
  uint8 program_type;
}fm_search_list_stations;

/*
* FM RX I2C request
*/
typedef struct fm_i2c_params
{
  uint8 slaveaddress;
  uint8 offset;
  uint8 payload_length;
  uint8 data[64];
}fm_i2c_params;

/* Structure containing the RDS PS Info to be transmitted */
typedef struct _tsFtmFmRdsTxPsType
{
  uint32                ulPSStrLen;
  /**< The size of the cTxPSStrPtr buffer.
  */

  uint32                ucTxPSRptCnt;
  /**< The number of times each 8 character string is repeated before the next
       string is transmitted.
  */

  uint16                tusTxPi;
  /**< RDS/RBDS Program Identification to use for Program Service transmissions.
  */

  uint8                 tucTxPSPty;
  /**< The RDS/RBDS Program Type to transmit.
  */

  const char            cTxPSStrPtr[108];
  /**< A pointer to a buffer containing the Program Service string to transmit
       (must be null terminated).
  */

} tsFtmFmRdsTxPsType;

typedef struct _tsFtmFmRdsTxRtType
{
  uint32                ulRTStrLen;
  /**< The size of the cTxRTStrPtr buffer.
  */

  uint16                tusTxPi;
  /**< RDS/RBDS Program Identification to use for RadioText transmissions.
  */

  uint8                 tucTxRTPty;
  /**< The RDS/RBDS Program Type to transmit.
  */

  const char            cTxRTStrPtr[65];
  /**< A pointer to a buffer containing the RadioText string to transmit
       (must be null terminated).
   */

} tsFtmFmRdsTxRtType;

typedef struct _ftm_def_data_rd_req
{
  uint8    mode;
  uint8    length;
  uint8    param_len;
  uint8    param;
} __attribute__((packed))ftm_fm_def_data_rd_req;

typedef struct _ftm_def_data_wr_req
{
  uint8    mode;
  uint8    length;
  uint8    data[DEFAULT_DATA_SIZE];
} __attribute__((packed))ftm_fm_def_data_wr_req;

typedef PACKED struct
{
  diagpkt_subsys_header_type  header ; /*Diag header*/
  char                        result ;/* result */
  uint8                       length; /*RDS PS string length*/
  uint8                       string[MAX_RDS_PS_LENGTH]; /* RDS string */
}__attribute__((packed)) fmrdsps_response;


typedef PACKED struct
{
  diagpkt_subsys_header_type  header ; /*Diag header*/
  char                        result ;/* result */
  uint8                       length; /*RDS PS string length*/
  uint8                       string[MAX_RDS_RT_LENGTH]; /* RDS string */
}__attribute__((packed)) fmrdsrt_response;


/**
*  FM All Request Union type.
*/
typedef union fm_cfg_request
{
  fm_config_data cfg_param;
  uint8 mute_param;
  uint8 stereo_param;
  uint32 freq;
  fm_rds_options rds_options;
  uint8 power_mode;
  uint8 signal_threshold;
  fm_search_stations search_stations_options;
  fm_search_rds_stations search_rds_stations_options;
  fm_search_list_stations search_list_stations_options;
  fm_i2c_params i2c_params;
  uint32 rds_group_options;
  uint16 rx_af_threshold;
  uint8 rx_rssi_checktimer;
  uint rx_rds_pi_timer;
  tsFtmFmRdsTxPsType tuFmPSParams;
  tsFtmFmRdsTxRtType tuFmRTParams;
  uint8 soft_mute_param;
  uint8 antenna_type;
  uint8 tx_tone_param;
  uint8 rds_grp_counters;
  uint8 rds_grp_counters_ext;
  uint8 hlsi;
  uint8 sinr_samples;
  char sinr_threshold;
  uint8 On_channel_threshold;
  uint8 Off_channel_threshold;
  uint8 notch;
  fm_riva_peek_word riva_peek_params;
  fm_riva_poke_word riva_data_access_params;
  fm_ssbi_poke_reg ssbi_access_params;
  fm_set_get_reset_agc_req set_get_agc_req_parameters;
  ftm_fm_def_data_rd_req rd_default;
  ftm_fm_def_data_wr_req wr_default;
  uint8 tx_pwr_cfg;
  uint8 audio_output;
  uint8 audio_vlm;
}fm_cfg_request;

/* FTM FM request type */
typedef PACKED struct
{
  diagpkt_cmd_code_type              cmd_code;
  diagpkt_subsys_id_type             subsys_id;
  diagpkt_subsys_cmd_code_type       subsys_cmd_code;
  uint16                             cmd_id; /* command id (required) */
  uint16                             cmd_data_len;
  uint16                             cmd_rsp_pkt_size;
  byte                               data[1];
}__attribute__((packed))ftm_fm_pkt_type;

/* Set MuteMode Response */
typedef PACKED struct
{
  diagpkt_subsys_header_type  header ; /*Diag header*/
  char                        result ;/* result */
  uint8                       mutemode;
}__attribute__((packed)) mutemode_response;

/* Set StereoMode Response */
typedef PACKED struct
{
  diagpkt_subsys_header_type  header ; /*Diag header*/
  char                        result ;/* result */
  uint8                       stereomode;
}__attribute__((packed)) stereomode_response;

/* I2C Response */
typedef PACKED struct
{
  diagpkt_subsys_header_type  header ; /*Diag header*/
  char                        result ;/* result */
  uint32                      length; /*length of data read */
  uint8                       data[64]; /* I2C read dat buffer */
}__attribute__((packed)) fmbusread_response;

typedef PACKED struct
{
  diagpkt_subsys_header_type  header ; /*Diag header*/
  char                        result ;/* result */
  uint8                       sub_opcode;
  uint32                      start_address;
  uint8                       length; /*length of data read */
  uint8                       data[MAX_RIVA_DATA_LEN]; /* read dat buffer */
}__attribute__((packed)) rivaData_response;


typedef PACKED struct
{
  diagpkt_subsys_header_type  header ; /*Diag header*/
  char                        result ;/* result */
  uint8                       data;
}__attribute__((packed)) ssbiPeek_response;

typedef PACKED struct
{
  diagpkt_subsys_header_type  header ; /*Diag header*/
  char                        result ;/* result */
  uint8                       uccurrentgainstate;
  uint8                       ucgainstatechange1;
  uint8                       ucgainstatechange2;
  uint8                       ucgainstatechange3;
}__attribute__((packed)) set_get_reset_agc_response;

/*Read RDS Group counters responce*/
typedef PACKED struct
{
  diagpkt_subsys_header_type  header ; /*Diag header*/
  char                        result ;/* result */
  fm_rds_grp_cntrsparams  read_rds_cntrs;
}__attribute__((packed)) ReadRDSCntrs_responce;

/*Read RDS Group counters response*/
typedef PACKED struct
{
  diagpkt_subsys_header_type  header ; /*Diag header*/
  char                        result ;/* result */
  fm_rds_grpcntrs_extendedparams  read_rds_cntrs_ext;
}__attribute__((packed)) ReadRDSCntrs_ext_response;


/* Generic Response */
typedef PACKED struct
{
  diagpkt_subsys_header_type  header ; /*Diag header*/
  char                        result ;/* result */
}__attribute__((packed)) generic_response;

typedef PACKED struct
{
  diagpkt_subsys_header_type  header ;
  char	                      result ;
  uint16                      afthreshold;
}  fmrxsetafthreshold_response;

typedef PACKED struct
{
  diagpkt_subsys_header_type  header ;
  char                        result ;
  uint8                       sinr_sample;
}  getsinrsamples_response;

typedef PACKED struct
{
  diagpkt_subsys_header_type  header ;
  char                        result ;
  char                        sinr_threshold;
}  getsinrthreshold_response;

typedef PACKED struct
{
  diagpkt_subsys_header_type  header ;
  char                        result ;
  uint8                       sinr_on_th;
}  getonchannelthreshold_response;
typedef PACKED struct
{
  diagpkt_subsys_header_type  header ;
  char                        result ;
  uint8                       sinr_off_th;
}  getoffchannelthreshold_response;

typedef PACKED struct
{
  diagpkt_subsys_header_type  header ;
  char                        result ;
  uint8                       rssitimer;
}  fmrxsetrssichecktimer_response;

typedef PACKED struct
{
  diagpkt_subsys_header_type  header ;
  char                        result ;
  uint8                       rdspitimer;
}  fmrxsetrdspitimer_response;

typedef PACKED struct
{
  diagpkt_subsys_header_type  header ;
  char                        result ;
  uint8                       threshold;
}  threshold_response;

typedef PACKED struct
{
  diagpkt_subsys_header_type  header ;
  char                        result ;
  uint32                      rdserrcount;
  uint32                      numofblocks;
}  rds_err_count_response;

/* Custom response for Get station parameters request */
struct fm_rx_get_station_parameters_response_t
{
  diagpkt_subsys_header_type  header ; /*Diag header*/
  char                        result ;/* result */
  uint32                      stationFreq;
  /* The currently tuned frequency in kHz (Example: 96500 -> 96.5Mhz)*/
  uint8                       servAvble;
  /* The current service available indicator for the current station */
  uint8                       rssi;
  /* The current signal strength level (0-100 range). */
  uint8                       stereoProgram;
  /* The current mono/stereo indicator for this station */
  uint8                       rdsSyncStatus;
  /* The current RDS/RBDS synchronization status */
  uint8                       muteMode;
  /* The current FM mute mode */
}__attribute__((packed));

/* FTM Log Packet - Used to send back the event of a HCI Command */
typedef PACKED struct
{
  log_hdr_type hdr;
  byte         EvName;
   /* Event ID indicates which event is being returned. */
  byte         EvResult;
  byte         data[1];         /* Variable length payload,
                                  look at FTM log id for contents */
} ftm_fm_log_pkt_type;
#define FTM_FM_LOG_HEADER_SIZE (sizeof (ftm_fm_log_pkt_type) - 1)

typedef struct fm_rx_get_station_parameters_response_t fm_rx_get_station_parameters_response;
/*===========================================================================
FUNCTION   ftm_fm_dispatch

DESCRIPTION
  Dispatch routine for the various FM Rx/Tx commands. Copies the data into
  a global union data structure before calling the processing routine

DEPENDENCIES
  NIL

RETURN VALUE
  A Packed structre pointer including the response to the FTM FM packet

SIDE EFFECTS
  None

===========================================================================*/

void * ftm_fm_dispatch(ftm_fm_pkt_type *ftm_fm_pkt, uint16 length );


/*===========================================================================

FUNCTION      ftm_fm_enable_audio

DESCRIPTION
  This function is used to take the audio output mode from QRCT.

DEPENDENCIES
  none

===========================================================================*/
PACKED void* ftm_fm_enable_audio( void );
PACKED void* ftm_fm_disable_audio( void );
PACKED void* ftm_fm_setting_volume(void);

#endif /* CONFIG_FTM_FM */
