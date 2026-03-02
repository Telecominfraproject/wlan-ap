/*==========================================================================

                     FTM FM PFAL Header File

Description
  Function declarations  of the PFAL interfaces for FM.

# Copyright (c) 2010-2012 by Qualcomm Technologies, Inc.  All Rights Reserved.
# Qualcomm Technologies Proprietary and Confidential.

===========================================================================*/

/*===========================================================================

                         Edit History


when       who     what, where, why
--------   ---     ----------------------------------------------------------
08/03/2011 uppalas  Adding support for new ftm commands
06/18/10   rakeshk  Created a header file to hold the interface declarations
                    for ftm fm commands
07/06/10   rakeshk  Added the support for new PFAL APIs for FM Rx
04/03/11   ananthk  Added the support for FM Tx functionalities
===========================================================================*/
#ifdef CONFIG_FTM_FM

#include "event.h"
#include "diagpkt.h"
#include "ftm_fm_common.h"

/*===========================================================================
FUNCTION  EnableFM

DESCRIPTION
  PFAL specific routine to enable FM with the Radio Configuration parameters
  passed.

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
);

/*===========================================================================
FUNCTION  ConfigureFM

DESCRIPTION
  PFAL specific routine to configure FM with the Radio Configuration
  parameters passed.

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
);

/*===========================================================================
FUNCTION  SetFrequencyTransmitter

DESCRIPTION
  PFAL specific routine to configure the FM transmitter's Frequency of reception

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
);

/*===========================================================================
FUNCTION  TransmitPS

DESCRIPTION
  PFAL specific routine to transmit RDS PS strings

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
);

/*===========================================================================
FUNCTION  stopTransmitPS

DESCRIPTION
  PFAL specific routine to stop transmitting the PS string.

PARAMS PASSED
  NIL

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
);

/*===========================================================================
FUNCTION  TransmitRT

DESCRIPTION
  PFAL specific routine to transmit the RT string which provides a brief info
  of the audio content being transmitted. This includes artist name, movie name
  few lines of the audio. Usually the metadata of the song is transmitted as RT

PARAMS PASSED
  'tuFmRTParams' containing RDS PI, and RT information of the transmitting
   station

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
);
/*===========================================================================
FUNCTION  stopTransmitRT

DESCRIPTION
  PFAL specific routine to stop transmitting the RT string for the
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
);

/*===========================================================================
FUNCTION  getTxPSFeatures

DESCRIPTION
  PFAL specific routine to get the supported PS features for the
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
);

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
);

/*===========================================================================
FUNCTION  DisableFM

DESCRIPTION
  PFAL specific routine to disable FM and free all the FM resources

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
);

/*===========================================================================
FUNCTION  SetFrequencyReceiver

DESCRIPTION
  PFAL specific routine to configure the FM receiver's Frequency of reception

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
);

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
fm_cmd_status_type SetSoftMuteModeReceiver
(
 mute_type mutemode
);

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
);

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
);


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
);

/*===========================================================================
FUNCTION  GetStationParametersReceiver

DESCRIPTION
  PFAL specific routine to get the station parameters of the Frequency at
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
);
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
);

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
);

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
);

/*===========================================================================
FUNCTION  SetSignalThresholdReceiver

DESCRIPTION
  PFAL specific routine to configure the signal threshold of FM receiver

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
);

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
);


/*===========================================================================
FUNCTION  GetRSSILimits

DESCRIPTION
  PFAL specific routine to print the RSSI limits of FM receiver

DEPENDENCIES
  NIL

RETURN VALUE
  FM command status

SIDE EFFECTS
  None

===========================================================================*/
fm_cmd_status_type GetRSSILimits
(
);


