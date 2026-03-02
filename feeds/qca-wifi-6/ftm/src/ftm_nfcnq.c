/*=========================================================================
                       NQ NFC FTM C File
Description
   This file contains the definitions of the functions
   used to communicate with the NQ Chip.

Copyright (c) 2015-2016 Qualcomm Technologies, Inc.
All Rights Reserved.
Confidential and Proprietary - Qualcomm Technologies, Inc.

===========================================================================*/
/*===========================================================================
                         Edit History
when        who              what,              where,     why
--------   ---     ----------------------------------------------------------
===========================================================================*/

#include "ftm_nfcnq.h"
#include "ftm_nfc.h"
#include "ftm_nfcnq_fwdl.h"

/* Global variables */
pthread_t           clientThread;
PNCI_MESSAGE        pNCIMessage;
sem_t               sRspReady;
int                 fdNfc                   = 0;
uint8_t             nciReplyMessage[ 255 ]  = { 0 };
NQ_CHIP_TYPE        whatNQChip              = UNKNOWN_NQ_CHIP_TYPE;
uint8_t RFdeactivateCmd[ ] = { 0x21, 0x06, 0x01, 0x03};
uint8_t EseDataRsp[ ] = { 0x03, 0x00, 0x21, 0x99, 0x50, 0xFE};

/*=========================================================================
FUNCTION   ftm_nq_nfc_close

DESCRIPTION
  Close the kernel driver for the NQ Chip

PARAMETERS
  None

RETURN VALUE
  int

===========================================================================*/
int ftm_nq_nfc_close( void )
{
    fdNfc = close( fdNfc );                         // close the file descriptor

    LOG_MESSAGE( "%s : Exit with fdNfc = %d \n", __func__, fdNfc );

    return fdNfc;                                   // return the result
}

/*=========================================================================
FUNCTION   ftm_nq_nfc_open

DESCRIPTION
  Open the kernel driver for the NQ Chip

PARAMETERS
  None

RETURN VALUE
  int

===========================================================================*/
int ftm_nq_nfc_open( void )
{
    fdNfc = open( "/dev/nq-nci",                    // try to open /dev/nq-nci
               O_RDWR );

    LOG_MESSAGE( "%s : Exit with fdNfc = %d \n", __func__, fdNfc );

    return fdNfc;                                   // return the result
}

/*=========================================================================
FUNCTION   ftm_nfc_hw_reset

DESCRIPTION
  Resets the NQ Chip

PARAMETERS
  None

RETURN VALUE
  int

===========================================================================*/
int ftm_nfc_hw_reset( void )
{

    int ret = -1;                                                   // return value

    do
    {
        if( fdNfc < 0 )                                             // fdNfc valid?
            break;

        ret = ioctl( fdNfc, NFC_SET_PWR, POWER_ON );                // turn the chip on
        if( ret != 0 )                                              // successful?
        {
            LOG_ERROR( "%s ioctl( fdNfc, NFC_SET_PWR, POWER_ON ) returned %d", __func__, ret );
            ret = -2;
            break;
        }
        usleep( 1000 );                                             // wait

        ret = ioctl( fdNfc, NFC_SET_PWR, POWER_OFF );               // turn the chip off
        if( ret != 0 )                                              // successful?
        {
            LOG_ERROR( "%s ioctl( fdNfc, NFC_SET_PWR, POWER_OFF ) returned %d", __func__, ret );
            ret = -3;
            break;
        }
        usleep( 1000 );                                             // wait

        ret = ioctl( fdNfc, NFC_SET_PWR, POWER_ON );                // turn the chip back on
        if( ret != 0 )                                              // successful?
        {
            LOG_ERROR( "%s ioctl( fdNfc, NFC_SET_PWR, POWER_ON ) returned %d", __func__, ret );
            ret = -4;
            break;
        }

    }while( 0 );

    return ret;
}

