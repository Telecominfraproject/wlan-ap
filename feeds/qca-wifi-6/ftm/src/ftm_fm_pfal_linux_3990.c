/*==========================================================================
*
* Description
*  Platform specific routines to program FM over Uart transport
* Copyright (c) 2010-2017 Qualcomm Technologies, Inc.
* All Rights Reserved.
* Confidential and Proprietary - Qualcomm Technologies, Inc.
*
*===========================================================================*/

/*===========================================================================

                         Edit History


when       who     what, where, why
--------   ---     ----------------------------------------------------------
3/10/16   ssugasi  Add FTM support for FM module for uart based chipsets
==========================================================================*/

#include "ftm_fm_pfal.h"
#include <linux/videodev2.h>
#include <math.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <string.h>
#ifdef ANDROID
#include <cutils/properties.h>
#endif
#include <dlfcn.h>
#include <stdio.h>
#include <signal.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>
#include "ftm_common.h"
#include "radio-helium-commands.h"
#include <stdint.h>
#define BIT16 (1<<16)

/* Multiplying factor to convert to Radio freqeuncy */
#define TUNE_MULT 16000
/* Constant to request for Radio Events */
#define EVENT_LISTEN 1
/* 1000 multiplier */
#define MULTIPLE_1000 1000
/* Tavaura I2C address */
int SLAVE_ADDR = 0x2A;
/* Tavaura I2C statu register*/
#define INTSTAT_0 0x0
#define ARRAY_SIZE(a)   (sizeof(a) / sizeof(*a))
/* Debug Macro */
#define FTM_DEBUG
#ifdef FTM_DEBUG
#define print(x) printf(x)
#define print2(x,y) printf(x,y)
#define print3(x,y,z) printf(x,y,z)
#else
#define print(x)
#define print2(x,y)
#define print3(x,y,z)
#endif
typedef unsigned char boolean;

#define TAVARUA_BUF_PS_RDS 3
#define V4L2_CID_PRIVATE_TAVARUA_REGION 0x08000007
#define V4L2_CID_PRIVATE_TAVARUA_TX_SETPSREPEATCOUNT 0x08000015
#define V4L2_CID_PRIVATE_TAVARUA_STOP_RDS_TX_PS_NAME 0x08000016
#define V4L2_CID_PRIVATE_TAVARUA_STOP_RDS_TX_RT 0x08000017
#define V4L2_CID_PRIVATE_TAVARUA_STATE 0x08000004
#define V4L2_CID_PRIVATE_TAVARUA_SET_AUDIO_PATH 0x8000029
#define V4L2_CID_PRIVATE_TAVARUA_RDSON 0x0800000F
#define V4L2_CID_PRIVATE_TAVARUA_RDSGROUP_PROC 0x08000010
#define V4L2_CID_PRIVATE_TAVARUA_RDSGROUP_MASK 0x08000006
#define V4L2_CID_PRIVATE_TAVARUA_RDSD_BUF 0x08000013
#define V4L2_CID_PRIVATE_TAVARUA_ANTENNA 0x08000012
#define V4L2_CID_PRIVATE_TAVARUA_RDSON 0x0800000F
#define V4L2_CID_PRIVATE_TAVARUA_EMPHASIS 0x0800000C
#define V4L2_CID_PRIVATE_TAVARUA_SPACING 0x0800000E
#define V4L2_CID_PRIVATE_TAVARUA_RDS_STD 0x0800000D
#define V4L2_CID_PRIVATE_TAVARUA_LP_MODE 0x08000011
#define V4L2_CID_PRIVATE_TAVARUA_SRCHMODE 0x08000001
#define V4L2_CID_PRIVATE_TAVARUA_SCANDWELL 0x08000002
#define V4L2_CID_PRIVATE_TAVARUA_SRCH_PI 0x0800000A
#define V4L2_CID_PRIVATE_TAVARUA_SRCH_CNT 0x0800000B
#define V4L2_CID_PRIVATE_TAVARUA_SRCH_PTY 0x08000009
#define V4L2_CID_PRIVATE_TAVARUA_SRCHON 0x08000003
#define V4L2_CID_PRIVATE_INTF_LOW_THRESHOLD 0x800002D
#define V4L2_CID_PRIVATE_INTF_HIGH_THRESHOLD 0x800002E
#define V4L2_CID_PRIVATE_SINR_THRESHOLD 0x800002F
#define V4L2_CID_PRIVATE_SINR_SAMPLES 0x8000030
#define V4L2_CID_PRIVATE_TAVARUA_SIGNAL_TH 0x08000008
#define V4L2_CID_PRV_ENABLE_SLIMBUS 0x00980940
#define SRCH_DIR_UP                 (0)
#define SRCH_DIR_DOWN               (1)

#define FM_TX_PWR_LVL_0         0 /* Lowest power lvl that can be set for Tx */
#define FM_TX_PWR_LVL_MAX       7 /* Max power lvl for Tx */
#define RDS_Tx                  0x80
const char* fm_i2c_path_8660 = "/dev/i2c-4";
const char* fm_i2c_path_7x30 = "/dev/i2c-2";
const char* fm_i2c_path_7627a = "/dev/i2c-1";
/* To get the current status of PS/RT transmission */
volatile unsigned char is_rt_transmitting = 0;
volatile unsigned char is_ps_transmitting = 0;
int ftm_audio_fd = -1;
const char *const audio_config = "-c /vendor/etc/ftm_test_config";
const unsigned int CMD_len = 16;
const int config_len = 31;
const int ext_config_len = 39;
const int sound_card_len = 200;
const char *const mm_audio_path = "/vendor/bin/mm-audio-ftm";
char *FM_LIBRARY_NAME = "fm_helium.so";
char *FM_LIBRARY_SYMBOL_NAME = "FM_HELIUM_LIB_INTERFACE";
void *lib_handle;

fm_config_data * fmconfig_ptr;
typedef void (*enb_result_cb)();
typedef void (*tune_rsp_cb)(int Freq);
typedef void (*seek_rsp_cb)(int Freq);
typedef void (*scan_rsp_cb)();
typedef void (*srch_list_rsp_cb)(uint16_t *scan_tbl);
typedef void (*stereo_mode_cb)(boolean status);
typedef void (*rds_avl_sts_cb)(boolean status);
typedef void (*af_list_cb)(uint16_t *af_list);
typedef void (*rt_cb)(char *rt);
typedef void (*ps_cb)(char *ps);
typedef void (*oda_cb)();
typedef void (*rt_plus_cb)(char *rt_plus);
typedef void (*ert_cb)(char *ert);
typedef void (*disable_cb)();
typedef void (*callback_thread_event)(unsigned int evt);
typedef void (*rds_grp_cntrs_cb)(char *rds_params);
typedef void (*rds_grp_cntrs_ext_cb)(char *rds_params);
typedef void (*fm_peek_cb)(char *peek_rsp);
typedef void (*fm_ssbi_peek_cb)(char *ssbi_peek_rsp);
typedef void (*fm_agc_gain_cb)(char *agc_gain_rsp);
typedef void (*fm_ch_det_th_cb)(char *ch_det_rsp);
typedef void (*fm_sig_thr_cb) (int val, int status);
typedef void (*fm_get_ch_det_thrs_cb) (int val, int status);
typedef void (*fm_def_data_rd_cb) (int val, int status);
typedef void (*fm_get_blnd_cb) (int val, int status);
typedef void (*fm_set_ch_det_thrs_cb) (int status);
typedef void (*fm_def_data_wrt_cb) (int status);
typedef void (*fm_set_blnd_cb) (int status);
typedef void (*fm_get_stn_prm_cb) (int val, int status);
typedef void (*fm_get_stn_dbg_prm_cb) (int val, int status);
typedef void (*fm_ecc_evt_cb)(char *ecc_rsp);
typedef void (*fm_enable_sb_cb) (int status);

/* enum to montior the Power On status */
typedef enum
{
  INPROGRESS,
  COMPLETE
}poweron_status;

boolean cmd_queued = FALSE;
/* Resourcse Numbers for Rx/TX */
int FM_RX = 1;
int FM_TX = 2;
/* Boolean to control the power down sequence */
volatile boolean power_down = FALSE;
/* V4L2 radio handle */
int fd_radio = -1;
/* FM asynchornous thread to perform the long running ON */
pthread_t fm_interrupt_thread,fm_on_thread;
/* Prototype ofFM ON thread */
void *(ftm_on_long_thread)(void *ptr);
/* Global state ofthe FM task */
fm_station_params_available fm_global_params;

volatile poweron_status poweron;

int chipVersion = 0;
extern volatile fm_power_state fmPowerState;
static char transport[PROPERTY_VALUE_MAX];

typedef struct {
    size_t  size;
    enb_result_cb  enabled_cb;
    tune_rsp_cb tune_cb;
    seek_rsp_cb  seek_cmpl_cb;
    scan_rsp_cb  scan_next_cb;
    srch_list_rsp_cb  srch_list_cb;
    stereo_mode_cb  stereo_status_cb;
    rds_avl_sts_cb  rds_avail_status_cb;
    af_list_cb  af_list_update_cb;
    rt_cb  rt_update_cb;
    ps_cb  ps_update_cb;
    oda_cb  oda_update_cb;
    rt_plus_cb  rt_plus_update_cb;
    ert_cb  ert_update_cb;
    disable_cb  disabled_cb;
    rds_grp_cntrs_cb rds_grp_cntrs_rsp_cb;
   rds_grp_cntrs_ext_cb rds_grp_cntrs_ext_rsp_cb;
    fm_peek_cb fm_peek_rsp_cb;
    fm_ssbi_peek_cb fm_ssbi_peek_rsp_cb;
    fm_agc_gain_cb fm_agc_gain_rsp_cb;
    fm_ch_det_th_cb fm_ch_det_th_rsp_cb;
    fm_ecc_evt_cb fm_ext_country_code_cb;
    callback_thread_event thread_evt_cb;
    fm_sig_thr_cb fm_get_sig_thres_cb;
    fm_get_ch_det_thrs_cb fm_get_ch_det_thr_cb;
    fm_def_data_rd_cb fm_def_data_read_cb;
    fm_get_blnd_cb fm_get_blend_cb;
    fm_set_ch_det_thrs_cb fm_set_ch_det_thr_cb;
    fm_def_data_wrt_cb fm_def_data_write_cb;
    fm_set_blnd_cb fm_set_blend_cb;
    fm_get_stn_prm_cb fm_get_station_param_cb;
    fm_get_stn_dbg_prm_cb fm_get_station_debug_param_cb;
    fm_enable_sb_cb fm_enable_slimbus_cb;
} fm_vendor_callbacks_t;

typedef struct {
    int (*hal_init)(fm_vendor_callbacks_t *p_cb);
    int (*set_fm_ctrl)(int ioctl, int val);
    int (*get_fm_ctrl) (int ioctl, int* val);
} fm_interface_t;

void fm_enabled_cb() {
    ALOGE("Entered %s", __func__);
}

void fm_tune_cb(int Freq)
{
   ALOGE("TUNE:Freq:%d", Freq);
   fm_global_params.current_station_freq =Freq;
}

void fm_seek_cmpl_cb(int Freq)
{
    ALOGE("SEEK_CMPL: Freq: %d", Freq);
    fm_global_params.current_station_freq =Freq;
}

void fm_scan_next_cb()
{
    ALOGE("SCAN_NEXT");
}

void fm_srch_list_cb(uint16_t *scan_tbl)
{
    ALOGE("SRCH_LIST");
}

void fm_stereo_status_cb(boolean stereo)
{
   ALOGE("STEREO: %d", stereo);
   fm_global_params.stype = stereo;
}

void fm_rds_avail_status_cb(boolean rds_avl)
{
   ALOGE("fm_rds_avail_status_cb: %d", rds_avl);
   fm_global_params.rds_sync_status = rds_avl;
}

void fm_rt_update_cb(char *rt)
{
    ALOGE("Entered %s", __func__);
    int radiotext_size = (int)(rt[0] & 0xFF);
    fm_global_params.fm_rt_length = radiotext_size;
    print2("radio text size = %d\n",fm_global_params.fm_rt_length);
    fm_global_params.pgm_id = (((rt[2] & 0xFF) << 8) | (rt[3] & 0xFF));
    fm_global_params.pgm_type = (int)( rt[1] & 0x1F);
    memset(fm_global_params.radio_text,0x0,MAX_RDS_RT_LENGTH);
    memcpy(fm_global_params.radio_text,&rt[5],radiotext_size);
}

