/*==========================================================================

                     FTM FM Source File

Description
  FTM platform independent processing of packet data

# Copyright (c) 2010-2012 by Qualcomm Technologies, Inc.  All Rights Reserved.
# Qualcomm Technologies Proprietary and Confidential.

===========================================================================*/

/*===========================================================================

                         Edit History


when       who     what, where, why
--------   ---     ----------------------------------------------------------
08/03/2011 uppalas  Adding support for new ftm commands
06/18/10   rakeshk  Created a source file to implement routines for FTM
                    command processing for FM
07/06/10   rakeshk  Updated the ftm_fm_dispatch with new commands
01/07/11   rakeshk  Removed the check condition in FM bus read/write for
                    FM On. This is due to the reason that the top level i2c
                    read/write of the Bahama/Marimba SoC doesnot depend on FM
                    SoC being intialised.
                    Added support for new FM FTM APIS
02/09/11   rakeshk  Added support for BLER FTM APIs
04/03/11   ananthk  Added support for FM FTM Transmit APIs
===========================================================================*/
#include <time.h>
#include "event.h"
#include "msg.h"
#include "log.h"

#include "diag_lsm.h"

#include "diagpkt.h"
#include "diagcmd.h"
#include "diag.h"
#include <errno.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <pthread.h>
#include "ftm_fm_pfal.h"
#include "ftm_common.h"
#include <unistd.h>
#include <stdio.h>
#include <utils/Log.h>
#include <string.h>
#include <cutils/properties.h>

#define UNUSED(x) (void)(x)

#define FTM_DEBUG
const int PROP_SET_SUCC = 0;
const int init_audio_vlm = 50; //set the initial volume to 50
const char *const fm_audio_disable = "quit";
const char *const audio_ftm_cmds = "/data/vendor/misc/audio/ftm_commands";
/* Global union type for FM params */
fm_cfg_request fmrequestparams;
static uint8 fm_audio_output = -1;
static uint8 enable_mm_fmconfig = 0;
extern int ftm_audio_fd;
volatile fm_power_state fmPowerState = FM_POWER_OFF;
fm_i2c_params common_handle;
extern fm_station_params_available fm_global_params;
/*===========================================================================
FUNCTION   ftm_send_async_msg

DESCRIPTION
  Processes the log buffer sent and writes it to the libdiag for sending the Cmd
  response

DEPENDENCIES
  NIL

RETURN VALUE
  NIL

SIDE EFFECTS
  None

===========================================================================*/

void ftm_send_async_msg(
   uint8                EvName,
   /**< Event ID indicates which event is being returned. */

   FmEvResultType     EvResult,
   /**< Event result indicates success or failure. */

   PACKED void*         response,
   /**< Event body contains the returned event information. */

   uint16               length
   /**< Event response length. */)
{
  int result = log_status(LOG_FTM_FM_C);
  ftm_fm_log_pkt_type*  ftm_fm_log_pkt_ptr = NULL;

  if((response == NULL) || (length == 0))
  {
    return;
  }

  if(result == 1)
  {
    ftm_fm_log_pkt_ptr = (ftm_fm_log_pkt_type *)log_alloc(LOG_FTM_FM_C,FTM_FM_LOG_HEADER_SIZE + (length-1));
    if(ftm_fm_log_pkt_ptr != NULL)
    {
      /* FTM LOG FM ID */
      ftm_fm_log_pkt_ptr->EvName         = EvName;
      ftm_fm_log_pkt_ptr->EvResult       = EvResult;
      memcpy((void *)ftm_fm_log_pkt_ptr->data,(void *)response,length);
      log_commit( ftm_fm_log_pkt_ptr );
    }
  }
}

/*===========================================================================
FUNCTION  convert_cmdstatus_to_ftmstatus

DESCRIPTION
  Helper routine to convert the FM command status to FTM command status

DEPENDENCIES
  NIL

RETURN VALUE
  FTM command status

SIDE EFFECTS
  None

===========================================================================*/
ftm_fm_api_result_type convert_cmdstatus_to_ftmstatus(fm_cmd_status_type cmdStatus)
{
  switch(cmdStatus)
  {
    case FM_CMD_SUCCESS:
      return FTM_FM_SUCCESS;
    case FM_CMD_NO_RESOURCES:
      return FTM_NO_RESOURCES;
    case FM_CMD_INVALID_PARAM:
      return FTM_INVALID_PARAM;
    case FM_CMD_DISALLOWED:
      return FTM_FM_DISALLOWED;
    case FM_CMD_UNRECOGNIZED_CMD:
      return FTM_FM_UNRECOGNIZED_CMD;
    case FM_CMD_FAILURE:
      return FTM_FAIL;
    case FM_CMD_PENDING:
      return FTM_FM_PENDING;
    default :
      return FTM_FAIL;
  }
}

/*===========================================================================
FUNCTION  ftm_fm_copy_request_data

DESCRIPTION
  Helper routine to copy the FTM command data to the global union data type

DEPENDENCIES
  NIL

RETURN VALUE
  NIL

SIDE EFFECTS
  None

===========================================================================*/
void ftm_fm_copy_request_data
(
  PACKED void * request ,
  uint16        length
)
{
  ftm_fm_pkt_type* req_pkt;
  int i;
  req_pkt = (ftm_fm_pkt_type *) request;
  /* Copy any params into our union if they exist. */
  memmove((void*)(&fmrequestparams),
               (void*)(&req_pkt->data),
               (length));
}

/*===========================================================================
FUNCTION  ftm_fm_rx_enable_receiver

DESCRIPTION
  HAL routine to process the response from the PFAL layer call
  EnableReceiver, also constructs the response packet

DEPENDENCIES
  NIL

RETURN VALUE
  A Packed structre pointer including the response to the Enable Receiver
  FTM FM packet

SIDE EFFECTS
  None

===========================================================================*/
PACKED void *  ftm_fm_rx_enable_receiver
(
  void
)
{
  fm_cmd_status_type cmdStatus = FM_CMD_UNRECOGNIZED_CMD;
  generic_response * response = (generic_response *)
                                              diagpkt_subsys_alloc( DIAG_SUBSYS_FTM
                                                                    , FTM_FM_CMD_CODE
                                                                    , sizeof(generic_response)
                                                                    );
  if(response ==  NULL)
  {
    printf("%s Failed to allocate resource",__func__);
    return (void *)convert_cmdstatus_to_ftmstatus(FM_CMD_NO_RESOURCES);
  }

  response->result =  FTM_FM_SUCCESS;
  if(fmPowerState == FM_TX_ON || fmPowerState == FM_POWER_TRANSITION)
  {
    response->result = convert_cmdstatus_to_ftmstatus(FM_CMD_DISALLOWED);
    return response;
  }
  else if (fmPowerState == FM_RX_ON)
    return response;

  ftm_fm_run_mm();
  cmdStatus = EnableFM(&fmrequestparams.cfg_param);
  response->result = convert_cmdstatus_to_ftmstatus(cmdStatus);
  return response;
}

/*===========================================================================
FUNCTION  ftm_fm_tx_enable_transmitter

DESCRIPTION
  HAL routine to process the response from the PFAL layer call
  EnableTransmitter, also constructs the response packet

DEPENDENCIES
  NIL

RETURN VALUE
  A Packed structre pointer including the response to the Enable Transmitter
  FTM FM packet

SIDE EFFECTS
  None

===========================================================================*/
PACKED void *  ftm_fm_tx_enable_transmitter
(
  void
)
{
  fm_cmd_status_type cmdStatus = FM_CMD_UNRECOGNIZED_CMD;
  generic_response * response = (generic_response *)
                                              diagpkt_subsys_alloc( DIAG_SUBSYS_FTM
                                                                    , FTM_FM_CMD_CODE
                                                                    , sizeof(generic_response)
                                                                  );

  if(response ==  NULL)
  {
    printf("%s Failed to allocate resource",__func__);
    return (void *)convert_cmdstatus_to_ftmstatus(FM_CMD_NO_RESOURCES);
  }
  printf("\n Inside ftm_fm_tx_enable_transmitter() function...\n");

  response->result =  FTM_FM_SUCCESS;
  if(fmPowerState == FM_RX_ON || fmPowerState == FM_POWER_TRANSITION)
  {
    response->result = convert_cmdstatus_to_ftmstatus(FM_CMD_DISALLOWED);
    return response;
  }
  else if (fmPowerState == FM_TX_ON)
    return response;

  fmrequestparams.cfg_param.is_fm_tx_on = 1;
  cmdStatus = EnableFM(&fmrequestparams.cfg_param);

  printf("\nValue of 'cmdStatus' is %d",cmdStatus);

  response->result = convert_cmdstatus_to_ftmstatus(cmdStatus);
  return response;
}
/*===========================================================================
FUNCTION  ftm_fm_tx_ps_info

DESCRIPTION
	This routine is used to send the RDS PS Info over the transmitting station

DEPENDENCIES
  NIL

RETURN VALUE
  A Packed structre pointer including the response to the Enable Transmitter
  FTM FM packet

SIDE EFFECTS
  None

===========================================================================*/
PACKED void *  ftm_fm_tx_ps_info
(
  void
)
{
  fm_cmd_status_type cmdStatus = FM_CMD_UNRECOGNIZED_CMD;
  generic_response * response = (generic_response *)
                                              diagpkt_subsys_alloc( DIAG_SUBSYS_FTM
                                                                    , FTM_FM_CMD_CODE
                                                                    , sizeof(generic_response)
                                                                  );

  if(response ==  NULL)
  {
    printf("%s Failed to allocate resource",__func__);
    return (void *)convert_cmdstatus_to_ftmstatus(FM_CMD_NO_RESOURCES);
  }

  if(fmPowerState != FM_TX_ON)
  {
    response->result = convert_cmdstatus_to_ftmstatus(FM_CMD_DISALLOWED);
    return response;
  }

  printf("\n Inside ftm_fm_tx_ps_info() function...\n");

  cmdStatus = TransmitPS(&fmrequestparams.tuFmPSParams);

  printf("\nValue of 'cmdStatus' is %d",cmdStatus);

  response->result = convert_cmdstatus_to_ftmstatus(cmdStatus);
  return response;
}

/*===========================================================================
FUNCTION  ftm_fm_tx_stop_ps_info

DESCRIPTION
  This routine is used to stop transmitting the RDS PS info

DEPENDENCIES
  NIL

RETURN VALUE
  A Packed structre pointer including the response to the stopTransmitPS
  FTM FM packet

SIDE EFFECTS
  None

===========================================================================*/
PACKED void *  ftm_fm_tx_stop_ps_info
(
  void
)
{
  fm_cmd_status_type cmdStatus = FM_CMD_UNRECOGNIZED_CMD;
  generic_response * response = (generic_response *)
                                              diagpkt_subsys_alloc( DIAG_SUBSYS_FTM
                                                                    , FTM_FM_CMD_CODE
                                                                    , sizeof(generic_response)
                                                                  );
  if(response ==  NULL)
  {
    printf("%s Failed to allocate resource",__func__);
    return (void *)convert_cmdstatus_to_ftmstatus(FM_CMD_NO_RESOURCES);
  }

  if(fmPowerState != FM_TX_ON)
  {
    response->result = convert_cmdstatus_to_ftmstatus(FM_CMD_DISALLOWED);
    return response;
  }

  printf("\n Inside ftm_fm_tx_stop_ps_info() function...\n");

  cmdStatus = stopTransmitPS();

  printf("\nValue of 'cmdStatus' is %d",cmdStatus);

  response->result = convert_cmdstatus_to_ftmstatus(cmdStatus);
  return response;
}
/*=========================================================================== */


/*===========================================================================
FUNCTION  ftm_fm_tx_rt_info

DESCRIPTION
        This routine is used to send the RDS RT Info over the transmitting station

DEPENDENCIES
  NIL

RETURN VALUE
  A Packed structre pointer including the response to the RT Start
  FTM FM packet

SIDE EFFECTS
  None

===========================================================================*/
PACKED void *  ftm_fm_tx_rt_info
(
  void
)
{
  fm_cmd_status_type cmdStatus = FM_CMD_UNRECOGNIZED_CMD;
  generic_response * response = (generic_response *)
                                              diagpkt_subsys_alloc( DIAG_SUBSYS_FTM
                                                                    , FTM_FM_CMD_CODE
                                                                    , sizeof(generic_response)
                                                                  );
  if(response ==  NULL)
  {
    printf("%s Failed to allocate resource",__func__);
    return (void *)convert_cmdstatus_to_ftmstatus(FM_CMD_NO_RESOURCES);
  }

  if(fmPowerState != FM_TX_ON)
  {
    response->result = convert_cmdstatus_to_ftmstatus(FM_CMD_DISALLOWED);
    return response;
  }

  printf("\n Inside ftm_fm_tx_rt_info() function...\n");

  cmdStatus = TransmitRT(&fmrequestparams.tuFmRTParams);

  printf("\nValue of 'cmdStatus' is %d",cmdStatus);

  response->result = convert_cmdstatus_to_ftmstatus(cmdStatus);
  return response;
}

/*=========================================================================== */
/*===========================================================================
FUNCTION  ftm_fm_tx_stop_rt_info

DESCRIPTION
        This routine is used to stop transmitting the RDS RT info

DEPENDENCIES
  NIL

RETURN VALUE
  A Packed structre pointer including the response to the stopTransmitRT
  FTM FM packet

SIDE EFFECTS
  None

===========================================================================*/
PACKED void *  ftm_fm_tx_stop_rt_info
(
  void
)
{
  fm_cmd_status_type cmdStatus = FM_CMD_UNRECOGNIZED_CMD;
  generic_response * response = (generic_response *)
                                              diagpkt_subsys_alloc( DIAG_SUBSYS_FTM
                                                                    , FTM_FM_CMD_CODE
                                                                    , sizeof(generic_response)
                                                                  );
  if(response ==  NULL)
  {
    printf("%s Failed to allocate resource",__func__);
    return (void *)convert_cmdstatus_to_ftmstatus(FM_CMD_NO_RESOURCES);
  }

  if(fmPowerState != FM_TX_ON)
  {
    response->result = convert_cmdstatus_to_ftmstatus(FM_CMD_DISALLOWED);
    return response;
  }

  printf("\n Inside ftm_fm_tx_stop_rt_info() function...\n");

  cmdStatus = stopTransmitRT();

  printf("\nValue of 'cmdStatus' is %d",cmdStatus);

  response->result = convert_cmdstatus_to_ftmstatus(cmdStatus);
  return response;
}
/*=========================================================================== */