/*=========================================================================
FUNCTION   PrintBytes

DESCRIPTION
  Print bytes from an array

PARAMETERS
  uint8_t *buf - Byte array to print
  uint8_t  len - Length of the array
RETURN VALUE
  void

===========================================================================*/
void PrintBytes( uint8_t *buf, uint8_t len)
{
#ifdef NFC_FTM_DEBUG
    int idx;

    LOG_INFORMATION( "%s: Length: %d bytes \n", __func__, len );    // print the number of bytes
    for( idx = 0; idx < len; idx++ )                            // print every byte
    {
        LOG_INFORMATION( "%02x ", buf[idx] );
    }
    LOG_INFORMATION( "\n" );
#else
    UNUSED_PARAMETER( buf );
    UNUSED_PARAMETER( len );
#endif
}

/*=========================================================================
FUNCTION   ftm_nfc_send

DESCRIPTION
  Sends a message to the chip

PARAMETERS
  uint8_t *buf - buffer to be sent
  int      len - the length of the buffer

RETURN VALUE
  int      ret - Status

===========================================================================*/
int ftm_nfc_send( uint8_t* buf )
{
    int             ret     = -1;                   // return value
    int             retries = 15;                   // number of retries
    int             i;
    uint16_t        nciSendMessageLength;
    PNCI_MESSAGE    pMessageToSend = ( PNCI_MESSAGE ) buf;
    pfirmware_download_packet_t pFirmwarePacketsToSend =
                                ( pfirmware_download_packet_t ) buf;


    do
    {
        if( fdNfc < 0 )                             // fdNfc valid?
            break;

        if( NULL == buf )                           // is the buffer valid?
        {
            ret = -2;
            LOG_ERROR( "%s: buf == NULL Invalid Buffer", __func__ );
            break;
        }

        if( ( pFirmwarePacketsToSend->fFragmentedPacket == FIRMWARE_DOWNLOAD_PACKET_FRAG_FLAG_NONE ) ||
            ( pFirmwarePacketsToSend->fFragmentedPacket == FIRMWARE_DOWNLOAD_PACKET_FRAG_FLAG_SET ) )
            nciSendMessageLength = pFirmwarePacketsToSend->payloadLen +
                                   FIRMWARE_DOWNLOAD_PACKET_HEADER_LEN +
                                   FIRMWARE_DOWNLOAD_PACKET_CRC16_LEN;
        else
            nciSendMessageLength = pMessageToSend->len + offsetof( NCI_MESSAGE, buf );

        PrintBytes( buf, nciSendMessageLength );

        do
        {
            retries--;                              // retries left
            ret = write( fdNfc,
                         buf,
                         nciSendMessageLength );    // try to write

            if( ret < nciSendMessageLength )        // did you write the length?
            {
                LOG_MESSAGE( "%s: %d = write( fdNfc, buf, nciSendMessageLength ), errno = %d, tries left = %d \n", __func__, ret, errno, retries );
                continue;                           // try again
            }
            else
                break;                              // done

        } while( retries > 0 );
    } while( 0 );

    return  ret;
}

