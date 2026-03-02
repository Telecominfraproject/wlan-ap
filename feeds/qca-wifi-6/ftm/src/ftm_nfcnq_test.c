/*
 * Copyright (c) 2017 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 */

#include <libgen.h>
#include "ftm_nfcnq.h"
#include "ftm_nfcnq_test.h"

/* Global variables */
pthread_t           clientThread;
PNCI_MESSAGE        pNCIMessage;
char                *progname;

/*==============================================================================
FUNCTION
    eseSpiTest

DESCRIPTION
    Send APDU for eSE SPI HLOS test

PARAMETERS
    int argc - argument count
    char **argv - argument vector

RETURN VALUE
    void

=============================================================================*/
void eseSpiTest(int argc, char **argv )
{
    int ret = 0;
    int test_mode = 0;
    unsigned char i = 0;
    int fp = 0;
    int choice = 0;
    unsigned char send_APDU[] = {0x5A,0x00,0x05,0x00,0xA4,0x04,0x00,0x00,0xA5};
    int size_APDU = 0;
    unsigned char recv_response[259] = {0};
    progname = basename(argv[2]);
    test_mode = getopt(argc, argv, "01");
    size_APDU = sizeof(send_APDU);

    LOG_INFORMATION("\n### eSE SPI test ###\n");

    if(test_mode == '0')
    {
        choice = 0;
        LOG_INFORMATION("\nInterrupt Mode test\n");
    }
    else
    {
        choice = 1;
        LOG_INFORMATION("\nPoll Mode test(default)\n");
    }

    do
    {
        //open module
        if ((ret = (fp = open("/dev/ese", O_RDWR))) < 0)
        {
            LOG_INFORMATION("eSE open error retcode = %d, errno = %d\n", ret, errno);
            LOG_INFORMATION("\n... eSE SPI Test requires modified boot and TZ image ...");
            break;
        }
        LOG_INFORMATION("eSE open : Ret = %2d\n", ret);

        //enable the logs
        ioctl(fp, ESE_SET_DBG, 1);
        //hardware reset
        ioctl(fp, ESE_SET_PWR, 1);

        ioctl(fp, ESE_SET_MODE, choice);

        //write one APDU
        ret = write(fp, send_APDU, sizeof(send_APDU));
        if (ret < 0)
        {
            LOG_INFORMATION("ese write error retcode = %d, errno = %d\n", ret, errno);
            break;
        }
        LOG_INFORMATION("ese Write : Ret = %.2X \n", ret);
        LOG_INFORMATION("APDU sent to eSE: ");
        for (i=0; i<size_APDU; i++)
        {
            LOG_INFORMATION("%.2X ", send_APDU[i]);
        }
        sleep(1);

        if ((ret = (read(fp, &recv_response[0], READ_SAMPLE_SIZE)), 0) < 0)
        {
            LOG_INFORMATION("\neSE read error retcode = %d, errno = %d", ret, errno);
        }
        else
        {
            LOG_INFORMATION("\nResponse from eSE: ");
            for (i=0;i<(recv_response[2]+1);i++)
            {
              LOG_INFORMATION("%.2X ", recv_response[i]);
            }
            LOG_INFORMATION("\n");
        }
    } while(0);

    close(fp);
}

/*==============================================================================
FUNCTION
    eseDwpTest

DESCRIPTION
    Send NCI commands to NFCC for eSE DWP detection

PARAMETERS

RETURN VALUE
    void

=============================================================================*/
void eseDwpTest()
{
    int cmds;
    cmds = sizeof(NQ330_ESE_DWP) / sizeof(NQ330_ESE_DWP[0]);
    LOG_INFORMATION( "\n### ese DWP Test ###\n" );
    if(whatNQChip == NQ_220 || whatNQChip == NQ_330)
        sendcmds(NQ330_ESE_DWP, cmds);
    else
        LOG_INFORMATION( "\nNQ Chipset in use doesn't support eSE\n" );
}