/*===========================================================================
FUNCTION  ftm_fm_tx_get_ps_features

DESCRIPTION
        This routine is used to get all the supported TX PS features

DEPENDENCIES
  NIL

RETURN VALUE
  A Packed structre pointer including the response to the getTxPSFeatures()
  FTM FM packet

SIDE EFFECTS
  None

===========================================================================*/
PACKED void *  ftm_fm_tx_get_ps_features
(
  void
)
{
  fm_cmd_status_type cmdStatus = FM_CMD_UNRECOGNIZED_CMD;
  generic_response * response = (generic_response *)
                                              diagpkt_subsys_alloc( DIAG_SUBSYS_FTM
                                                                    , FTM_FM_CMD_CODE
                                                                    , sizeof(generic_response)
                                                                  );
  if(response ==  NULL)
  {
    printf("%s Failed to allocate resource",__func__);
    return (void *)convert_cmdstatus_to_ftmstatus(FM_CMD_NO_RESOURCES);
  }

  if(fmPowerState != FM_TX_ON)
  {
    response->result = convert_cmdstatus_to_ftmstatus(FM_CMD_DISALLOWED);
    return response;
  }

#ifdef FTM_DEBUG
  printf("\n Inside ftm_fm_tx_get_ps_features function...\n");
#endif

  cmdStatus = getTxPSFeatures();

#ifdef FTM_DEBUG
  printf("\nValue of 'cmdStatus' is %d",cmdStatus);
#endif

  response->result = convert_cmdstatus_to_ftmstatus(cmdStatus);
  return response;
}


/*===========================================================================

FUNCTION      ftm_fm_disable_audio

DESCRIPTION
  This function is used to disable FTM FM audio.

DEPENDENCIES
  none

===========================================================================*/
PACKED void* ftm_fm_disable_audio
(
void
)
{
  int ret;
  generic_response * response = (generic_response *)
                                                 diagpkt_subsys_alloc( DIAG_SUBSYS_FTM
                                                   , FTM_FM_CMD_CODE
                                                   , sizeof( generic_response )
                                                 );

  if(response ==  NULL)
  {
    printf("%s Failed to allocate resource",__func__);
    return (void *)convert_cmdstatus_to_ftmstatus(FM_CMD_NO_RESOURCES);
  }

  printf("enter ftm_fm_disable_audio\n");

  if (response != NULL) {
      response->result = FTM_FM_SUCCESS;
      ret = property_set("ftm.fm_stop", "true");
      if (ret != PROP_SET_SUCC) {
          response->result = FTM_FAIL;
          return response;
      }
  } else
      printf("ftm_fm_disable_audio unable to allocate memory for response packet \n");
  if (ftm_audio_fd > 0)
      close(ftm_audio_fd);
  printf("Disable audio\n");
  return response;
}

/*===========================================================================
FUNCTION  ftm_fm_rx_disable_receiver

DESCRIPTION
  HAL routine to process the response from the PFAL layer call
  DisableFM, also constructs the response packet

DEPENDENCIES
  NIL

RETURN VALUE
  A Packed structre pointer including the response to the DisableFM
  FTM FM packet

SIDE EFFECTS
  None

===========================================================================*/
PACKED void *  ftm_fm_rx_disable_receiver
(
  void
)
{
  fm_cmd_status_type cmdStatus = FM_CMD_UNRECOGNIZED_CMD;
  generic_response * response = (generic_response *)
                                              diagpkt_subsys_alloc( DIAG_SUBSYS_FTM
                                                                    , FTM_FM_CMD_CODE
                                                                    , sizeof(generic_response)
                                                                  );
  if(response ==  NULL)
  {
    printf("%s Failed to allocate resource",__func__);
    return (void *)convert_cmdstatus_to_ftmstatus(FM_CMD_NO_RESOURCES);
  }

  response->result =  FTM_FM_SUCCESS;

  if(fmPowerState == FM_POWER_OFF)
    return response;

  else if(fmPowerState == FM_TX_ON || fmPowerState == FM_POWER_TRANSITION)
  {
    response->result = convert_cmdstatus_to_ftmstatus(FM_CMD_DISALLOWED);
    return response;
  }

  cmdStatus = DisableFM(&fmrequestparams.cfg_param);
  response->result = convert_cmdstatus_to_ftmstatus(cmdStatus);
  fmPowerState = FM_POWER_OFF;
  ftm_fm_disable_audio();
  return response;
}
/*===========================================================================
FUNCTION  ftm_fm_tx_disable_transmitter

DESCRIPTION
  HAL routine to process the response from the PFAL layer call
  DisableFM, also constructs the response packet

DEPENDENCIES
  NIL

RETURN VALUE
  A Packed structre pointer including the response to the DisableFM
  FTM FM packet

SIDE EFFECTS
  None

===========================================================================*/
PACKED void *  ftm_fm_tx_disable_transmitter
(
  void
)
{
  fm_cmd_status_type cmdStatus = FM_CMD_UNRECOGNIZED_CMD;
  generic_response * response = (generic_response *)
                                              diagpkt_subsys_alloc( DIAG_SUBSYS_FTM
                                                                    , FTM_FM_CMD_CODE
                                                                    , sizeof(generic_response)
                                                                  );
  if(response ==  NULL)
  {
    printf("%s Failed to allocate resource",__func__);
    return (void *)convert_cmdstatus_to_ftmstatus(FM_CMD_NO_RESOURCES);
  }

  response->result =  FTM_FM_SUCCESS;

  if(fmPowerState == FM_POWER_OFF)
    return response;
  else if (fmPowerState == FM_RX_ON || fmPowerState == FM_POWER_TRANSITION)
  {
    response->result = convert_cmdstatus_to_ftmstatus(FM_CMD_DISALLOWED);
    return response;
  }

  cmdStatus = DisableFM(&fmrequestparams.cfg_param);
  response->result = convert_cmdstatus_to_ftmstatus(cmdStatus);
  fmPowerState = FM_POWER_OFF;
  return response;
}

/*===========================================================================
FUNCTION   ftm_fm_rx_configure_receiver

DESCRIPTION
  HAL routine to process the response from the PFAL layer call
  ConfigureFM, also constructs the response packet

DEPENDENCIES
  NIL

RETURN VALUE
  A Packed structre pointer including the response to the ConfigureFM
  FTM FM packet

SIDE EFFECTS
  None

===========================================================================*/
PACKED void *  ftm_fm_rx_configure_receiver
(
  void
)
{
  fm_cmd_status_type cmdStatus = FM_CMD_UNRECOGNIZED_CMD;
  generic_response * response = (generic_response *)
                                              diagpkt_subsys_alloc( DIAG_SUBSYS_FTM
                                                                    , FTM_FM_CMD_CODE
                                                                    , sizeof(generic_response)
                                                                  );
  if(response ==  NULL)
  {
    printf("%s Failed to allocate resource",__func__);
    return (void *)convert_cmdstatus_to_ftmstatus(FM_CMD_NO_RESOURCES);
  }

  response->result =  FTM_FM_SUCCESS;

  if(fmPowerState != FM_RX_ON)
  {
    response->result = convert_cmdstatus_to_ftmstatus(FM_CMD_NO_RESOURCES);
    return response;
  }

  fmrequestparams.cfg_param.is_fm_tx_on = 0;
  cmdStatus = ConfigureFM(&fmrequestparams.cfg_param);
  response->result = convert_cmdstatus_to_ftmstatus(cmdStatus);
  return response;
}

/*===========================================================================
FUNCTION   ftm_fm_tx_configure_transmitter

DESCRIPTION
  HAL routine to process the response from the PFAL layer call
  ConfigureFM, also constructs the response packet

DEPENDENCIES
  NIL

RETURN VALUE
  A Packed structre pointer including the response to the ConfigureFM
  FTM FM packet

SIDE EFFECTS
  None

===========================================================================*/
PACKED void *  ftm_fm_tx_configure_transmitter
(
  void
)
{
  fm_cmd_status_type cmdStatus = FM_CMD_UNRECOGNIZED_CMD;
  generic_response * response = (generic_response *)
                                              diagpkt_subsys_alloc( DIAG_SUBSYS_FTM
                                                                    , FTM_FM_CMD_CODE
                                                                    , sizeof(generic_response)
                                                                  );
  if(response ==  NULL)
  {
    printf("%s Failed to allocate resource",__func__);
    return (void *)convert_cmdstatus_to_ftmstatus(FM_CMD_NO_RESOURCES);
  }

  response->result =  FTM_FM_SUCCESS;
  if(fmPowerState != FM_TX_ON)
  {
    response->result = convert_cmdstatus_to_ftmstatus(FM_CMD_NO_RESOURCES);
    return response;
  }
  fmrequestparams.cfg_param.is_fm_tx_on = 1;
  cmdStatus = ConfigureFM(&fmrequestparams.cfg_param);
  response->result = convert_cmdstatus_to_ftmstatus(cmdStatus);
  return response;
}

/*===========================================================================
FUNCTION   ftm_fm_rx_setfrequency_receiver

DESCRIPTION
  HAL routine to process the response from the PFAL layer call
  SetFrequencyReceiver, also constructs the response packet

DEPENDENCIES
  NIL

RETURN VALUE
  A Packed structre pointer including the response to the Set Frequency
  FTM FM packet

SIDE EFFECTS
  None

===========================================================================*/
PACKED void *  ftm_fm_rx_setfrequency_receiver
(
  void
)
{
  fm_cmd_status_type cmdStatus = FM_CMD_UNRECOGNIZED_CMD;
  generic_response * response = (generic_response *)
                                              diagpkt_subsys_alloc( DIAG_SUBSYS_FTM
                                                                    , FTM_FM_CMD_CODE
                                                                    , sizeof(generic_response)
                                                                  );
  if(response ==  NULL)
  {
    printf("%s Failed to allocate resource",__func__);
    return (void *)convert_cmdstatus_to_ftmstatus(FM_CMD_NO_RESOURCES);
  }

  if(fmPowerState != FM_RX_ON)
  {
    response->result = convert_cmdstatus_to_ftmstatus(FM_CMD_NO_RESOURCES);
    return response;
  }

  response->result =  FTM_FM_SUCCESS;
  cmdStatus = SetFrequencyReceiver(fmrequestparams.freq);
  response->result = convert_cmdstatus_to_ftmstatus(cmdStatus);
  return response;
}

/*===========================================================================
FUNCTION   ftm_fm_tx_setfrequency_transmitter

DESCRIPTION
  HAL routine to process the response from the PFAL layer call
  SetFrequencyTransmitter, also constructs the response packet

DEPENDENCIES
  NIL

RETURN VALUE
  A Packed structre pointer including the response to the Set Frequency
  FTM FM packet

SIDE EFFECTS
  None

===========================================================================*/
PACKED void *  ftm_fm_tx_setfrequency_transmitter
(
  void
)
{
  fm_cmd_status_type cmdStatus = FM_CMD_UNRECOGNIZED_CMD;
  generic_response * response = (generic_response *)
                                              diagpkt_subsys_alloc( DIAG_SUBSYS_FTM
                                                                    , FTM_FM_CMD_CODE
                                                                    , sizeof(generic_response)
                                                                  );
  if(response ==  NULL)
  {
    printf("%s Failed to allocate resource",__func__);
    return (void *)convert_cmdstatus_to_ftmstatus(FM_CMD_NO_RESOURCES);
  }

  if(fmPowerState != FM_TX_ON)
  {
    response->result = convert_cmdstatus_to_ftmstatus(FM_CMD_NO_RESOURCES);
    return response;
  }

  response->result =  FTM_FM_SUCCESS;
  cmdStatus = SetFrequencyTransmitter(fmrequestparams.freq);
  response->result = convert_cmdstatus_to_ftmstatus(cmdStatus);
  return response;
}

/*===========================================================================
FUNCTION   ftm_fm_rx_setmutemode_receiver

DESCRIPTION
  HAL routine to process the response from the PFAL layer call
  SetMuteModeReceiver, also constructs the response packet

DEPENDENCIES
  NIL

RETURN VALUE
  A Packed structre pointer including the response to the Set Mute Mode
  FTM FM packet

SIDE EFFECTS
  None

===========================================================================*/
PACKED void *  ftm_fm_rx_setmutemode_receiver
(
  void
)
{
  fm_cmd_status_type cmdStatus = FM_CMD_UNRECOGNIZED_CMD;
  mutemode_response * response = (mutemode_response *)
                                              diagpkt_subsys_alloc( DIAG_SUBSYS_FTM
                                                                    , FTM_FM_CMD_CODE
                                                                    , sizeof(mutemode_response)
                                                                  );
  if(response ==  NULL)
  {
    printf("%s Failed to allocate resource",__func__);
    return (void *)convert_cmdstatus_to_ftmstatus(FM_CMD_NO_RESOURCES);
  }

  if(fmPowerState != FM_RX_ON)
  {
    response->result = convert_cmdstatus_to_ftmstatus(FM_CMD_NO_RESOURCES);
    return response;
  }

  response->result =  FTM_FM_SUCCESS;
  cmdStatus = SetMuteModeReceiver(fmrequestparams.mute_param);
  response->result = convert_cmdstatus_to_ftmstatus(cmdStatus);
  response->mutemode = fmrequestparams.mute_param;
  return response;
}


/*===========================================================================
FUNCTION   ftm_fm_set_soft_mute_receiver

DESCRIPTION
  HAL routine to process the response from the PFAL layer call
  SetMuteModeReceiver, also constructs the response packet

DEPENDENCIES
  NIL

RETURN VALUE
  A Packed structre pointer including the response to the Set Mute Mode
  FTM FM packet

SIDE EFFECTS
  None

===========================================================================*/
PACKED void *  ftm_fm_set_soft_mute_receiver
(
  void
)
{
  fm_cmd_status_type cmdStatus = FM_CMD_UNRECOGNIZED_CMD;
  generic_response * response = (generic_response *)
                                              diagpkt_subsys_alloc( DIAG_SUBSYS_FTM
                                                                    , FTM_FM_CMD_CODE
                                                                    , sizeof(generic_response)
                                                                  );
  if(response ==  NULL)
  {
    printf("%s Failed to allocate resource",__func__);
    return (void *)convert_cmdstatus_to_ftmstatus(FM_CMD_NO_RESOURCES);
  }

  if(fmPowerState != FM_RX_ON)
  {
    response->result = convert_cmdstatus_to_ftmstatus(FM_CMD_NO_RESOURCES);
    return response;
  }

  response->result =  FTM_FM_SUCCESS;
  cmdStatus = SetSoftMuteModeReceiver(fmrequestparams.soft_mute_param);
  response->result = convert_cmdstatus_to_ftmstatus(cmdStatus);
  return response;
}

/*===========================================================================
FUNCTION   ftm_fm_set_antenna

DESCRIPTION
  HAL routine to process the response from the PFAL layer call
  setAntenna, also constructs the response packet

DEPENDENCIES
  NIL

RETURN VALUE
  A Packed structre pointer including the response to the Set Antenna
  FTM FM packet

SIDE EFFECTS
  None

===========================================================================*/
PACKED void * ftm_fm_set_antenna
(
  void
)
{
  fm_cmd_status_type cmdStatus = FM_CMD_UNRECOGNIZED_CMD;
  generic_response * response = (generic_response *)
                                                 diagpkt_subsys_alloc( DIAG_SUBSYS_FTM
                                               , FTM_FM_CMD_CODE
                                               , sizeof(generic_response)
                                               );
  if(response ==  NULL)
  {
    printf("%s Failed to allocate resource",__func__);
    return (void *)convert_cmdstatus_to_ftmstatus(FM_CMD_NO_RESOURCES);
  }

  if(fmPowerState != FM_RX_ON)
  {
    response->result = convert_cmdstatus_to_ftmstatus(FM_CMD_NO_RESOURCES);
    return response;
  }
  response->result =  FTM_FM_SUCCESS;
  cmdStatus = SetAntenna(fmrequestparams.antenna_type);
  response->result = convert_cmdstatus_to_ftmstatus(cmdStatus);
  return response;
}