void fm_ps_update_cb(char *buf)
{
    ALOGE("Entered %s", __func__);
    int num_of_ps = (int)(buf[0] & 0xFF);
    int ps_services_len = ((int )((num_of_ps*8) + 5)) - 5;
    fm_global_params.fm_ps_length = ps_services_len;
    fm_global_params.pgm_id = (((buf[2] & 0xFF) << 8) | (buf[3] & 0xFF));
    fm_global_params.pgm_type = (int)( buf[1] & 0x1F);
    memset(fm_global_params.pgm_services,0x0,MAX_RDS_PS_LENGTH);
    memcpy(fm_global_params.pgm_services,&buf[5],ps_services_len);
    fm_global_params.pgm_services[ps_services_len] = '\0';
    print2("Pid = %d\n",fm_global_params.pgm_id);
    print2("Ptype = %d\n",fm_global_params.pgm_type);
    print2("PS name %s\n",fm_global_params.pgm_services);

}
void fm_oda_update_cb()
{
   ALOGD("ODA_EVT");
}

void fm_af_list_update_cb(uint16_t *af_list)
{
   ALOGD("Entered %s", __func__);

}
void fm_rt_plus_update_cb(char *rt_plus)
{
   ALOGD("Entered %s", __func__);
}

void fm_ert_update_cb(char *ert)
{
   ALOGD("Entered %s", __func__);
}
void fm_disabled_cb()
{
   ALOGD("Entered %s", __func__);
}

void rds_grp_cntrs_rsp_cb(char *rds_grp_cntr_buff)
{
    ALOGV("Entered %s", __func__);
    struct fm_rds_grp_cntrsparams  *response = (struct fm_rds_grp_cntrsparams *)rds_grp_cntr_buff;
    ALOGI("copy RDS grp counter response ");
    memcpy((void*)&fm_global_params.rds_group_counters,(void*)response,RDS_GRP_CNTRS_SIZE);
    pthread_mutex_lock(&fm_event_lock);
    pthread_cond_signal(&fm_event_cond);
    pthread_mutex_unlock(&fm_event_lock);
    ALOGD("Exit %s", __func__);
}
void rds_grp_cntrs_ext_rsp_cb(char * rds_grp_cntr_buff)
{
    ALOGD("rds_grp_cntrs_ext_rsp_cb");
    struct fm_rds_grpcntrs_extendedparams  *response = (struct fm_rds_grpcntrs_extendedparams*)rds_grp_cntr_buff;
    memcpy((void*)&fm_global_params.rds_group_counters_extended,(void*)response,sizeof(fm_rds_grpcntrs_extendedparams));
    ALOGV("  response totalRdsSyncLoss:%d\n  totalRdsNotSync:%d\n totalRdsSyncInt:%d\n ",
            fm_global_params.rds_group_counters_extended.totalRdsNotSync,
            fm_global_params.rds_group_counters_extended.totalRdsSyncInt,
            fm_global_params.rds_group_counters_extended.totalRdsSyncLoss);
    ALOGV("Exit %s", __func__);
}

void fm_peek_rsp_cb(char *peek_rsp) {
  struct fm_riva_poke_word  *responce = (struct fm_riva_poke_word *)peek_rsp;
  memcpy((void*)&fm_global_params.riva_data_access_params,
         (void*)responce,sizeof(struct fm_riva_poke_word));
}

void fm_ssbi_peek_rsp_cb(char *ssbi_peek_rsp){
    fm_global_params.ssbi_peek_data = ssbi_peek_rsp[0];
}

void fm_agc_gain_rsp_cb(char *agc_gain_Resp){
    ALOGD("Entered %s", __func__);
    struct fm_set_get_reset_agc_params *response =(
                            struct fm_set_get_reset_agc_params *)agc_gain_Resp;
    memcpy((void*)&fm_global_params.set_get_reset_agc_params,
                            (void*)response, sizeof(struct fm_set_get_reset_agc_params));

    ALOGV("Get AGC gain state success\n");
    ALOGV("Current state: %x\nUcGainStateChng1: %x\nUcGainStateChng2: %x\nUcGainStateChng3: %x",
                fm_global_params.set_get_reset_agc_params.ucCurrentGainState,
                fm_global_params.set_get_reset_agc_params.ucGainStateChange1,
                fm_global_params.set_get_reset_agc_params.ucGainStateChange2,
                fm_global_params.set_get_reset_agc_params.ucGainStateChange3);
}

void fm_ch_det_th_rsp_cb(char *ch_det_rsp){
    ALOGD("Entered %s", __func__);
}

void fm_thread_evt_cb(unsigned int event)
{
   ALOGE("Entered %s", __func__);
}

 static void fm_get_sig_thres_cb(int val, int status)
{
    ALOGD("Get signal Thres callback");
}

static void fm_get_ch_det_thr_cb(int val, int status)
{
    ALOGI("fm_get_ch_det_thr_cb");
}

static void fm_set_ch_det_thr_cb(int status)
{
    ALOGE("fm_set_ch_det_thr_cb");
}

static void fm_def_data_read_cb(int val, int status)
{
    ALOGE("fm_def_data_read_cb");
}

static void fm_def_data_write_cb(int status)
{
    ALOGE("fm_def_data_write_cb");
}

static void fm_get_blend_cb(int val, int status)
{
    ALOGE("fm_get_blend_cb");
}

static void fm_set_blend_cb(int status)
{
    ALOGE("fm_set_blend_cb");
}

static void fm_get_station_param_cb(int val, int status)
{
    if (status == 0)
        fm_global_params.rssi = val;
    else
        fm_global_params.rssi = 0;
        pthread_mutex_lock(&fm_event_lock);
        pthread_cond_signal(&fm_event_cond);
        pthread_mutex_unlock(&fm_event_lock);
    ALOGD("fm_get_station_param_cb rssi =%d\n",fm_global_params.rssi);
}

static void fm_get_station_debug_param_cb(int val, int status)
{
    ALOGE("fm_get_station_debug_param_cb");
}

static void fm_ext_country_code_cb(char *ecc_rsp)
{
     ALOGD("Entered %s", __func__);
}

static void fm_enable_slimbus_cb(int status)
{
    ALOGD("%s status %d", __func__, status);
    pthread_mutex_lock(&fm_event_lock);
    pthread_cond_signal(&fm_event_cond);
    pthread_mutex_unlock(&fm_event_lock);
}

fm_interface_t *vendor_interface;
static   fm_vendor_callbacks_t fm_callbacks = {
    sizeof(fm_callbacks),
    fm_enabled_cb,
    fm_tune_cb,
    fm_seek_cmpl_cb,
    fm_scan_next_cb,
    fm_srch_list_cb,
    fm_stereo_status_cb,
    fm_rds_avail_status_cb,
    fm_af_list_update_cb,
    fm_rt_update_cb,
    fm_ps_update_cb,
    fm_oda_update_cb,
    fm_rt_plus_update_cb,
    fm_ert_update_cb,
    fm_disabled_cb,
    rds_grp_cntrs_rsp_cb,
    rds_grp_cntrs_ext_rsp_cb,
    fm_peek_rsp_cb,
    fm_ssbi_peek_rsp_cb,
    fm_agc_gain_rsp_cb,
    fm_ch_det_th_rsp_cb,
    fm_ext_country_code_cb,
    fm_thread_evt_cb,
    fm_get_sig_thres_cb,
    fm_get_ch_det_thr_cb,
    fm_def_data_read_cb,
    fm_get_blend_cb,
    fm_set_ch_det_thr_cb,
    fm_def_data_write_cb,
    fm_set_blend_cb,
    fm_get_station_param_cb,
    fm_get_station_debug_param_cb,
    fm_enable_slimbus_cb
};

/*===========================================================================
FUNCTION  extract_rds_grp_cntr_data

DESCRIPTION
  Helper routine to read the data from the V4L2 buffer
  following a Read Rds Group counters command complete

DEPENDENCIES
 NIL

RETURN VALUE
  void

SIDE EFFECTS
  Updates the Global data strutures member rds_group_counters

===========================================================================*/

void extract_rds_grp_cntr_data()
{
  uint8 buf[RDS_GRP_CNTRS_SIZE];
  struct RDSCntrsParams  *responce = (struct RDSCntrsParams *)buf;
  memcpy((void*)&fm_global_params.rds_group_counters,(void*)responce,sizeof(buf));
}
/*===========================================================================
FUNCTION  extract_default_read_data

DESCRIPTION
  Helper routine to read the data from the V4L2 buffer
  following a defaultRead command complete

DEPENDENCIES
 NIL

RETURN VALUE
  void

SIDE EFFECTS
  Updates the Global data struture member rd_default

===========================================================================*/

void extract_default_read_data()
{
 print("Not implemented");
}
/*===========================================================================
FUNCTION reset_rds

DESCRIPTION
  Reset the existing RDS data.

DEPENDENCIES
  Radio event

RETURN VALUE
  void

IDE EFFECTS
  Reset the Global RDS data info like RT, PS etc.
===========================================================================*/

void reset_rds()
{
  memset(fm_global_params.pgm_services,0x0,MAX_RDS_PS_LENGTH);
  memset(fm_global_params.radio_text,0x0,MAX_RDS_RT_LENGTH);
  fm_global_params.pgm_id = 0;
  fm_global_params.pgm_type = 0;
}

struct timespec* set_time_out(int secs)
{
  struct timespec *ts;
  struct timeval tp;
  ts = malloc(sizeof(struct timespec));
  if(!ts)
  {
      printf("malloc failed");
      return NULL;
  }
  gettimeofday(&tp, NULL);
  ts->tv_sec = tp.tv_sec;
  ts->tv_nsec = tp.tv_usec * 1000;
  ts->tv_sec += secs;

  return ts;
}

/*===========================================================================
FUNCTION  EnableFM

DESCRIPTION
  PFAL specific routine to enable the FM receiver/transmitter with the Radio
  Configuration parameters passed.

PLATFORM SPECIFIC DESCRIPTION
  Opens the handle to /dev/radio0 V4L2 device and intiates a Soc Patch
  download, configurs the Init parameters like emphasis, channel spacing,
  band limit, RDS type, frequency band, and Radio State.

DEPENDENCIES
  NIL

RETURN VALUE
  FM command status

SIDE EFFECTS
  None

===========================================================================*/
fm_cmd_status_type EnableFM
(
  fm_config_data*      radiocfgptr
)
{
 int status;
#ifdef FTM_DEBUG

  if(radiocfgptr->is_fm_tx_on)
    print("\nEnable Transmitter entry\n");
  else
    print("\nEnable Receiver entry\n");
#endif

  lib_handle = dlopen(FM_LIBRARY_NAME, RTLD_NOW);
  if (!lib_handle) {
     ALOGE("%s unable to open %s: %s", __func__, FM_LIBRARY_NAME, dlerror());
     lib_handle = NULL;
     return FM_CMD_NO_RESOURCES;
  }

  printf("Obtaining handle: '%s' to the shared object library...\n", FM_LIBRARY_SYMBOL_NAME);
  vendor_interface = (fm_interface_t *)dlsym(lib_handle, FM_LIBRARY_SYMBOL_NAME);
  if (!vendor_interface) {
     ALOGE("%s unable to find symbol %s in %s: %s", __func__, FM_LIBRARY_SYMBOL_NAME, FM_LIBRARY_NAME, dlerror());
     vendor_interface = NULL;
      if (lib_handle)
        dlclose(lib_handle);
      return FM_CMD_NO_RESOURCES;
  }

  if (vendor_interface) {
        ALOGE("Initializing the FM HAL module & registering the JNI callback functions...");
        status = vendor_interface->hal_init(&fm_callbacks);
        if (status) {
           ALOGE("%s unable to initialize vendor library: %d", __func__, status);
           return FM_CMD_NO_RESOURCES;
        }
        ALOGD("***** FM HAL Initialization complete *****\n");
     }

  if(radiocfgptr->is_fm_tx_on)
    print("\nOpened Transmitter\n");
  else
    print("\nOpened Receiver\n");

/*
  Starting 'ftm_on_long_thread' where we do all the FM configurations
  and intializations.

*/
  fmPowerState = FM_POWER_TRANSITION;
  pthread_create( &fm_on_thread, NULL, ftm_on_long_thread, radiocfgptr);

  return FM_CMD_SUCCESS;
}