/*==============================================================================
FUNCTION
    printTecnologyDetails

DESCRIPTION
    Print the technology supported and protocols details

PARAMETERS
    char technology - technology supported identifier
    char protocol - protocol identifier

RETURN VALUE
    void

=============================================================================*/
void printTecnologyDetails(char technology, char protocol)
{
    switch (protocol)
    {
      case NFC_PROTOCOL_ISO_DEP:
          LOG_INFORMATION( "ISO-DEP Protocol");
          break;

      case NFC_PROTOCOL_NFC_DEP:
          LOG_INFORMATION( "NFC-DEP Protocol");
          break;

      case NFC_PROTOCOL_T1T:
          LOG_INFORMATION( "T1T Protocol");
          break;

      case NFC_PROTOCOL_T2T:
          LOG_INFORMATION( "T2T Protocol");
          break;

      case NFC_PROTOCOL_T3T:
          LOG_INFORMATION( "T3T Protocol");
          break;

      case NFC_PROTOCOL_UNKNOWN:
          LOG_INFORMATION( "unknown Protocol");
          break;

      default:
          break;
    }

    switch (technology)
    {
      case NFC_NFCA_Poll:
          LOG_INFORMATION("\nNFC A POLL MODE TECHNOLOGY\n");
          break;
      case NFC_NFCB_Poll:
          LOG_INFORMATION("\nNFC B POLL MODE TECHNOLOGY\n");
          break;
      case NFC_NFCF_Poll:
          LOG_INFORMATION("\nNFC F POLL MODE TECHNOLOGY\n");
          break;
      case NFC_NFCA_Listen:
          LOG_INFORMATION("\nNFC A LISTEN MODE TECHNOLOGY\n");
          break;
      case NFC_NFCB_Listen:
          LOG_INFORMATION("\nNFC B LISTEN MODE TECHNOLOGY\n");
          break;
      case NFC_NFCF_Listen:
          LOG_INFORMATION("\nNFC F LISTEN MODE TECHNOLOGY\n");
          break;
      case NFC_NFCISO15693_Poll:
          LOG_INFORMATION("\nNFC ISO15693 POLL MODE TECHNOLOGY\n");
          break;
      default:
          LOG_INFORMATION("\nother TECHNOLOGY\n");
          break;
    }

}

/*==============================================================================
FUNCTION
    sendcmds

DESCRIPTION
    Send sequence of commands to NFCC

PARAMETERS
    uint8_t buffer[] - Command buffer array
    int no_of_cmds - Number of commands to be sent

RETURN VALUE
    void

=============================================================================*/
void sendcmds(uint8_t buffer[][MAX_CMD_LEN], int no_of_cmds)
{
    int rows=0,payloadlen=0;
    int ret = 0;
    ftm_nfc_pkt_type *nfc_pkt  = (ftm_nfc_pkt_type *)malloc(no_of_cmds*255);

    LOG_INFORMATION("\nTotal cmds to be sent = %d\n",no_of_cmds);
    LOG_INFORMATION("Wait for Commands to be sent... \n\n");

    for(rows = 0; rows < no_of_cmds; rows++)
    {
#ifdef NFC_FTM_DEBUG
        LOG_INFORMATION ("Number of cmds sent = %d \n",rows+1);
#endif
        payloadlen = 0;
        payloadlen = 3 + buffer[rows][2];
        memset(nfc_pkt->nci_data, -1, MAX_CMD_LEN);
        memcpy(nfc_pkt->nci_data, &buffer[rows], payloadlen);
        ret = ProcessCommand( nfc_pkt->nci_data );
        if( ret == -1 )                                  // wait finished, not signalled?
        {
            LOG_ERROR( "Waited for NCI NTF/DATA timeout\n" );
        }
    }
}

/*==============================================================================
FUNCTION
    usage

DESCRIPTION
    Print usage information for test

PARAMETERS

RETURN VALUE
    void

=============================================================================*/
void usage()
{
    LOG_INFORMATION("\nUsage:");
    LOG_INFORMATION(" %s [-n] [-e] [-d] [h] \n", progname);
    LOG_INFORMATION(" %s -n ..for NFC test only\n", progname);
    LOG_INFORMATION(" %s -e ..for eSE SPI test only\n \t-0 ..Interrupt Mode\n \t-1 ..Poll Mode\n", progname);
    LOG_INFORMATION(" %s -d ..for eSE DWP test only\n", progname);
    LOG_INFORMATION(" %s -h HELP\n", progname);
    LOG_INFORMATION(" %s default NFC test only\n", progname);
}

/*==============================================================================
FUNCTION
    nfc_ese_pwr

DESCRIPTION
    Set ESE power using NFC driver

PARAMETERS

RETURN VALUE
    void

=============================================================================*/
void nfc_ese_pwr()
{
    int ret;
    ret = ioctl( fdNfc, NFC_ESE_SET_PWR, POWER_ON );                // turn the chip on
    if( ret != 0 )
    {
        LOG_INFORMATION("Can't find ESE GPIO in NFC driver: ");
        LOG_INFORMATION("ret=%d\n",ret);
    }
}