/*===========================================================================
FUNCTION   ftm_fm_peek_riva_word

DESCRIPTION
  HAL routine to process the response from the PFAL layer call
  FmRivaPeekData, also constructs the response packet

DEPENDENCIES
  NIL

RETURN VALUE
  A Packed structre pointer including the response to the peek riva word
  FTM FM packet

SIDE EFFECTS
  None

===========================================================================*/
PACKED void * ftm_fm_peek_riva_word
(
  void
)
{
  fm_cmd_status_type cmdStatus = FM_CMD_UNRECOGNIZED_CMD;
  rivaData_response * response = (rivaData_response *)
                                              diagpkt_subsys_alloc( DIAG_SUBSYS_FTM
                                                                    , FTM_FM_CMD_CODE
                                                                    , sizeof(rivaData_response)
                                                                  );
  if(response ==  NULL)
  {
    printf("%s Failed to allocate resource",__func__);
    return (void *)convert_cmdstatus_to_ftmstatus(FM_CMD_NO_RESOURCES);
  }

  if((fmPowerState != FM_RX_ON) && (fmPowerState != FM_TX_ON))
  {
    response->result = convert_cmdstatus_to_ftmstatus(FM_CMD_NO_RESOURCES);
    return response;
  }

  response->result =  FTM_FM_SUCCESS;

  cmdStatus = FmRivaPeekData(fmrequestparams.riva_peek_params);
  response->start_address = fm_global_params.riva_data_access_params.startaddress;
  response->length   = fm_global_params.riva_data_access_params.payload_length;
  memcpy(&response->data[0], &fm_global_params.riva_data_access_params.data[0],
                                                             response->length);
  response->sub_opcode =  fm_global_params.riva_data_access_params.subOpcode;
  response->result = convert_cmdstatus_to_ftmstatus(cmdStatus);
  return response;

}

/*===========================================================================
FUNCTION   ftm_fm_poke_riva_word

DESCRIPTION
  HAL routine to process the response from the PFAL layer call
  FmRivaPokeData, also constructs the response packet

DEPENDENCIES
  NIL

RETURN VALUE
  A Packed structre pointer including the response to the poke riva word
  FTM FM packet

SIDE EFFECTS
  None

===========================================================================*/
PACKED void * ftm_fm_poke_riva_word
(
  void
)
{
  fm_cmd_status_type cmdStatus = FM_CMD_UNRECOGNIZED_CMD;
  generic_response * response = (generic_response *)
                                              diagpkt_subsys_alloc( DIAG_SUBSYS_FTM
                                                                    , FTM_FM_CMD_CODE
                                                                    , sizeof(generic_response)
                                                                  );
  if(response ==  NULL)
  {
    printf("%s Failed to allocate resource",__func__);
    return (void *)convert_cmdstatus_to_ftmstatus(FM_CMD_NO_RESOURCES);
  }

  if((fmPowerState != FM_RX_ON) && (fmPowerState != FM_TX_ON))
  {
    response->result = convert_cmdstatus_to_ftmstatus(FM_CMD_NO_RESOURCES);
    return response;
  }

   response->result =  FTM_FM_SUCCESS;
  cmdStatus = FmRivaPokeData(fmrequestparams.riva_data_access_params);
  response->result = convert_cmdstatus_to_ftmstatus(cmdStatus);
  return response;

}

/*===========================================================================
FUNCTION   ftm_fm_poke_ssbi_reg

DESCRIPTION
  HAL routine to process the response from the PFAL layer call
  FmSSBIPokeData, also constructs the response packet

DEPENDENCIES
  NIL

RETURN VALUE
  A Packed structre pointer including the response to the poke ssbi reg
  FTM FM packet

SIDE EFFECTS
  None

===========================================================================*/
PACKED void * ftm_fm_poke_ssbi_reg
(
  void
)
{
  fm_cmd_status_type cmdStatus = FM_CMD_UNRECOGNIZED_CMD;
  generic_response * response = (generic_response *)
                                              diagpkt_subsys_alloc( DIAG_SUBSYS_FTM
                                                                    , FTM_FM_CMD_CODE
                                                                    , sizeof(generic_response)
                                                                  );
  if(response ==  NULL)
  {
    printf("%s Failed to allocate resource",__func__);
    return (void *)convert_cmdstatus_to_ftmstatus(FM_CMD_NO_RESOURCES);
  }

  if((fmPowerState != FM_RX_ON) && (fmPowerState != FM_TX_ON))
  {
    response->result = convert_cmdstatus_to_ftmstatus(FM_CMD_NO_RESOURCES);
    return response;
  }

  response->result =  FTM_FM_SUCCESS;
#ifdef FTM_DEBUG
  printf("ssbi address = %x \n", fmrequestparams.ssbi_access_params.startaddress);
  printf("ssbi data = %x \n",fmrequestparams.ssbi_access_params.data);
#endif

  cmdStatus = FmSSBIPokeData(fmrequestparams.ssbi_access_params);

  response->result = convert_cmdstatus_to_ftmstatus(cmdStatus);
  return response;
}

/*===========================================================================
FUNCTION   ftm_fm_peek_ssbi_reg

DESCRIPTION
  HAL routine to process the response from the PFAL layer call
  FmSSBIPeekData, also constructs the response packet

DEPENDENCIES
  NIL

RETURN VALUE
  A Packed structre pointer including the response to the poke ssbi reg
  FTM FM packet

SIDE EFFECTS
  None

===========================================================================*/
PACKED void * ftm_fm_peek_ssbi_reg
(
  void
)
{
  fm_cmd_status_type cmdStatus = FM_CMD_UNRECOGNIZED_CMD;
  ssbiPeek_response * response = (ssbiPeek_response *)
                                              diagpkt_subsys_alloc( DIAG_SUBSYS_FTM
                                                                    , FTM_FM_CMD_CODE
                                                                    , sizeof(ssbiPeek_response)
                                                                  );
  if(response ==  NULL)
  {
    printf("%s Failed to allocate resource",__func__);
    return (void *)convert_cmdstatus_to_ftmstatus(FM_CMD_NO_RESOURCES);
  }

  if((fmPowerState != FM_RX_ON) && (fmPowerState != FM_TX_ON))
  {
    response->result = convert_cmdstatus_to_ftmstatus(FM_CMD_NO_RESOURCES);
    return response;
  }

  response->result =  FTM_FM_SUCCESS;
#ifdef FTM_DEBUG
  printf("SSBI peek address = %x",fmrequestparams.ssbi_access_params.startaddress);
#endif

  cmdStatus = FmSSBIPeekData(fmrequestparams.ssbi_access_params);

  response->result = convert_cmdstatus_to_ftmstatus(cmdStatus);
  response->data = fm_global_params.ssbi_peek_data;
   return response;

}

/*===========================================================================
FUNCTION   ftm_fm_set_get_reset_agc

DESCRIPTION
  This is a synchronous command for Set/Get Gain State and Reset AGC.

DEPENDENCIES
  NIL

RETURN VALUE
  Current AGC Gain State
  AGC Gain State one Change ago
  AGC Gain State two Changes ago
  AGC Gain State three changes ago

SIDE EFFECTS
  None

===========================================================================*/

PACKED void * ftm_fm_set_get_reset_agc
(
  void
)
{
  fm_cmd_status_type cmdStatus = FM_CMD_UNRECOGNIZED_CMD;
  set_get_reset_agc_response * response = (set_get_reset_agc_response *)
                                            diagpkt_subsys_alloc( DIAG_SUBSYS_FTM
                                                                  , FTM_FM_CMD_CODE
                                               , sizeof(set_get_reset_agc_response)
                                                                );

  if(response ==  NULL)
  {
    printf("%s Failed to allocate resource",__func__);
    return (void *)convert_cmdstatus_to_ftmstatus(FM_CMD_NO_RESOURCES);
  }

  if(fmPowerState != FM_RX_ON)   //&& (fmPowerState != FM_TX_ON))
  {
    response->result = convert_cmdstatus_to_ftmstatus(FM_CMD_NO_RESOURCES);
    return response;
  }

#ifdef FM_SOC_TYPE_CHEROKEE
  response->result =  FTM_FM_SUCCESS;

  printf("AGC req. ucctrl params = %x\n",fmrequestparams.set_get_agc_req_parameters.ucCtrl);
  printf("AGC req. params state = %x\n",fmrequestparams.set_get_agc_req_parameters.ucGainState);

  cmdStatus = FmSetGetResetAGC(fmrequestparams.set_get_agc_req_parameters);

  sleep(1);     //In order to get the current value which can be delayed.

  response->result = convert_cmdstatus_to_ftmstatus(cmdStatus);
  response->uccurrentgainstate = fm_global_params.set_get_reset_agc_params.ucCurrentGainState;
  response->ucgainstatechange1 = fm_global_params.set_get_reset_agc_params.ucGainStateChange1;
  response->ucgainstatechange2 = fm_global_params.set_get_reset_agc_params.ucGainStateChange2;
  response->ucgainstatechange3 = fm_global_params.set_get_reset_agc_params.ucGainStateChange3;

  printf("response->result = %d\n", response->result);

  if (fmrequestparams.set_get_agc_req_parameters.ucCtrl == 0x00) {
    printf("AGC states set successfully\n");
  } else if (fmrequestparams.set_get_agc_req_parameters.ucCtrl == 0x02) {
    printf("AGC states reset successfully\n");
  } else {
    printf("AGC response: \nCurrentGainState: %x\nGainStateChangeBack1: %x\nGainStateChangeBack2:\
    %x\nGainStateChangeBack3: %x\n", response->uccurrentgainstate, response->ucgainstatechange1,
    response->ucgainstatechange2, response->ucgainstatechange3);
  }

  return response;

#else
  printf("response->result = %d\n", response->result);
  response->result = convert_cmdstatus_to_ftmstatus(FM_CMD_NO_RESOURCES);
  return response;

#endif

}

/*===========================================================================
FUNCTION   ftm_fm_read_rds_grp_cntrs

DESCRIPTION
  HAL routine to process the response from the PFAL layer call
  FmReadRdsGrpCntrs, also constructs the response packet

DEPENDENCIES
  NIL

RETURN VALUE
  A Packed structre pointer including the response to the Read RDS Grp
  counters FTM FM packet

SIDE EFFECTS
  None

===========================================================================*/


PACKED void * ftm_fm_read_rds_grp_cntrs
(
)
{
  fm_cmd_status_type cmdStatus = FM_CMD_UNRECOGNIZED_CMD;
  ReadRDSCntrs_responce * response = (ReadRDSCntrs_responce *)
                                              diagpkt_subsys_alloc( DIAG_SUBSYS_FTM
                                                                    , FTM_FM_CMD_CODE
                                                                    , sizeof(ReadRDSCntrs_responce)
                                                                  );
  if(response ==  NULL)
  {
    printf("%s Failed to allocate resource",__func__);
    return (void *)convert_cmdstatus_to_ftmstatus(FM_CMD_NO_RESOURCES);
  }

  if(fmPowerState != FM_RX_ON)
  {
    response->result = convert_cmdstatus_to_ftmstatus(FM_CMD_NO_RESOURCES);
    return response;
  }
  response->result =  FTM_FM_SUCCESS;
  cmdStatus = FmRDSGrpcntrs(fmrequestparams.rds_grp_counters);
  response->result = convert_cmdstatus_to_ftmstatus(cmdStatus);
  memcpy(&response->read_rds_cntrs.totalRdsSBlockErrors ,
         &fm_global_params.rds_group_counters,sizeof(fm_rds_grp_cntrsparams));
  return response;
}
/*===========================================================================
FUNCTION   ftm_fm_read_rds_grp_cntrs_ext

DESCRIPTION
  HAL routine to process the response from the PFAL layer call
  FmReadRdsGrpCntrsExt, also constructs the response packet

DEPENDENCIES
  NIL

RETURN VALUE
  A Packed structre pointer including the response to the Read RDS Grp
  counters FTM FM packet

SIDE EFFECTS
  None

===========================================================================*/


PACKED void * ftm_fm_read_rds_grp_cntrs_ext
(
)
{
  fm_cmd_status_type cmdStatus = FM_CMD_UNRECOGNIZED_CMD;
  ReadRDSCntrs_ext_response * response = (ReadRDSCntrs_ext_response *)
                                              diagpkt_subsys_alloc( DIAG_SUBSYS_FTM
                                                                    , FTM_FM_CMD_CODE
                                                                    , sizeof(ReadRDSCntrs_ext_response)
                                                                  );
  if(response ==  NULL)
  {
    printf("%s Failed to allocate resource",__func__);
    return (void *)convert_cmdstatus_to_ftmstatus(FM_CMD_NO_RESOURCES);
  }

  if(fmPowerState != FM_RX_ON)
  {
    response->result = convert_cmdstatus_to_ftmstatus(FM_CMD_NO_RESOURCES);
    return response;
  }

#ifdef FM_SOC_TYPE_CHEROKEE
  response->result =  FTM_FM_SUCCESS;
  cmdStatus = FmRDSGrpcntrsExt(fmrequestparams.rds_grp_counters_ext);
  response->result = convert_cmdstatus_to_ftmstatus(cmdStatus);
  memcpy(&response->read_rds_cntrs_ext.totalRdsSyncLoss ,
         &fm_global_params.rds_group_counters_extended,sizeof(fm_rds_grpcntrs_extendedparams));
  return response;
#else
  printf("response->result = %d\n", response->result);
  response->result = convert_cmdstatus_to_ftmstatus(FM_CMD_NO_RESOURCES);
  return response;
#endif
}

/*===========================================================================
FUNCTION   ftm_fm_default_read

DESCRIPTION
  HAL routine to process the response from the PFAL layer call
  FmDefaultRead, also constructs the response packet

DEPENDENCIES
  NIL

RETURN VALUE
  A Packed structre pointer including the response to the Read Default FTM FM packet

SIDE EFFECTS
  None

===========================================================================*/

PACKED void * ftm_fm_default_read
(
)
{
  fm_cmd_status_type cmdStatus = FM_CMD_UNRECOGNIZED_CMD;
  default_read_rsp * response = (default_read_rsp *)
                                             diagpkt_subsys_alloc( DIAG_SUBSYS_FTM
                                                           , FTM_FM_CMD_CODE
                                                           , sizeof(default_read_rsp)
                                                           );
  if(response ==  NULL)
  {
    printf("%s Failed to allocate resource",__func__);
    return (void *)convert_cmdstatus_to_ftmstatus(FM_CMD_NO_RESOURCES);
  }

  if ((fmPowerState != FM_RX_ON) && (fmPowerState != FM_TX_ON))
  {
    response->status= convert_cmdstatus_to_ftmstatus(FM_CMD_NO_RESOURCES);
    return response;
  }
#ifdef FTM_DEBUG
  printf("\n Inside ftm_fm_read_defaults() function...\n");
#endif

  cmdStatus = FmDefaultRead(fmrequestparams.rd_default);
  response->status = convert_cmdstatus_to_ftmstatus(cmdStatus);
#ifdef FTM_DEBUG
  printf("\nValue of 'response->status'is %d",response->status);
#endif
  if (response->status ==  FTM_FM_SUCCESS) {
      response->data_length = fm_global_params.default_read_data.data_length;
      memcpy(&response->data[0], &fm_global_params.default_read_data.data[0],
                                         response->data_length);
  }
  return response;
}
/*===========================================================================
FUNCTION   ftm_fm_default_write

DESCRIPTION
  HAL routine to process the response from the PFAL layer call
  DefaultWrite, also constructs the response packet

DEPENDENCIES
  NIL

RETURN VALUE
   A Packed structre pointer including the response to the write Default FTM FM packet

SIDE EFFECTS
  None

===========================================================================*/