/*===========================================================================
FUNCTION  TransmitPS

DESCRIPTION
  PFAL specific routine to transmit RDS PS strings

PARAMS PASSED
  'tuFmPSParams' containing RDS PI, PTY, max. no. of PS repeat count and
  PS name of the transmitting station

PLATFORM SPECIFIC DESCRIPTION
  This routine is used to transmit the PS string describing the transmitter's
  information and genre of the audio content being transmitted

DEPENDENCIES
  NIL

RETURN VALUE
  FM command status

SIDE EFFECTS
  None

===========================================================================*/
fm_cmd_status_type TransmitPS
(
   tsFtmFmRdsTxPsType*      tuFmPSParams
)
{
  int ret = 0;

  struct v4l2_control control;
  struct v4l2_ext_controls v4l2_ctls;
  struct v4l2_ext_control ext_ctl;

  print("\n Entering Transmit PS \n");

  /* Set Program Type (PTY) */
  control.id = V4L2_CID_RDS_TX_PTY;
  control.value = 0;
  ret = ioctl(fd_radio, VIDIOC_S_CTRL,&control );
  if(ret < 0 )
  {
    print("\n VIDIOC_S_CTRL ioctl: Set PTY failed \n");
    return FM_CMD_FAILURE;
  }

  /* Set Program Identifier (PI) */
  control.id = V4L2_CID_RDS_TX_PI;
  control.value = tuFmPSParams->tusTxPi;
  ret = ioctl(fd_radio, VIDIOC_S_CTRL,&control );
  if(ret < 0 )
  {
    print("\n VIDIOC_S_CTRL ioctl: Set PI failed \n");
    return FM_CMD_FAILURE;
  }

  /*Set PS max. repeat count */
  control.id = V4L2_CID_PRIVATE_TAVARUA_TX_SETPSREPEATCOUNT;
  control.value = tuFmPSParams->ucTxPSRptCnt;
  ret = ioctl(fd_radio, VIDIOC_S_CTRL,&control );
  if(ret < 0 )
  {
    print("\n VIDIOC_S_CTRL ioctl: Set MAX_REPEAT_CNT  failed \n");
    return FM_CMD_FAILURE;
  }

  /*Set Program Service name (PS) */
  ext_ctl.id              = V4L2_CID_RDS_TX_PS_NAME;
  ext_ctl.string          = (char *)tuFmPSParams->cTxPSStrPtr;
  ext_ctl.size            = tuFmPSParams->ulPSStrLen;
  v4l2_ctls.ctrl_class = V4L2_CTRL_CLASS_FM_TX;
  v4l2_ctls.count      = 0;
  v4l2_ctls.controls   = &ext_ctl;

  ret = ioctl(fd_radio, VIDIOC_S_EXT_CTRLS, &v4l2_ctls );
  if(ret < 0 )
  {
    print("\n VIDIOC_S_EXT_CTRLS ioctl: Set PS failed \n");
    return FM_CMD_FAILURE;
  }
  is_ps_transmitting = 1;
  print("\n Exiting Transmit PS \n");
  return FM_CMD_SUCCESS;
}

/*===========================================================================
FUNCTION  stopTransmitPS

DESCRIPTION
  PFAL specific routine to stop transmitting the PS string.

PARAMS PASSED
  NIL

PLATFORM SPECIFIC DESCRIPTION
  This routine is used to stop transmitting the PS string

DEPENDENCIES
  NIL

RETURN VALUE
  FM command status

SIDE EFFECTS
  None

============================================================================*/
fm_cmd_status_type stopTransmitPS
(
        void
)
{
  int ret = 0;
  struct v4l2_control control;

  print("\n Entering stopTransmitPS \n");

  control.id = V4L2_CID_PRIVATE_TAVARUA_STOP_RDS_TX_PS_NAME;
  ret = ioctl(fd_radio, VIDIOC_S_CTRL , &control);
  if(ret < 0){
    print("Failed to stop Transmit PS");
    return FM_CMD_FAILURE;
  }
  else {
    print("\nStopped transmitting PS\n");
    is_ps_transmitting = 0;
    return FM_CMD_SUCCESS;
  }
}

/*===========================================================================
FUNCTION  TransmitRT

DESCRIPTION
  PFAL specific routine to transmit the RT string.

PARAMS PASSED
  'tuFmRTParams' containing RDS PI, and RT information of the transmitting
  station

PLATFORM SPECIFIC DESCRIPTION
  This routine is used to transmit the RT string which provides a brief info
  of the audio content being transmitted. This includes artist name, movie name
  few lines of the audio. Usually the metadata of the song is transmitted as RT


DEPENDENCIES
  NIL

RETURN VALUE
  FM command status

SIDE EFFECTS
  None
===========================================================================*/
fm_cmd_status_type TransmitRT
(
   tsFtmFmRdsTxRtType*      tuFmRTParams
)
{
  int ret = 0;

  struct v4l2_control control;
  struct v4l2_ext_controls v4l2_ctls;
  struct v4l2_ext_control ext_ctl;

  v4l2_ctls.ctrl_class = V4L2_CTRL_CLASS_FM_TX;
  v4l2_ctls.count      = 1;
  v4l2_ctls.controls   = &ext_ctl;

  print("\n Entering Transmit RT \n");

  if(tuFmRTParams == NULL)
  {
    print("\n 'tuFmRTParams ' is not NULL \n ");
    return FM_CMD_FAILURE;
  }
  else
  {
    ext_ctl.id              = V4L2_CID_RDS_TX_RADIO_TEXT;
    ext_ctl.string          = (char *)tuFmRTParams->cTxRTStrPtr;
    ext_ctl.size            = tuFmRTParams->ulRTStrLen;
  }
  /* Set Program Type (PTY) */
  control.id = V4L2_CID_RDS_TX_PTY;
  control.value = 0;
  ret = ioctl(fd_radio, VIDIOC_S_CTRL,&control );
  if(ret < 0 )
  {
    print("\n VIDIOC_S_CTRL ioctl: Set PTY failed \n");
    return FM_CMD_FAILURE;
  }

  /* Set Program Identifier (PI) */
  control.id = V4L2_CID_RDS_TX_PI;
  control.value = tuFmRTParams->tusTxPi;
  ret = ioctl(fd_radio, VIDIOC_S_CTRL,&control );
  if(ret < 0 )
  {
    print("\n VIDIOC_S_CTRL ioctl: Set PI failed \n");
    return FM_CMD_FAILURE;
  }

  /* Set the Radio Text (RT) to be transmitted */
  ret = ioctl(fd_radio, VIDIOC_S_EXT_CTRLS, &v4l2_ctls );
  if(ret < 0 )
  {
    print("\n VIDIOC_S_EXT_CTRLS ioctl: Set RT failed \n");
    return FM_CMD_FAILURE;
  }

  is_rt_transmitting = 1;
  return FM_CMD_SUCCESS;

}

/*===========================================================================
FUNCTION  stopTransmitRT

DESCRIPTION
  PFAL specific routine to stop transmitting the RT string.

PLATFORM SPECIFIC DESCRIPTION
  This routine is called to stop transmitting the RT string for the
  transmitting station

DEPENDENCIES
  NIL

RETURN VALUE
  FM command status

SIDE EFFECTS
  None

===========================================================================*/
fm_cmd_status_type stopTransmitRT
(
        void
)
{
  int ret = 0;

  print("\n Entering stopTransmitRT \n");

  struct v4l2_control control;
  control.id = V4L2_CID_PRIVATE_TAVARUA_STOP_RDS_TX_RT;
  ret = ioctl(fd_radio, VIDIOC_S_CTRL , &control);
  if(ret < 0){
     print("Failed to stop Transmit RT");
     return FM_CMD_FAILURE;
  }
  else {
     print("\nStopped transmitting RT\n");
     is_rt_transmitting = 0;
     return FM_CMD_SUCCESS;
  }
}

/*===========================================================================
FUNCTION  getTxPSFeatures

DESCRIPTION
  PFAL specific routine to get all the supported Tx PS features

PLATFORM SPECIFIC DESCRIPTION
  This routine is called to get the supported PS features for the
  transmitting station

DEPENDENCIES
  NIL

RETURN VALUE
  FM command status

SIDE EFFECTS
  None

===========================================================================*/
fm_cmd_status_type getTxPSFeatures
(
        void
)
{
  print("\n Entering getTxPSFeatures() \n");
  printf("\n Supported Tx PS Features:\n");
  printf("\n Max PS Count : %d\n",MAX_TX_PS_LEN);
  printf("\n Max PS Repeat Count : %d\n",MAX_TX_PS_RPT_CNT);
  return FM_CMD_SUCCESS;
}

/*===========================================================================
FUNCTION  ftm_on_long_thread

DESCRIPTION
  Helper routine to perform the rest ofthe FM calibration and SoC Patch
  download and configuration settings following the opening ofradio handle

DEPENDENCIES
  NIL

RETURN VALUE
  NIL

SIDE EFFECTS
  None

===========================================================================*/
void *(ftm_on_long_thread)(void *ptr)
{
  int ret = 0;
  struct v4l2_control control;
  struct v4l2_tuner tuner;
  int i,init_success = 0;
  char value[PROPERTY_VALUE_MAX];
  struct v4l2_capability cap;
  char versionStr[40];
  char cmdBuffer[40];
  char product_board_platform_type[PROPERTY_VALUE_MAX];
  fm_cmd_status_type status;

  fm_config_data*      radiocfgptr = (fm_config_data *)ptr;

  property_get("qcom.bluetooth.soc", value, NULL);
  print2("BT soc is %s\n", value);
  /**
   * V4L2_CID_PRIVATE_TAVARUA_STATE
   * V4L2_CID_PRIVATE_TAVARUA_EMPHASIS
   * V4L2_CID_PRIVATE_TAVARUA_SPACING
   * V4L2_CID_PRIVATE_TAVARUA_RDS_STD
   * V4L2_CID_PRIVATE_TAVARUA_REGION
   */

 /* Switching on FM */
  if (radiocfgptr->is_fm_tx_on)
  {
    ret = vendor_interface->set_fm_ctrl(V4L2_CID_PRIVATE_TAVARUA_STATE, FM_TX);
    if(ret < FALSE)
    {
      print("Failed to turn on FM Trnasmitter\n");
      return NULL;
    }
    else
      print("\nEnabled FM Transmitter successfully\n");
  }
  else
  {
    ret = vendor_interface->set_fm_ctrl(V4L2_CID_PRIVATE_TAVARUA_STATE, FM_RX);
    if(ret < FALSE)
    {
      print("Failed to turn on FM Receiver\n");
      return NULL;
    }
    else
      print("\nEnabled FM Receiver successfully\n");
  }

  ret = vendor_interface->set_fm_ctrl(V4L2_CID_PRIVATE_TAVARUA_SET_AUDIO_PATH,1);
  if(ret < FALSE)
  {
    print("Failed to set Audio path \n");
  }

  status = ConfigureFM(radiocfgptr);
  if(status != FM_CMD_SUCCESS)
  {
    print("Failed to configure fm\n");
    return NULL;
  }

  /* Setting RDS On */
  ret = vendor_interface->set_fm_ctrl(V4L2_CID_PRIVATE_TAVARUA_RDSON,1);
  if(ret <FALSE)
  {
    print("Failed to set RDS on \n");
    return NULL;
  }

  /* Set the RDS Group Processing : Only in case of FM Receiver */
  if (!radiocfgptr->is_fm_tx_on)
  {
    int rdsMask =   FM_RX_RDS_GRP_RT_EBL | FM_RX_RDS_GRP_PS_EBL |
                    FM_RX_RDS_GRP_AF_EBL | FM_RX_RDS_GRP_PS_SIMPLE_EBL |
                    FM_RX_RDS_GRP_ECC_EBL| FM_RX_RDS_GRP_PTYN_EBL |
                    FM_RX_RDS_GRP_RT_PLUS_EBL;

    byte rdsFilt = 0;
    int  psAllVal=rdsMask & (1 << 4);
    print2("rdsOptions: rdsMask: %x\n",rdsMask);
    ret = vendor_interface->set_fm_ctrl(V4L2_CID_PRIVATE_TAVARUA_RDSGROUP_PROC,
                  rdsMask);
    if(ret < FALSE)
    {
      print("Failed to set RDS GROUP PROCESSING!!!\n");
      return NULL;
    }
        ret = vendor_interface->set_fm_ctrl(V4L2_CID_PRIVATE_TAVARUA_RDSGROUP_MASK,0x0FFF);
        if (ret < FALSE)
        {
            print("Failed to set RDS GRP MASK!!!\n");
            return NULL;
        }
      ret = vendor_interface->set_fm_ctrl(V4L2_CID_PRIVATE_TAVARUA_RDSD_BUF,1);
        if (ret < FALSE)
        {
            print("Failed to set RDS BUF!!!\n");
            return NULL;
        }
    ret = vendor_interface->set_fm_ctrl(V4L2_CID_PRIVATE_TAVARUA_ANTENNA,0);
    if(ret < FALSE)
    {
      print("Failed to set ANTENNA!!!\n");
      return NULL;
    }

    ret = vendor_interface->set_fm_ctrl(V4L2_CID_PRIVATE_IRIS_SOFT_MUTE, 0);
    if(ret < FALSE)
    {
      print("Failed to Disable Soft Mute!!!\n");
      return NULL;
    }

  }
  power_down = FALSE;

  if(radiocfgptr->is_fm_tx_on) {
#ifdef FTM_DEBUG
    print("\nEnable Transmitter exit\n");
#endif
    fmPowerState = FM_TX_ON;
  }
  else {
#ifdef FTM_DEBUG
    print("\nEnable Receiver exit\n");
#endif
    fmPowerState = FM_RX_ON;
  }
  poweron = COMPLETE;
  return NULL;
}