/*==============================================================================
FUNCTION
    ftm_nfc_dispatch_nq_test

DESCRIPTION
    called by main() in ftm_main.c to start the nfc test routine

PARAMETERS
    int argc - argument count
    char **argv - argument vector

RETURN VALUE
    void

=============================================================================*/
void ftm_nfc_dispatch_nq_test( int argc, char **argv )
{
    int cmds = 0;
    unsigned int chip_version = 0x00;
    unsigned int major_version = 0x00;
    unsigned int minor_version = 0x00;
    unsigned int rom_version = 0x00;
    char firmware_version[10];
    struct timespec  time_sec;
    int type_of_test = 0;
    int default_test = 0;
    int          status                 = 0;

    union        nqx_uinfo           nqx_info;
    pthread_t                       readerThread;

    do
    {
        if( !fdNfc )
        {
            status = ftm_nq_nfc_open( );                                                // get a handle to the kernel driver
            if( status < 0 )
            {
                LOG_ERROR( "\n%s: ftm_nq_nfc_open() failed with status = %d \n", __FUNCTION__, status );
                break;
            }

            status = ftm_nfc_hw_reset( );                                               // reset NFC hardware
            if( status < 0 )
            {
                LOG_ERROR( "%s: ftm_nq_nfc_reset() failed with status = %d \n", __FUNCTION__, status );
                break;
            }

            nqx_info.i = ioctl( fdNfc, NFCC_GET_INFO, 0 );
            if( nqx_info.i < 0 )
            {
                LOG_ERROR( "%s: nqnfcinfo not enabled, info  = %d \n", __FUNCTION__, nqx_info.i );
            }

            chip_version = nqx_info.info.chip_type;
            rom_version = nqx_info.info.rom_version;
            major_version = nqx_info.info.fw_major;
            minor_version = nqx_info.info.fw_minor;

            LOG_INFORMATION( "\n NQ Chip ID : %x\n", chip_version);
            snprintf(firmware_version, 10, "%02x.%02x.%02x", rom_version, major_version, minor_version);
            LOG_INFORMATION(" Firmware version : %s\n\n", firmware_version);


            if(sem_init(&sRspReady, 0, 0) != 0)
            {
                    LOG_ERROR("NFC FTM :semaphore_halcmd_complete creation failed \n");
                    break;
            }
            if(sem_init(&sRfNtf, 0, 0) != 0)
            {
                    LOG_ERROR("NFC FTM :semaphore_halcmd_complete creation failed \n");
                    break;
            }

            pNCIMessage     = ( PNCI_MESSAGE ) nciReplyMessage;
            status = pthread_create( &clientThread, NULL, &nfc_read_thread, NULL );            // Start the Read Thread

            if( status != 0 )                                      // successful?
            {
                 LOG_ERROR("nqnfc %s: pthread_create( nfc_read_thread ) failed with ret = %d \n", __func__, status );
                 break;
            }

            status = ftm_nfc_nq_vs_nxp( );
            if( status < 0 )                                   // Not an NQ Chip?
            {
                LOG_ERROR("ERROR NOT A KNOWN NQ Chip \n"  );
            }
        }

        progname = basename(argv[1]);
        type_of_test = getopt(argc, argv, "nedhf");

        switch (type_of_test) {
        case 'n':
            LOG_INFORMATION("NFC test only\n");
            break;
        case 'e':
            LOG_INFORMATION("eSE SPI test only\n");
            nfc_ese_pwr();
            ese_spi_test = 1;
            eseSpiTest(argc, argv);
            break;
        case 'd':
            LOG_INFORMATION("eSE DWP test only\n");
            ese_dwp_test = 1;
            eseDwpTest();
            break;
        case 'h':
            usage();
            break;
        default:
            usage();
            default_test = 1;
            LOG_INFORMATION("\nDefault NFC test only\n");
        }

        if(ese_dwp_test || ese_spi_test)
            break;

        if(type_of_test == 'n' || default_test)
        {
            switch(whatNQChip)
            {
                case NQ_210:
                case NQ_220:
                    cmds = sizeof(NQ220_cmds) / sizeof(NQ220_cmds[0]);
                    sendcmds(NQ220_cmds, cmds);
                    break;
                case NQ_310:
                case NQ_330:
                    cmds = sizeof(NQ330_cmds) / sizeof(NQ330_cmds[0]);
                    sendcmds(NQ330_cmds, cmds);
                    break;
                default:
                    LOG_INFORMATION( "Chip not supported, taking NQ330 as default\n ");
                    cmds = sizeof(NQ330_cmds) / sizeof(NQ330_cmds[0]);
                    sendcmds(NQ330_cmds, cmds);
                    break;
            }

            LOG_INFORMATION("\n<<>> Waiting for TAG detect or 20sec timeout <<>> ...\n");
            status = clock_gettime( CLOCK_REALTIME, &time_sec );
            time_sec.tv_sec += NFC_NTF_TIMEOUT;
            status = sem_timedwait( &sRfNtf, &time_sec );  //start waiting
            if (status <0) {
                   LOG_INFORMATION("\n No NFC Tag detected, continue ...\n");
            }
        }

        status = ftm_nq_nfc_close( );                                                   // release the handle to the kernel driver
        if( 0 != status )
        {
            LOG_ERROR( "%s: ftm_nq_nfc_close() failed with status = %d \n", __FUNCTION__, status );
        }

    } while( FALSE );

}