PACKED void *  ftm_fm_default_write
(
  void
)
{
  fm_cmd_status_type cmdStatus = FM_CMD_UNRECOGNIZED_CMD;
  generic_response * response = (generic_response *)
                                              diagpkt_subsys_alloc( DIAG_SUBSYS_FTM
                                                                    , FTM_FM_CMD_CODE
                                                                    , sizeof(generic_response)
                                                                  );
  if(response ==  NULL)
  {
    printf("%s Failed to allocate resource",__func__);
    return (void *)convert_cmdstatus_to_ftmstatus(FM_CMD_NO_RESOURCES);
  }

  if ((fmPowerState != FM_RX_ON) && (fmPowerState != FM_TX_ON))
  {
    response->result = convert_cmdstatus_to_ftmstatus(FM_CMD_DISALLOWED);
    return response;
  }
#ifdef FTM_DEBUG
  printf("\n Inside ftm_fm_rx_write_defaults() function...\n");
#endif
  cmdStatus = FmDefaultWrite(&fmrequestparams.wr_default);
#ifdef FTM_DEBUG
  printf("\nValue of 'cmdStatus' is %d",cmdStatus);
#endif
  response->result = convert_cmdstatus_to_ftmstatus(cmdStatus);
#ifdef FTM_DEBUG
  printf("\nValue of 'response->result' is %d",response->result);
#endif
  return response;
}

/*===========================================================================
FUNCTION   ftm_fm_tx_pwr_lvl_cfg

DESCRIPTION


DEPENDENCIES
  NIL

RETURN VALUE
  A Packed structre pointer including the response to the tx power level
  configuration command

SIDE EFFECTS
  None

===========================================================================*/
PACKED void * ftm_fm_tx_pwr_lvl_cfg
(
  void
)
{
  fm_cmd_status_type cmdStatus = FM_CMD_UNRECOGNIZED_CMD;
  generic_response * response = (generic_response *)
                                              diagpkt_subsys_alloc(DIAG_SUBSYS_FTM
                                                                    , FTM_FM_CMD_CODE
                                                                    , sizeof(generic_response)
                                                                  );
  if(response ==  NULL)
  {
    printf("%s Failed to allocate resource",__func__);
    return (void *)convert_cmdstatus_to_ftmstatus(FM_CMD_NO_RESOURCES);
  }

  if (fmPowerState != FM_TX_ON) {
      response->result = convert_cmdstatus_to_ftmstatus(FM_CMD_NO_RESOURCES);
      return response;
  }
  response->result =  FTM_FM_SUCCESS;
#ifdef FTM_DEBUG
  printf("FM Tx power configuration to = %x", fmrequestparams.tx_pwr_cfg);
#endif

  cmdStatus = FmTxPwrLvlCfg(fmrequestparams.tx_pwr_cfg);
  response->result = convert_cmdstatus_to_ftmstatus(cmdStatus);
  return response;
}

/*===========================================================================
FUNCTION   ftm_fm_tx_tone_generation

DESCRIPTION


DEPENDENCIES
  NIL

RETURN VALUE
  A Packed structre pointer including the response to the tx tonoe generator
  command

SIDE EFFECTS
  None

===========================================================================*/
PACKED void * ftm_fm_tx_tone_generation
(
  void
)
{
  fm_cmd_status_type cmdStatus = FM_CMD_UNRECOGNIZED_CMD;
  generic_response * response = (generic_response *)
                                              diagpkt_subsys_alloc( DIAG_SUBSYS_FTM
                                                                    , FTM_FM_CMD_CODE
                                                                    , sizeof(generic_response)
                                                                  );
  if(response ==  NULL)
  {
    printf("%s Failed to allocate resource",__func__);
    return (void *)convert_cmdstatus_to_ftmstatus(FM_CMD_NO_RESOURCES);
  }

  if(fmPowerState != FM_TX_ON)
  {
    response->result = convert_cmdstatus_to_ftmstatus(FM_CMD_NO_RESOURCES);
    return response;
  }
  response->result =  FTM_FM_SUCCESS;

#ifdef FTM_DEBUG
  printf("FM Tx tone type is setting to = %x",fmrequestparams.tx_tone_param);
#endif

  cmdStatus = FmTxToneGen(fmrequestparams.tx_tone_param);
  response->result = convert_cmdstatus_to_ftmstatus(cmdStatus);
  return response;
}

/*===========================================================================
FUNCTION   ftm_fm_set_hlsi

DESCRIPTION


DEPENDENCIES
  NIL

RETURN VALUE
  A Packed structre pointer including the response to the Set HlSi
  command

SIDE EFFECTS
  None

===========================================================================*/

PACKED void *  ftm_fm_set_hlsi
(
  void
)
{
  fm_cmd_status_type cmdStatus = FM_CMD_UNRECOGNIZED_CMD;
  generic_response * response = (generic_response *)
                                              diagpkt_subsys_alloc( DIAG_SUBSYS_FTM
                                                                    , FTM_FM_CMD_CODE
                                                                    , sizeof(generic_response)
                                                                  );

  if(response ==  NULL)
  {
    printf("%s Failed to allocate resource",__func__);
    return (void *)convert_cmdstatus_to_ftmstatus(FM_CMD_NO_RESOURCES);
  }

  if(fmPowerState != FM_RX_ON)
  {
    response->result = convert_cmdstatus_to_ftmstatus(FM_CMD_NO_RESOURCES);
    return response;
  }
  response->result =  FTM_FM_SUCCESS;

#ifdef FTM_DEBUG
  printf("FM Rx HlSi is setting to = %x",fmrequestparams.hlsi);
#endif

  cmdStatus = FmSetHlSi(fmrequestparams.hlsi);
  response->result = convert_cmdstatus_to_ftmstatus(cmdStatus);
  return response;
}
/*===========================================================================
FUNCTION   ftm_fm_set_notch_filter

DESCRIPTION


DEPENDENCIES
  NIL

RETURN VALUE
  A Packed structre pointer including the response to the Set notch filter
  command

SIDE EFFECTS
  None

===========================================================================*/

PACKED void *  ftm_fm_set_notch_filter
(
  void
)
{
  fm_cmd_status_type cmdStatus = FM_CMD_UNRECOGNIZED_CMD;
  generic_response * response = (generic_response *)
                                              diagpkt_subsys_alloc( DIAG_SUBSYS_FTM
                                                                    , FTM_FM_CMD_CODE
                                                                    , sizeof(generic_response)
                                                                  );

  if(response ==  NULL)
  {
    printf("%s Failed to allocate resource",__func__);
    return (void *)convert_cmdstatus_to_ftmstatus(FM_CMD_NO_RESOURCES);
  }

  if(fmPowerState != FM_RX_ON)
  {
    response->result = convert_cmdstatus_to_ftmstatus(FM_CMD_NO_RESOURCES);
    return response;
  }
  response->result =  FTM_FM_SUCCESS;

#ifdef FTM_DEBUG
  printf("FM Rx Notch filter is setting to = %x",fmrequestparams.notch);
#endif

  cmdStatus = FmSetNotchFilter(fmrequestparams.notch);
  response->result = convert_cmdstatus_to_ftmstatus(cmdStatus);
    printf("responce->staus= %d",response->result);
  return response;
}




/*===========================================================================
FUNCTION   ftm_fm_rx_setstereomode_receiver

DESCRIPTION
  HAL routine to process the response from the PFAL layer call
  SetStereoModeReceiver, also constructs the response packet

DEPENDENCIES
  NIL

RETURN VALUE
  A Packed structre pointer including the response to the Set Stereo Mode
  FTM FM packet

SIDE EFFECTS
  None

===========================================================================*/
PACKED void *  ftm_fm_rx_setstereomode_receiver
(
  void
)
{
  fm_cmd_status_type cmdStatus = FM_CMD_UNRECOGNIZED_CMD;
  stereomode_response * response = (stereomode_response *)
                                              diagpkt_subsys_alloc( DIAG_SUBSYS_FTM
                                                                    , FTM_FM_CMD_CODE
                                                                    , sizeof(stereomode_response)
                                                                  );
  if(response ==  NULL)
  {
    printf("%s Failed to allocate resource",__func__);
    return (void *)convert_cmdstatus_to_ftmstatus(FM_CMD_NO_RESOURCES);
  }

  if(fmPowerState != FM_RX_ON)
  {
    response->result = convert_cmdstatus_to_ftmstatus(FM_CMD_NO_RESOURCES);
    return response;
  }

  response->result =  FTM_FM_SUCCESS;
  cmdStatus = SetStereoModeReceiver(fmrequestparams.stereo_param);
  response->result = convert_cmdstatus_to_ftmstatus(cmdStatus);
  response->stereomode = fmrequestparams.stereo_param;
  return response;
}

/*===========================================================================
FUNCTION   ftm_fm_rx_getstationparameters_receiver

DESCRIPTION
  HAL routine to aggregrate the reponse from the PFAL layer call
  GetStationParametersReceiver, also constructs the response packet

DEPENDENCIES
  NIL

RETURN VALUE
  A Packed structre pointer including the response to the Get Station
  parameters FTM FM packet

SIDE EFFECTS
  None

===========================================================================*/
PACKED void * ftm_fm_rx_getstationparameters_receiver
(
  void
)
{
  fm_cmd_status_type cmdStatus = FM_CMD_UNRECOGNIZED_CMD;
  fm_station_params_available   getparamavble;
  fm_rx_get_station_parameters_response * response = (fm_rx_get_station_parameters_response *)
                                                     diagpkt_subsys_alloc( DIAG_SUBSYS_FTM
                                                                           , FTM_FM_CMD_CODE
                                                                           , sizeof(fm_rx_get_station_parameters_response)
                                                                         );
  if(response ==  NULL)
  {
    printf("%s Failed to allocate resource",__func__);
    return (void *)convert_cmdstatus_to_ftmstatus(FM_CMD_NO_RESOURCES);
  }

  if(fmPowerState != FM_RX_ON)
  {
    response->result = convert_cmdstatus_to_ftmstatus(FM_CMD_NO_RESOURCES);
    return response;
  }
  response->result =  FTM_FM_SUCCESS;
  cmdStatus = GetStationParametersReceiver(&getparamavble);
  if(cmdStatus == FM_CMD_SUCCESS)
  {
    response->result = FTM_FM_SUCCESS;
    response->stationFreq = getparamavble.current_station_freq;
    response->servAvble = getparamavble.service_available;
    response->rssi = getparamavble.rssi;
    response->stereoProgram = getparamavble.stype;
    response->rdsSyncStatus =  getparamavble.rds_sync_status;
    response->muteMode = getparamavble.mute_status;
  }
  return response;
}
/*===========================================================================
FUNCTION   ftm_fm_rx_setrdsoptions_receiver

DESCRIPTION
  HAL routine to set the RDS options for the RDS/RDBS subsystem.

DEPENDENCIES
  NIL

RETURN VALUE
  A Packed structre pointer including the response to the Set RDS options
  FTM FM packet

SIDE EFFECTS
  None

===========================================================================*/
PACKED void * ftm_fm_rx_setrdsoptions_receiver
(
  void
)
{
  fm_cmd_status_type cmdStatus = FM_CMD_UNRECOGNIZED_CMD;
  generic_response * response = (generic_response *)
                                              diagpkt_subsys_alloc( DIAG_SUBSYS_FTM
                                                                    , FTM_FM_CMD_CODE
                                                                    , sizeof(generic_response)
                                                                  );
  if(response ==  NULL)
  {
    printf("%s Failed to allocate resource",__func__);
    return (void *)convert_cmdstatus_to_ftmstatus(FM_CMD_NO_RESOURCES);
  }

  if(fmPowerState != FM_RX_ON)
  {
    response->result = convert_cmdstatus_to_ftmstatus(FM_CMD_NO_RESOURCES);
    return response;
  }

  response->result =  FTM_FM_SUCCESS;
  cmdStatus = SetRdsOptionsReceiver(fmrequestparams.rds_options);
  response->result = convert_cmdstatus_to_ftmstatus(cmdStatus);
  return response;
}

/*===========================================================================
FUNCTION   ftm_fm_rx_setrdsgroupproc_receiver

DESCRIPTION
  HAL routine to set the RDS group process options  for the RDS/RDBS
  subsystem.

DEPENDENCIES
  NIL

RETURN VALUE
  A Packed structre pointer including the response to the Set RDS group proc
  options FTM FM packet

SIDE EFFECTS
  None

===========================================================================*/

PACKED void * ftm_fm_rx_setrdsgroupproc_receiver
(
  void
)
{
  fm_cmd_status_type cmdStatus = FM_CMD_UNRECOGNIZED_CMD;
  generic_response * response = (generic_response *)
                                              diagpkt_subsys_alloc( DIAG_SUBSYS_FTM
                                                                    , FTM_FM_CMD_CODE
                                                                    , sizeof(generic_response)
                                                                  );
  if(response ==  NULL)
  {
    printf("%s Failed to allocate resource",__func__);
    return (void *)convert_cmdstatus_to_ftmstatus(FM_CMD_NO_RESOURCES);
  }

  if(fmPowerState != FM_RX_ON)
  {
    response->result = convert_cmdstatus_to_ftmstatus(FM_CMD_NO_RESOURCES);
    return response;
  }

  response->result =  FTM_FM_SUCCESS;
  cmdStatus = SetRdsGroupProcReceiver(fmrequestparams.rds_group_options);
  response->result = convert_cmdstatus_to_ftmstatus(cmdStatus);
  return response;
}

/*===========================================================================
FUNCTION   ftm_fm_rx_setpowermode_receiver

DESCRIPTION
  HAL routine to set the power mode of FM Receiver Operation

DEPENDENCIES
  NIL

RETURN VALUE
  A Packed structre pointer including the generic response to the FTM FM
  packet

SIDE EFFECTS
  None

===========================================================================*/
PACKED void * ftm_fm_rx_setpowermode_receiver
(
  void
)
{
  fm_cmd_status_type cmdStatus = FM_CMD_UNRECOGNIZED_CMD;
  generic_response * response = (generic_response *)
                                              diagpkt_subsys_alloc( DIAG_SUBSYS_FTM
                                                                    , FTM_FM_CMD_CODE
                                                                    , sizeof(generic_response)
                                                                  );
  if(response ==  NULL)
  {
    printf("%s Failed to allocate resource",__func__);
    return (void *)convert_cmdstatus_to_ftmstatus(FM_CMD_NO_RESOURCES);
  }

  if(fmPowerState != FM_RX_ON)
  {
    response->result = convert_cmdstatus_to_ftmstatus(FM_CMD_NO_RESOURCES);
    return response;
  }

  response->result =  FTM_FM_SUCCESS;
  cmdStatus = SetPowerModeReceiver(fmrequestparams.power_mode);
  response->result = convert_cmdstatus_to_ftmstatus(cmdStatus);
  return response;
}