/*===========================================================================
FUNCTION  DisableFM

DESCRIPTION
  PFAL specific routine to disable FM and free the FM resources

PLATFORM SPECIFIC DESCRIPTION
  Closes the handle to /dev/radio0 V4L2 device

DEPENDENCIES
  NIL

RETURN VALUE
  FM command status

SIDE EFFECTS
  None
===========================================================================*/
fm_cmd_status_type DisableFM
(
        fm_config_data*      radiocfgptr
)
{
  struct v4l2_control control;
  uint8 buf[128];
  double tune;
  char value[PROPERTY_VALUE_MAX];
  struct v4l2_frequency freq_struct;
  int ret;

  /* Wait till the previous ON sequence has completed */
  while(poweron != COMPLETE);

#ifdef FTM_DEBUG
  print("DisableFM start\n");
#endif

  power_down = TRUE;

  /* Set RDS Off */
  ret = vendor_interface->set_fm_ctrl(V4L2_CID_PRIVATE_TAVARUA_RDSON,0);
  if(ret < FALSE)
  {
    print("DisableFM failed to set RDS off \n");
    return FM_CMD_FAILURE;
  }

  /* Turn off FM */
  /* As part of Turning off FM we will get 'READY' event */
  ret = vendor_interface->set_fm_ctrl(V4L2_CID_PRIVATE_TAVARUA_STATE,0);
  if(ret < FALSE)
  {
    if(radiocfgptr->is_fm_tx_on)
    {
      print("\nFailed to Turn Off FM Transmitter\n");
      return FM_CMD_FAILURE;
    }
    else
    {
      print("\nFailed to Turn Off FM Receiver\n");
      return FM_CMD_FAILURE;
    }
  }

#ifdef FTM_DEBUG
  print("Stopping the FM control\n");
#endif

#ifdef ANDROID

  property_get("qcom.bluetooth.soc", value, NULL);
  print2("BT soc is %s\n", value);
  if (strcmp(value, "rome") != 0)
  {
    property_set("ctl.stop", "fm_dl");
  }

#endif/*ANDROID*/
cmd_queued = TRUE;

#ifdef FTM_DEBUG

  if(radiocfgptr->is_fm_tx_on)
    print("Disabled FM Transmitter\n");
  else
    print("Disabled FM Receiver\n");

#endif
  fmPowerState = FM_POWER_OFF;
  return FM_CMD_SUCCESS;
}

/*===========================================================================
FUNCTION  ConfigureFM

DESCRIPTION
  PFAL specific routine to configure FM with the Radio Configuration
  parameters passed.

PLATFORM SPECIFIC DESCRIPTION
  Configures the Init parameters like emphasis, channel spacing, Band Limit,
  RDS type, Frequency Band, and Radio State.

DEPENDENCIES
  NIL

RETURN VALUE
  FM command status

SIDE EFFECTS
  None

===========================================================================*/
fm_cmd_status_type ConfigureFM
(
 fm_config_data*      radiocfgptr
)
{
 int ret = 0;
 struct v4l2_control control;
 struct v4l2_tuner tuner;

#ifdef FTM_DEBUG

  if(radiocfgptr->is_fm_tx_on)
    print("\nConfigure FM Transmitter entry\n");
  else
    print("\nConfigure FM Receiver entry\n");
#endif

  /* Set Emphasis, Channel spacing and RDS Standard :
  Emphasis :
      '0' - 75 - US/EU
      '1' - 50 - JAPAN/JAPAN-WIDE/ASIA
  Channel Spacing :
      '0' - 200kHz - US/EU
      '1' - 100kHz - JAPAN
      '2' - 50kHz  - JAPAN-WIDE
  RDS/RDBS Standard :
      '0' - RDBS - US/EU
      '1' - RDS  - All regions
  */
  fmconfig_ptr =(fm_config_data *) malloc(sizeof(fm_config_data));
  if(!fmconfig_ptr)
  {
     print("malloc failed");
     return FM_CMD_FAILURE;
  }
  fmconfig_ptr->band = radiocfgptr->band;
  switch(radiocfgptr->band)
  {
      case FM_US_EU:
          radiocfgptr->emphasis = 0;
          radiocfgptr->spacing = 0;
          radiocfgptr->rds_system = 0;
          break;
      case FM_JAPAN_STANDARD:
          radiocfgptr->emphasis = 1;
          radiocfgptr->spacing = 1;
          radiocfgptr->rds_system = 1;
      case FM_JAPAN_WIDE:
          radiocfgptr->emphasis = 1;
          radiocfgptr->spacing = 2;
          radiocfgptr->rds_system = 1;
          break;
  }

  ret = vendor_interface->set_fm_ctrl(V4L2_CID_PRIVATE_TAVARUA_EMPHASIS, radiocfgptr->emphasis);
  if(ret < FALSE)
  {
    print("ConfigureFM : Failed to set Emphasis \n");
    return FM_CMD_FAILURE;
  }

  /* Set channel spacing */
  ret = vendor_interface->set_fm_ctrl(V4L2_CID_PRIVATE_TAVARUA_SPACING,radiocfgptr->spacing);
  if(ret < FALSE)
  {
    print("ConfigureFM : Failed to set channel spacing  \n");
    return FM_CMD_FAILURE;
  }

  /* Set RDS/RDBS Standard */
  ret = vendor_interface->set_fm_ctrl(V4L2_CID_PRIVATE_TAVARUA_RDS_STD,radiocfgptr->rds_system);
  if(ret < FALSE)
  {
    print("ConfigureFM : Failed to set RDS std \n");
    return FM_CMD_FAILURE;
  }

  /* Set band limit and audio mode to Mono/Stereo */
  tuner.index = 0;
  tuner.signal = 0;

 print2("ConfigureFM : radio band = %d\n",radiocfgptr->band);
  switch(radiocfgptr->band)
  {
      case FM_US_EU:
        tuner.rangelow = REGION_US_EU_BAND_LOW ;
        tuner.rangehigh = REGION_US_EU_BAND_HIGH;
        break;
      case FM_JAPAN_STANDARD:
        tuner.rangelow = REGION_JAPAN_STANDARD_BAND_LOW;
        tuner.rangehigh = REGION_JAPAN_STANDARD_BAND_HIGH;
        break;
      case FM_JAPAN_WIDE:
        tuner.rangelow = REGION_JAPAN_WIDE_BAND_LOW;
        tuner.rangehigh = REGION_JAPAN_WIDE_BAND_HIGH;
        break;
      default:
        tuner.rangelow = radiocfgptr->bandlimits.lower_limit;
        tuner.rangehigh = radiocfgptr->bandlimits.upper_limit;
        break;
  }

  ret = vendor_interface->set_fm_ctrl(HCI_FM_HELIUM_UPPER_BAND, tuner.rangehigh);
  ret = vendor_interface->set_fm_ctrl(HCI_FM_HELIUM_LOWER_BAND, tuner.rangelow);
  fmconfig_ptr->bandlimits.lower_limit = tuner.rangelow;
  fmconfig_ptr->bandlimits.upper_limit = tuner.rangehigh;
  print3("ConfigureFM : set band limits: lower_limit=%d\t upper_limit = %d \n",  fmconfig_ptr->bandlimits.lower_limit,fmconfig_ptr->bandlimits.upper_limit );
  if(ret < 0)
  {
    print("ConfigureFM : Failed to set band limits and audio mode \n");
    return FM_CMD_FAILURE;
  }

  /* Set Region */
  radiocfgptr->band = FM_USER_DEFINED;
  ret = vendor_interface->set_fm_ctrl(V4L2_CID_PRIVATE_TAVARUA_REGION,radiocfgptr->band);
  if(ret < FALSE)
  {
    print("ConfigureFM : Failed to set band\n");
    return FM_CMD_FAILURE;
  }

   ret = vendor_interface->set_fm_ctrl(V4L2_CID_PRIVATE_TAVARUA_RDSON,1);
   if(ret < 0)
   {
      print("ConfigureFM : Failed to set RDS ON \n");
      return FM_CMD_FAILURE;
   }
out :
#ifdef FTM_DEBUG

  if(radiocfgptr->is_fm_tx_on)
    print("\nConfigure FM Transmitter exit\n");
  else
    print("\nConfigure FM Receiver exit\n");
#endif

  return FM_CMD_SUCCESS;
}

/*===========================================================================
FUNCTION  SetFrequencyReceiver

DESCRIPTION
  PFAL specific routine to configure the FM receiver's Frequency ofreception

DEPENDENCIES
  NIL

RETURN VALUE
  FM command status

SIDE EFFECTS
  None

===========================================================================*/
fm_cmd_status_type SetFrequencyReceiver
(
 uint32 ulfreq
)
{
    int err;
    int freq_rangehigh,freq_rangelow;
#ifdef FTM_DEBUG
    print2("\nSetFrequency Receiver entry freq = %d\n",(int)ulfreq);
#endif
    freq_rangehigh = fmconfig_ptr->bandlimits.upper_limit;
    freq_rangelow = fmconfig_ptr->bandlimits.lower_limit;
    printf("\n set frequency=%d\t  range_high=%d\t range_low =%d\n",ulfreq,freq_rangehigh,freq_rangelow);
    ALOGE("\n set frequency=%d\t  range_high=%d\t range_low =%d\n",ulfreq,freq_rangehigh,freq_rangelow);
    if (ulfreq >= freq_rangelow && ulfreq <= freq_rangehigh)
    {
        err = vendor_interface->set_fm_ctrl(HCI_FM_HELIUM_FREQ, ulfreq);
        if (err < 0)
        {
            print("SetFrequencyReceiver : Failed to set Freq \n");
            return FM_CMD_FAILURE;
        }
    }
    else
    {
        print("SetFrequencyReceiver : frequency out of band limits \n");
        return FM_CMD_DISALLOWED;
    }

#ifdef FTM_DEBUG
  print("\nSetFrequency Receiver exit\n");
#endif
  return FM_CMD_SUCCESS;
}

/*===========================================================================
FUNCTION  SetFrequencyTransmitter

DESCRIPTION
  PFAL specific routine to configure the FM Transmitter's frequency

DEPENDENCIES
  NIL

RETURN VALUE
  FM command status

SIDE EFFECTS
  None

===========================================================================*/
fm_cmd_status_type SetFrequencyTransmitter
(
 uint32 ulfreq
)
{
  int err;
  double tune;
  struct v4l2_frequency freq_struct;
  struct v4l2_control control;
  struct v4l2_tuner tuner;
#ifdef FTM_DEBUG
  print2("\n SetFrequencyTransmitter() entry : Freq = %d \n",(int)ulfreq);
#endif

  if(fd_radio < 0)
    return FM_CMD_NO_RESOURCES;

  /* Stop transmitting PS/RT frequency of currently tuned station */
  print2("\n Currently tuned station is : %ld \n",fm_global_params.current_station_freq);
  if(is_ps_transmitting)
    stopTransmitPS();
  if(is_rt_transmitting)
    stopTransmitRT();

  tuner.index = 0;

  err = ioctl(fd_radio,VIDIOC_G_TUNER,&tuner);
  if(err < 0)
  {
    print("SetFrequencyTransmitter : Failed to get band limits and audio mode \n");
    return FM_CMD_FAILURE;
  }

  /* Fill up the 'v4l2_frequency' structure */
  freq_struct.type = V4L2_TUNER_RADIO;
  freq_struct.frequency = (ulfreq) * (TUNE_MULT/1000);
  if (freq_struct.frequency >= tuner.rangelow && freq_struct.frequency <= tuner.rangehigh)
  {
    err = ioctl(fd_radio, VIDIOC_S_FREQUENCY, &freq_struct);
    if(err < 0)
      return FM_CMD_FAILURE;
    else
      print2("\n Frequency : %ld \n",ulfreq);
  }
  else
  {
     print("SetFrequencyTransmitter : Frequency out of bandlimits\n");
     return FM_CMD_DISALLOWED;
  }

#ifdef FTM_DEBUG
  print("\n SetFrequencyTransmitter() exit \n");
#endif
  return FM_CMD_SUCCESS;
}

