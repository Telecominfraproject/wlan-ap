/*==========================================================================

Description
  Platform specific routines to program the V4L2 driver for FM

# Copyright (c) 2010-2015 by Qualcomm Technologies, Inc.  All Rights Reserved.
# Qualcomm Technologies Proprietary and Confidential.

===========================================================================*/

/*===========================================================================

                         Edit History


when       who     what, where, why
--------   ---     ----------------------------------------------------------
08/03/2011 uppalas  Adding support for new ftm commands
04/05/11   ananthk  Added support for FM Tx functionalities
03/15/11   naveenr  Choosing I2C device path based on board type. Added
                    support for 7x30
02/08/11   braghave Calling the fm_qsoc_patches with right parameter
                    for non-Android case.
06/30/10   rakeshk  Created a source file to implement platform specific
                    routines for FM
07/06/10   rakeshk  Added support for all the Rx commands and clean roomed
                    the header and data structures
01/07/11   rakeshk  Added two support APIs to read/write the I2C bus with
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
#include <pthread.h>
#include <stdio.h>
#include <signal.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>
#include "ftm_common.h"
#include <stdint.h>
#define BIT16 (1<<16)
#define FM_SLAVE_ADDR 0x2A
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

#define UNUSED(x) (void)(x)

enum tavarua_buf_t {
 TAVARUA_BUF_SRCH_LIST,
 TAVARUA_BUF_EVENTS,
 TAVARUA_BUF_RT_RDS,
 TAVARUA_BUF_PS_RDS,
 TAVARUA_BUF_RAW_RDS,
 TAVARUA_BUF_AF_LIST,
 TAVARUA_BUF_MAX
};

enum tavarua_xfr_ctrl_t {
 RDS_PS_0 = 0x01,
 RDS_PS_1,
 RDS_PS_2,
 RDS_PS_3,
 RDS_PS_4,
 RDS_PS_5,
 RDS_PS_6
};

enum tavarua_evt_t {
 TAVARUA_EVT_RADIO_READY,
 TAVARUA_EVT_TUNE_SUCC,
 TAVARUA_EVT_SEEK_COMPLETE,
 TAVARUA_EVT_SCAN_NEXT,
 TAVARUA_EVT_NEW_RAW_RDS,
 TAVARUA_EVT_NEW_RT_RDS,
 TAVARUA_EVT_NEW_PS_RDS,
 TAVARUA_EVT_ERROR,
 TAVARUA_EVT_BELOW_TH,
 TAVARUA_EVT_ABOVE_TH,
 TAVARUA_EVT_STEREO,
 TAVARUA_EVT_MONO,
 TAVARUA_EVT_RDS_AVAIL,
 TAVARUA_EVT_RDS_NOT_AVAIL,
 TAVARUA_EVT_NEW_SRCH_LIST,
 TAVARUA_EVT_NEW_AF_LIST,
 TAVARUA_EVT_TXRDSDAT,
 TAVARUA_EVT_TXRDSDONE,
 TAVARUA_EVT_RADIO_DISABLED
};

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
const char *const ext_audio_config = "-c /system/etc/ftm_test_config_wcd9335";
const unsigned int CMD_len = 16;
const int config_len = 31;
const int ext_config_len = 39;
const int sound_card_name_len = 16;
const char *const mm_audio_path = "/vendor/bin/mm-audio-ftm";
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

/*===========================================================================
FUNCTION  WaitonInterrupt

DESCRIPTION
  Helper function to read the Interrupt register to check for a trasnfer
  complete interrupt

DEPENDENCIES
  NIL

RETURN VALUE
  -1 in failure,positive or zero in success

SIDE EFFECTS
  None

===========================================================================*/

int WaitonInterrupt(int fd,unsigned int intmask,unsigned int waittime)
{
  unsigned char buf[4];
  unsigned int maxtries=0;
  unsigned int readdata=0;
  int ret;
  print("WaitonInterrupt >\n");
  while(((readdata & intmask) != intmask)&&(maxtries < 10))
  {
    usleep(waittime*MULTIPLE_1000);
    /* Read the 3 interrupt registers */
    ret = i2c_read(fd, INTSTAT_0, buf, 3,SLAVE_ADDR);
    if (ret < 0)
    {
      return -1;
    }
    readdata |= buf[0];
    readdata |= buf[1] << 8;
    readdata |= buf[2] << 16;
    maxtries++;
  }
  if((readdata & intmask) != intmask)
  {
    return -1;
  }
   print("WaitonInterrupt <\n");
  return 0;
}

/*===========================================================================
FUNCTION  set_v4l2_ctrl

DESCRIPTION
  Sets the V4L2 control sent as argument with the requested value and returns the status

DEPENDENCIES
  NIL

RETURN VALUE
  FALSE in failure,TRUE in success

SIDE EFFECTS
  None

===========================================================================*/
boolean set_v4l2_ctrl(int fd,uint32 id,int32 value)
{
  struct v4l2_control control;
  int err;

  control.value = value;
  control.id = id;
  switch(id)
  {
    case V4L2_CID_PRIVATE_TAVARUA_REGION :
      if (value == FM_US_EU)
      {
        print("\n  Region  : US-EUROPE\n");
        control.value = FM_US_EU;
      }
      /*
        Increment the 'control.value' to match the 'tavarua_region_t' enum
        variable defined in the V4L2 driver.

           FTM:                               V4L2:

        US/EUROPE       - '0'           US              - '0'
        JAPAN STD       - '1'           EU              - '1'
        JAPAN WIDE      - '2'           JAPAN           - '2'
        USER-DEFINED    - '3'           JAPAN WIDE      - '3'
                                        REGION OTHER    - '4'
      */

      else if(value == FM_JAPAN_STANDARD)
      {
        print("\n Region  : JAPAN-STANARD\n");
        control.value = FM_JAPAN_STANDARD+1;
      }
      else if(value == FM_JAPAN_WIDE)
      {
        print("\n Region  : JAPAN-WIDE\n");
        control.value = FM_JAPAN_WIDE+1;
      }
      else if(value == 3)
      {
        print("\n Region  : USER-DEFINED\n");
        control.value = FM_USER_DEFINED;
      }
      break;
  }
  err = ioctl(fd,VIDIOC_S_CTRL,&control);
  if(err < 0)
  {
      print3("set_v4l2_ctrl failed for control : %x with return value : %d\n",control.id,err);
      return FALSE;
  }
  return TRUE;
}