/*===========================================================================
FUNCTION   ftm_fm_rx_setsignalthreshold_receiver

DESCRIPTION
  HAL routine to set the signal threshold of FM Receiver

DEPENDENCIES
  NIL

RETURN VALUE
  A Packed structre pointer including the generic response to the FTM FM
  packet

SIDE EFFECTS
  None

===========================================================================*/
PACKED void * ftm_fm_rx_setsignalthreshold_receiver
(
  void
)
{
  fm_cmd_status_type cmdStatus = FM_CMD_UNRECOGNIZED_CMD;
  generic_response * response = (generic_response *)
                                              diagpkt_subsys_alloc( DIAG_SUBSYS_FTM
                                                                    , FTM_FM_CMD_CODE
                                                                    , sizeof(generic_response)
                                                                  );
  if(response ==  NULL)
  {
    printf("%s Failed to allocate resource",__func__);
    return (void *)convert_cmdstatus_to_ftmstatus(FM_CMD_NO_RESOURCES);
  }

  if(fmPowerState != FM_RX_ON)
  {
    response->result = convert_cmdstatus_to_ftmstatus(FM_CMD_NO_RESOURCES);
    return response;
  }

  response->result =  FTM_FM_SUCCESS;
  cmdStatus = SetSignalThresholdReceiver(fmrequestparams.signal_threshold);
  response->result = convert_cmdstatus_to_ftmstatus(cmdStatus);
  return response;
}

/*===========================================================================
FUNCTION   ftm_fm_rx_getsignalthreshold_receiver

DESCRIPTION
  HAL routine to get the signal threshold of FM Receiver

DEPENDENCIES
  NIL

RETURN VALUE
  A Packed structre pointer including the generic response to the FTM FM
  packet

SIDE EFFECTS
  None

===========================================================================*/
PACKED void * ftm_fm_rx_getsignalthreshold_receiver
(
  void
)
{
  fm_cmd_status_type cmdStatus = FM_CMD_UNRECOGNIZED_CMD;
  threshold_response * response = (threshold_response *)
                                              diagpkt_subsys_alloc( DIAG_SUBSYS_FTM
                                                                    , FTM_FM_CMD_CODE
                                                                    , sizeof(threshold_response)
                                                                  );
  uint8 threshold;
  if(response ==  NULL)
  {
    printf("%s Failed to allocate resource",__func__);
    return (void *)convert_cmdstatus_to_ftmstatus(FM_CMD_NO_RESOURCES);
  }

  if(fmPowerState != FM_RX_ON)
  {
    response->result = convert_cmdstatus_to_ftmstatus(FM_CMD_NO_RESOURCES);
    return response;
  }

  response->result =  FTM_FM_SUCCESS;
  cmdStatus = GetSignalThresholdReceiver(&threshold);
  response->result = convert_cmdstatus_to_ftmstatus(cmdStatus);
  response->threshold = threshold;
  return response;
}


/*===========================================================================
FUNCTION   ftm_fm_rx_getrssilimit_receiver

DESCRIPTION
  HAL routine to print the rssi limit of FM Receiver
  This command is used for debug purposes

DEPENDENCIES
  NIL

RETURN VALUE
  A Packed structre pointer including the generic response to the FTM FM
  packet

SIDE EFFECTS
  None

===========================================================================*/
PACKED void * ftm_fm_rx_getrssilimit_receiver
(
  void
)
{
  fm_cmd_status_type cmdStatus = FM_CMD_UNRECOGNIZED_CMD;
  generic_response * response = (generic_response *)
                                              diagpkt_subsys_alloc( DIAG_SUBSYS_FTM
                                                                    , FTM_FM_CMD_CODE
                                                                    , sizeof(generic_response)
                                                                  );
  if(response ==  NULL)
  {
    printf("%s Failed to allocate resource",__func__);
    return (void *)convert_cmdstatus_to_ftmstatus(FM_CMD_NO_RESOURCES);
  }

  if(fmPowerState != FM_RX_ON)
  {
    response->result = convert_cmdstatus_to_ftmstatus(FM_CMD_NO_RESOURCES);
    return response;
  }

  response->result =  FTM_FM_SUCCESS;
  cmdStatus = GetRSSILimits();
  response->result = convert_cmdstatus_to_ftmstatus(cmdStatus);
  return response;
}

/*===========================================================================
FUNCTION   ftm_fm_rx_getpsinfo_receiver

DESCRIPTION
  HAL routine to print the PS info of tuned channel on the Diag
  This command is used for debug purposes

DEPENDENCIES
  NIL

RETURN VALUE
  A Packed structre pointer including the generic response to the FTM FM
  packet

SIDE EFFECTS
  None

===========================================================================*/
PACKED void * ftm_fm_rx_getpsinfo_receiver
(
  void
)
{
  fm_cmd_status_type cmdStatus = FM_CMD_UNRECOGNIZED_CMD;
  fmrdsps_response * response = (fmrdsps_response *)
                                              diagpkt_subsys_alloc( DIAG_SUBSYS_FTM
                                                                    , FTM_FM_CMD_CODE
                                                                    , sizeof(fmrdsps_response)
                                                                  );
  if(response ==  NULL)
  {
    printf("%s Failed to allocate resource",__func__);
    return (void *)convert_cmdstatus_to_ftmstatus(FM_CMD_NO_RESOURCES);
  }

  if(fmPowerState != FM_RX_ON)
  {
    response->result = convert_cmdstatus_to_ftmstatus(FM_CMD_NO_RESOURCES);
    return response;
  }
  response->result =  FTM_FM_SUCCESS;
  memcpy(response->string, fm_global_params.pgm_services,
         fm_global_params.fm_ps_length);
  response->length = fm_global_params.fm_ps_length;
  return response;
}

/*===========================================================================
FUNCTION   ftm_fm_rx_getrtinfo_receiver

DESCRIPTION
  HAL routine to print the Radio text data of tuned channel on the Diag
  This command is used for debug purposes

DEPENDENCIES
  NIL

RETURN VALUE
  A Packed structre pointer including the generic response to the FTM FM
  packet

SIDE EFFECTS
  None

===========================================================================*/
PACKED void * ftm_fm_rx_getrtinfo_receiver
(
  void
)
{
  fm_cmd_status_type cmdStatus = FM_CMD_UNRECOGNIZED_CMD;
  fmrdsrt_response * response = (fmrdsrt_response *)
                                              diagpkt_subsys_alloc( DIAG_SUBSYS_FTM
                                                                    , FTM_FM_CMD_CODE
                                                                    , sizeof(fmrdsrt_response)
                                                                  );
  if(response ==  NULL)
  {
    printf("%s Failed to allocate resource",__func__);
    return (void *)convert_cmdstatus_to_ftmstatus(FM_CMD_NO_RESOURCES);
  }

  if(fmPowerState != FM_RX_ON)
  {
    response->result = convert_cmdstatus_to_ftmstatus(FM_CMD_NO_RESOURCES);
    return response;
  }

  response->result =  FTM_FM_SUCCESS;
  memcpy(response->string, fm_global_params.radio_text,
                           fm_global_params.fm_rt_length);
  response->length = fm_global_params.fm_rt_length;
  return response;
}


/*===========================================================================
FUNCTION   ftm_fm_rx_getafinfo_receiver

DESCRIPTION
  HAL routine to print the Alternate Frequency list of current tuned channel
  on the Diag. This command is used for debug purposes

DEPENDENCIES
  NIL

RETURN VALUE
  A Packed structre pointer including the generic response to the FTM FM
  packet

SIDE EFFECTS
  None

===========================================================================*/
PACKED void * ftm_fm_rx_getafinfo_receiver
(
  void
)
{
  fm_cmd_status_type cmdStatus = FM_CMD_UNRECOGNIZED_CMD;
  generic_response * response = (generic_response *)
                                              diagpkt_subsys_alloc( DIAG_SUBSYS_FTM
                                                                    , FTM_FM_CMD_CODE
                                                                    , sizeof(generic_response)
                                                                  );
  if(response ==  NULL)
  {
    printf("%s Failed to allocate resource",__func__);
    return (void *)convert_cmdstatus_to_ftmstatus(FM_CMD_NO_RESOURCES);
  }

  if(fmPowerState != FM_RX_ON)
  {
    response->result = convert_cmdstatus_to_ftmstatus(FM_CMD_NO_RESOURCES);
    return response;
  }

  response->result =  FTM_FM_SUCCESS;
  cmdStatus = GetAFInfoReceiver();
  response->result = convert_cmdstatus_to_ftmstatus(cmdStatus);
  return response;
}

/*===========================================================================
FUNCTION   ftm_fm_rx_searchstations_receiver

DESCRIPTION
  HAL routine to search for stations from the currently tuned frequency

DEPENDENCIES
  NIL

RETURN VALUE
  A Packed structre pointer including the generic response to the FTM FM
  packet

SIDE EFFECTS
  The Result of the search will be output on the Diag through debug messages

===========================================================================*/
PACKED void * ftm_fm_rx_searchstations_receiver
(
  void
)
{
  fm_cmd_status_type cmdStatus = FM_CMD_UNRECOGNIZED_CMD;
  generic_response * response = (generic_response *)
                                              diagpkt_subsys_alloc( DIAG_SUBSYS_FTM
                                                                    , FTM_FM_CMD_CODE
                                                                    , sizeof(generic_response)
                                                                  );
  if(response ==  NULL)
  {
    printf("%s Failed to allocate resource",__func__);
    return (void *)convert_cmdstatus_to_ftmstatus(FM_CMD_NO_RESOURCES);
  }

  if(fmPowerState != FM_RX_ON)
  {
    response->result = convert_cmdstatus_to_ftmstatus(FM_CMD_NO_RESOURCES);
    return response;
  }

  response->result =  FTM_FM_SUCCESS;
  cmdStatus = SearchStationsReceiver(fmrequestparams.search_stations_options);
  response->result = convert_cmdstatus_to_ftmstatus(cmdStatus);
  return response;
}

/*===========================================================================
FUNCTION   ftm_fm_rx_searchrdsstations_receiver

DESCRIPTION
  HAL routine to search for stations from the currently tuned frequency with
  a specific program type match

DEPENDENCIES
  NIL

RETURN VALUE
  A Packed structre pointer including the generic response to the FTM FM
  packet

SIDE EFFECTS
  The Result of the search will be output on the Diag through debug messages

===========================================================================*/
PACKED void * ftm_fm_rx_searchrdsstations_receiver
(
  void
)
{
  fm_cmd_status_type cmdStatus = FM_CMD_UNRECOGNIZED_CMD;
  generic_response * response = (generic_response *)
                                              diagpkt_subsys_alloc( DIAG_SUBSYS_FTM
                                                                    , FTM_FM_CMD_CODE
                                                                    , sizeof(generic_response)
                                                                  );
  if(response ==  NULL)
  {
    printf("%s Failed to allocate resource",__func__);
    return (void *)convert_cmdstatus_to_ftmstatus(FM_CMD_NO_RESOURCES);
  }

  if(fmPowerState != FM_RX_ON)
  {
    response->result = convert_cmdstatus_to_ftmstatus(FM_CMD_NO_RESOURCES);
    return response;
  }

  response->result =  FTM_FM_SUCCESS;
  cmdStatus = SearchRdsStationsReceiver(fmrequestparams.search_rds_stations_options);
  response->result = convert_cmdstatus_to_ftmstatus(cmdStatus);
  return response;
}

/*===========================================================================
FUNCTION   ftm_fm_rx_searchstationslist_receiver

DESCRIPTION
  HAL routine to search for list of stations from the currently tuned frequency with
  a specific program type match,signal quality etc

DEPENDENCIES
  NIL

RETURN VALUE
  A Packed structre pointer including the generic response to the FTM FM
  packet

SIDE EFFECTS
  The Result of the search will be output on the Diag through debug messages

===========================================================================*/
PACKED void * ftm_fm_rx_searchstationslist_receiver
(
  void
)
{
  fm_cmd_status_type cmdStatus = FM_CMD_UNRECOGNIZED_CMD;
  generic_response * response = (generic_response *)
                                              diagpkt_subsys_alloc( DIAG_SUBSYS_FTM
                                                                    , FTM_FM_CMD_CODE
                                                                    , sizeof(generic_response)
                                                                  );
  if(response ==  NULL)
  {
    printf("%s Failed to allocate resource",__func__);
    return (void *)convert_cmdstatus_to_ftmstatus(FM_CMD_NO_RESOURCES);
  }

  if(fmPowerState != FM_RX_ON)
  {
    response->result = convert_cmdstatus_to_ftmstatus(FM_CMD_NO_RESOURCES);
    return response;
  }

  response->result =  FTM_FM_SUCCESS;
  cmdStatus = SearchStationListReceiver(fmrequestparams.search_list_stations_options);
  response->result = convert_cmdstatus_to_ftmstatus(cmdStatus);
  return response;
}


/*===========================================================================
FUNCTION   ftm_fm_rx_cancelsearch_receiver

DESCRIPTION
  HAL routine to cancel the previous search for stations

DEPENDENCIES
  NIL

RETURN VALUE
  A Packed structre pointer including the generic response to the FTM FM
  packet

SIDE EFFECTS
  NONE

===========================================================================*/
PACKED void * ftm_fm_rx_cancelsearch_receiver
(
  void
)
{
  fm_cmd_status_type cmdStatus = FM_CMD_UNRECOGNIZED_CMD;
  generic_response * response = (generic_response *)
                                              diagpkt_subsys_alloc( DIAG_SUBSYS_FTM
                                                                    , FTM_FM_CMD_CODE
                                                                    , sizeof(generic_response)
                                                                  );
  if(response ==  NULL)
  {
    printf("%s Failed to allocate resource",__func__);
    return (void *)convert_cmdstatus_to_ftmstatus(FM_CMD_NO_RESOURCES);
  }

  if(fmPowerState != FM_RX_ON)
  {
    response->result = convert_cmdstatus_to_ftmstatus(FM_CMD_NO_RESOURCES);
    return response;
  }

  response->result =  FTM_FM_SUCCESS;
  cmdStatus = CancelSearchReceiver();
  response->result = convert_cmdstatus_to_ftmstatus(cmdStatus);
  return response;
}
/*===========================================================================
FUNCTION   ftm_fm_rx_fmbuswrite_receiver

DESCRIPTION
  HAL routine to perform a I2C bus write operation to FM Slave ID

DEPENDENCIES
  NIL

RETURN VALUE
  A Packed structre pointer including the generic response to the FTM FM
  packet

SIDE EFFECTS
  NONE

===========================================================================*/
PACKED void * ftm_fm_rx_fmbuswrite_receiver
(
  void
)
{
  fm_cmd_status_type cmdStatus = FM_CMD_UNRECOGNIZED_CMD;
  generic_response * response = (generic_response *)
                                              diagpkt_subsys_alloc( DIAG_SUBSYS_FTM
                                                                    , FTM_FM_CMD_CODE
                                                                    , sizeof(generic_response)
                                                                  );

  if(response ==  NULL)
  {
    printf("%s Failed to allocate resource",__func__);
    return (void *)convert_cmdstatus_to_ftmstatus(FM_CMD_NO_RESOURCES);
  }

  response->result =  FTM_FM_SUCCESS;
  cmdStatus = FmBusWriteReceiver(fmrequestparams.i2c_params);
  response->result = convert_cmdstatus_to_ftmstatus(cmdStatus);
  return response;
}