/*===========================================================================
FUNCTION  SetTxPowerLevel

DESCRIPTION
  PFAL specific routine to configure the FM transmitter's power level

DEPENDENCIES
  NIL

RETURN VALUE
  FM command status

SIDE EFFECTS
  None

===========================================================================*/
fm_cmd_status_type SetTxPowerLevel
(
 uint32 ulfreq
)
{
  int err;
  struct v4l2_control control;

  /* Set the power level as requested */
  control.id      = V4L2_CID_TUNE_POWER_LEVEL;
  control.value   = FM_TX_PWR_LVL_MAX;
  err = ioctl(fd_radio, VIDIOC_S_CTRL, &control);
  if(err < 0)
  {
    if( err == -ETIME)
      print("\nTimeout to read PHY_TX gain Register\n");
    else
    {
      print2("\nFailed to set the Power Level for %ld\n",ulfreq);
      return FM_CMD_FAILURE;
    }
  }
  else
    print2("\nSuccessfully set the Power Level for %ld\n",ulfreq);

  return FM_CMD_SUCCESS;
}

/*===========================================================================
FUNCTION  SetMuteModeReceiver

DESCRIPTION
  PFAL specific routine to configure the FM receiver's mute status

DEPENDENCIES
  NIL

RETURN VALUE
  FM command status

SIDE EFFECTS
  None

===========================================================================*/
fm_cmd_status_type SetMuteModeReceiver
(
 mute_type mutemode
)
{
  int err,i;
  struct v4l2_control control;
  print2("SetMuteModeReceiver mode = %d\n",mutemode);

  err = vendor_interface->set_fm_ctrl(HCI_FM_HELIUM_AUDIO_MUTE,mutemode);
  if(err >= 0)
  {
     print("SetMuteMode Success\n");
     return FM_CMD_SUCCESS;
  }
  print2("Set mute mode ret = %d\n",err);
  return FM_CMD_FAILURE;
}

/*===========================================================================

FUNCTION  SetSoftMuteModeReceiver

DESCRIPTION
  PFAL specific routine to configure the FM receiver's soft mute status

DEPENDENCIES
  NIL

RETURN VALUE
  FM command status

SIDE EFFECTS
  None

===========================================================================*/
fm_cmd_status_type SetSoftMuteModeReceiver
(
 mute_type mutemode
)
{
  int err;
  struct v4l2_control control;
  print2("SetMuteModeReceiver mode = %d\n",mutemode);


  err = vendor_interface->set_fm_ctrl(V4L2_CID_PRIVATE_IRIS_SOFT_MUTE,mutemode);
  if(err >= 0)
  {
    print("SetMuteMode Success\n");
    return FM_CMD_SUCCESS;
  }
  print2("Set mute mode ret = %d\n",err);
  return FM_CMD_FAILURE;
}

/*===========================================================================
FUNCTION  SetAntenna

DESCRIPTION
  PFAL specific routine to configure the FM receiver's antenna type

DEPENDENCIES
  NIL

RETURN VALUE
  FM command status

SIDE EFFECTS
  None

===========================================================================*/


fm_cmd_status_type SetAntenna
(
  antenna_type antenna
)
{
  int err;
  struct v4l2_control control;
  control.value = antenna;
  control.id = V4L2_CID_PRIVATE_TAVARUA_ANTENNA;

  err = vendor_interface->set_fm_ctrl(HCI_FM_HELIUM_ANTENNA,antenna);
  if(err >= 0)
  {
    print("SetAntenna Success\n");
    return FM_CMD_SUCCESS;
  }

  print2("Set antenna ret = %d\n",err);
  return FM_CMD_FAILURE;
}

/*===========================================================================
FUNCTION  FmRivaPeekData

DESCRIPTION
  PFAL specific routine to get the data from Riva Memory
DEPENDENCIES
  NIL

RETURN VALUE
  FM command status

SIDE EFFECTS
  None

===========================================================================*/


fm_cmd_status_type FmRivaPeekData
(
  fm_riva_peek_word peek_word
)
{
  int err;
  struct v4l2_control control;

  if(fd_radio < 0)
  return FM_CMD_NO_RESOURCES;

  control.value = peek_word.startaddress;
  control.id = V4L2_CID_PRIVATE_IRIS_RIVA_ACCS_ADDR;

  err = vendor_interface->set_fm_ctrl(V4L2_CID_PRIVATE_IRIS_RIVA_ACCS_ADDR,
                                      peek_word.startaddress);
  if(err >= 0)
  {
    print("SetRiva Address Success\n");
  }
  control.value = peek_word.payload_length;
  control.id = V4L2_CID_PRIVATE_IRIS_RIVA_ACCS_LEN;
  err = vendor_interface->set_fm_ctrl(V4L2_CID_PRIVATE_IRIS_RIVA_ACCS_LEN,
                                      peek_word.payload_length);
  if(err >= 0)
  {
    print("SetDataLen Success\n");
  }
  control.id = V4L2_CID_PRIVATE_IRIS_RIVA_PEEK;
  err = vendor_interface->set_fm_ctrl(V4L2_CID_PRIVATE_IRIS_RIVA_PEEK, control.value);
  if(err >= 0)
  {
    print("RivaPeek Success\n");
    return FM_CMD_SUCCESS;
  }

  print2("RivaPeekData ret = %d\n",err);
  return FM_CMD_FAILURE;
}


/*===========================================================================
FUNCTION  FmRivaPokeData

DESCRIPTION
  PFAL specific routine to write the data into the Riva Memory
DEPENDENCIES
  NIL

RETURN VALUE
  FM command status

SIDE EFFECTS
  None

===========================================================================*/


fm_cmd_status_type FmRivaPokeData
(
  fm_riva_poke_word poke_word
)
{
  int err;
  struct v4l2_control control;

  if(fd_radio < 0)
    return FM_CMD_NO_RESOURCES;

  control.value = poke_word.startaddress;
  control.id = V4L2_CID_PRIVATE_IRIS_RIVA_ACCS_ADDR;

  err = vendor_interface->set_fm_ctrl(V4L2_CID_PRIVATE_IRIS_RIVA_ACCS_ADDR,
                                      poke_word.startaddress);
  if(err >= 0)
  {
    print("SetRiva Address Success\n");
  }
  control.value = poke_word.payload_length;
  control.id = V4L2_CID_PRIVATE_IRIS_RIVA_ACCS_LEN;
//  err = ioctl(fd_radio,VIDIOC_S_CTRL,&control);
  err = vendor_interface->set_fm_ctrl(V4L2_CID_PRIVATE_IRIS_RIVA_ACCS_LEN,
                                      poke_word.payload_length);
  if(err >= 0)
  {
    print("SetDataLen Success\n");
  }
  control.id = V4L2_CID_PRIVATE_IRIS_RIVA_POKE;
  control.value = (uint32)(uintptr_t)(&poke_word.data);
  err = vendor_interface->set_fm_ctrl(V4L2_CID_PRIVATE_IRIS_RIVA_POKE,
                                      control.value);
  if(err >= 0)
  {
    print("RivaPoke Success\n");
    return FM_CMD_SUCCESS;
  }

  print2("RivaPokeData ret = %d\n",err);
  return FM_CMD_FAILURE;
}


/*===========================================================================
FUNCTION  FmSSBIPeekData

DESCRIPTION
  PFAL specific routine to get the data from SSBI registers
DEPENDENCIES
  NIL

RETURN VALUE
  FM command status

SIDE EFFECTS
  None

===========================================================================*/


fm_cmd_status_type FmSSBIPeekData
(
  fm_ssbi_poke_reg peek_reg
)
{
  int err;
  struct v4l2_control control;

  if(fd_radio < 0)
    return FM_CMD_NO_RESOURCES;

  control.id = V4L2_CID_PRIVATE_IRIS_SSBI_PEEK;
  control.value = peek_reg.startaddress;
  if( control.value == 0x00)
    return FM_CMD_FAILURE;
  err = vendor_interface->set_fm_ctrl(V4L2_CID_PRIVATE_IRIS_SSBI_PEEK,
                                      peek_reg.startaddress);
  if(err >= 0)
  {
    //extract_ssbi_peek_data();
    print2("SSBIPeek Success\n %d",peek_reg.data);
    return FM_CMD_SUCCESS;
  }

  print2("SSBIPeekData ret = %d\n",err);
  return FM_CMD_FAILURE;
}

fm_cmd_status_type FmSetGetResetAGC
(
  fm_set_get_reset_agc_req agc_params
)
{
  int err;
  struct v4l2_control control;
  control.value = agc_params.ucCtrl;
  control.id = V4L2_CID_PRIVATE_IRIS_AGC_CTRL;

  err = vendor_interface->set_fm_ctrl(V4L2_CID_PRIVATE_IRIS_AGC_CTRL,
                                                agc_params.ucCtrl);
  if(err >= 0)
  {
    print("Sending AGC ucCtrL Success\n");
  }
  control.value = agc_params.ucGainState;
  control.id = V4L2_CID_PRIVATE_IRIS_AGC_STATE;

  err = vendor_interface->set_fm_ctrl(V4L2_CID_PRIVATE_IRIS_AGC_STATE,
                                            agc_params.ucGainState);
  if(err >= 0)
  {
    print("Set ucGainstate Success\n");
    return FM_CMD_SUCCESS;
  }

  print2("SetGetResetAGC ret = %d\n",err);
  return FM_CMD_FAILURE;
}

/*===========================================================================
FUNCTION  FmSSBIPokeData

DESCRIPTION
  PFAL specific routine to program the SSBI registers
DEPENDENCIES
  NIL

RETURN VALUE
  FM command status

SIDE EFFECTS
  None

===========================================================================*/


fm_cmd_status_type FmSSBIPokeData
(
  fm_ssbi_poke_reg poke_reg
)
{
  int err;
  struct v4l2_control control;

  if(fd_radio < 0)
    return FM_CMD_NO_RESOURCES;

  control.value = poke_reg.startaddress;
  control.id = V4L2_CID_PRIVATE_IRIS_SSBI_ACCS_ADDR;

  err = vendor_interface->set_fm_ctrl(V4L2_CID_PRIVATE_IRIS_SSBI_ACCS_ADDR,
                                     poke_reg.startaddress);
  if(err >= 0)
  {
    print("SetRiva Address Success\n");
  }
  control.id = V4L2_CID_PRIVATE_IRIS_SSBI_POKE;
  control.value = poke_reg.data;
  err = vendor_interface->set_fm_ctrl(V4L2_CID_PRIVATE_IRIS_SSBI_POKE,
                                      control.value);
  if(err >= 0)
  {
    print2("SSBIPoke Success\n value =%d",poke_reg.data);
    return FM_CMD_SUCCESS;
  }

  print2("SSBIPokeData ret = %d\n",err);
  return FM_CMD_FAILURE;
}
/*===========================================================================
FUNCTION  FmTxPwrLvlCfg

DESCRIPTION
  PFAL specific routine to configure the FM Tx power level

DEPENDENCIES
  NIL

RETURN VALUE
  FM command status

SIDE EFFECTS
  None

===========================================================================*/