/*===========================================================================
FUNCTION  read_data_from_v4l2

DESCRIPTION
  reads the fm_radio handle and updates the FM global configuration based on
  the interrupt data received

DEPENDENCIES
  NIL

RETURN VALUE
  FALSE in failure,TRUE in success

SIDE EFFECTS
  None

===========================================================================*/
int read_data_from_v4l2(int fd,uint8* buf,int index)
{
  struct v4l2_requestbuffers reqbuf;
  struct v4l2_buffer v4l2_buf;
  int err;
  memset(&reqbuf, 0x0, sizeof(reqbuf));
  enum v4l2_buf_type type = V4L2_BUF_TYPE_PRIVATE;

  reqbuf.type = V4L2_BUF_TYPE_PRIVATE;
  reqbuf.memory = V4L2_MEMORY_USERPTR;
  memset(&v4l2_buf, 0x0, sizeof(v4l2_buf));
  v4l2_buf.index = index;
  v4l2_buf.type = type;
  v4l2_buf.length = 128;
  v4l2_buf.m.userptr = (unsigned long)buf;
  err = ioctl(fd,VIDIOC_DQBUF,&v4l2_buf) ;
  if(err < 0)
  {
    print2("ioctl failed with error = %d\n",err);
    return -1;
  }
  return v4l2_buf.bytesused;
}

/*===========================================================================
FUNCTION  extract_program_service

DESCRIPTION
  Helper routine to read the Program Services data from the V4L2 buffer
  following a PS event

DEPENDENCIES
  PS event

RETURN VALUE
  TRUE if success,else FALSE

SIDE EFFECTS
  Updates the Global data strutures PS info entry

===========================================================================*/
boolean extract_program_service()
{
  uint8 buf[64];
  int ret;
  print("extract_program_service\n");
  ret = read_data_from_v4l2(fd_radio,buf,TAVARUA_BUF_PS_RDS);
  print2("read_data_from_v4l2 ret = %d\n",ret);
  int num_of_ps = (int)(buf[0] & 0x0F);
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
  return TRUE;
}
/*===========================================================================
FUNCTION  extract_radio_text

DESCRIPTION
  Helper routine to read the Radio text data from the V4L2 buffer
  following a RT event

DEPENDENCIES
  RT event

RETURN VALUE
  TRUE if success,else FALSE

SIDE EFFECTS
  Updates the Global data strutures RT info entry

===========================================================================*/

boolean extract_radio_text()
{
  uint8 buf[120];

  int bytesread = read_data_from_v4l2(fd_radio,buf,TAVARUA_BUF_RT_RDS);
  int radiotext_size = (int)(buf[0] & 0x0F);
  fm_global_params.fm_rt_length = radiotext_size;
  fm_global_params.pgm_id = (((buf[2] & 0xFF) << 8) | (buf[3] & 0xFF));
  fm_global_params.pgm_type = (int)( buf[1] & 0x1F);
  memset(fm_global_params.radio_text,0x0,MAX_RDS_RT_LENGTH);
  memcpy(fm_global_params.radio_text,&buf[5],radiotext_size);
  printf("RT is %s\n", fm_global_params.radio_text);
  return TRUE;
}


/*===========================================================================
FUNCTION  extract_peek_data

DESCRIPTION
  Helper routine to read the data from the V4L2 buffer
  following a riva peek data command complete

DEPENDENCIES
 NIL

RETURN VALUE
  void

SIDE EFFECTS
  Updates the Global data strutures member riva_data_access_params

===========================================================================*/

void extract_peek_data()
{
  uint8 buf[MAX_RIVA_PEEK_RSP_SIZE];
  int bytesread = read_data_from_v4l2(fd_radio,buf,IRIS_BUF_PEEK);
  struct fm_riva_poke_word  *responce = (struct fm_riva_poke_word *)buf;
  memcpy((void*)&fm_global_params.riva_data_access_params,
         (void*)responce,sizeof(buf));
}
/*===========================================================================
FUNCTION  extract_ssbi_peek_data

DESCRIPTION
  Helper routine to read the data from the V4L2 buffer
  following a ssbi peek data command complete

DEPENDENCIES
 NIL

RETURN VALUE
  void

SIDE EFFECTS
  Updates the Global data strutures member ssbi_peek_data

===========================================================================*/


void extract_ssbi_peek_data()
{
  uint8 buf[SSBI_PEEK_DATA_SIZE];
  int bytesread = read_data_from_v4l2(fd_radio,buf,IRIS_BUF_SSBI_PEEK);
  fm_global_params.ssbi_peek_data = buf[0];
}

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
  int bytesread = read_data_from_v4l2(fd_radio,buf,IRIS_BUF_RDS_CNTRS);
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
  uint8 buf[DEFAULT_DATA_SIZE+2]; //2-bytes for status and  data length
  int bytesread = read_data_from_v4l2(fd_radio,buf,IRIS_BUF_RD_DEFAULT);
  readDefaults_data *response = (readDefaults_data *)buf;
  memcpy((void*)&fm_global_params.default_read_data,(void*)response,bytesread);
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
/*===========================================================================
FUNCTION  process_radio_event

DESCRIPTION
  Helper routine to process the radio event read from the V4L2 and performs
  the corresponding action.

DEPENDENCIES
  Radio event

RETURN VALUE
  TRUE if success,else FALSE

SIDE EFFECTS
  Updates the Global data strutures info entry like frequency, station
  available, RDS sync status etc.

===========================================================================*/