/*===========================================================================
FUNCTION   ftm_fm_rx_fmbusread_receiver

DESCRIPTION
  HAL routine to perform a I2C bus read operation to FM Slave ID

DEPENDENCIES
  NIL

RETURN VALUE
  A Packed structre pointer including the generic response to the FTM FM
  packet

SIDE EFFECTS
  NONE

===========================================================================*/
PACKED void * ftm_fm_rx_fmbusread_receiver
(
  void
)
{
  fm_cmd_status_type cmdStatus = FM_CMD_UNRECOGNIZED_CMD;
  fmbusread_response * response = (fmbusread_response *)
                                              diagpkt_subsys_alloc( DIAG_SUBSYS_FTM
                                                                    , FTM_FM_CMD_CODE
                                                                    , sizeof(fmbusread_response)
                                                                  );

  if(response ==  NULL)
  {
    printf("%s Failed to allocate resource",__func__);
    return (void *)convert_cmdstatus_to_ftmstatus(FM_CMD_NO_RESOURCES);
  }

  response->result =  FTM_FM_SUCCESS;
  cmdStatus = FmBusReadReceiver(&fmrequestparams.i2c_params);
  response->length = fmrequestparams.i2c_params.payload_length;
  memcpy(response->data,fmrequestparams.i2c_params.data,fmrequestparams.i2c_params.payload_length);
  response->result = convert_cmdstatus_to_ftmstatus(cmdStatus);
  return response;
}


/*===========================================================================

FUNCTION       ftm_fm_rx_get_af_threshold

DESCRIPTION
  This function is used to get the current AF threshold of the FM SoC

DEPENDENCIES
  none

===========================================================================*/
PACKED void* ftm_fm_rx_get_af_threshold
(
void
)
{
  fm_cmd_status_type fmCmdStatus = FM_CMD_SUCCESS;
  /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
  fmrxsetafthreshold_response * response = (fmrxsetafthreshold_response *)
						   diagpkt_subsys_alloc( DIAG_SUBSYS_FTM
						 , FTM_FM_CMD_CODE
						 , sizeof( fmrxsetafthreshold_response )
					   );
  if(response ==  NULL)
  {
    printf("%s Failed to allocate resource",__func__);
    return (void *)convert_cmdstatus_to_ftmstatus(FM_CMD_NO_RESOURCES);
  }

  /* Populate the registers 0x20-0x2F */
  common_handle.slaveaddress = FM_SLAVE_ADDR;
  common_handle.offset = XFR_CTRL_OFFSET;
  common_handle.data[0] = 0x0F;
  common_handle.payload_length = 0x01;

  if (FM_CMD_SUCCESS != FmBusWriteReceiver(common_handle))
  {
     fmCmdStatus = FM_CMD_FAILURE;
     goto out;
  }

  usleep(WAIT_ON_ISR_DELAY);

  /* Read the RDS AF threshold value */
  common_handle.offset = AFTH_OFFSET;
  common_handle.payload_length = 0x02;

  /* Clear buffer */
  memset(&common_handle.data[0],
                   0,
                   sizeof(common_handle.data));

  if (FM_CMD_SUCCESS != FmBusReadReceiver(&common_handle))
  {
     fmCmdStatus = FM_CMD_FAILURE;
     goto out;
  }

  printf(" Get AF threshold data [0] = 0x%x data[1] = 0x%x\n",common_handle.data[0],common_handle.data[1]);

out :
  response->afthreshold = (common_handle.data[0] << 8) | (common_handle.data[1]);

  if(FM_CMD_SUCCESS != fmCmdStatus)
  {
    printf(" ftm_fm_get_af_threshold Failure - %d", fmCmdStatus);
    response->result = FTM_FAIL;
  }
  else
  {
    printf(" ftm_fm_get_af_threshold - FM_CMD_SUCCESS\n");
    printf(" ftm_fm_get_af_threshold = 0x%x  \n", response->afthreshold);
    response->result = FTM_FM_SUCCESS;
  }

  return response;
}

/*===========================================================================

FUNCTION       ftm_fm_rx_get_rssi_check_timer

DESCRIPTION
  This function is used to get the configured periodic timer to monitor
  channel quality

DEPENDENCIES
  none

===========================================================================*/
PACKED void* ftm_fm_rx_get_rssi_check_timer
(
void
)
{
  fm_cmd_status_type fmCmdStatus = FM_CMD_SUCCESS;

  /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  fmrxsetrssichecktimer_response * response = (fmrxsetrssichecktimer_response *)
							   diagpkt_subsys_alloc( DIAG_SUBSYS_FTM
							 , FTM_FM_CMD_CODE
							 , sizeof( fmrxsetrssichecktimer_response )
							   );
  if(response ==  NULL)
  {
    printf("%s Failed to allocate resource",__func__);
    return (void *)convert_cmdstatus_to_ftmstatus(FM_CMD_NO_RESOURCES);
  }

  /* Populate the registers 0x20-0x2C */
  common_handle.slaveaddress = FM_SLAVE_ADDR;
  common_handle.offset = XFR_CTRL_OFFSET;
  common_handle.data[0] = 0x16;
  common_handle.payload_length = 0x01;

  if (FM_CMD_SUCCESS != FmBusWriteReceiver(common_handle))
  {
     fmCmdStatus = FM_CMD_FAILURE;
     goto out;
  }

  usleep(WAIT_ON_ISR_DELAY);

  /* Read the CHCOND value */
  common_handle.offset = CHCOND_OFFSET;
  common_handle.payload_length = 0x01;

  /* Clear buffer */
  memset(&common_handle.data[0],
                   0,
                   sizeof(common_handle.data));

  if (FM_CMD_SUCCESS != FmBusReadReceiver(&common_handle))
  {
     fmCmdStatus = FM_CMD_FAILURE;
  }
out :
  response->rssitimer = common_handle.data[0];

  if(FM_CMD_SUCCESS != fmCmdStatus)
  {
    printf(" ftm_fm_get_rssi_check_timer Failure - %d\n", fmCmdStatus);
    response->result = FTM_FAIL;
  }
  else
  {
    printf(" ftm_fm_get_rssi_check_timer - FM_CMD_SUCCESS\n");
    printf(" ftm_fm_get_rssi_check_timer = 0x%x  \n", response->rssitimer);
    response->result = FTM_FM_SUCCESS;
  }
  return response;
}

/*===========================================================================

FUNCTION       ftm_fm_rx_get_rds_pi_timer

DESCRIPTION
  This function is used to get the time set to wait for an RDS interrupt before
  reporting no RDS data

DEPENDENCIES
  none

===========================================================================*/
PACKED void* ftm_fm_rx_get_rds_pi_timer
(
void
)
{
  fm_cmd_status_type fmCmdStatus = FM_CMD_SUCCESS;

  /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  fmrxsetrdspitimer_response * response = (fmrxsetrdspitimer_response *)
						 diagpkt_subsys_alloc( DIAG_SUBSYS_FTM
						   , FTM_FM_CMD_CODE
						   , sizeof( fmrxsetrdspitimer_response )
						 );
  if(response ==  NULL)
  {
    printf("%s Failed to allocate resource",__func__);
    return (void *)convert_cmdstatus_to_ftmstatus(FM_CMD_NO_RESOURCES);
  }

  /* Populate the registers 0x20-0x2C */
  common_handle.slaveaddress = FM_SLAVE_ADDR;
  common_handle.offset = XFR_CTRL_OFFSET;
  common_handle.data[0] = 0x16;
  common_handle.payload_length = 0x01;

  if (FM_CMD_SUCCESS != FmBusWriteReceiver(common_handle))
  {
     fmCmdStatus = FM_CMD_FAILURE;
     goto out;
  }

  usleep(WAIT_ON_ISR_DELAY);
  /* Read the RDS timeout value */
  common_handle.offset = RDSTIMEOUT_OFFSET;
  common_handle.payload_length = 0x01;

  /* Clear buffer */
  memset(&common_handle.data[0],
                   0,
                   sizeof(common_handle.data));

  if (FM_CMD_SUCCESS != FmBusReadReceiver(&common_handle))
  {
     fmCmdStatus = FM_CMD_FAILURE;
  }

out :
  response->rdspitimer = common_handle.data[0];

  if(FM_CMD_SUCCESS != fmCmdStatus)
  {
    printf(" ftm_fm_get_rds_pi_timer Failure - %d\n", fmCmdStatus);
    response->result = FTM_FAIL;
  }
  else
  {
    printf(" ftm_fm_get_rds_pi_timer - FM_CMD_SUCCESS\n");
    printf(" ftm_fm_get_rds_pi_timer = 0x%x  \n", response->rdspitimer);
    response->result = FTM_FM_SUCCESS;
  }
  return response;
}
/*===========================================================================

FUNCTION       ftm_fm_rx_set_af_threshold

DESCRIPTION
  This function is used to set the AF threshold of the FM SoC

DEPENDENCIES
  none

===========================================================================*/

PACKED void* ftm_fm_rx_set_af_threshold
(
void
)
{
  fm_cmd_status_type fmCmdStatus = FM_CMD_SUCCESS;

  /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
  fmrxsetafthreshold_response * response = (fmrxsetafthreshold_response *)
							   diagpkt_subsys_alloc( DIAG_SUBSYS_FTM
							 , FTM_FM_CMD_CODE
							 , sizeof( fmrxsetafthreshold_response )
						   );
  if(response ==  NULL)
  {
    printf("%s Failed to allocate resource",__func__);
    return (void *)convert_cmdstatus_to_ftmstatus(FM_CMD_NO_RESOURCES);
  }

  /* Populate the registers 0x20-0x2F */
  common_handle.slaveaddress = FM_SLAVE_ADDR;
  common_handle.offset = XFR_CTRL_OFFSET;
  common_handle.data[0] = 0x0F;
  common_handle.payload_length = 0x01;

  if (FM_CMD_SUCCESS != FmBusWriteReceiver(common_handle))
  {
     fmCmdStatus = FM_CMD_FAILURE;
     goto out;
  }

  /* Wait for 15 ms to allow the SoC to poulate the registers 0x20-0x2F */
  usleep(WAIT_ON_ISR_DELAY);

  /* Override the RDS AF threshold value */
  common_handle.offset = AFTH_OFFSET;
  common_handle.data[0] = ((fmrequestparams.rx_af_threshold >> 8) & 0xFF);
  common_handle.data[1] = ((fmrequestparams.rx_af_threshold) & 0xFF);
  common_handle.payload_length = 0x02;

  printf(" Set AF threshold  data [0] = 0x%x data[1] = 0x%x\n",common_handle.data[0],common_handle.data[1]);

  if (FM_CMD_SUCCESS != FmBusWriteReceiver(common_handle))
  {
   fmCmdStatus = FM_CMD_FAILURE;
   goto out;
  }

  /* Apply back the new values */
  common_handle.offset = XFR_CTRL_OFFSET;
  common_handle.data[0] = 0x8F;
  common_handle.payload_length = 0x01;

  if (FM_CMD_SUCCESS != FmBusWriteReceiver(common_handle))
  {
     fmCmdStatus = FM_CMD_FAILURE;
     goto out;
  }

out :
  response->afthreshold = fmrequestparams.rx_af_threshold;

  if(FM_CMD_SUCCESS != fmCmdStatus)
  {
    printf(" FmApi_SetAfThreshold Failure - %d\n", fmCmdStatus);
    response->result = FTM_FAIL;
  }
  else
  {
    printf(" FmApi_SetAfThreshold - fmrequestparams.rx_af_threshold = 0x%x\n", fmrequestparams.rx_af_threshold);
    response->result = FTM_FM_SUCCESS;
  }

  return response;

}

/*===========================================================================

FUNCTION       ftm_fm_rx_set_rssi_check_timer

DESCRIPTION
  This function is used to set the periodic timer to monitor channel quality

DEPENDENCIES
  none

===========================================================================*/

PACKED void* ftm_fm_rx_set_rssi_check_timer
(
void
)
{
  fm_cmd_status_type fmCmdStatus= FM_CMD_SUCCESS;

  /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  fmrxsetrssichecktimer_response * response = (fmrxsetrssichecktimer_response *)
								   diagpkt_subsys_alloc( DIAG_SUBSYS_FTM
								 , FTM_FM_CMD_CODE
								 , sizeof( fmrxsetrssichecktimer_response )
								);
  if(response ==  NULL)
  {
    printf("%s Failed to allocate resource",__func__);
    return (void *)convert_cmdstatus_to_ftmstatus(FM_CMD_NO_RESOURCES);
  }

  printf(" ftm_fm_set_rssi_check_timer gtsFtmFM.tuFmParams.ucRxRssichecktimer = %d\n",fmrequestparams.rx_rssi_checktimer);
  /* Populate the registers 0x20-0x2C */
  common_handle.slaveaddress = FM_SLAVE_ADDR;
  common_handle.offset = XFR_CTRL_OFFSET;
  common_handle.data[0] = 0x16;
  common_handle.payload_length = 0x01;

  if (FM_CMD_SUCCESS != FmBusWriteReceiver(common_handle))
  {
     fmCmdStatus = FM_CMD_FAILURE;
     goto out;
  }

  /* Wait for 15 ms to allow the SoC to poulate the registers 0x20-0x2C */
  usleep(WAIT_ON_ISR_DELAY);

  /* Override the CHCOND value */
  common_handle.offset = CHCOND_OFFSET;
  common_handle.data[0] = (fmrequestparams.rx_rssi_checktimer & 0xFF);
  common_handle.payload_length = 0x01;

  if (FM_CMD_SUCCESS != FmBusWriteReceiver(common_handle))
  {
     fmCmdStatus = FM_CMD_FAILURE;
     goto out;
  }

  /* Apply back the new values */
  common_handle.offset = XFR_CTRL_OFFSET;
  common_handle.data[0] = 0x96;
  common_handle.payload_length = 0x01;

  if (FM_CMD_SUCCESS != FmBusWriteReceiver(common_handle))
  {
     fmCmdStatus = FM_CMD_FAILURE;
     goto out;
  }

out :
  response->rssitimer = fmrequestparams.rx_rssi_checktimer;

  if(FM_CMD_SUCCESS != fmCmdStatus)
  {
    printf(" FmApi_SetRssiCheckTimer Failure - %d\n", fmCmdStatus);
    response->result = FTM_FAIL;
  }
  else
  {
    printf(" FmApi_SetRssiCheckTimer - fmrequestparams.rx_rssi_checktimer = %d\n",
						fmrequestparams.rx_rssi_checktimer);
    response->result = FTM_FM_SUCCESS;
  }

  return response;
}

/*===========================================================================

FUNCTION       ftm_fm_rx_set_rds_pi_timer

DESCRIPTION
  This function is used to set the time to wait for an RDS interrupt before
  reporting no RDS data

DEPENDENCIES
  none

===========================================================================*/