fm_cmd_status_type FmTxPwrLvlCfg
(
  uint8 pwrlvl
)
{
  int err;
  struct v4l2_control control;
  control.value = pwrlvl;
  control.id = V4L2_CID_TUNE_POWER_LEVEL;
  err = ioctl(fd_radio, VIDIOC_S_CTRL, &control);
  if (err >= 0) {
      print2("Tx power level set to %d\n", pwrlvl);
      return FM_CMD_SUCCESS;
  }
  print2("Set Tx power level failed = %d\n", err);
  return FM_CMD_FAILURE;
}
/*===========================================================================
FUNCTION  FmTxToneGen

DESCRIPTION
  PFAL specific routine to configure the FM Tx internal tone Generation

DEPENDENCIES
  NIL

RETURN VALUE
  FM command status

SIDE EFFECTS
  None

===========================================================================*/


fm_cmd_status_type FmTxToneGen
(
  uint8 txTone
)
{
  int err;
  struct v4l2_control control;
  control.value = txTone;
  control.id = V4L2_CID_PRIVATE_IRIS_TX_TONE;
  err = ioctl(fd_radio,VIDIOC_S_CTRL,&control);
  if(err >= 0)
  {
    print("Set Tx internal tone Success\n");
    return FM_CMD_SUCCESS;
  }
  print2("Set Tx internal tone = %d\n",err);
  return FM_CMD_FAILURE;
}
/*===========================================================================
FUNCTION  FmRDSGrpcntrs

DESCRIPTION
  PFAL specific routine to get the FM RDS group conuters

DEPENDENCIES
  NIL

RETURN VALUE
  FM command status

SIDE EFFECTS
  None

===========================================================================*/

fm_cmd_status_type FmRDSGrpcntrs
(
  uint8 rdsCounters
)
{
  int err;
  struct timespec* ts;
  pthread_mutex_lock(&fm_event_lock);

  err = vendor_interface->set_fm_ctrl(V4L2_CID_PRIVATE_IRIS_RDS_GRP_COUNTERS, rdsCounters);
  if (err >= 0)
  {
      print("Read RDS GROUP counters success\n");
      ts = set_time_out(2);
      if(ts)
      {
          err = pthread_cond_timedwait(&fm_event_cond, &fm_event_lock, ts);
          ALOGD("Unlocked mutex timedout or condition satisfied %s", __func__);
          pthread_mutex_unlock(&fm_event_lock);
          free(ts);
          return FM_CMD_SUCCESS;
      }
  }
  print2("Read RDS GROUP counters = %d\n",err);
  pthread_mutex_unlock(&fm_event_lock);
  return FM_CMD_FAILURE;
}

/*===========================================================================
FUNCTION  FmRDSGrpcntrsExt

DESCRIPTION
  PFAL specific routine to get the FM RDS group conuters

DEPENDENCIES
  NIL

RETURN VALUE
  FM command status

SIDE EFFECTS
  None

===========================================================================*/

fm_cmd_status_type FmRDSGrpcntrsExt
(
  uint8 rdsCounters
)
{
  int err;
  struct v4l2_control control;
  control.value = rdsCounters;
  control.id = V4L2_CID_PRIVATE_IRIS_RDS_GRP_COUNTERS_EXT;

  err = vendor_interface->set_fm_ctrl(V4L2_CID_PRIVATE_IRIS_RDS_GRP_COUNTERS_EXT, rdsCounters);
  if(err >= 0)
  {
    print("Read RDS GROUP counters extn success\n");
    return FM_CMD_SUCCESS;
  }
  print2("Read RDS GROUP counters extn  = %d\n",err);
  return FM_CMD_FAILURE;
}

/*===========================================================================
FUNCTION  FmDefaultRead

DESCRIPTION
  PFAL specific routine to get the FM Default Values

DEPENDENCIES
  NIL

RETURN VALUE
  FM command status

SIDE EFFECTS
  None

===========================================================================*/

fm_cmd_status_type FmDefaultRead
(
  ftm_fm_def_data_rd_req defaultRead
)
{
  int err;
  struct v4l2_control control;
  struct v4l2_ext_controls v4l2_ctls;
  struct v4l2_ext_control ext_ctl;

  ext_ctl.id              = V4L2_CID_PRIVATE_IRIS_READ_DEFAULT;
  ext_ctl.size            = sizeof(ftm_fm_def_data_rd_req);
  memcpy(ext_ctl.string,(char *)&defaultRead,ext_ctl.size);

  v4l2_ctls.ctrl_class = V4L2_CTRL_CLASS_USER;
  v4l2_ctls.count      = 1;
  v4l2_ctls.controls   = &ext_ctl;

  err = ioctl(fd_radio,VIDIOC_G_EXT_CTRLS,&v4l2_ctls);
  if(err >= 0)
  {
    extract_default_read_data();
    print("Read Defaults success\n");
    return FM_CMD_SUCCESS;
  }
  print2("Read Defaults =%d  \n",err);
  return FM_CMD_FAILURE;
}
/*===========================================================================
FUNCTION  FmDefaultWrite

DESCRIPTION
  PFAL specific routine to write the default values

DEPENDENCIES
  NIL

RETURN VALUE
  FM command status

SIDE EFFECTS
  None

===========================================================================*/

fm_cmd_status_type FmDefaultWrite
(
   ftm_fm_def_data_wr_req*      writedefaults
)
{
  int ret = 0;

  struct v4l2_control control;
  struct v4l2_ext_controls v4l2_ctls;
  struct v4l2_ext_control ext_ctl;

  if(writedefaults == NULL)
  {
    print("\n 'writedefaults' is NULL \n ");
    return FM_CMD_FAILURE;
  }
  ext_ctl.id              = V4L2_CID_PRIVATE_IRIS_WRITE_DEFAULT;
  ext_ctl.size            = (writedefaults->length + 2);
  ext_ctl.string          = (char *)&writedefaults->mode;

  v4l2_ctls.ctrl_class = V4L2_CTRL_CLASS_USER;
  v4l2_ctls.count      = 1;
  v4l2_ctls.controls   = &ext_ctl;

  print("\n Entering Write defaults \n");

  ret = ioctl(fd_radio, VIDIOC_S_EXT_CTRLS, &v4l2_ctls );
  if(ret < 0 )
  {
    print("\n VIDIOC_S_EXT_CTRLS ioctl: Set Default failed \n");
    return FM_CMD_FAILURE;
  }

  return FM_CMD_SUCCESS;

}

/*===========================================================================
FUNCTION  FmSetHlSi

DESCRIPTION
  PFAL specific routine to configure the FM receiver's HlSi

DEPENDENCIES
  NIL

RETURN VALUE
  FM command status

SIDE EFFECTS
  None

===========================================================================*/

fm_cmd_status_type FmSetHlSi
(
  uint8 hlsi
)
{
  int err = -1;
  struct v4l2_control control;
  control.value = hlsi;
  control.id = V4L2_CID_PRIVATE_IRIS_HLSI;
  err = vendor_interface->set_fm_ctrl( FTM_FM_SET_HLSI, hlsi);
  print2("Setting HLSI to  = %d",hlsi);
  if(err >= 0)
  {
    print("Set HlSi success");
    return FM_CMD_SUCCESS;
  }
  print2("Set HlSi status = %d\n",err);
  return FM_CMD_FAILURE;
}
/*===========================================================================
FUNCTION  FmSetNotchFilter

DESCRIPTION
  PFAL specific routine to configure the FM receiver's notch filter

DEPENDENCIES
  NIL

RETURN VALUE
  FM command status

SIDE EFFECTS
  None

===========================================================================*/

fm_cmd_status_type FmSetNotchFilter
(
  uint8 notch
)
{
  int err = -1 ;
  struct v4l2_control control;
  control.value = notch;
  control.id = V4L2_CID_PRIVATE_IRIS_SET_NOTCH_FILTER;
  print2("Setting Notch filter  to  = %d",notch);
  err = vendor_interface->set_fm_ctrl(V4L2_CID_PRIVATE_IRIS_SET_NOTCH_FILTER, notch);
  if(err >= 0)
  {
    print("Set Notch filter success");
    return FM_CMD_SUCCESS;
  }
  print2("Set Notch filter status = %d\n",err);
  return FM_CMD_FAILURE;
}
/*===========================================================================
FUNCTION  SetStereoModeReceiver

DESCRIPTION
  PFAL specific routine to configure the FM receiver's Audio mode on the
  frequency tuned

DEPENDENCIES
  NIL

RETURN VALUE
  FM command status

SIDE EFFECTS
  None

===========================================================================*/
fm_cmd_status_type SetStereoModeReceiver
(
 stereo_type stereomode
)
{
    int err = -1;
   err = vendor_interface->set_fm_ctrl(HCI_FM_HELIUM_AUDIO_MODE,stereomode);
   ALOGD("Stereo mode =%d ...  %s", stereomode,__func__);
   if(err >= 0) {
      print("SetStereoMode Success\n");
      return FM_CMD_SUCCESS;
   }
   print2("Set Stereo Mode status = %d\n",err);
   return FM_CMD_FAILURE;
}

/*===========================================================================
FUNCTION  GetStationParametersReceiver

DESCRIPTION
  PFAL specific routine to get the station parameters ofthe Frequency at
  which the Radio receiver is  tuned

DEPENDENCIES
  NIL

RETURN VALUE
  FM command status

SIDE EFFECTS
  None

===========================================================================*/
fm_cmd_status_type GetStationParametersReceiver
(
 fm_station_params_available*    configparams
)
{
  int ret = 0, value = 0;
  struct timespec* ts;
  configparams->current_station_freq = fm_global_params.current_station_freq;
  configparams->service_available = fm_global_params.service_available;

  pthread_mutex_lock(&fm_event_lock);
  ret = vendor_interface->get_fm_ctrl(HCI_FM_HELIUM_RMSSI, &value);
  if (ret < 0) {
      print2("GetStationParametersReceiver:Failed to get Rssi  error = %d\n",ret);
      pthread_mutex_unlock(&fm_event_lock);
      return FM_CMD_FAILURE;
  }
  ts = set_time_out(2);
  if(ts)
  {
      ret = pthread_cond_timedwait(&fm_event_cond, &fm_event_lock, ts);
      ALOGD("Unlocked mutex timedout or condition satisfied %s", __func__);
      free(ts);
  }
  else
  {
      print("Cmd timeout failed ..");
      pthread_mutex_unlock(&fm_event_lock);
      return FM_CMD_FAILURE;
  }
  pthread_mutex_unlock(&fm_event_lock);

  ret = vendor_interface->get_fm_ctrl(HCI_FM_HELIUM_AUDIO_MUTE,&value);
  if (ret < 0) {
      print2("Failed to get mute status error = %d\n",ret);
      return FM_CMD_FAILURE;
  }
  configparams->mute_status = value;
  print2("GetStationParametersReceiver: Rssi  = %d\n",fm_global_params.rssi);
  configparams->rssi = fm_global_params.rssi;
  configparams->stype = fm_global_params.stype;
  configparams->rds_sync_status = fm_global_params.rds_sync_status;

  return FM_CMD_SUCCESS;
}

/*===========================================================================
FUNCTION  SetRdsOptionsReceiver

DESCRIPTION
  PFAL specific routine to configure the FM receiver's RDS options

DEPENDENCIES
  NIL

RETURN VALUE
  FM command status

SIDE EFFECTS
  None

===========================================================================*/
fm_cmd_status_type SetRdsOptionsReceiver
(
 fm_rds_options rdsoptions
)
{
  int ret;
  print("SetRdsOptionsReceiver\n");
   ALOGE("SetRdsOptionsReceiver\n");

  ret = vendor_interface->set_fm_ctrl(V4L2_CID_PRIVATE_TAVARUA_RDSGROUP_MASK,
          rdsoptions.rds_group_mask);
  if(ret < FALSE)
  {
    print2("SetRdsOptionsReceiver Failed to set RDS group options = %d\n",ret);
    return FM_CMD_FAILURE;
  }

  ret = vendor_interface->set_fm_ctrl(V4L2_CID_PRIVATE_TAVARUA_RDSD_BUF,
         rdsoptions.rds_group_buffer_size);
  if(ret < FALSE)
  {
    print2("SetRdsOptionsReceiver Failed to set RDS group options = %d\n",ret);
    return FM_CMD_FAILURE;
  }

  /*Chnage Filter not supported */
  print("SetRdsOptionsReceiver<\n");

  return FM_CMD_SUCCESS;
}