/*=========================================================================
FUNCTION   ProcessCommand

DESCRIPTION
  Processes a Command for the NQ Chip

PARAMETERS
  uint8_t *nci_data - NCI Data to send

RETURN VALUE
  int ret - 0 if successfully received a reply

===========================================================================*/
int ProcessCommand( uint8_t *nci_data )
{
    int              ret = -1;                              // return value
    struct timespec  time_sec;

    do
    {
        LOG_MESSAGE( "%s: FTM_NFC_SEND_DATA \n", __func__ );

        ret = ftm_nfc_send( nci_data );                     // send the message

        LOG_MESSAGE( "%s: Wait for response \n", __func__ );

        ret = clock_gettime( CLOCK_REALTIME, &time_sec );

        if( ret == -1 )
        {                                                   // didn't get the time?
            LOG_ERROR( "%s: clock_gettime for nci_data error \n", __func__ );
            break;
        }

        time_sec.tv_sec += FTM_NFC_CMD_CMPL_TIMEOUT;        // maximum wait
        ret = sem_timedwait( &sRspReady,                    // start waiting
                             &time_sec );

        if( ret == -1 )                                     // wait finished, not signalled?
        {
            if(!ese_dwp_test)
                LOG_ERROR( "%s: nfc ftm command timed out \n", __func__ );
            break;
        }
    } while( 0 );

    return ret;
}
/*=========================================================================
FUNCTION   ftm_nfc_read

DESCRIPTION
  Reads a message from the chip

PARAMETERS
  int      len - the length of the buffer

RETURN VALUE
  int      ret - Number of bytes read

===========================================================================*/
int ftm_nfc_read( uint8_t* buf, int len )
{
    int ret = -1;

    do
    {
        if( fdNfc < 0 )                                // fdNfc valid?
            break;

        ret = read( fdNfc, buf, len );                 // try to read

    } while( 0 );

    return ret;
}

/*==========================================================================
FUNCTION
  CommitLog

DESCRIPTION
  This commits the log to Diag

PARAMETERS
  NCI_MESSAGE pReadNCIMessage - Pointer to the read NCI Message

RETURN VALUE
  void
==========================================================================*/
void CommitLog( PNCI_MESSAGE pReadNCIMessage )
{
    pftm_nfc_log_pkt_type         pLogBuff;

    do
    {
        pLogBuff = ( ftm_nfc_log_pkt_type * ) log_alloc( LOG_NFC_FTM,        // allocate a buffer for the log
                                                         pReadNCIMessage->len + offsetof( NCI_MESSAGE, buf ) + LOG_HEADER_LENGTH );
        if( NULL == pLogBuff )
        {
            LOG_ERROR( "%s: log_alloc returned NULL \n", __func__ );
            break;
        }

        memcpy( pLogBuff->data,                                             // fill the buffer
                pReadNCIMessage,
                pReadNCIMessage->len + offsetof( NCI_MESSAGE, buf ) );

        log_commit( pLogBuff );                                             // commit the log
    } while ( 0 );

}

/*=============================================================================
FUNCTION
  ProcessReturnedMessage

DESCRIPTION
  Routine that processes an NCI Message that was returned and
  will decide if the message is a notification or a response.

PARAMETERS
  PNCI_MESSAGE pReadNCIMessage - Pointer to the read message

RETURN VALUE
  void
==============================================================================*/
void ProcessReturnedMessage( PNCI_MESSAGE pReadNCIMessage )
{

    switch( pReadNCIMessage->gid & NCIMT_NTF )      // check the first byte
    {
    case NCIMT_RSP:                                 // reply?
        sem_post( &sRspReady );                     // notify the dispatch function
        break;

    case NCIMT_NTF:                                 // notification?
        if (pReadNCIMessage->oid == 0x05)
        {
            LOG_INFORMATION("\n << ...TAG DETECTED... >> \n");
            printTecnologyDetails(pReadNCIMessage->buf[3],pReadNCIMessage->buf[2]);
            sem_post( &sRfNtf );
            ProcessCommand( RFdeactivateCmd );
        }
    case NCIMT_DATA:                                // data?
        if (ese_dwp_test)
        {
            if( memcmp( EseDataRsp, nciReplyMessage, sizeof( EseDataRsp ) ) == 0 )
            {
                LOG_INFORMATION("\n << ESE detected over DWP >> \n\n");
            }
        }
        if( log_status( LOG_NFC_FTM ) )             // logging enabled?
        {
            CommitLog( pReadNCIMessage );
        }
        break;

    default:
        LOG_ERROR( "%s: ERROR - SHOULD NOT HAVE REACHED THIS POINT", __func__ );
        break;
    }

}