boolean process_radio_event(uint8 event_buf)
{
  print2("Process event %d\n",event_buf);
  struct v4l2_frequency freq;
  boolean ret= TRUE;
  switch(event_buf)
  {
    case TAVARUA_EVT_RADIO_READY:
      print("Radio ON complete\n");
      break;
    case TAVARUA_EVT_TUNE_SUCC:
      print("Tune successful\n");
      reset_rds();
      freq.type = V4L2_TUNER_RADIO;
      if(ioctl(fd_radio, VIDIOC_G_FREQUENCY, &freq)< 0)
      {
        return FALSE;
      }
      fm_global_params.current_station_freq =((freq.frequency*MULTIPLE_1000)/TUNE_MULT);
      break;
    case TAVARUA_EVT_SEEK_COMPLETE:
      print("Seek Complete\n");
      freq.type = V4L2_TUNER_RADIO;
      if(ioctl(fd_radio, VIDIOC_G_FREQUENCY, &freq)< 0)
      {
        return FALSE;
      }
      fm_global_params.current_station_freq =((freq.frequency*MULTIPLE_1000)/TUNE_MULT);
      break;
    case TAVARUA_EVT_SCAN_NEXT:
      print("Event Scan next\n");
      break;
    case TAVARUA_EVT_NEW_RAW_RDS:
      print("Received Raw RDS info\n");
      break;
    case TAVARUA_EVT_NEW_RT_RDS:
      print("Received RT \n");
      ret = extract_radio_text();
      break;
    case TAVARUA_EVT_NEW_PS_RDS:
      print("Received PS\n");
      ret = extract_program_service();
      break;
    case TAVARUA_EVT_ERROR:
      print("Received Error\n");
      break;
    case TAVARUA_EVT_BELOW_TH:
      print("Received Below TH\n");
      fm_global_params.service_available = FM_SERVICE_NOT_AVAILABLE;
      break;
    case TAVARUA_EVT_ABOVE_TH:
      print("Received above TH\n");
      fm_global_params.service_available = FM_SERVICE_AVAILABLE;
      break;
    case TAVARUA_EVT_STEREO:
      print("Received Stereo Mode\n");
      fm_global_params.stype = FM_RX_STEREO;
      break;
    case TAVARUA_EVT_MONO:
      print("Received Mono Mode\n");
      fm_global_params.stype = FM_RX_MONO;
      break;
    case TAVARUA_EVT_RDS_AVAIL:
      print("Received RDS Available\n");
      fm_global_params.rds_sync_status = FM_RDS_SYNCED;
      break;
    case TAVARUA_EVT_RDS_NOT_AVAIL:
      print("Received RDS Not Available\n");
      fm_global_params.rds_sync_status = FM_RDS_NOT_SYNCED;
      break;
    case TAVARUA_EVT_NEW_SRCH_LIST:
      print("Received new search list\n");
      break;
    case TAVARUA_EVT_NEW_AF_LIST:
      print("Received new AF List\n");
      break;
    case (RDS_Tx | RDS_PS_0):
      print("\nSuccessfully transmitted PS Header\n");
      break;
    case (RDS_Tx | RDS_PS_1):
    case (RDS_Tx | RDS_PS_2):
    case (RDS_Tx | RDS_PS_3):
    case (RDS_Tx | RDS_PS_4):
    case (RDS_Tx | RDS_PS_5):
    case (RDS_Tx | RDS_PS_6):
      print("\n Successfully transmitted PS Contents \n");
      break;

  }
  /*
   * This logic is applied to ensure the exit ofthe Event read thread
   * before the FM Radio control is turned off. This is a temporary fix
   */
  if(power_down == TRUE)
    return FALSE;
  return ret;
}

/*===========================================================================
FUNCTION  ftm_fm_interrupt_thread

DESCRIPTION
  Thread to perform a continous read on the radio handle for events

DEPENDENCIES
  NIL

RETURN VALUE
  NIL

SIDE EFFECTS
  None

===========================================================================*/