/*===========================================================================
FUNCTION  SetRdsGroupProcReceiver

DESCRIPTION
  PFAL specific routine to configure the FM receiver's RDS group proc options

DEPENDENCIES
  NIL

RETURN VALUE
  FM command status

SIDE EFFECTS
  None

===========================================================================*/
fm_cmd_status_type SetRdsGroupProcReceiver
(
 uint32 rdsgroupoptions
)
{
  int ret;
  print("SetRdsGroupProcReceiver\n");
  ret = vendor_interface->set_fm_ctrl(V4L2_CID_PRIVATE_TAVARUA_RDSGROUP_PROC,
         rdsgroupoptions);
  if(ret < FALSE)
  {
    print2("SetRdsGroupProcReceiver Failed to set RDS proc = %d\n",ret);
    return FM_CMD_FAILURE;
  }
  ret = vendor_interface->set_fm_ctrl(V4L2_CID_PRIVATE_TAVARUA_RDSON,1);
  if(ret < FALSE)
  {
    print2(" Failed to set RDS proc ON = %d\n",ret);
    return FM_CMD_FAILURE;
  }
  return FM_CMD_SUCCESS;
}


/*===========================================================================
FUNCTION  SetPowerModeReceiver

DESCRIPTION
  PFAL specific routine to configure the power mode of FM receiver

DEPENDENCIES
  NIL

RETURN VALUE
  FM command status

SIDE EFFECTS
  None

===========================================================================*/
fm_cmd_status_type SetPowerModeReceiver
(
 uint8 powermode
)
{
  struct v4l2_control control;
  int i,err;
  print2("SetPowerModeReceiver mode = %d\n",powermode);
  err = vendor_interface->set_fm_ctrl(V4L2_CID_PRIVATE_TAVARUA_LP_MODE, powermode);
  if (err < 0)
  {
      print("SetPowerMode Failed\n");
     return FM_CMD_FAILURE;
  }
  return FM_CMD_SUCCESS;
}

/*===========================================================================
FUNCTION  SetSignalThresholdReceiver

DESCRIPTION
  PFAL specific routine to configure the signal threshold ofFM receiver

DEPENDENCIES
  NIL

RETURN VALUE
  FM command status

SIDE EFFECTS
  None

===========================================================================*/
fm_cmd_status_type SetSignalThresholdReceiver
(
 uint8 signalthreshold
)
{
  struct v4l2_control control;
  int i,err;
  print2("SetSignalThresholdReceiver threshold = %d\n",signalthreshold);
  err = vendor_interface->set_fm_ctrl(V4L2_CID_PRIVATE_TAVARUA_SIGNAL_TH,signalthreshold);
  if(err >= 0)
  {
      print("SetSignalThresholdReceiver Success\n");
      return FM_CMD_SUCCESS;
  }
  return FM_CMD_FAILURE;
}

/*===========================================================================
FUNCTION  GetSignalThresholdReceiver

DESCRIPTION
  PFAL specific routine to get the signal threshold of FM receiver

DEPENDENCIES
  NIL

RETURN VALUE
  FM command status

SIDE EFFECTS
  None

===========================================================================*/
fm_cmd_status_type GetSignalThresholdReceiver
(
 uint8* signalthreshold
)
{
  int i,err,value=0;

  err = vendor_interface->get_fm_ctrl(V4L2_CID_PRIVATE_TAVARUA_SIGNAL_TH, &value);
  if(err >= 0)
   {
      print2("GetSignalThresholdReceiver Success = %d\n",value);
      *signalthreshold = value;
      return FM_CMD_SUCCESS;
   }

  return FM_CMD_FAILURE;
}

/*===========================================================================
FUNCTION  GetRSSILimits

DESCRIPTION
  PFAL specific routine to print the RSSI limts ofFM receiver

DEPENDENCIES
  NIL

RETURN VALUE
  FM command status

SIDE EFFECTS
  None

===========================================================================*/
fm_cmd_status_type GetRSSILimits
(
)
{
  int limits[] ={0,100};

  return FM_CMD_SUCCESS;
}


/*===========================================================================
FUNCTION  GetPSInfoReceiver

DESCRIPTION
  PFAL specific routine to print the PS info ofcurrent frequency of
  FM receiver

DEPENDENCIES
  NIL

RETURN VALUE
  FM command status

SIDE EFFECTS
  None

===========================================================================*/
fm_cmd_status_type GetPSInfoReceiver
(
)
{
  return FM_CMD_SUCCESS;
}

/*===========================================================================
FUNCTION  GetRTInfoReceiver

DESCRIPTION
  PFAL specific routine to print the Radio text info ofcurrent frequency of
  FM receiver

DEPENDENCIES
  NIL

RETURN VALUE
  FM command status

SIDE EFFECTS
  None

===========================================================================*/
fm_cmd_status_type GetRTInfoReceiver
(
)
{
  return FM_CMD_SUCCESS;
}

/*===========================================================================
FUNCTION  GetAFInfoReceiver

DESCRIPTION
  PFAL specific routine to print the AF list for current frequency of
  FM receiver

DEPENDENCIES
  NIL

RETURN VALUE
  FM command status

SIDE EFFECTS
  None

===========================================================================*/
fm_cmd_status_type GetAFInfoReceiver
(
)
{
  return FM_CMD_SUCCESS;
}

/*===========================================================================
FUNCTION  SearchStationsReceiver

DESCRIPTION
  PFAL specific routine to search for stations from the current frequency of
  FM receiver and print the information on diag

DEPENDENCIES
  NIL

RETURN VALUE
  FM command status

SIDE EFFECTS
  None

===========================================================================*/
fm_cmd_status_type SearchStationsReceiver
(
fm_search_stations searchstationsoptions
)
{
  int err,i;
  int ret;
  print("SearchStationsReceiver\n");

  ret = vendor_interface->set_fm_ctrl(V4L2_CID_PRIVATE_TAVARUA_SRCHMODE,
        searchstationsoptions.search_mode);
  if(ret < FALSE)
  {
    print("SearchStationsReceiver failed \n");
    return FM_CMD_FAILURE;
  }

  ret = vendor_interface->set_fm_ctrl(V4L2_CID_PRIVATE_TAVARUA_SCANDWELL,
        searchstationsoptions.dwell_period);
  if(ret < FALSE)
  {
    print("SearchStationsReceiver failed \n");
    return FM_CMD_FAILURE;
  }

  if (searchstationsoptions.search_dir)
    searchstationsoptions.search_dir = SRCH_DIR_UP;
  else
    searchstationsoptions.search_dir = SRCH_DIR_DOWN;

  err = vendor_interface->set_fm_ctrl(HCI_FM_HELIUM_SEEK,searchstationsoptions.search_dir);
  if(err < 0)
  {
    print("SearchStationsReceiver failed \n");
    return FM_CMD_FAILURE;
  }
  print("SearchRdsStationsReceiver<\n");
  return FM_CMD_SUCCESS;
}


/*===========================================================================
FUNCTION  SearchRDSStationsReceiver

DESCRIPTION
  PFAL specific routine to search for stations from the current frequency of
  FM receiver with a specific program type and print the information on diag

DEPENDENCIES
  NIL

RETURN VALUE
  FM command status

SIDE EFFECTS
  None

===========================================================================*/
fm_cmd_status_type SearchRdsStationsReceiver
(
fm_search_rds_stations searchrdsstationsoptions
)
{
  int i,err;
  int ret;

  print("SearchRdsStationsReceiver>\n");
  ret = vendor_interface->set_fm_ctrl(V4L2_CID_PRIVATE_TAVARUA_SRCHMODE,
        searchrdsstationsoptions.search_mode);
  if(ret < FALSE)
  {
    print("SearchRdsStationsReceiver failed \n");
    return FM_CMD_FAILURE;
  }

 ret = vendor_interface->set_fm_ctrl(V4L2_CID_PRIVATE_TAVARUA_SCANDWELL,
             searchrdsstationsoptions.dwell_period);
  if(ret < FALSE)
  {
    print("SearchRdsStationsReceiver failed \n");
    return FM_CMD_FAILURE;
  }

  ret = vendor_interface->set_fm_ctrl(V4L2_CID_PRIVATE_TAVARUA_SRCH_PTY,
             searchrdsstationsoptions.program_type);
  if(ret < FALSE)
  {
    print("SearchRdsStationsReceiver failed \n");
    return FM_CMD_FAILURE;
  }

  ret = vendor_interface->set_fm_ctrl(V4L2_CID_PRIVATE_TAVARUA_SRCH_PI,
             searchrdsstationsoptions.program_id);
  if(ret <FALSE)
  {
    print("SearchRdsStationsReceiver failed \n");
    return FM_CMD_FAILURE;
  }

  err = vendor_interface->set_fm_ctrl(HCI_FM_HELIUM_SEEK,searchrdsstationsoptions.search_dir);
  if(err < 0)
  {
    print("SearchRdsStationsReceiver failed \n");
    return FM_CMD_FAILURE;
  }

  print("SearchRdsStationsReceiver<\n");
  return FM_CMD_SUCCESS;
}

/*===========================================================================
FUNCTION  SearchStationListReceiver

DESCRIPTION
  PFAL specific routine to search for stations with a specific mode of
  informaation like WEAK,STRONG,STRONGEST etc

DEPENDENCIES
  NIL

RETURN VALUE
  FM command status

SIDE EFFECTS
  None

===========================================================================*/
fm_cmd_status_type SearchStationListReceiver
(
fm_search_list_stations searchliststationsoptions
)
{
  int i,ret;

  print("SearchStationListReceiver>\n");

  ret = vendor_interface->set_fm_ctrl(V4L2_CID_PRIVATE_TAVARUA_SRCHMODE,
        searchliststationsoptions.search_mode);
  if(ret < FALSE)
  {
    print("SearchStationListReceiver failed \n");
    return FM_CMD_FAILURE;
  }

  ret = vendor_interface->set_fm_ctrl(V4L2_CID_PRIVATE_TAVARUA_SRCH_CNT,
          searchliststationsoptions.srch_list_max);
  if(ret < FALSE)
  {
    print("SearchStationListReceiver failed \n");
    return FM_CMD_FAILURE;
  }

  ret = vendor_interface->set_fm_ctrl(V4L2_CID_PRIVATE_TAVARUA_SRCH_PTY,
          searchliststationsoptions.program_type);
  if(ret <FALSE)
  {
    print("SearchStationListReceiver failed \n");
    return FM_CMD_FAILURE;
  }

  ret = vendor_interface->set_fm_ctrl(HCI_FM_HELIUM_SEEK,searchliststationsoptions.search_dir);
  if(ret < 0)
  {
    print("SearchStationListReceiver failed \n");
    return FM_CMD_FAILURE;
  }
  print("SearchStationListReceiver<\n");
  return FM_CMD_SUCCESS;
}

/*===========================================================================
FUNCTION

DESCRIPTION
  PFAL specific routine to cancel the ongoing search operation

DEPENDENCIES
  NIL

RETURN VALUE
  FM command status

SIDE EFFECTS
  None

===========================================================================*/
fm_cmd_status_type CancelSearchReceiver
(
)
{
  int ret;

  ret = vendor_interface->set_fm_ctrl(V4L2_CID_PRIVATE_TAVARUA_SRCHON,0);
  if(ret < FALSE)
  {
    return FM_CMD_FAILURE;
  }
  return FM_CMD_SUCCESS;
}

/*===========================================================================
FUNCTION  GetSINRSamples

DESCRIPTION
  PFAL specific routine to get the FM receiver's SINR sample

DEPENDENCIES
  NIL

RETURN VALUE
  FM command status

SIDE EFFECTS
  None

===========================================================================*/

fm_cmd_status_type GetSINRSamples
(

)
{
  int err,value=0;
  err = vendor_interface->get_fm_ctrl(V4L2_CID_PRIVATE_SINR_SAMPLES,&value);
  if( err < 0 )
  {
    print("Failed to get the SINR samples");
    return FM_CMD_FAILURE;
  }
  else
  {
    fm_global_params.sinr_samples = value;
    print2("Successfully get the  SINR samples %d\n", value);
    return FM_CMD_SUCCESS;
  }
}

/*===========================================================================
FUNCTION  SetSINRSamples

DESCRIPTION
  PFAL specific routine to set the FM receiver's SINR sample

DEPENDENCIES
  NIL

RETURN VALUE
  FM command status

SIDE EFFECTS
  None

===========================================================================*/
fm_cmd_status_type SetSINRSamples
(
  uint8 sinr_sample
)
{
  int err;
  struct v4l2_control control;

  control.id = V4L2_CID_PRIVATE_SINR_SAMPLES;
  control.value = sinr_sample;
  err = vendor_interface->set_fm_ctrl(V4L2_CID_PRIVATE_SINR_SAMPLES, sinr_sample);
  if( err < 0 )
  {
    print("Failed to set the SINR samples\n");
    return FM_CMD_FAILURE;
  }
  else
  {
    print2("Successfully set the SINR samples %d\n", sinr_sample);
    return FM_CMD_SUCCESS;
  }
}