/*=========================================================================
FUNCTION   nfc_read_thread

DESCRIPTION
  Thread that constantly looks for messages from the chip

PARAMETERS
  void

RETURN VALUE
  void

===========================================================================*/
void *nfc_read_thread( void *arg )
{
    uint8_t  readLength = 0;
    int      i;
    uint8_t  readNCIUpToLength = offsetof( NCI_MESSAGE, buf );

    UNUSED_PARAMETER( arg );

    for( ; ; )                                                                          // keep reading
    {
        readLength = ftm_nfc_read( nciReplyMessage, readNCIUpToLength );                // read the first 3 bytes

        if( readLength == readNCIUpToLength )                                           // read the message up to NCI Len?
        {
            readLength = ftm_nfc_read( pNCIMessage->buf,                                // go and get the rest
                                       pNCIMessage->len  );

            if( readLength == pNCIMessage->len )                                        // successful?
            {
                PrintBytes( nciReplyMessage, pNCIMessage->len + readNCIUpToLength );

                ProcessReturnedMessage( pNCIMessage );                                  // Process the read message

            }
        }
    }

}

/*==========================================================================
FUNCTION    PrepareRsp

DESCRIPTION
  Routine to prepare a response for diag.

PARAMETERS
  ftm_nfc_pkt_type *nfc_ftm_pkt - FTM Packet

RETURN VALUE
  void *
==========================================================================*/
void *PrepareRsp( ftm_nfc_pkt_type *nfc_ftm_pkt )
{
    void *response = NULL;
    switch( nfc_ftm_pkt->ftm_nfc_hdr.nfc_cmd_id )
    {
        case FTM_NFC_NFCC_COMMAND:
        {
            ftm_nfc_pkt_type *nfc_nci_rsp = ( ftm_nfc_pkt_type* ) diagpkt_subsys_alloc( DIAG_SUBSYS_FTM,
                                                      FTM_NFC_CMD_CODE,
                                              sizeof( ftm_nfc_pkt_type ) );                                 // get a Response Buffer for NFCC Command

            if( NULL == nfc_nci_rsp )
            {
                LOG_ERROR( "%s: diagpkt_subsys_alloc( DIAG_SUBSYS_FTM, FTM_NFC_CMD_CODE, sizeof( ftm_nfc_pkt_type ) ) returned NULL \n", __func__ );
            }
            else
            {
                nfc_nci_rsp->ftm_nfc_hdr.nfc_cmd_id  = FTM_NFC_NFCC_COMMAND;
                nfc_nci_rsp->ftm_nfc_hdr.nfc_cmd_len = offsetof( ftm_nfc_cmd_header, nfc_cmd_len ) + offsetof( NCI_MESSAGE, buf ) + pNCIMessage->len ;
                nfc_nci_rsp->nfc_nci_pkt_len         = offsetof( NCI_MESSAGE, buf ) + pNCIMessage->len;

                memcpy( nfc_nci_rsp->nci_data,
                        pNCIMessage,
                        nfc_nci_rsp->nfc_nci_pkt_len );

                response = ( void* ) nfc_nci_rsp;
            }
            break;
        }

        case FTM_NFC_REQ_CHIP_TYPE:
        {
            // change from a NCI packet type to a request chip type packet type
            ftm_nfc_chip_type_pkt_type  *nfc_chip_type_rsp = ( ftm_nfc_chip_type_pkt_type* ) diagpkt_subsys_alloc( DIAG_SUBSYS_FTM,
                                                                        FTM_NFC_CMD_CODE,
                                                                sizeof( ftm_nfc_chip_type_pkt_type ) );     // get a Response Buffer for Request Chip Type Command
            if( NULL == nfc_chip_type_rsp )
            {
                LOG_ERROR( "%s: diagpkt_subsys_alloc( DIAG_SUBSYS_FTM, FTM_NFC_CMD_CODE, sizeof( ftm_nfc_chip_type_pkt_type ) ) returned NULL \n", __func__ );
            }
            else
            {
                nfc_chip_type_rsp->nfc_chip_type_cmd_id = FTM_NFC_REQ_CHIP_TYPE;
                nfc_chip_type_rsp->nfc_chip_type_pkt_len = 1;                           // only 1 byte for response packet data
                if( chipType == 1 )                                                     // 1 for QTI, 2 for NQ
                    nfc_chip_type_rsp->nfc_chip_type_pkt_data = FTM_NFC_QTI_CHIP;
                else
                    nfc_chip_type_rsp->nfc_chip_type_pkt_data = FTM_NFC_NQ_CHIP;

                response = ( void* ) nfc_chip_type_rsp;
            }
            break;
        }

        case FTM_NFC_FWPIN_CTRL:
        {
            // change from a NCI packet type to a firmware download packet type
            ftm_nfc_fwdl_pkt_type       *nfc_fwdl_rsp = ( ftm_nfc_fwdl_pkt_type* ) diagpkt_subsys_alloc( DIAG_SUBSYS_FTM,
                                                          FTM_NFC_CMD_CODE,
                                                  sizeof( ftm_nfc_fwdl_pkt_type ) );                        // get a Response Buffer for Firmware Download Pin Command
            if( NULL == nfc_fwdl_rsp )
            {
                LOG_ERROR( "%s: diagpkt_subsys_alloc( DIAG_SUBSYS_FTM, FTM_NFC_CMD_CODE, sizeof( ftm_nfc_fwdl_pkt_type ) ) returned NULL \n", __func__ );
            }
            else
            {
                nfc_fwdl_rsp->nfc_fwdl_cmd_id = FTM_NFC_FWPIN_CTRL;
                nfc_fwdl_rsp->nfc_fwdl_pkt_len = 1;                                     // only 1 byte for response packet data
                nfc_fwdl_rsp->nfc_fwdl_pkt_data = FTM_NFC_FWDL_SUCCESS;                 // 0 for fail, 1 for success

                response = ( void* ) nfc_fwdl_rsp;
            }
            break;
        }

        default :

            LOG_ERROR( "%s: ERROR - SHOULD NOT HAVE ENDED UP HERE: default case \n", __func__ );
            break;
    }

    return response;

}