PACKED void* ftm_fm_rx_set_rds_pi_timer
(
void
)
{
  fm_cmd_status_type fmCmdStatus = FM_CMD_SUCCESS;

  /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  fmrxsetrdspitimer_response * response = (fmrxsetrdspitimer_response *)
							 diagpkt_subsys_alloc( DIAG_SUBSYS_FTM
							   , FTM_FM_CMD_CODE
							   , sizeof( fmrxsetrdspitimer_response )
							 );
  if(response ==  NULL)
  {
    printf("%s Failed to allocate resource",__func__);
    return (void *)convert_cmdstatus_to_ftmstatus(FM_CMD_NO_RESOURCES);
  }

  printf(" ftm_fm_set_rds_pi_timer gtsFtmFM.tuFmParams.ucRxRdsPItimer = %d\n",fmrequestparams.rx_rds_pi_timer);
  /* Populate the registers 0x20-0x2C */
  common_handle.slaveaddress = FM_SLAVE_ADDR;
  common_handle.offset = XFR_CTRL_OFFSET;
  common_handle.data[0] = 0x16;
  common_handle.payload_length = 0x01;

  if (FM_CMD_SUCCESS != FmBusWriteReceiver(common_handle))
  {
     fmCmdStatus = FM_CMD_FAILURE;
     goto out;
  }

  /* Wait for 15 ms to allow the SoC to poulate the registers 0x20-0x2C */
  usleep(WAIT_ON_ISR_DELAY);

  /* Override the RDS timeout value */
  common_handle.offset = RDSTIMEOUT_OFFSET;
  common_handle.data[0] = (fmrequestparams.rx_rds_pi_timer & 0xFF);
  common_handle.payload_length = 0x01;

  if (FM_CMD_SUCCESS != FmBusWriteReceiver(common_handle))
  {
     fmCmdStatus = FM_CMD_FAILURE;
     goto out;
  }


  /* Apply back the new values */
  common_handle.offset = XFR_CTRL_OFFSET;
  common_handle.data[0] = 0x96;
  common_handle.payload_length = 0x01;

  if (FM_CMD_SUCCESS != FmBusWriteReceiver(common_handle))
  {
     fmCmdStatus = FM_CMD_FAILURE;
     goto out;
  }

out :
  response->rdspitimer = fmrequestparams.rx_rds_pi_timer;
  if(FM_CMD_SUCCESS != fmCmdStatus)
  {
    printf(" FmApi_SetRdsPiTimer Failure - %d\n", fmCmdStatus);
    response->result = FTM_FAIL;
  }
  else
  {
    printf(" FmApi_SetRdsPiTimer - fmrequestparams.rx_rds_pi_timer = %d\n",
			fmrequestparams.rx_rds_pi_timer);
    response->result = FTM_FM_SUCCESS;
  }

  return response;

}
/*===========================================================================

FUNCTION       ftm_fm_rx_get_sinr_samples

DESCRIPTION
  This function is used to get the signal to noise ratio samples

DEPENDENCIES
  none

===========================================================================*/

PACKED void * ftm_fm_rx_get_sinr_samples
(
  void
)
{
  fm_cmd_status_type cmdStatus = FM_CMD_UNRECOGNIZED_CMD;
  getsinrsamples_response * response = (getsinrsamples_response *)
                                              diagpkt_subsys_alloc( DIAG_SUBSYS_FTM
                                                                    , FTM_FM_CMD_CODE
                                                                    , sizeof(getsinrsamples_response)
                                                                  );
  if(response ==  NULL)
  {
    printf("%s Failed to allocate resource",__func__);
    return (void *)convert_cmdstatus_to_ftmstatus(FM_CMD_NO_RESOURCES);
  }

  if(fmPowerState != FM_RX_ON)
  {
    response->result = convert_cmdstatus_to_ftmstatus(FM_CMD_NO_RESOURCES);
    return response;
  }
  response->result =  FTM_FM_SUCCESS;
  cmdStatus = GetSINRSamples();
  response->sinr_sample = fm_global_params.sinr_samples;
  response->result = convert_cmdstatus_to_ftmstatus(cmdStatus);
  return response;
}

/*===========================================================================

FFUNCTION       ftm_fm_rx_set_sinr_samples

DESCRIPTION
  This function is used to set the signal to noise ratio samples to take into
  accounts for SINR avg calculation

DEPENDENCIES
  none

===========================================================================*/

PACKED void * ftm_fm_rx_set_sinr_samples
(
  void
)
{
  fm_cmd_status_type cmdStatus = FM_CMD_UNRECOGNIZED_CMD;
  generic_response * response = (generic_response *)
                                              diagpkt_subsys_alloc( DIAG_SUBSYS_FTM
                                                                    , FTM_FM_CMD_CODE
                                                                    , sizeof(generic_response)
                                                                  );
  if(response ==  NULL)
  {
    printf("%s Failed to allocate resource",__func__);
    return (void *)convert_cmdstatus_to_ftmstatus(FM_CMD_NO_RESOURCES);
  }

  if(fmPowerState != FM_RX_ON)
  {
    response->result = convert_cmdstatus_to_ftmstatus(FM_CMD_NO_RESOURCES);
    return response;
  }
  response->result =  FTM_FM_SUCCESS;
  cmdStatus = SetSINRSamples( fmrequestparams.sinr_samples );
  response->result = convert_cmdstatus_to_ftmstatus(cmdStatus);
  return response;
}

/*========================================================================
 FUNCTION       ftm_fm_rx_get_sinr_threshold

DESCRIPTION
  This function is used to get the SINR threshold

DEPENDENCIES
  none

==========================================================================*/
PACKED void * ftm_fm_rx_get_sinr_threshold
(
  void
)
{
  fm_cmd_status_type cmdStatus = FM_CMD_UNRECOGNIZED_CMD;
  getsinrthreshold_response * response = (getsinrthreshold_response *)
                                              diagpkt_subsys_alloc( DIAG_SUBSYS_FTM
                                                                    , FTM_FM_CMD_CODE
                                                                    , sizeof(getsinrthreshold_response)
                                                                  );
  if(response ==  NULL)
  {
    printf("%s Failed to allocate resource",__func__);
    return (void *)convert_cmdstatus_to_ftmstatus(FM_CMD_NO_RESOURCES);
  }

  if(fmPowerState != FM_RX_ON)
  {
    response->result = convert_cmdstatus_to_ftmstatus(FM_CMD_NO_RESOURCES);
    return response;
  }
  response->result =  FTM_FM_SUCCESS;
  cmdStatus = GetSINRThreshold();
  response->sinr_threshold = fm_global_params.sinr_threshold;
  response->result = convert_cmdstatus_to_ftmstatus(cmdStatus);
  return response;
}
/*========================================================================
 FUNCTION       ftm_fm_rx_set_sinr_threshold

DESCRIPTION
  This function is used to set the SINR threshold

DEPENDENCIES
  none

==========================================================================*/
PACKED void * ftm_fm_rx_set_sinr_threshold
(
  void
)
{
  fm_cmd_status_type cmdStatus = FM_CMD_UNRECOGNIZED_CMD;
  generic_response * response = (generic_response *)
                                              diagpkt_subsys_alloc( DIAG_SUBSYS_FTM
                                                                    , FTM_FM_CMD_CODE
                                                                    , sizeof(generic_response)
                                                                  );
  if(response ==  NULL)
  {
    printf("%s Failed to allocate resource",__func__);
    return (void *)convert_cmdstatus_to_ftmstatus(FM_CMD_NO_RESOURCES);
  }

  if(fmPowerState != FM_RX_ON)
  {
    response->result = convert_cmdstatus_to_ftmstatus(FM_CMD_NO_RESOURCES);
    return response;
  }
  response->result =  FTM_FM_SUCCESS;
  cmdStatus = SetSINRThreshold( fmrequestparams.sinr_threshold );
  response->result = convert_cmdstatus_to_ftmstatus(cmdStatus);
  return response;
}

/*========================================================================
 FUNCTION       ftm_fm_rx_get_onchannel_threshold

DESCRIPTION
  This function is used to get the on channel threshold

DEPENDENCIES
  none

==========================================================================*/
PACKED void * ftm_fm_rx_get_onchannel_threshold
(
  void
)
{
  fm_cmd_status_type cmdStatus = FM_CMD_UNRECOGNIZED_CMD;
  getonchannelthreshold_response * response = (getonchannelthreshold_response *)
                                              diagpkt_subsys_alloc( DIAG_SUBSYS_FTM
                                                                    , FTM_FM_CMD_CODE
                                                                    , sizeof(getonchannelthreshold_response)
                                                                  );
  if(response ==  NULL)
  {
    printf("%s Failed to allocate resource",__func__);
    return (void *)convert_cmdstatus_to_ftmstatus(FM_CMD_NO_RESOURCES);
  }

  if(fmPowerState != FM_RX_ON)
  {
    response->result = convert_cmdstatus_to_ftmstatus(FM_CMD_NO_RESOURCES);
    return response;
  }
  response->result =  FTM_FM_SUCCESS;
  cmdStatus = GetOnChannelThreshold();
  response->sinr_on_th = fm_global_params.On_channel_threshold;
  response->result = convert_cmdstatus_to_ftmstatus(cmdStatus);
  return response;
}

/*========================================================================
 FUNCTION       ftm_fm_rx_set_onchannel_threshold

DESCRIPTION
  This function is used to set the on channel threshold

DEPENDENCIES
  none

==========================================================================*/

PACKED void * ftm_fm_rx_set_onchannel_threshold
(
  void
)
{
  fm_cmd_status_type cmdStatus = FM_CMD_UNRECOGNIZED_CMD;
  generic_response * response = (generic_response *)
                                              diagpkt_subsys_alloc( DIAG_SUBSYS_FTM
                                                                    , FTM_FM_CMD_CODE
                                                                    , sizeof(generic_response)
                                                                  );
  if(response ==  NULL)
  {
    printf("%s Failed to allocate resource",__func__);
    return (void *)convert_cmdstatus_to_ftmstatus(FM_CMD_NO_RESOURCES);
  }

  if(fmPowerState != FM_RX_ON)
  {
    response->result = convert_cmdstatus_to_ftmstatus(FM_CMD_NO_RESOURCES);
    return response;
  }
  response->result =  FTM_FM_SUCCESS;
  cmdStatus = SetOnChannelThreshold( fmrequestparams.On_channel_threshold  );
  response->result = convert_cmdstatus_to_ftmstatus(cmdStatus);
  return response;
}

/*========================================================================
 FUNCTION       ftm_fm_rx_get_offchannel_threshold

DESCRIPTION
  This function is used to get the Off channel threshold

DEPENDENCIES
  none

==========================================================================*/
PACKED void * ftm_fm_rx_get_offchannel_threshold
(
  void
)
{
  fm_cmd_status_type cmdStatus = FM_CMD_UNRECOGNIZED_CMD;
  getoffchannelthreshold_response * response = (getoffchannelthreshold_response *)
                                              diagpkt_subsys_alloc( DIAG_SUBSYS_FTM
                                                                    , FTM_FM_CMD_CODE
                                                                    , sizeof(getoffchannelthreshold_response)
                                                                  );
  if(response ==  NULL)
  {
    printf("%s Failed to allocate resource",__func__);
    return (void *)convert_cmdstatus_to_ftmstatus(FM_CMD_NO_RESOURCES);
  }

  if(fmPowerState != FM_RX_ON)
  {
    response->result = convert_cmdstatus_to_ftmstatus(FM_CMD_NO_RESOURCES);
    return response;
  }
  response->result =  FTM_FM_SUCCESS;
  cmdStatus = GetOffChannelThreshold();
  response->sinr_off_th = fm_global_params.Off_channel_threshold;
  response->result = convert_cmdstatus_to_ftmstatus(cmdStatus);
  return response;
}
/*========================================================================
 FUNCTION       ftm_fm_rx_set_offchannel_threshold

DESCRIPTION
  This function is used to set the off channel threshold

DEPENDENCIES
  none

==========================================================================*/
PACKED void * ftm_fm_rx_set_offchannel_threshold
(
  void
)
{
  fm_cmd_status_type cmdStatus = FM_CMD_UNRECOGNIZED_CMD;
  generic_response * response = (generic_response *)
                                              diagpkt_subsys_alloc( DIAG_SUBSYS_FTM
                                                                    , FTM_FM_CMD_CODE
                                                                    , sizeof(generic_response)
                                                                  );
  if(response ==  NULL)
  {
    printf("%s Failed to allocate resource",__func__);
    return (void *)convert_cmdstatus_to_ftmstatus(FM_CMD_NO_RESOURCES);
  }

  if(fmPowerState != FM_RX_ON)
  {
    response->result = convert_cmdstatus_to_ftmstatus(FM_CMD_NO_RESOURCES);
    return response;
  }
  response->result =  FTM_FM_SUCCESS;
  cmdStatus = SetOffChannelThreshold( fmrequestparams.Off_channel_threshold  );
  response->result = convert_cmdstatus_to_ftmstatus(cmdStatus);
  return response;
}


/*===========================================================================

FUNCTION       ftm_fm_rx_get_rds_block_err

DESCRIPTION
  This function is used to get the current RDS block error parameters

DEPENDENCIES
  none

===========================================================================*/
PACKED void* ftm_fm_rx_get_rds_block_err
(
void
)
{
  fm_cmd_status_type fmCmdStatus = FM_CMD_SUCCESS;


  rds_err_count_response * response = (rds_err_count_response *)
                                                 diagpkt_subsys_alloc( DIAG_SUBSYS_FTM
                                                   , FTM_FM_CMD_CODE
                                                   , sizeof( rds_err_count_response )
                                                 );
  if(response ==  NULL)
  {
    printf("%s Failed to allocate resource",__func__);
    return (void *)convert_cmdstatus_to_ftmstatus(FM_CMD_NO_RESOURCES);
  }

  common_handle.slaveaddress = FM_SLAVE_ADDR;
  common_handle.offset = XFR_CTRL_OFFSET;
  common_handle.data[0] = FTM_FM_RDS_COUNT;
  common_handle.payload_length = 0x01;

  if (FM_CMD_SUCCESS != FmBusWriteReceiver(common_handle))
  {
     fmCmdStatus = FM_CMD_FAILURE;
     goto out;
  }

  usleep(WAIT_ON_ISR_DELAY);
  /* Read the RDS block error rate */
  common_handle.offset = RDSERR_OFFSET;
  common_handle.payload_length = 0x08;

  /* Clear buffer */
  memset(&common_handle.data[0],
                   0,
                   sizeof(common_handle.data));

  if (FM_CMD_SUCCESS != FmBusReadReceiver(&common_handle))
  {
     fmCmdStatus = FM_CMD_FAILURE;
  }

out :
  response->rdserrcount = ((common_handle.data[0] << 24) | (common_handle.data[1] << 16)
                          | (common_handle.data[2] << 8) | common_handle.data[3]);
  response->numofblocks = BLOCKS_PER_GROUP *((common_handle.data[4] << 24) | (common_handle.data[5] << 16)
                          | (common_handle.data[6] << 8) | common_handle.data[7]);

  if(FM_CMD_SUCCESS != fmCmdStatus)
  {
    printf(" ftm_fm_rx_get_rds_block_err Failure - %d\n", fmCmdStatus);
    response->result = FTM_FAIL;
  }
  else
  {
    printf(" ftm_fm_rx_get_rds_block_err - FM_CMD_SUCCESS\n");
    printf(" ftm_fm_rx_get_rds_block_err = 0x%x  0x%x\n", (unsigned int)response->rdserrcount,(unsigned int)response->numofblocks);
    response->result = FTM_FM_SUCCESS;
  }
  return response;
}