/*===========================================================================
FUNCTION  GetSINRThreshold

DESCRIPTION
  PFAL specific routine to get the FM receiver's SINR Threshold

DEPENDENCIES
  NIL

RETURN VALUE
  FM command status

SIDE EFFECTS
  None

===========================================================================*/

fm_cmd_status_type GetSINRThreshold
(

)
{
  int err,value=0;
  err = vendor_interface->get_fm_ctrl(V4L2_CID_PRIVATE_SINR_THRESHOLD,
                                      &value);
  if( err < 0 )
  {
    print("Failed to get the SINR threshold\n");
    return FM_CMD_FAILURE;
  }
  else
  {
    fm_global_params.sinr_threshold = value;
    print2("Successfully get the SINR threshold %d\n", value);
    return FM_CMD_SUCCESS;
  }
}


/*===========================================================================
FUNCTION  SetSINRThreshold

DESCRIPTION
  PFAL specific routine to set the FM receiver's SINR Threshold

DEPENDENCIES
  NIL

RETURN VALUE
  FM command status

SIDE EFFECTS
  None

===========================================================================*/
fm_cmd_status_type SetSINRThreshold
(
  char sinr_th
)
{
  int err,value;
  value = (int)sinr_th;
  err = vendor_interface->set_fm_ctrl(V4L2_CID_PRIVATE_SINR_THRESHOLD,
                                      sinr_th);
  if( err < 0 )
  {
    print("Failed to set the SINR threshold\n");
    return FM_CMD_FAILURE;
  }
  else
  {
    print2("Successfully set the SINR threshold  %d\n", sinr_th);
    return FM_CMD_SUCCESS;
  }
}


/*===========================================================================
FUNCTION  GetOnChannelThreshold

DESCRIPTION
  PFAL specific routine to get the FM receiver's On channel Threshold

DEPENDENCIES
  NIL

RETURN VALUE
  FM command status

SIDE EFFECTS
  None

===========================================================================*/

fm_cmd_status_type GetOnChannelThreshold
(

)
{
  int err,value=0;

  err = vendor_interface->get_fm_ctrl(V4L2_CID_PRIVATE_INTF_LOW_THRESHOLD,
                                      &value);
  if( err < 0 )
  {
    print("Failed to get the On channel threshold \n");
    return FM_CMD_FAILURE;
  }
  else
  {
    fm_global_params.On_channel_threshold = value;
    print2("Successfully get the On channel threshold  %d\n",value);
    return FM_CMD_SUCCESS;
  }
}

/*===========================================================================
FUNCTION  SetOnChannelThreshold

DESCRIPTION
  PFAL specific routine to set the FM receiver's On channel Threshold

DEPENDENCIES
  NIL

RETURN VALUE
  FM command status

SIDE EFFECTS
  None

===========================================================================*/

fm_cmd_status_type SetOnChannelThreshold
(
 uint8 on_channel_th
)
{
  int err;

  err = vendor_interface->set_fm_ctrl(V4L2_CID_PRIVATE_INTF_LOW_THRESHOLD, on_channel_th);
  if( err < 0 )
  {
    print("Failed to set the On channel threshold\n");
    return FM_CMD_FAILURE;
  }
  else
  {
    print2("Successfully set the On channel threshold %d\n", on_channel_th);
    return FM_CMD_SUCCESS;
  }
}
/*===========================================================================
FUNCTION  GetOffChannelThreshold

DESCRIPTION
  PFAL specific routine to get the FM receiver's Off channel Threshold

DEPENDENCIES
  NIL

RETURN VALUE
  FM command status

SIDE EFFECTS
  None

===========================================================================*/

fm_cmd_status_type GetOffChannelThreshold
(

)
{
  int err,value=0;

  err = vendor_interface->get_fm_ctrl(V4L2_CID_PRIVATE_INTF_HIGH_THRESHOLD,
                                      &value);
  if( err < 0 )
  {
    print("Failed to get the Off channel threshold \n");
    return FM_CMD_FAILURE;
  }
  else
  {
    fm_global_params.Off_channel_threshold = value;
    print2("Successfully get the Off channel threshold  %d\n",value);
    return FM_CMD_SUCCESS;
  }
}

/*===========================================================================
FUNCTION  SetOffChannelThreshold

DESCRIPTION
  PFAL specific routine to set the FM receiver's Off channel Threshold

DEPENDENCIES
  NIL

RETURN VALUE
  FM command status

SIDE EFFECTS
  None

===========================================================================*/

fm_cmd_status_type SetOffChannelThreshold
(
 uint8 off_channel_th
)
{
  int err;
  struct v4l2_control control;

  control.id = V4L2_CID_PRIVATE_INTF_HIGH_THRESHOLD;
  control.value = off_channel_th;
  err = vendor_interface->set_fm_ctrl(V4L2_CID_PRIVATE_INTF_HIGH_THRESHOLD,
                                      off_channel_th);
  if( err < 0 )
  {
    print("Failed to set the Off channel threshold\n");
    return FM_CMD_FAILURE;
  }
  else
  {
    print2("Successfully set the Off channel threshold %d\n", off_channel_th);
    return FM_CMD_SUCCESS;
  }
}

/*===========================================================================
FUNCTION  get_fm_i2c_path

DESCRIPTION
  Helper function to get the path of i2c based on the board

DEPENDENCIES
  NIL

RETURN VALUE
  Path to the i2c device. NULL in case of failure.

SIDE EFFECTS
  None

===========================================================================*/

/*===========================================================================
FUNCTION  FmBusWriteReceiver

DESCRIPTION
  PFAL specific routine to program the FM I2C bus

DEPENDENCIES
  NIL

RETURN VALUE
  FM command status

SIDE EFFECTS
  None

===========================================================================*/
fm_cmd_status_type FmBusWriteReceiver
(
fm_i2c_params writeparams
)
{
  print("FmBusWriteReceiver <\n");
  return FM_CMD_SUCCESS;
}

/*===========================================================================
FUNCTION  FmBusReadReceiver

DESCRIPTION
  PFAL specific routine to read the FM I2C bus

DEPENDENCIES
  NIL

RETURN VALUE
  FM command status

SIDE EFFECTS
  None

===========================================================================*/
fm_cmd_status_type FmBusReadReceiver
(
fm_i2c_params *readparams
)
{

  print("FmBusReadReceiver <\n");
  return FM_CMD_SUCCESS;
}

/*===========================================================================

FUNCTION      ftm_fm_run_mm

DESCRIPTION
  This function is used to run mm-audio-ftm.

DEPENDENCIES
  none

===========================================================================*/
void ftm_fm_run_mm(void) {
   pid_t pid;
   int err = 1;

   printf("entered ftm_fm_run_mm\n");
   pid = fork();

   if (!pid) {
       err = execl(mm_audio_path, mm_audio_path, NULL);
       printf("mm module error %d\n", err);
       exit(0);
   } else if (pid < 0) {
       printf("could not create child process to execute mm audio module\n");
   }
   return;
}

/*===========================================================================

FUNCTION      ftm_fm_audio

DESCRIPTION
  This function is used to load the target based config file and
  set the audio output and volume.

DEPENDENCIES
  none

===========================================================================*/
fm_cmd_status_type ftm_fm_audio(uint8 source, uint8 volume) {

    char *cmd = NULL;
    char vlm[4];
    FILE *sound_card_fp = NULL;
    FILE *config_fp = NULL;
    char sound_card_name[sound_card_len] = {0};
    char sound_card_info[sound_card_len] = {0};
    char config_path[sound_card_len] = {0};
    char *buffer;
    char *config_file = NULL;
    size_t len;
    ssize_t ret;
    int tmp;
    fm_cmd_status_type status = FM_CMD_FAILURE;

    printf("Enter ftm_fm_audio\n");
    cmd = (char*) malloc(CMD_len * sizeof(char));
    if (!cmd) {
       printf("memory allocation failed\n");
       goto failure;
    }
    memset(cmd, 0, CMD_len);

   if (strlcat(cmd, "-tc ", CMD_len) >= CMD_len)
       goto failure;

   printf("string = %s \n",cmd);
   printf("audio_output = %d\n", source);

   buffer = source ? "83 -v " : "74 -v ";
   if (strlcat(cmd, buffer, CMD_len) >= CMD_len)
       goto failure;

   printf("string = %s \n",cmd);
   tmp = snprintf(vlm, sizeof(vlm), "%d", volume);
   if (tmp < 0 || sizeof (vlm) <= (size_t)tmp)
       goto failure;
   if (strlcat(cmd, vlm, CMD_len) >= CMD_len)
       goto failure;

   sound_card_fp = fopen("/proc/asound/cards", "r");
   if (!sound_card_fp) {
       printf("failed to open sound card. err: %d (%s)\n", errno,
                                                        strerror(errno));
       goto failure;
   }

   while (fgets(sound_card_info, sizeof(sound_card_info), sound_card_fp) != NULL) {
       sscanf(sound_card_info, "%*s%*s%*s%*s%s", sound_card_name);
       printf("soundCard Name = %s\n", sound_card_name);
       snprintf(config_path, sizeof(config_path), "%s_%s",audio_config,
               sound_card_name);
   }
   fclose(sound_card_fp);

   if (!(config_file = strchr(config_path, '/'))) {
       memset(config_path, 0,sizeof(config_path));
       snprintf(config_path, sizeof(config_path), "%s", audio_config);
   } else if ((config_fp = fopen(config_file, "r")) == NULL) {
       printf("%s file doesn't exist.\n", config_file);
       memset(config_path, 0,sizeof(config_path));
       snprintf(config_path, sizeof(config_path), "%s", audio_config);
   }

   if (config_fp)
       fclose (config_fp);

   printf("Use audio config file %s\n", config_path);
   printf("string = %s \n",cmd);

   ftm_audio_fd = open("/data/vendor/misc/audio/ftm_commands", O_RDWR);
   if (ftm_audio_fd < 0) {
       printf("Failed to open ftm_commands with write. err: %d (%s)\n", errno,
                                                        strerror(errno));
       goto failure;
   }
   printf(" writing config path\n");
   buffer = config_path;
   len = strlen(config_path) + 1;

   while (len > 0) {
       ret = write(ftm_audio_fd, buffer, len);
       if (ret < 0) {
            printf("Failed to write config_file. err: %d (%s)\n", errno,
                                                        strerror(errno));
            goto failure;
       }
       len -= ret;
       buffer += ret;
   }
   printf("done write config path\n");
   sleep(1);
   printf("writing command for path = 0x%zx \n", strlen(cmd));
   len = strlen(cmd) + 1;

   buffer = cmd;
   while (len > 0) {
       ret = write(ftm_audio_fd, buffer, strlen(cmd) + 1);
       if (ret < 0) {
            printf("Failed to write cmd. err: %d (%s)\n", errno,
                                                        strerror(errno));
            goto failure;
       }
       len -= ret;
       buffer += ret;
   }
   printf("done write command for path\n");
   status = FM_CMD_SUCCESS;
   // fall through intentional
failure:
   if (cmd)
       free(cmd);
   if (ftm_audio_fd > 0) {
       close(ftm_audio_fd);
       ftm_audio_fd = -1;
   }
   return status;
}

void ftm_fm_enable_slimbus(int val)
{
    int err = 0;
    struct timespec* ts;

    printf("++%s val %d\n", __func__, val);

    pthread_mutex_lock(&fm_event_lock);
    err = vendor_interface->set_fm_ctrl(V4L2_CID_PRV_ENABLE_SLIMBUS, val);
    if (err < 0)
        printf("set_fm_ctrl failed for V4L2_CID_PRV_ENABLE_SLIMBUS\n");
    else {
      ts = set_time_out(1);
      if(ts) {
          err = pthread_cond_timedwait(&fm_event_cond, &fm_event_lock, ts);
          ALOGD("%s: event received", __func__);
          pthread_mutex_unlock(&fm_event_lock);
          free(ts);
      }
    }
    pthread_mutex_unlock(&fm_event_lock);
    printf("--%s\n", __func__);
}