/*=========================================================================
FUNCTION   ftm_nfc_nq_vs_nxp

DESCRIPTION
  Check whether the chip is an NQ Chip

PARAMETERS
  None

RETURN VALUE
  int

===========================================================================*/
int ftm_nfc_nq_vs_nxp( void )
{
    int         ret = 0;
    uint8_t     coreResetCmd[ ] = { 0x20, 0x00, 0x01, 0x00 };
    uint8_t     coreResetRsp[ ] = { 0x40, 0x00, 0x03, 0x00, 0x11, 0x00 };
    uint8_t     coreInitCmd[  ] = { 0x20, 0x01, 0x00 };

    do
    {
        ret = ProcessCommand( coreResetCmd );           // send a Core Reset CMD

        if( ret == -1 )                                 // wait finished, not signalled?
        {
            LOG_ERROR( "%s: ProcessCommand( coreResetCmd ) error %d \n", __func__, ret );
            break;
        }

        if( memcmp( coreResetRsp, nciReplyMessage, sizeof( coreResetRsp ) ) )
        {                                                // not a good reply?
            coreResetRsp[4] = 0x10;
            if( memcmp( coreResetRsp, nciReplyMessage, sizeof( coreResetRsp ) ) )
            {                                            // check if NCI version is 1.0
                ret = -1;
                LOG_ERROR( "%s: bad reply for coreResetRsp", __func__ );
                break;
            }
        }

        ret = ProcessCommand( coreInitCmd );             // send the message

        if( ret == -1 )                                  // wait finished, not signalled?
        {
            LOG_ERROR( "%s: ProcessCommand( coreInitCmd ) error %d \n", __func__, ret );
            break;
        }

        switch( nciReplyMessage[ CHIP_ID ] )             // what type of chip is it?
        {
        case 0x48:
            whatNQChip = NQ_210;
            LOG_INFORMATION( "Connected to NQ210 \n" );
            break;

        case 0x58:
            whatNQChip = NQ_220;
            LOG_INFORMATION( "Connected to NQ220 \n" );
            break;

        case 0x40:
        case 0x41:
            whatNQChip = NQ_310;
            LOG_INFORMATION( "Connected to NQ310 \n" );
            break;

        case 0x50:
        case 0x51:
            whatNQChip = NQ_330;
            LOG_INFORMATION( "Connected to NQ330 \n" );
            break;

        default:
            whatNQChip = UNKNOWN_NQ_CHIP_TYPE;
            ret     = -1;
            LOG_INFORMATION( "ERROR Connected to an unknown NQ Chip \n" );
            break;
        }
    }while( 0 );

    return ret;
}