/*===========================================================================

FUNCTION      ftm_fm_enable_audio

DESCRIPTION
  This function is used to take the audio output mode from QRCT.

DEPENDENCIES
  none

===========================================================================*/
PACKED void* ftm_fm_enable_audio
(
void
)
{
  fm_cmd_status_type cmdStatus = FM_CMD_UNRECOGNIZED_CMD;
  generic_response * response = (generic_response *)
                                                 diagpkt_subsys_alloc( DIAG_SUBSYS_FTM
                                                   , FTM_FM_CMD_CODE
                                                   , sizeof( generic_response )
                                                 );

  if(response ==  NULL)
  {
    printf("%s Failed to allocate resource",__func__);
    return (void *)convert_cmdstatus_to_ftmstatus(FM_CMD_NO_RESOURCES);
  }

  printf("enter ftm_fm_enable_audio\n");

  if (response != NULL) {
      response->result = FTM_FM_SUCCESS;
      mkfifo(audio_ftm_cmds, 0777) ;

      fm_audio_output = fmrequestparams.audio_output;
      cmdStatus = ftm_fm_audio(fm_audio_output, init_audio_vlm);
      response->result = convert_cmdstatus_to_ftmstatus(cmdStatus);
  } else
      printf("ftm_fm_enable_audio unable to allocate memory for response packet \n");

  return response;
}


/*===========================================================================

FUNCTION      ftm_fm_setting_volume

DESCRIPTION
  This function is used to set the FM volume.

DEPENDENCIES
  none

===========================================================================*/
#define MSEC_TO_NSEC   (1000 * 1000)

#ifdef FM_SOC_TYPE_CHEROKEE
#define DISABLE_SLIMBUS_DATA_PORT    0
#define ENABLE_SLIMBUS_DATA_PORT     1
#define ENABLE_SLIMBUS_CLOCK_DATA    2
#endif

PACKED void* ftm_fm_setting_volume
(
void
)
{
  fm_cmd_status_type cmdStatus = FM_CMD_UNRECOGNIZED_CMD;
  struct timespec ts;
  generic_response * response = (generic_response *)
                                                 diagpkt_subsys_alloc( DIAG_SUBSYS_FTM
                                                   , FTM_FM_CMD_CODE
                                                   , sizeof( generic_response )
                                                 );
  if(response ==  NULL)
  {
    printf("%s Failed to allocate resource",__func__);
    return (void *)convert_cmdstatus_to_ftmstatus(FM_CMD_NO_RESOURCES);
  }

  printf("enter ftm_fm_setting_volume\n");
  ts.tv_sec = 0;
  ts.tv_nsec = 100 * MSEC_TO_NSEC;

  if (response != NULL) {
      response->result = FTM_FM_SUCCESS;
      printf("Disabling audio");
      ftm_fm_disable_audio();
#ifdef FM_SOC_TYPE_CHEROKEE
      nanosleep(&ts, NULL);
      ftm_fm_enable_slimbus(DISABLE_SLIMBUS_DATA_PORT);
#endif

      printf("audio_output = %d", fmrequestparams.audio_output);
      printf("audio_vlm = %d",fmrequestparams.audio_vlm);
      cmdStatus = ftm_fm_audio(fm_audio_output, fmrequestparams.audio_vlm);
      response->result = convert_cmdstatus_to_ftmstatus(cmdStatus);
#ifdef FM_SOC_TYPE_CHEROKEE
      sleep(1);
      ftm_fm_enable_slimbus(ENABLE_SLIMBUS_DATA_PORT);
#endif
  } else
      printf("ftm_fm_setting_volume NULL repsonse packet\n");

  return response;
}
/*===========================================================================

FUNCTION       ftm_fm_rx_reset_rds_err_count

DESCRIPTION
  This function is used to reseet the current RDS block error parameters

DEPENDENCIES
  none

===========================================================================*/
PACKED void* ftm_fm_rx_reset_rds_err_count
(
void
)
{
  fm_cmd_status_type fmCmdStatus = FM_CMD_SUCCESS;
  /* Default data to be written for Marimba versions
   * not applicable for Bahama versions
   */
  uint8 uData[] = { 0xD0 ,0x03 ,0xCE, 0x00, 0x00,
                    0x00, 0x00, 0x00, 0x00, 0x00, 0x00};


  generic_response * response = (generic_response *)
                                                 diagpkt_subsys_alloc( DIAG_SUBSYS_FTM
                                                   , FTM_FM_CMD_CODE
                                                   , sizeof( generic_response )
                                                 );
  if(response ==  NULL)
  {
    printf("%s Failed to allocate resource",__func__);
    return (void *)convert_cmdstatus_to_ftmstatus(FM_CMD_NO_RESOURCES);
  }

  /* if chip version not determined we can't reset */
  if(chipVersion == 0)
  {
     fmCmdStatus = FM_CMD_FAILURE;
     goto out;
  }

  if((chipVersion == FMQSOCCOM_FM6500_WCN2243_10_VERSION) ||
	(chipVersion == FMQSOCCOM_FM6500_WCN2243_20_VERSION))
  {
    uint8 uData_bahama[] = {0xD0, 0x00, 0xE6, 0x00, 0x00,
                            0x00, 0x00, 0x00, 0x00, 0x00,
                            0x00};
    printf("Chip Id is 0x%x\n",chipVersion);
    /* The Reset data for Bahama is different from
     * Marimba the data is dynamically selected based
     * on ChipVersion
     */
    memcpy(uData,uData_bahama,sizeof(uData));
  }
  common_handle.slaveaddress = FM_SLAVE_ADDR;
  common_handle.offset = XFR_CTRL_OFFSET;
  /* Copy the data to i2c request buffer */
  memcpy(&common_handle.data[0],
           uData,
           sizeof(uData));
  common_handle.payload_length = sizeof(uData);

  if (FM_CMD_SUCCESS != FmBusWriteReceiver(common_handle))
  {
     fmCmdStatus = FM_CMD_FAILURE;
     goto out;
  }

  usleep(WAIT_ON_ISR_DELAY);

out:
  if(FM_CMD_SUCCESS != fmCmdStatus)
  {
    printf(" ftm_fm_reset_rds_err_count Failure - %d", fmCmdStatus);
    response->result = FTM_FAIL;
  }
  else
  {
    printf(" ftm_fm_reset_rds_err_count - FM_CMD_SUCCESS");
    response->result = FTM_FM_SUCCESS;
  }
  return(response);

}


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
void * ftm_fm_dispatch(ftm_fm_pkt_type *ftm_fm_pkt, uint16 length )
{
  ftm_fm_sub_cmd_type ftm_fm_sub_cmd;

  UNUSED(length);
  ftm_fm_sub_cmd = (ftm_fm_sub_cmd_type)ftm_fm_pkt->cmd_id;
#ifdef FTM_DEBUG
  printf("ftm_fm_pkt->cmd_id = %d\n cmd_data_len = %d\n", ftm_fm_pkt->cmd_id,ftm_fm_pkt->cmd_data_len);
#endif
  ftm_fm_copy_request_data(ftm_fm_pkt,ftm_fm_pkt->cmd_data_len);
  switch(ftm_fm_sub_cmd)
  {
    if(!fm_passthrough)
    {
	case FTM_FM_RX_ENABLE_RECEIVER:
      return(ftm_fm_rx_enable_receiver());
    case FTM_FM_RX_DISABLE_RECEIVER:
      return(ftm_fm_rx_disable_receiver());
    case FTM_FM_RX_CONFIGURE_RECEIVER:
      return(ftm_fm_rx_configure_receiver());
    case FTM_FM_RX_SET_STATION:
      return(ftm_fm_rx_setfrequency_receiver());
    case FTM_FM_RX_SET_MUTE_MODE:
      return(ftm_fm_rx_setmutemode_receiver());
    case FTM_FM_RX_SET_STEREO_MODE:
      return(ftm_fm_rx_setstereomode_receiver());
    case FTM_FM_RX_GET_STATION_PARAMETERS:
      return(ftm_fm_rx_getstationparameters_receiver());
    case FTM_FM_RX_RDS_GROUP_OPTIONS :
      return (ftm_fm_rx_setrdsoptions_receiver());
    case FTM_FM_RX_SET_POWER_MODE :
      return (ftm_fm_rx_setpowermode_receiver());
    case FTM_FM_RX_SET_SIGNAL_THRESHOLD :
      return (ftm_fm_rx_setsignalthreshold_receiver());
    case FTM_FM_RX_GET_RSSI_LIMIT :
      return (ftm_fm_rx_getrssilimit_receiver());
    case FTM_FM_RX_SEARCH_STATIONS :
      return (ftm_fm_rx_searchstations_receiver());
    case FTM_FM_RX_SEARCH_RDS_STATIONS :
      return (ftm_fm_rx_searchrdsstations_receiver());
    case FTM_FM_RX_SEARCH_STATIONS_LIST :
      return (ftm_fm_rx_searchstationslist_receiver());
    case FTM_FM_RX_CANCEL_SEARCH:
      return (ftm_fm_rx_cancelsearch_receiver());
    case FTM_FM_RX_GET_PS_INFO :
      return (ftm_fm_rx_getpsinfo_receiver());
    case FTM_FM_RX_GET_RT_INFO :
      return (ftm_fm_rx_getrtinfo_receiver());
    case FTM_FM_RX_GET_AF_INFO :
      return (ftm_fm_rx_getafinfo_receiver());
    case FTM_FM_BUS_WRITE:
      return (ftm_fm_rx_fmbuswrite_receiver());
    case FTM_FM_BUS_READ:
      return (ftm_fm_rx_fmbusread_receiver());
    case FTM_FM_RX_RDS_GROUP_PROC_OPTIONS:
      return (ftm_fm_rx_setrdsgroupproc_receiver());
    case FTM_FM_RX_GET_AF_THRESHOLD:
      return(ftm_fm_rx_get_af_threshold());
    case FTM_FM_RX_GET_RSSI_CHECK_TIMER:
      return(ftm_fm_rx_get_rssi_check_timer());
    case FTM_FM_RX_GET_RDS_PI_TIMER:
      return(ftm_fm_rx_get_rds_pi_timer());
    case FTM_FM_RX_SET_AF_THRESHOLD:
      return(ftm_fm_rx_set_af_threshold());
    case FTM_FM_RX_SET_RSSI_CHECK_TIMER:
      return(ftm_fm_rx_set_rssi_check_timer());
    case FTM_FM_RX_SET_RDS_PI_TIMER:
      return(ftm_fm_rx_set_rds_pi_timer());
    case FTM_FM_RX_GET_SIGNAL_THRESHOLD :
      return (ftm_fm_rx_getsignalthreshold_receiver());
    case FTM_FM_RX_GET_RDS_ERR_COUNT :
      return (ftm_fm_rx_get_rds_block_err());
    case FTM_FM_RX_RESET_RDS_ERR_COUNT :
      return (ftm_fm_rx_reset_rds_err_count());
    case FTM_FM_TX_ENABLE_TRANSMITTER :
      return(ftm_fm_tx_enable_transmitter());
    case FTM_FM_TX_CONFIGURE_TRANSMITTER:
      return(ftm_fm_tx_configure_transmitter());
    case FTM_FM_TX_DISABLE_TRANSMITTER:
      return(ftm_fm_tx_disable_transmitter());
    case FTM_FM_TX_SET_STATION:
      return(ftm_fm_tx_setfrequency_transmitter());
    case FTM_FM_TX_TX_PS_INFO :
      return ftm_fm_tx_ps_info();
    case FTM_FM_TX_STOP_PS_INFO_TX :
      return ftm_fm_tx_stop_ps_info();
    case FTM_FM_TX_TX_RT_INFO :
      return ftm_fm_tx_rt_info();
    case FTM_FM_TX_STOP_RT_INFO_TX :
      return ftm_fm_tx_stop_rt_info();
    case FTM_FM_TX_GET_PS_FEATURES :
      return ftm_fm_tx_get_ps_features();
    case FTM_FM_SET_SOFT_MUTE:
      return ftm_fm_set_soft_mute_receiver();
    case FTM_FM_SET_ANTENNA:
      return ftm_fm_set_antenna();
    case FTM_FM_POKE_RIVA_WORD:
      return ftm_fm_poke_riva_word();
    case FTM_FM_PEEK_RIVA_WORD:
      return ftm_fm_peek_riva_word();
    case FTM_FM_PEEK_SSBI:
      return ftm_fm_peek_ssbi_reg();
    case FTM_FM_POKE_SSBI:
      return ftm_fm_poke_ssbi_reg();
    case FTM_FM_SET_TONE_GENERATION:
      return (ftm_fm_tx_tone_generation());
    case FTM_FM_READ_RDS_GRP_CNTRS:
      return (ftm_fm_read_rds_grp_cntrs());
    case FTM_FM_READ_RDS_GRP_CNTRS_EXT:
      return (ftm_fm_read_rds_grp_cntrs_ext());
    case FTM_FM_SET_HLSI:
      return (ftm_fm_set_hlsi());
    case FTM_FM_RX_GET_SINR_SAMPLES:
      return (ftm_fm_rx_get_sinr_samples());
    case FTM_FM_RX_SET_SINR_SAMPLES:
      return (ftm_fm_rx_set_sinr_samples());
    case FTM_FM_RX_GET_SINR_THRESHOLD:
      return (ftm_fm_rx_get_sinr_threshold());
    case FTM_FM_RX_SET_SINR_THRESHOLD:
      return (ftm_fm_rx_set_sinr_threshold());
    case FTM_FM_RX_GET_ONCHANNEL_TH:
      return (ftm_fm_rx_get_onchannel_threshold());
    case FTM_FM_RX_SET_ONCHANNEL_TH:
      return (ftm_fm_rx_set_onchannel_threshold());
    case FTM_FM_RX_GET_OFFCHANNEL_TH:
      return (ftm_fm_rx_get_offchannel_threshold());
    case FTM_FM_RX_SET_OFFCHANNEL_TH:
      return (ftm_fm_rx_set_offchannel_threshold());
    case FTM_FM_SET_NOTCH_FILTER:
      return (ftm_fm_set_notch_filter());
    case FTM_FM_RX_GET_DEFAULTS:
      return (ftm_fm_default_read());
    case FTM_FM_RX_SET_DEFAULTS:
      return (ftm_fm_default_write());
    case FTM_FM_TX_PWR_LVL_CFG:
      return ftm_fm_tx_pwr_lvl_cfg();
    case FTM_FM_SET_GET_RESET_AGC:
      return ftm_fm_set_get_reset_agc();
    }
    case FTM_FM_ENABLE_AUDIO:
      return ftm_fm_enable_audio();
    case FTM_FM_DISABLE_AUDIO:
      return ftm_fm_disable_audio();
    case FTM_FM_VOLUME_SETTING:
      return ftm_fm_setting_volume();
    default:
      if(fm_passthrough) {
         if(!enable_mm_fmconfig)
         {
            ftm_fm_run_mm();
            enable_mm_fmconfig = 1;
         }
         return bt_ftm_diag_dispatch(ftm_fm_pkt, length);
      }
      return NULL;
  }
}