/*===========================================================================
FUNCTION  GetPSInfoReceiver

DESCRIPTION
  PFAL specific routine to print the PS info of current frequency of
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
);

/*===========================================================================
FUNCTION  GetRTInfoReceiver

DESCRIPTION
  PFAL specific routine to print the Radio text info of current frequency of
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
);

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
);

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
);


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
);


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
);


/*===========================================================================
FUNCTION  CancelSearchReceiver

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
);

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
);

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
);

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
);

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
);

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
fm_cmd_status_type FmSSBIPeekData(
fm_ssbi_poke_reg peek_reg
);

/*===========================================================================
FUNCTION  FmSetGetResetAGC

SIDE EFFECTS
  None

===========================================================================*/
fm_cmd_status_type FmSetGetResetAGC(
fm_set_get_reset_agc_req agc_params
);

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
fm_cmd_status_type FmSSBIPokeData(
fm_ssbi_poke_reg peek_reg
);

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
);

/*===========================================================================
FUNCTION  FmRDSGrpcntrs

DESCRIPTION
  PFAL specific routine to read  the FM RDS group counters

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
);

/*===========================================================================
FUNCTION  FmRDSGrpcntrsExt

DESCRIPTION
  PFAL specific routine to read  the FM RDS group counters extended

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
);

/*===========================================================================
FUNCTION  FmSetHlSi

DESCRIPTION
  PFAL specific routine to configure the FM receiver's HlSi on the
  frequency tuned

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
);

/*===========================================================================
FUNCTION  GetSINRSamples

DESCRIPTION
  PFAL specific routine to get the FM receiver's SINR samples

DEPENDENCIES
  NIL

RETURN VALUE
  FM command status

SIDE EFFECTS
  None

===========================================================================*/

fm_cmd_status_type GetSINRSamples
(

);
/*===========================================================================
FUNCTION  SetSINRSamples

DESCRIPTION
  PFAL specific routine to set the FM receiver's SINR samples

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
);

/*===========================================================================
FUNCTION GetSINRThreshold

DESCRIPTION
  PFAL specific routine to get the FM receiver's SINR threshold

DEPENDENCIES
  NIL

RETURN VALUE
  FM command status

SIDE EFFECTS
  None

===========================================================================*/

fm_cmd_status_type GetSINRThreshold
(

);
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
  char sinr_threshold
);

/*===========================================================================
FUNCTION GetOnChannelThreshold

DESCRIPTION
  PFAL specific routine to get the FM receiver's on channel threshold

DEPENDENCIES
  NIL

RETURN VALUE
  FM command status

SIDE EFFECTS
  None

===========================================================================*/

fm_cmd_status_type GetOnChannelThreshold
(

);

/*===========================================================================
FUNCTION SetOnChannelThreshold

DESCRIPTION
  PFAL specific routine to set the FM receiver's on channel threshold

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
);

/*===========================================================================
FUNCTION GetOffChannelThreshold

DESCRIPTION
  PFAL specific routine to get the FM receiver's off channel threshold

DEPENDENCIES
  NIL

RETURN VALUE
  FM command status

SIDE EFFECTS
  None

===========================================================================*/

fm_cmd_status_type GetOffChannelThreshold
(

);

/*===========================================================================
FUNCTION SetOffChannelThreshold

DESCRIPTION
  PFAL specific routine to set the FM receiver's off channel threshold

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
);
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
);

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
);

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
   ftm_fm_def_data_wr_req*
   defaults
);

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
 uint8 pwrCfg
);

/*===========================================================================

FUNCTION      ftm_fm_run_mm

DESCRIPTION
  This function is used to run mm-audio-ftm.

DEPENDENCIES
  none

===========================================================================*/
void ftm_fm_run_mm
(
 void
);

/*===========================================================================

FUNCTION      ftm_fm_audio

DESCRIPTION
  This function is used to load the target based config file and
  set the audio output and volume.

DEPENDENCIES
  none

===========================================================================*/
fm_cmd_status_type ftm_fm_audio
(
 uint8 source,
 uint8 volume
);

#endif /* CONFIG_FTM_FM */
void ftm_fm_enable_slimbus(int val);