/*=========================================================================
FUNCTION   ftm_nfc_set_fwdl_pin

DESCRIPTION
  Sets or resets the firmware download pin high or low

PARAMETERS
  ftm_nfc_pkt_type *nfc_ftm_pkt - FTM Packet

RETURN VALUE
  void

===========================================================================*/
void ftm_nfc_set_fwdl_pin( ftm_nfc_pkt_type *nfc_ftm_pkt )
{
    int                         ret = 0;
    // change from a NCI packet type to a firmware download packet type
    pftm_nfc_fwdl_pkt_type      pnfc_fwdl_pkt = ( pftm_nfc_fwdl_pkt_type ) nfc_ftm_pkt;

    switch ( pnfc_fwdl_pkt->nfc_fwdl_pkt_data )
    {
        case 0:

            ret = ftm_nfc_hw_reset( );                          // Can you reset the hardware?
            if( ret < 0 )                                       // successful?
            {
                LOG_ERROR( "%s: ftm_nfc_hw_reset() failed with ret = %d \n", __func__, ret );
                break;
            }

            LOG_MESSAGE( "%s: Firmware download pin set LOW\n", __func__ );
            break;


        case 1:

            ret = ioctl( fdNfc, NFC_SET_PWR, FIRMWARE_MODE );
            if( ret != 0 )                                      // successful?
            {
                LOG_ERROR( "%s ioctl( fdNfc, NFC_SET_PWR, FIRMWARE_MODE ) returned %d", __func__, ret );
                break;
            }

            LOG_MESSAGE( "%s: Firmware download pin set HIGH\n", __func__ );
            break;

        default :

            LOG_ERROR( "%s: ERROR - SHOULD NOT HAVE ENDED UP HERE: default case \n", __func__ );
            break;
    }

    ret = ftm_nq_nfc_close( );                                  // close the handle
    if( ret != 0 )                                              // not successful?
    {
        LOG_ERROR( "\n\t %s: ftm_nq_nfc_close() failed with ret = %d \n", __func__, ret );
    }

    ret = ftm_nq_nfc_open( );                                   // open the kernel driver
    if( ret < 0 )                                               // successful?
    {
        LOG_ERROR( "\n\t %s: ftm_nq_nfc_open() failed with ret = %d \n", __func__, ret );
    }
}