void * ftm_fm_interrupt_thread(void *ptr)
{
  print("Starting FM event listener\n");
  uint8 buf[128];
  boolean status = TRUE;
  int i =0;
  int bytesread = 0;

  UNUSED(ptr);

  while(1)
  {
    bytesread = read_data_from_v4l2(fd_radio,buf,EVENT_LISTEN);
    if(bytesread == -1)
      break;
    for(i =0;i<bytesread;i++)
    {
      status = process_radio_event(buf[i]);
      if(status != TRUE)
      {
        print("FM listener thread exited\n");
        return NULL;
      }
    }
  }
  print("FM listener thread exited\n");
  return NULL;
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
#ifdef FTM_DEBUG

  if(radiocfgptr->is_fm_tx_on)
    print("\nEnable Transmitter entry\n");
  else
    print("\nEnable Receiver entry\n");
#endif
  /*Opening the handle to the V4L2 device */
  fd_radio = open("/dev/radio0",O_RDONLY, O_NONBLOCK);
  if(fd_radio < 0)
  {
    if(radiocfgptr->is_fm_tx_on)
    {
      print2("EnableTransmitter Failed to open = %d\n",fd_radio);
      return FM_CMD_FAILURE;
    }
    else
    {
      print2("EnableReceiver Failed to open = %d\n",fd_radio);
      return FM_CMD_FAILURE;
    }
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

   /* Query the V4L2 device for capabilities  */
  ret = ioctl(fd_radio, VIDIOC_QUERYCAP, &cap);

  if(ret < 0 )
  {
    print("Failed to retrieve the Fm SOC version\n");
    return NULL;
  }
  else
  {
    print3("VIDIOC_QUERYCAP returns :%d: version: %d \n", ret , cap.version );
    chipVersion = cap.version;
  }

  property_get("qcom.bluetooth.soc", value, NULL);
  print2("BT soc is %s\n", value);
  if (strcmp(value, "rome") != 0)
  {
    print("Initiating Soc patch download\n");

#ifndef ANDROID
    snprintf(cmdBuffer, sizeof(cmdBuffer)-1, "fm_qsoc_patches %d %d", cap.version, 0);
    ret = system(cmdBuffer);
    if(ret !=  0)
    {
     print2("Failed to download patches = %d\n",ret);
     return NULL;
    }
#else
    if( ret >= 0 )
    {
      print2("Driver Version(Same as ChipId): %x \n",  cap.version );
      /*Convert the integer to string */
      snprintf(versionStr, sizeof(versionStr)-1, "%d", cap.version );
      property_set("hw.fm.version", versionStr);
    }
    else
    {
      return NULL;
    }
    /*Set the mode for soc downloader*/
    property_set("hw.fm.mode", "normal");
    property_set("ctl.start", "fm_dl");
    sleep(1);
    for(i=0;i<9;i++)
    {
      property_get("hw.fm.init", value, NULL);
      if(strcmp(value, "1") == 0)
      {
        init_success = 1;
        break;
      }
      else
      {
        sleep(1);
      }
    }
    print3("init_success:%d after %d seconds \n", init_success, i);
    if(!init_success)
    {
      property_set("ctl.stop", "fm_dl");
      // close the fd(power down)
      close(fd_radio);
      return NULL;
    }

    property_get("ro.qualcomm.bt.hci_transport", transport, NULL);
    print2("ro.qualcomm.bt.hci_transport = %s\n", transport);
    if ((3 == strlen(transport)) && (!strncmp("smd", transport, 3))) {
      print("Not a WCN2243 target.\n");
    } else {
      /*
       * For WCN2243 based targets check what is the target type.
       * DAC configuration is applicable only msm7627a target.
       */
      property_get("ro.board.platform", product_board_platform_type, NULL);
      if (!strncmp("msm7627a", product_board_platform_type,
          strlen(product_board_platform_type))) {
          property_set("hw.fm.mode", "config_dac");
          property_set("hw.fm.init", "0");
          property_set("hw.fm.isAnalog", "true");
          property_set("ctl.start", "fm_dl");
          for (i = 0; i < 3; i++) {
              property_get("hw.fm.init", value, NULL);
              if (strcmp(value, "1") == 0) {
                  init_success = 1;
                  break;
              } else {
                  sleep(1);
              }
          }
          print3("init_success:%d after %d seconds \n", init_success, i);
          if (!init_success) {
              property_set("ctl.stop", "fm_dl");
              close(fd_radio);
              return NULL;
          }
      } else {
          print("Analog audio path not supported\n");
      }
    }

#endif
  }
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
    ret = set_v4l2_ctrl(fd_radio,V4L2_CID_PRIVATE_TAVARUA_STATE,FM_TX);
    if(ret == FALSE)
    {
      print("Failed to turn on FM Trnasmitter\n");
      return NULL;
    }
    else
      print("\nEnabled FM Transmitter successfully\n");
  }
  else
  {
    ret = set_v4l2_ctrl(fd_radio,V4L2_CID_PRIVATE_TAVARUA_STATE,FM_RX);
    if(ret == FALSE)
    {
      print("Failed to turn on FM Receiver\n");
      return NULL;
    }
    else
      print("\nEnabled FM Receiver successfully\n");
  }

  /*Enable Analog audio path*/
  ret = set_v4l2_ctrl(fd_radio, V4L2_CID_PRIVATE_TAVARUA_SET_AUDIO_PATH, 1);
  if(ret == FALSE)
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
  ret = set_v4l2_ctrl(fd_radio,V4L2_CID_PRIVATE_TAVARUA_RDSON,1);
  if(ret == FALSE)
  {
    print("Failed to set RDS on \n");
    return NULL;
  }

  /* Set the RDS Group Processing : Only in case of FM Receiver */
  if (!radiocfgptr->is_fm_tx_on)
  {
    control.id = V4L2_CID_PRIVATE_TAVARUA_RDSGROUP_PROC;
    ret = ioctl(fd_radio,VIDIOC_G_CTRL,&control);
    if(ret < 0)
    {
      print2("Failed to set RDS group!!!Return value : %d\n",ret);
      return NULL;
    }

    int rdsMask =   FM_RX_RDS_GRP_RT_EBL | FM_RX_RDS_GRP_PS_EBL |
                    FM_RX_RDS_GRP_AF_EBL | FM_RX_RDS_GRP_PS_SIMPLE_EBL ;

    byte rds_group_mask = (byte)control.value;
    byte rdsFilt = 0;
    int  psAllVal=rdsMask & (1 << 4);
    print2("rdsOptions: rdsMask: %x\n",rdsMask);
    rds_group_mask &= 0xC7;

    rds_group_mask  |= ((rdsMask & 0x07) << 3);

    ret = set_v4l2_ctrl(fd_radio,V4L2_CID_PRIVATE_TAVARUA_RDSGROUP_PROC,
                  rds_group_mask);

    if(ret == FALSE)
    {
      print("Failed to set RDS GROUP PROCESSING!!!\n");
      return NULL;
    }

    if (strcmp(value, "rome") == 0)
    {
        ret = set_v4l2_ctrl(fd_radio,V4L2_CID_PRIVATE_TAVARUA_RDSGROUP_MASK, 1);
        if (ret == FALSE)
        {
            print("Failed to set RDS GRP MASK!!!\n");
            return NULL;
        }
        ret = set_v4l2_ctrl(fd_radio,V4L2_CID_PRIVATE_TAVARUA_RDSD_BUF, 1);
        if (ret == FALSE)
        {
            print("Failed to set RDS BUF!!!\n");
            return NULL;
        }
    }
    ret = set_v4l2_ctrl(fd_radio,V4L2_CID_PRIVATE_TAVARUA_ANTENNA,0);
    if(ret == FALSE)
    {
      print("Failed to set ANTENNA!!!\n");
      return NULL;
    }
    ret = set_v4l2_ctrl(fd_radio, V4L2_CID_PRIVATE_IRIS_SOFT_MUTE, 0);
    if(ret == FALSE)
    {
      print("Failed to Disable Soft Mute!!!\n");
      return NULL;
    }

  }

  /*

    Start the 'ftm_fm_interrupt_thread' thread which listens for and processes
    radio events received from the SoC

  */
  pthread_create( &fm_interrupt_thread, NULL, ftm_fm_interrupt_thread, NULL);
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
  ret = set_v4l2_ctrl(fd_radio,V4L2_CID_PRIVATE_TAVARUA_RDSON,0);
  if(ret == FALSE)
  {
    print("DisableFM failed to set RDS off \n");
    return FM_CMD_FAILURE;
  }

  /* Turn off FM */
  /* As part of Turning off FM we will get 'READY' event */
  ret = set_v4l2_ctrl(fd_radio,V4L2_CID_PRIVATE_TAVARUA_STATE,0);
  if(ret == FALSE)
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

  /* Wait for 'fm_interrupt_thread' thread to complete its execution */
  pthread_join(fm_interrupt_thread,NULL);

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

  print2("Stopping the FM control = %d\n",close(fd_radio));
  fd_radio = -1;
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
  if(fd_radio < 0)
    return FM_CMD_NO_RESOURCES;

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

  ret = set_v4l2_ctrl(fd_radio,V4L2_CID_PRIVATE_TAVARUA_EMPHASIS,
                radiocfgptr->emphasis);
  if(ret == FALSE)
  {
    print("ConfigureFM : Failed to set Emphasis \n");
    return FM_CMD_FAILURE;
  }

  /* Set channel spacing */
  ret = set_v4l2_ctrl(fd_radio,V4L2_CID_PRIVATE_TAVARUA_SPACING,
                radiocfgptr->spacing);
  if(ret == FALSE)
  {
    print("ConfigureFM : Failed to set channel spacing  \n");
    return FM_CMD_FAILURE;
  }

  /* Set RDS/RDBS Standard */
  ret = set_v4l2_ctrl(fd_radio,V4L2_CID_PRIVATE_TAVARUA_RDS_STD,
                radiocfgptr->rds_system);
  if(ret == FALSE)
  {
    print("ConfigureFM : Failed to set RDS std \n");
    return FM_CMD_FAILURE;
  }

  /* Set band limit and audio mode to Mono/Stereo */
  tuner.index = 0;
  tuner.signal = 0;

  switch(radiocfgptr->band)
  {
      case FM_US_EU:
        tuner.rangelow = REGION_US_EU_BAND_LOW * (TUNE_MULT/1000);
        tuner.rangehigh = REGION_US_EU_BAND_HIGH * (TUNE_MULT/1000);
        break;
      case FM_JAPAN_STANDARD:
        tuner.rangelow = REGION_JAPAN_STANDARD_BAND_LOW * (TUNE_MULT/1000);
        tuner.rangehigh = REGION_JAPAN_STANDARD_BAND_HIGH * (TUNE_MULT/1000);
        break;
      case FM_JAPAN_WIDE:
        tuner.rangelow = REGION_JAPAN_WIDE_BAND_LOW * (TUNE_MULT/1000);
        tuner.rangehigh = REGION_JAPAN_WIDE_BAND_HIGH * (TUNE_MULT/1000);
        break;
      default:
        tuner.rangelow = radiocfgptr->bandlimits.lower_limit * (TUNE_MULT/1000);
        tuner.rangehigh = radiocfgptr->bandlimits.upper_limit * (TUNE_MULT/1000);
        break;
  }

  ret = ioctl(fd_radio,VIDIOC_S_TUNER,&tuner);
  if(ret < 0)
  {
    print("ConfigureFM : Failed to set band limits and audio mode \n");
    return FM_CMD_FAILURE;
  }

  /* Set Region */
  radiocfgptr->band = FM_USER_DEFINED;
  ret = set_v4l2_ctrl(fd_radio,V4L2_CID_PRIVATE_TAVARUA_REGION,radiocfgptr->band);
  if(ret == FALSE)
  {
    print("ConfigureFM : Failed to set band\n");
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
  double tune;
  struct v4l2_frequency freq_struct;
  struct v4l2_tuner tuner;
#ifdef FTM_DEBUG
  print2("\nSetFrequency Receiver entry freq = %d\n",(int)ulfreq);
#endif

  if(fd_radio < 0)
    return FM_CMD_NO_RESOURCES;

  tuner.index = 0;

  err = ioctl(fd_radio,VIDIOC_G_TUNER,&tuner);
  if(err < 0)
  {
    print("SetFrequencyReceiver : Failed to get band limits and audio mode \n");
    return FM_CMD_FAILURE;
  }
  freq_struct.type = V4L2_TUNER_RADIO;
  freq_struct.frequency = (ulfreq) * (TUNE_MULT/1000);
  if (freq_struct.frequency >= tuner.rangelow && freq_struct.frequency <= tuner.rangehigh)
  {
    err = ioctl(fd_radio, VIDIOC_S_FREQUENCY, &freq_struct);
    if(err < 0)
    {
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
  print2("\n Currently tuned station is : %u \n",fm_global_params.current_station_freq);
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
      print2("\n Frequency : %u \n",ulfreq);
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
      print2("\nFailed to set the Power Level for %u\n",ulfreq);
      return FM_CMD_FAILURE;
    }
  }
  else
    print2("\nSuccessfully set the Power Level for %u\n",ulfreq);

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
  if(fd_radio < 0)
    return FM_CMD_NO_RESOURCES;

  control.value = mutemode;
  control.id = V4L2_CID_AUDIO_MUTE;

  for(i=0;i<3;i++)
  {
    err = ioctl(fd_radio,VIDIOC_S_CTRL,&control);
    if(err >= 0)
    {
      print("SetMuteMode Success\n");
      return FM_CMD_SUCCESS;
    }
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
  if(fd_radio < 0)
    return FM_CMD_NO_RESOURCES;

  control.value = mutemode;
  control.id = V4L2_CID_PRIVATE_IRIS_SOFT_MUTE;

  err = ioctl(fd_radio,VIDIOC_S_CTRL,&control);
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

  err = ioctl(fd_radio,VIDIOC_S_CTRL,&control);
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

  err = ioctl(fd_radio,VIDIOC_S_CTRL,&control);
  if(err >= 0)
  {
    print("SetRiva Address Success\n");
  }
  control.value = peek_word.payload_length;
  control.id = V4L2_CID_PRIVATE_IRIS_RIVA_ACCS_LEN;
  err = ioctl(fd_radio,VIDIOC_S_CTRL,&control);
  if(err >= 0)
  {
    print("SetDataLen Success\n");
  }
  control.id = V4L2_CID_PRIVATE_IRIS_RIVA_PEEK;
  err = ioctl(fd_radio,VIDIOC_S_CTRL,&control);
  if(err >= 0)
  {
    extract_peek_data();
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

  err = ioctl(fd_radio,VIDIOC_S_CTRL,&control);
  if(err >= 0)
  {
    print("SetRiva Address Success\n");
  }
  control.value = poke_word.payload_length;
  control.id = V4L2_CID_PRIVATE_IRIS_RIVA_ACCS_LEN;
  err = ioctl(fd_radio,VIDIOC_S_CTRL,&control);
  if(err >= 0)
  {
    print("SetDataLen Success\n");
  }
  control.id = V4L2_CID_PRIVATE_IRIS_RIVA_POKE;
  control.value = (uint32)(uintptr_t)(&poke_word.data);
  err = ioctl(fd_radio,VIDIOC_S_CTRL,&control);
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
  err = ioctl(fd_radio,VIDIOC_S_CTRL,&control);
  if(err >= 0)
  {
    extract_ssbi_peek_data();
    print2("SSBIPeek Success\n %d",peek_reg.data);
    return FM_CMD_SUCCESS;
  }

  print2("SSBIPeekData ret = %d\n",err);
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

  err = ioctl(fd_radio,VIDIOC_S_CTRL,&control);
  if(err >= 0)
  {
    print("SetRiva Address Success\n");
  }
  control.id = V4L2_CID_PRIVATE_IRIS_SSBI_POKE;
  control.value = poke_reg.data;
  err = ioctl(fd_radio,VIDIOC_S_CTRL,&control);
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
  struct v4l2_control control;
  control.value = rdsCounters;
  control.id = V4L2_CID_PRIVATE_IRIS_RDS_GRP_COUNTERS;

  err = ioctl(fd_radio,VIDIOC_S_CTRL,&control);
  if(err >= 0)
  {
    extract_rds_grp_cntr_data();
    print("Read RDS GROUP counters success\n");
    return FM_CMD_SUCCESS;
  }
  print2("Read RDS GROUP counters = %d\n",err);
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
  int err;
  struct v4l2_control control;
  control.value = hlsi;
  control.id = V4L2_CID_PRIVATE_IRIS_HLSI;
  print2("Setting HLSI to  = %d",hlsi);
  err = ioctl(fd_radio,VIDIOC_S_CTRL,&control);
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
  int err;
  struct v4l2_control control;
  control.value = notch;
  control.id = V4L2_CID_PRIVATE_IRIS_SET_NOTCH_FILTER;
  print2("Setting Notch filter  to  = %d",notch);
  err = ioctl(fd_radio,VIDIOC_S_CTRL,&control);
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
  struct v4l2_tuner tuner;
  int err;
  print2("SetStereoModeReceiver stereomode = %d \n",stereomode);
  if(fd_radio < 0)
    return FM_CMD_NO_RESOURCES;

  tuner.index = 0;
  err = ioctl(fd_radio, VIDIOC_G_TUNER, &tuner);
  print3("Get stereo mode ret = %d tuner.audmode = %d\n",err,tuner.audmode);
  if(err < 0)
    return FM_CMD_FAILURE;

/*There is a discrepancy between V4L2 macros and FTM/HCI
Interface documentation. The stereo/mono settings are swapped
In FTM/HCI documentation.  So we are providing a work around by
Swapping the mono/stereo settings here*/

  tuner.audmode = (!stereomode);
  err = ioctl(fd_radio, VIDIOC_S_TUNER, &tuner);
  print2("Set stereo mode ret = %d\n",err);
  if(err < 0)
    return FM_CMD_FAILURE;
  print("SetStereoMode Success\n");
  return FM_CMD_SUCCESS;
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
  int i;
  if(fd_radio < 0)
    return FM_CMD_NO_RESOURCES;
  configparams->current_station_freq = fm_global_params.current_station_freq;
  configparams->service_available = fm_global_params.service_available;

  struct v4l2_tuner tuner;
  tuner.index = 0;
  tuner.signal = 0;
  if(ioctl(fd_radio, VIDIOC_G_TUNER, &tuner) < 0)
    return FM_CMD_FAILURE;

  configparams->rssi = tuner.signal;
  configparams->stype = fm_global_params.stype;
  configparams->rds_sync_status = fm_global_params.rds_sync_status;

  struct v4l2_control control;
  control.id = V4L2_CID_AUDIO_MUTE;

  for(i=0;i<3;i++)
  {
    int err = ioctl(fd_radio,VIDIOC_G_CTRL,&control);
    if(err >= 0)
    {
      configparams->mute_status = control.value;
      return FM_CMD_SUCCESS;
    }
  }

  return FM_CMD_FAILURE;
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

  ret = set_v4l2_ctrl(fd_radio,V4L2_CID_PRIVATE_TAVARUA_RDSGROUP_MASK,
        rdsoptions.rds_group_mask);
  if(ret == FALSE)
  {
    print2("SetRdsOptionsReceiver Failed to set RDS group options = %d\n",ret);
    return FM_CMD_FAILURE;
  }

  ret = set_v4l2_ctrl(fd_radio,V4L2_CID_PRIVATE_TAVARUA_RDSD_BUF,
        rdsoptions.rds_group_buffer_size);
  if(ret == FALSE)
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
  ret = set_v4l2_ctrl(fd_radio,V4L2_CID_PRIVATE_TAVARUA_RDSGROUP_PROC,
        rdsgroupoptions);
  if(ret == FALSE)
  {
    print2("SetRdsGroupProcReceiver Failed to set RDS proc = %d\n",ret);
    return FM_CMD_FAILURE;
  }

  print("SetRdsGroupProcReceiver<\n");
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
  if(fd_radio < 0)
    return FM_CMD_NO_RESOURCES;

  control.value = powermode;
  control.id = V4L2_CID_PRIVATE_TAVARUA_LP_MODE;

  for(i=0;i<3;i++)
  {
    err = ioctl(fd_radio,VIDIOC_S_CTRL,&control);
    if(err >= 0)
    {
      print("SetPowerMode Success\n");
      return FM_CMD_SUCCESS;
    }
  }
  return FM_CMD_FAILURE;
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
  if(fd_radio < 0)
    return FM_CMD_NO_RESOURCES;

  control.value = signalthreshold;
  control.id = V4L2_CID_PRIVATE_TAVARUA_SIGNAL_TH;

  for(i=0;i<3;i++)
  {
    err = ioctl(fd_radio,VIDIOC_S_CTRL,&control);
    if(err >= 0)
    {
      print("SetSignalThresholdReceiver Success\n");
      return FM_CMD_SUCCESS;
    }
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
  struct v4l2_control control;
  int i,err;
  if(fd_radio < 0)
    return FM_CMD_NO_RESOURCES;

  control.id = V4L2_CID_PRIVATE_TAVARUA_SIGNAL_TH;

  for(i=0;i<3;i++)
  {
    err = ioctl(fd_radio,VIDIOC_G_CTRL,&control);
    if(err >= 0)
    {
      print2("GetSignalThresholdReceiver Success = %d\n",control.value);
      *signalthreshold = control.value;
      return FM_CMD_SUCCESS;
    }
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
  struct v4l2_control control;
  struct v4l2_hw_freq_seek hwseek;
  boolean ret;
  print("SearchStationsReceiver\n");
  if(fd_radio < 0)
    return FM_CMD_NO_RESOURCES;

  ret = set_v4l2_ctrl(fd_radio,V4L2_CID_PRIVATE_TAVARUA_SRCHMODE,
        searchstationsoptions.search_mode);
  if(ret == FALSE)
  {
    print("SearchStationsReceiver failed \n");
    return FM_CMD_FAILURE;
  }

  ret = set_v4l2_ctrl(fd_radio,V4L2_CID_PRIVATE_TAVARUA_SCANDWELL,
        searchstationsoptions.dwell_period);
  if(ret == FALSE)
  {
    print("SearchStationsReceiver failed \n");
    return FM_CMD_FAILURE;
  }

  if (searchstationsoptions.search_dir)
    searchstationsoptions.search_dir = SRCH_DIR_UP;
  else
    searchstationsoptions.search_dir = SRCH_DIR_DOWN;

  hwseek.seek_upward = searchstationsoptions.search_dir;
  hwseek.type = V4L2_TUNER_RADIO;
  err = ioctl(fd_radio,VIDIOC_S_HW_FREQ_SEEK,&hwseek);

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
  boolean ret;
  struct v4l2_control control;
  struct v4l2_hw_freq_seek hwseek;

  print("SearchRdsStationsReceiver>\n");
  if(fd_radio < 0)
    return FM_CMD_NO_RESOURCES;

  ret = set_v4l2_ctrl(fd_radio,V4L2_CID_PRIVATE_TAVARUA_SRCHMODE,
        searchrdsstationsoptions.search_mode);
  if(ret == FALSE)
  {
    print("SearchRdsStationsReceiver failed \n");
    return FM_CMD_FAILURE;
  }

  ret = set_v4l2_ctrl(fd_radio,V4L2_CID_PRIVATE_TAVARUA_SCANDWELL,
        searchrdsstationsoptions.dwell_period);
  if(ret == FALSE)
  {
    print("SearchRdsStationsReceiver failed \n");
    return FM_CMD_FAILURE;
  }

  ret = set_v4l2_ctrl(fd_radio,V4L2_CID_PRIVATE_TAVARUA_SRCH_PTY,
        searchrdsstationsoptions.program_type);
  if(ret == FALSE)
  {
    print("SearchRdsStationsReceiver failed \n");
    return FM_CMD_FAILURE;
  }

  ret = set_v4l2_ctrl(fd_radio,V4L2_CID_PRIVATE_TAVARUA_SRCH_PI,
        searchrdsstationsoptions.program_id);
  if(ret == FALSE)
  {
    print("SearchRdsStationsReceiver failed \n");
    return FM_CMD_FAILURE;
  }

  hwseek.seek_upward = searchrdsstationsoptions.search_dir;
  hwseek.type = V4L2_TUNER_RADIO;
  err = ioctl( fd_radio, VIDIOC_S_HW_FREQ_SEEK,&hwseek);

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
  int i,err;
  boolean ret;
  struct v4l2_control control;
  struct v4l2_hw_freq_seek hwseek;

  print("SearchStationListReceiver>\n");
  if(fd_radio < 0)
    return FM_CMD_NO_RESOURCES;

  ret = set_v4l2_ctrl( fd_radio, V4L2_CID_PRIVATE_TAVARUA_SRCHMODE,
        searchliststationsoptions.search_mode);
  if(ret == FALSE)
  {
    print("SearchStationListReceiver failed \n");
    return FM_CMD_FAILURE;
  }

  ret = set_v4l2_ctrl( fd_radio, V4L2_CID_PRIVATE_TAVARUA_SRCH_CNT,
        searchliststationsoptions.srch_list_max);
  if(ret == FALSE)
  {
    print("SearchStationListReceiver failed \n");
    return FM_CMD_FAILURE;
  }

  ret = set_v4l2_ctrl( fd_radio, V4L2_CID_PRIVATE_TAVARUA_SRCH_PTY,
        searchliststationsoptions.program_type);
  if(ret == FALSE)
  {
    print("SearchStationListReceiver failed \n");
    return FM_CMD_FAILURE;
  }

  hwseek.seek_upward = searchliststationsoptions.search_dir;
  hwseek.type = V4L2_TUNER_RADIO;
  err = ioctl( fd_radio, VIDIOC_S_HW_FREQ_SEEK,&hwseek);

  if(err < 0)
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
  struct v4l2_control control;
  boolean ret;

  if(fd_radio < 0)
    return FM_CMD_NO_RESOURCES;
  ret = set_v4l2_ctrl( fd_radio, V4L2_CID_PRIVATE_TAVARUA_SRCHON,0);
  if(ret == FALSE)
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
  int err;
  struct v4l2_control control;

  control.id = V4L2_CID_PRIVATE_SINR_SAMPLES;
  err = ioctl(fd_radio, VIDIOC_G_CTRL, &control);
  if( err < 0 )
  {
    print("Failed to get the SINR samples");
    return FM_CMD_FAILURE;
  }
  else
  {
    fm_global_params.sinr_samples = control.value;
    print2("Successfully get the  SINR samples %d\n", control.value);
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
  err = ioctl(fd_radio, VIDIOC_S_CTRL, &control);
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
  int err;
  struct v4l2_control control;

  control.id = V4L2_CID_PRIVATE_SINR_THRESHOLD;
  err = ioctl(fd_radio, VIDIOC_G_CTRL, &control);
  if( err < 0 )
  {
    print("Failed to get the SINR threshold\n");
    return FM_CMD_FAILURE;
  }
  else
  {
    fm_global_params.sinr_threshold = control.value;
    print2("Successfully get the SINR threshold %d\n", control.value);
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
  int err;
  struct v4l2_control control;

  control.id = V4L2_CID_PRIVATE_SINR_THRESHOLD;
  control.value = sinr_th;
  err = ioctl(fd_radio, VIDIOC_S_CTRL, &control);
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
  int err;
  struct v4l2_control control;

  control.id = V4L2_CID_PRIVATE_INTF_LOW_THRESHOLD;
  err = ioctl(fd_radio, VIDIOC_G_CTRL, &control);
  if( err < 0 )
  {
    print("Failed to get the On channel threshold \n");
    return FM_CMD_FAILURE;
  }
  else
  {
    fm_global_params.On_channel_threshold = control.value;
    print2("Successfully get the On channel threshold  %d\n", control.value);
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
  struct v4l2_control control;

  control.id = V4L2_CID_PRIVATE_INTF_LOW_THRESHOLD;
  control.value = on_channel_th;
  err = ioctl(fd_radio, VIDIOC_S_CTRL, &control);
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
  int err;
  struct v4l2_control control;

  control.id = V4L2_CID_PRIVATE_INTF_HIGH_THRESHOLD;
  err = ioctl(fd_radio, VIDIOC_G_CTRL, &control);
  if( err < 0 )
  {
    print("Failed to get the Off channel threshold \n");
    return FM_CMD_FAILURE;
  }
  else
  {
    fm_global_params.Off_channel_threshold = control.value;
    print2("Successfully get the Off channel threshold  %d\n", control.value);
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
  err = ioctl(fd_radio, VIDIOC_S_CTRL, &control);
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

static char* get_fm_i2c_path( void )
{
  char *fm_i2c_path = NULL;
  char board_platform_type[92]; // max possible length for 'value' from property_get()
  property_get("ro.board.platform", board_platform_type, "default");
  print2("ro.board.platform %s \n", board_platform_type);
  if(strcasestr(board_platform_type, "default"))
  {
       print("unable to determine the board platform type \n");
       return NULL;
  }
  if(strcasestr(board_platform_type, "msm8660"))
  {
        fm_i2c_path = (char *)fm_i2c_path_8660;
  }
  else if(strcasestr(board_platform_type, "msm7630_surf"))
  {
        fm_i2c_path = (char *)fm_i2c_path_7x30;
  }
  else if(strcasestr(board_platform_type, "msm7627a"))
  {
       fm_i2c_path = (char *)fm_i2c_path_7627a;
  }
  if(fm_i2c_path != NULL)
    print2("I2c device path is %s \n", fm_i2c_path);

  return fm_i2c_path;
}


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
  int fd_i2c;
  int ret;
  char *i2cdevice = get_fm_i2c_path();
  if (i2cdevice == NULL)
  {
    print("== I2C device path not available \n");
    return FM_CMD_FAILURE;
  }
  fd_i2c = open(i2cdevice, O_RDWR);
  print("FmBusWriteReceiver >\n");
  if(fd_i2c < 0)
    return FM_CMD_FAILURE;
  ret = i2c_write( fd_i2c, writeparams.offset, writeparams.data,
        writeparams.payload_length, writeparams.slaveaddress);
  if (ret < 0)
  {
    print(" i2c_write failed to Write the data  \n");
    close(fd_i2c);
    return FM_CMD_FAILURE;
  }
  print2("== I2C Write SLAVE_ADDR = 0x%x\n",writeparams.slaveaddress);
  print2("== I2C reg = 0x%x ===\n",writeparams.offset);
  print2("== I2C data  = 0x%x \n",writeparams.data[0]);

  close(fd_i2c);
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
  int fd_i2c;
  int ret;
  char *i2cdevice = get_fm_i2c_path();
  if (i2cdevice == NULL)
  {
    print("== I2C device path not available \n");
    return FM_CMD_FAILURE;
  }
  fd_i2c = open(i2cdevice, O_RDWR);
  print("FmBusReadReceiver >\n");
  ret = i2c_read( fd_i2c, readparams->offset, readparams->data,
                        readparams->payload_length, readparams->slaveaddress);
  if (ret < 0)
  {
    print(" i2c_read failed to read the data  \n");
    close(fd_i2c);
    return FM_CMD_FAILURE;
  }
  close(fd_i2c);
  print2("== I2C Read SLAVE_ADDR = 0x%x\n",readparams->slaveaddress);
  print2("== I2C reg = 0x%x ===\n",readparams->offset);
  print2("== I2C data  = 0x%x \n",readparams->data[0]);

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
    int sound_card_fd = -1;
    char sound_card_name[sound_card_name_len];
    char *buffer;
    size_t len;
    ssize_t ret;
    int tmp;
    fm_cmd_status_type status = FM_CMD_FAILURE;
    int prop_ret = 0;
    char bt_soc_type[PROPERTY_VALUE_MAX];

    printf("Enter ftm_fm_audio\n");
    memset(sound_card_name, 0, sizeof(sound_card_name));
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
   buffer = source ? "51 -v " : "74 -v ";
   if (strlcat(cmd, buffer, CMD_len) >= CMD_len)
       goto failure;

   printf("string = %s \n",cmd);
   tmp = snprintf(vlm, sizeof(vlm), "%d", volume);
   if (tmp < 0 || sizeof (vlm) <= (size_t)tmp)
       goto failure;
   if (strlcat(cmd, vlm, CMD_len) >= CMD_len)
       goto failure;

   sound_card_fd = open("/proc/asound/card0/id", O_RDWR);
   if (sound_card_fd < 0) {
       printf("failed to open sound card. err: %d (%s)\n", errno,
                                                        strerror(errno));
       goto failure;
   }

   len = sound_card_name_len - 1;
   while (len > 0) {
       ret = read (sound_card_fd,
                       &sound_card_name[sound_card_name_len - 1 - len], len);
       if (ret < 0) {
           printf("Failed to read sound card name. err: %d (%s)\n", errno,
                                                        strerror(errno));
           goto failure;
       }
       if (!ret) {
           if(!sound_card_name[0])
               goto failure;
           else
               break;
       }
       len -= ret;
   }
   printf("sound_card_name = %s\n", sound_card_name);
   printf("string = %s \n",cmd);

   ftm_audio_fd = open("/data/vendor/misc/audio/ftm_commands", O_RDWR);
   if (ftm_audio_fd < 0) {
       printf("Failed to open ftm_commands with write. err: %d (%s)\n", errno,
                                                        strerror(errno));
       goto failure;
   }
   printf(" writing config path\n");
   prop_ret = property_get("qcom.bluetooth.soc", bt_soc_type, NULL);
   if (prop_ret != 0) {
       printf("qcom.bluetooth.soc set to %s\n", bt_soc_type);
       if (!strncasecmp(bt_soc_type, "rome", sizeof("rome"))) {
           buffer = audio_config;
           len = config_len;
           goto skip;
       }
   }
   if (strstr(sound_card_name, "tasha")) {
       printf("using external codec\n");
       buffer = (char *)ext_audio_config;
       len = ext_config_len;
   } else {
       printf("using internal codec\n");
       buffer = (char *)audio_config;
       len = config_len;
   }

skip:
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
   if (sound_card_fd > 0)
       close(sound_card_fd);
   if (ftm_audio_fd > 0) {
       close(ftm_audio_fd);
       ftm_audio_fd = -1;
   }
   return status;
}