/*=========================================================================
FUNCTION   ftm_nfc_dispatch_nq

DESCRIPTION
  Dispatches QRCT commands and Chip Replies/Notifications/Data

PARAMETERS
  ftm_nfc_pkt_type *nfc_ftm_pkt - FTM Packet
  uint16            pkt_len     - FTM Packet Length

RETURN VALUE
  void *

===========================================================================*/
void* ftm_nfc_dispatch_nq( ftm_nfc_pkt_type *nfc_ftm_pkt, uint16 pkt_len )
{
    int                 ret = 0;
    int                 len = 0;
    struct timespec     time_sec;
    char               *SkipNQHardwareCheck = NULL;

    void *rsp             = NULL;
    UNUSED_PARAMETER( pkt_len );

    do
    {
        if( !fdNfc )                                               // Already initialized?
        {
            ret = ftm_nq_nfc_open( );                           // open the kernel driver
            if( ret < 0 )                                       // successful?
            {
                LOG_ERROR( "\n\t %s: ftm_nq_nfc_open() failed with ret = %d \n", __func__, ret );
                break;
            }

            ret = ftm_nfc_hw_reset( );                          // Can you reset the hardware?
            if( ret < 0 )                                       // successful?
            {
                LOG_ERROR( "%s: ftm_nfc_hw_reset() failed with ret = %d \n", __func__, ret );
                break;
            }

            pNCIMessage     = ( PNCI_MESSAGE ) nciReplyMessage;

            ret = pthread_create( &clientThread,                // Start the Read Thread
                                   NULL,
                                  &nfc_read_thread,
                                   NULL );
            if( ret != 0 )                                      // successful?
            {
                LOG_MESSAGE( "%s: pthread_create( nfc_read_thread ) failed with ret = %d \n", __func__, ret );
                break;
            }

            SkipNQHardwareCheck = getenv( SKIP_NQ_HARDWARE_CHECK );
            LOG_MESSAGE( "%s: SkipNQHardwareCheck = %s \n", __func__, SkipNQHardwareCheck );

            if(  NULL == SkipNQHardwareCheck )                  // no value so check for NQ Chip?
            {
                ret = ftm_nfc_nq_vs_nxp( );
                if( ret < 0 )                                   // Not an NQ Chip?
                {
                    LOG_ERROR( "ERROR NOT A KNOWN NQ Chip \n" );
                    break;
                }
            }
            else
            {
                LOG_INFORMATION( " Skipping NQ Chip Check \n" );
                whatNQChip = SKIP_CHIP_CHECK;
            }

            LOG_INFORMATION( "FTM for NFC SUCCESSFULLY STARTED \n" );
        }

        if( UNKNOWN_NQ_CHIP_TYPE == whatNQChip )
        {
            LOG_ERROR( "ERROR This version of the chip is not accepted" );
            break;
        }

        if( NULL == nfc_ftm_pkt )                               // valid packet?
        {
            LOG_ERROR( "%s: Error : nfc_ftm_pkt is NULL \n", __func__ );
            break;
        }

        if( offsetof( ftm_nfc_pkt_type, ftm_nfc_hdr ) < MIN_CMD_PKT_LEN )
        {                                                       // packet contains anything?
            LOG_ERROR( "%s: Error : Invalid FTM Packet \n", __func__ );
            break;
        }

        switch( nfc_ftm_pkt->ftm_nfc_hdr.nfc_cmd_id )           // what type of packet is it?
        {

        case FTM_NFC_NFCC_COMMAND:                              // NFC Command?
        case FTM_NFC_SEND_DATA:                                 // NFC Data?

            ret = ProcessCommand( nfc_ftm_pkt->nci_data );
            if( ret == -1 )                                     // wait finished, not signalled?
            {
                LOG_ERROR( "%s: ProcessCommand( nfc_ftm_pkt->nci_data ) error %d \n", __func__, ret );
                break;
            }
            rsp = PrepareRsp( nfc_ftm_pkt );                    // Prepare the response for Diag

            break;

        case FTM_NFC_REQ_CHIP_TYPE:
        case FTM_NFC_FWPIN_CTRL:

            ftm_nfc_set_fwdl_pin( nfc_ftm_pkt );

            rsp = PrepareRsp( nfc_ftm_pkt );                    // Prepare the response for Diag
            break;

        default :
            LOG_ERROR( "%s: ERROR - SHOULD NOT HAVE ENDED UP HERE: default case \n", __func__ );
            break;

        }
    } while( 0 );

    return rsp;
}
