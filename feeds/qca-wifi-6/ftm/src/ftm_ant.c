/*==========================================================================

         FTM ANT Source File

Description
  FTM platform independent processing of packet data

# Copyright (c) 2010-2012 by Qualcomm Technologies, Inc.  All Rights Reserved.
# Qualcomm Technologies Proprietary and Confidential.

===========================================================================*/

/*===========================================================================

         Edit History


when       who      what, where, why
--------   ---      ----------------------------------------------------------
05/16/12  ankurn    Adding support for ANT commands
11/28/12  c_ssugas  implements efficent method for Ant cmd transfer
                     and implements Rx thread for event handling.
===========================================================================*/
#include "event.h"
#include "msg.h"
#include "log.h"

#include "diag_lsm.h"
#include "diagpkt.h"
#include "diagcmd.h"
#include "diag.h"
#include "termios.h"

#include <errno.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <pthread.h>
#include <stdio.h>
#include <stdbool.h>
#include <dlfcn.h>
#include "bt_vendor_qcom.h"
#include "ftm_ant_common.h"
#include "ftm_bt.h"
#include <string.h>
#include "hidl_client.h"

#ifdef ANDROID
#include <cutils/properties.h>
#endif

#ifdef ANDROID
extern int soc_type;
#endif

#define ANT_CTRL_PACKET_TYPE    0x0c
#define ANT_DATA_PACKET_TYPE    0x0e

#define UNUSED(x) (void)(x)

int init_transport_ant(int on);
// The following functions are dummy implementations of the callbacks required by libbt-vendor.
static void vendor_fwcfg_cb(bt_vendor_op_result_t result) {
    UNUSED(result);
}
static void vendor_scocfg_cb(bt_vendor_op_result_t result) {
    UNUSED(result);
}
static void vendor_lpm_vnd_cb(bt_vendor_op_result_t result) {
    UNUSED(result);
}
static void vendor_audio_state_cb(bt_vendor_op_result_t result) {
    UNUSED(result);
}
static void* vendor_alloc(int size) {
    UNUSED(size);
    return NULL;
}
static void vendor_dealloc(void *p_buf) {
    UNUSED(p_buf);
}
static uint8_t vendor_xmit_cb(uint16_t opcode, void *p_buf, tINT_CMD_CBACK p_cback) {
    UNUSED(opcode);
    UNUSED(p_buf);
    UNUSED(p_cback);
    return 0;
}
static void vendor_epilog_cb(bt_vendor_op_result_t result) {
    UNUSED(result);
}
static void vendor_a2dp_offload_cb(bt_vendor_op_result_t result, bt_vendor_opcode_t op, unsigned char handle) {
    UNUSED(result);
    UNUSED(op);
    UNUSED(handle);
}

// This struct is used to regsiter the dummy callbacks with libbt-vendor
static bt_vendor_interface_t *vendor_interface=NULL;
static const bt_vendor_callbacks_t vendor_callbacks = {
  sizeof(bt_vendor_callbacks_t),
    vendor_fwcfg_cb,
    vendor_scocfg_cb,
    vendor_lpm_vnd_cb,
    vendor_audio_state_cb,
    vendor_alloc,
    vendor_dealloc,
    vendor_xmit_cb,
    vendor_epilog_cb,
    vendor_a2dp_offload_cb
};

/* Transport file descriptor */
int fd_transport_ant_cmd;
extern int first_ant_command;
/* Reader thread handle */
pthread_t ant_cmd_thread_hdl;
/* Pipe file descriptors for cancelling read operation */
int ant_pipefd[2];

/* Enable FTM_DEBUG to turn on Debug messages  */
//#define FTM_DEBUG

/*===========================================================================
FUNCTION   ftm_ant_readerthread

DESCRIPTION
  Thread Routine to perfom asynchrounous handling of events coming on Smd
  descriptor. It invokes a callback to the FTM ANT layer to intiate a request
  to read event bytes.

DEPENDENCIES
  The LifeTime of ReaderThraad is dependent on the status returned by the
  call to ftm_ant_qcomm_handle_event

RETURN VALUE
  RETURN NULL

SIDE EFFECTS
  None

===========================================================================*/
void *ftm_ant_readerthread(void *ptr)
{
   boolean status = FALSE;
   int retval;
   fd_set readfds;
   int buf;

   UNUSED(ptr);
#ifdef FTM_DEBUG
   printf("ftm_ant_readerthread --> \n");
#endif
   do
   {
      FD_ZERO(&readfds);
      FD_SET(fd_transport_ant_cmd, &readfds);
      FD_SET(ant_pipefd[0],&readfds);
      retval = select((fd_transport_ant_cmd>ant_pipefd[0]?fd_transport_ant_cmd
                  :ant_pipefd[0]) + 1, &readfds, NULL, NULL, NULL);
      if(retval == -1)
      {
         printf("select failed\n");
         break;
      }
      if(FD_ISSET(ant_pipefd[0],&readfds))
      {
#ifdef FTM_DEBUG
         printf("Pipe descriptor set\n");
#endif
         read(ant_pipefd[0],&buf,1);
         if(buf == 1)
            break;
      }
      if(FD_ISSET(fd_transport_ant_cmd,&readfds))
      {
#ifdef FTM_DEBUG
         printf("Read descriptor set\n");
#endif
         status = ftm_ant_qcomm_handle_event();
         if(TRUE != status)
            break;
      }
   }
   while(1);
#ifdef FTM_DEBUG
   printf("\nReader thread exited\n");
#endif
   return 0;
}

/*===========================================================================
FUNCTION   ftm_ant_open_channel

DESCRIPTION
   Open the SMD transport associated with ANT

DEPENDENCIES
  NIL

RETURN VALUE
  int value indicating success or failure

SIDE EFFECTS
  NONE

===========================================================================*/
static bool ftm_ant_open_channel()
{
   struct termios term_port;
   int opts;

   printf("%s: \n",__func__ );
   switch (soc_type)
   {
       case BT_SOC_ROME:
       case BT_SOC_CHEROKEE:
       case BT_SOC_NAPIER:
           //Use hidl_client_initialize for chip initialization
           if (hidl_client_initialize(MODE_ANT,&fd_transport_ant_cmd) == false) {
               printf("%s: HIDL client initialization failed, opening port with init_transpor_ant\n", __func__);
               //Use libbt-vendor for chip initialization
               fd_transport_ant_cmd = init_transport_ant(TRUE);
               if (fd_transport_ant_cmd == -1) {
                   printf("%s: ANT Device open Failed, fd:%d: \n", __func__, fd_transport_ant_cmd);
                   return false;
               }
           }
           break;
       case BT_SOC_AR3K:
       case BT_SOC_SMD:
#ifdef FTM_DEBUG
           printf("ftm_ant_open_channel --> \n");
#endif

           fd_transport_ant_cmd = open(APPS_RIVA_ANT_CMD_CH, (O_RDWR));
           if (fd_transport_ant_cmd == -1) {
               printf("Ant Device open Failed= %d\n ", fd_transport_ant_cmd);
               return false;
           }

           // Blocking Read
           opts = fcntl(fd_transport_ant_cmd, F_GETFL);
           if (opts < 0) {
               perror("fcntl(F_GETFL)");
               exit(EXIT_FAILURE);
           }

           opts = opts & (~O_NONBLOCK);
           if (fcntl(fd_transport_ant_cmd, F_SETFL, opts) < 0) {
               perror("fcntl(F_SETFL)");
               exit(EXIT_FAILURE);
           }

           if (tcgetattr(fd_transport_ant_cmd, &term_port) < 0)
               close(fd_transport_ant_cmd);
           cfmakeraw(&term_port);
           if (tcsetattr(fd_transport_ant_cmd, TCSANOW, &term_port) < 0) {
               printf("\n Error while setting attributes\n");
               return false;
           }

           tcflush(fd_transport_ant_cmd, TCIFLUSH);
#ifdef FTM_DEBUG
           printf("ftm_ant_open_channel success \n");
#endif
           break;
       default:
           ALOGE("%s:Unknown soc type.",__func__);
           return false;
   }
   if (pipe(ant_pipefd) == -1)
   {
       printf("pipe create error");
       return STATUS_FAIL;
   }
   /* Creating read thread which listens for various masks & pkt requests */
   pthread_create( &ant_cmd_thread_hdl, NULL, ftm_ant_readerthread, NULL);
   return true;
}

int init_transport_ant(int on) {

    void *so_handle;
    unsigned char bdaddr[] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06};
    int  fd[CH_MAX], powerstate, ret = -1;
    char ref_count[PROPERTY_VALUE_MAX];
    int value;

    if (on) {
        so_handle = dlopen("libbt-vendor.so", RTLD_NOW);
        if (!so_handle)
        {
           ALOGE("Failed to load vendor component");
           return -1;
        }

        vendor_interface = (bt_vendor_interface_t *) dlsym(so_handle, "BLUETOOTH_VENDOR_LIB_INTERFACE");
        if (!vendor_interface)
        {
            ALOGE("Failed to accesst bt vendor interface");
            return -1;
        }

        vendor_interface->init(&vendor_callbacks, bdaddr);

        ALOGI("Turn On BT power");
        powerstate = BT_VND_PWR_ON;
        ret = vendor_interface->op(BT_VND_OP_POWER_CTRL, &powerstate);
        if (ret < 0)
        {
            ALOGE("Failed to turn on power from  bt vendor interface");
            return -1;
        }
        for (int i = 0; i < CH_MAX; i++)
            fd[i] = -1;

#ifdef ANDROID
        if (soc_type == BT_SOC_ROME || soc_type == BT_SOC_CHEROKEE || soc_type == BT_SOC_NAPIER) {
            /*call ANT_USERIAL_OPEN to get ANT handle*/
            ret = vendor_interface->op((bt_vendor_opcode_t)BT_VND_OP_ANT_USERIAL_OPEN, fd);
    }
#else
#ifdef BT_SOC_TYPE_ROME
        /*call ANT_USERIAL_OPEN to get ANT handle*/
        ret = vendor_interface->op((bt_vendor_opcode_t)BT_VND_OP_ANT_USERIAL_OPEN, fd);
#endif
#endif
        ALOGE("ret value: %d", ret);
        if (ret != 1)
        {
            ALOGE("Failed to get fd from  bt vendor interface");
            return -1;
        } else {
            ALOGE("FD: %x", fd[0]);
            return fd[0];
        }
    } else {
        if (vendor_interface) {
            ALOGE("Close and cleanup the interfaces");

#ifdef ANDROID
            if (soc_type == BT_SOC_ROME || soc_type == BT_SOC_CHEROKEE || soc_type == BT_SOC_NAPIER) {
                int ret = vendor_interface->op((bt_vendor_opcode_t)BT_VND_OP_ANT_USERIAL_CLOSE, NULL);
            }
#else
#ifdef BT_SOC_TYPE_ROME
            int ret = vendor_interface->op((bt_vendor_opcode_t)BT_VND_OP_ANT_USERIAL_CLOSE, NULL);
#endif
#endif

            ALOGE("ret value: %d", ret);
            ALOGI("Turn off BT power");
            powerstate = BT_VND_PWR_OFF;
            ret = vendor_interface->op(BT_VND_OP_POWER_CTRL, &powerstate);
            if (ret < 0)
            {
                ALOGE("Failed to turn off power from  bt vendor interface");
                return -1;
            }
            vendor_interface->cleanup();
            vendor_interface = NULL;
            return 0;
        } else {

            ALOGE("Not able to find vendor interface handle");
            return -1;
        }
    }
}

/*===========================================================================
FUNCTION   ftm_log_send_msg

DESCRIPTION
  Processes the buffer sent and sends it to the libdiag for sending the Cmd
  response

DEPENDENCIES
  NIL

RETURN VALUE
  NIL

SIDE EFFECTS
  None

===========================================================================*/
void ftm_ant_log_send_msg(const uint8 *pEventBuf,int event_bytes)
{
   int result = log_status(LOG_FTM_VER_2_C);
   ftm_ant_log_pkt_type*  ftm_ant_log_pkt_ptr = NULL;

   if((pEventBuf == NULL) || (event_bytes == 0))
      return;
#ifdef FTM_DEBUG
   printf("ftm_ant_log_send_msg --> \n");
#endif
   if(result == 1)
   {
      ftm_ant_log_pkt_ptr = (ftm_ant_log_pkt_type *)log_alloc(LOG_FTM_VER_2_C,
      FTM_ANT_LOG_HEADER_SIZE + (event_bytes-1));
      if(ftm_ant_log_pkt_ptr != NULL)
      {
         /* FTM ANT Log PacketID */
         ftm_ant_log_pkt_ptr->ftm_log_id = FTM_ANT_LOG_PKT_ID;
         memcpy((void *)ftm_ant_log_pkt_ptr->data,(void *)pEventBuf,event_bytes);
         log_commit( ftm_ant_log_pkt_ptr );
      }
   }
}

/*===========================================================================
FUNCTION   ftm_ant_dispatch

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
void * ftm_ant_dispatch(ftm_ant_pkt_type *ant_ftm_pkt, uint16 pkt_len)
{
   ftm_ant_generic_sudo_res *rsp;
   int err = 0, i;
   int data_len = ant_ftm_pkt->cmd_data_len;
   bool resp = false;
   unsigned char *pdata = NULL, *ptemp;
#ifdef FTM_DEBUG
   printf("ftm_ant_dispatch --> \n");
#endif

   UNUSED(pkt_len);

   if (first_ant_command == 0) {
       first_ant_command = 1;
       ftm_ant_open_channel();
   }

   rsp = (ftm_ant_generic_sudo_res*)diagpkt_subsys_alloc( DIAG_SUBSYS_FTM
                                                , FTM_ANT_CMD_CODE
                                                , sizeof(ftm_ant_generic_sudo_res)
                                                );
   if(rsp ==  NULL)
   {
       printf("%s Failed to allocate resource",__func__);
       return NULL;
   }

   switch (soc_type) {
      //Rome shares the same UART transport for ANT and BT. Hence, to differenciate the
      //packets by controller, adding one extra byte for ANT data and control packets
       case BT_SOC_ROME:
       case BT_SOC_CHEROKEE:
       case BT_SOC_NAPIER:
           data_len = data_len + 1;
           pdata = (unsigned char *) malloc(data_len);
           if (pdata == NULL) {
               ALOGE("Failed to allocate the memory for ANT command packet");
               rsp->result = FTM_ANT_FAIL;
               return (void *) rsp;
           }
           //To be compatible with Legacy, SMD based PLs, send all the packets
           //with cmd opcode 0x0c
           pdata[0] = 0x0c;
           memcpy(pdata+1, ant_ftm_pkt->data, data_len-1);
           err = write(fd_transport_ant_cmd, pdata, data_len);
           ptemp = pdata;
           break;
       case BT_SOC_AR3K:
       case BT_SOC_SMD:
           /* Send the packet to controller and send a dummy response back to host*/
           err = write(fd_transport_ant_cmd, ant_ftm_pkt->data, data_len);
           ptemp = ant_ftm_pkt->data;
           break;
       default:
           ALOGE("%s:Unknown soc type", __func__);
           break;
   }
   if (err == data_len) {
       rsp->result = FTM_ANT_SUCCESS;
       printf("ANT CMD: ");
       for (i = 1; i<data_len; i++) {
           printf("%02X ", ptemp[i]);
       }
       printf("\n");
   } else {
       rsp->result = FTM_ANT_FAIL;
       printf("FTM ANT write fail len: %d\n", err);
   }
   if (pdata)
     free(pdata);
   return (void *)rsp;
}

/*===========================================================================
FUNCTION   ftm_bt_hci_qcomm_handle_event

DESCRIPTION
 Routine called by the HAL layer reader thread to process the HCI events
 The post conditions of each event is covered in a state machine pattern

DEPENDENCIES
  NIL

RETURN VALUE
  RETURN VALUE
  FALSE = failure, else TRUE

SIDE EFFECTS
  None

===========================================================================*/

boolean ftm_ant_qcomm_handle_event ()
{
   boolean status = TRUE;
   int nbytes,i,len =0;
   int event_type;
   ftm_ant_generic_res *res = (ftm_ant_generic_res *)diagpkt_subsys_alloc(
                                   DIAG_SUBSYS_FTM
                                   , FTM_ANT_CMD_CODE
                                   , sizeof(ftm_ant_generic_res)
                               );
   if(res ==  NULL)
    {
       printf("%s Failed to allocate res",__func__);
       tcflush(fd_transport_ant_cmd, TCIFLUSH);
       return FALSE;
    }
#ifdef FTM_DEBUG
   printf("ftm_ant_hci_qcomm_handle_event --> \n");
#endif

   /* Read length and event type  of Ant Resp event*/
   nbytes = read(fd_transport_ant_cmd, (void *)res->evt, 2);
   if(nbytes <= 0) {
      status = FALSE;
      printf("ftm_ant_qcomm_handle_event read fail len=%d\n", nbytes);
      return status;
   }
   event_type = res->evt[0];
   len = res->evt[1];
#ifdef FTM_DEBUG
   printf(" event type  =%d\n",event_type);
   printf("length of event =%d\n",len);
#endif
   /* Read out the Ant Resp event*/
   if (len <= (int)sizeof(res->evt))
   {
      nbytes = read(fd_transport_ant_cmd, (void *)res->evt, len);
      if (nbytes != len) {
          res->result = FTM_ANT_FAIL;
          status = FALSE;
          printf("ftm_ant_qcomm_handle_event read fail len=%d\n", nbytes);
      }
      else  {
          res->result = FTM_ANT_SUCCESS;
          printf("ANT EVT: ");
          for (i=0; i<nbytes; i++) {
            printf("%02X ", res->evt[i]);
          }
          printf("\n");
          ftm_ant_log_send_msg(res->evt, nbytes);
          tcflush(fd_transport_ant_cmd, TCIOFLUSH);
      }
   }
   else
   {
       res->result = FTM_ANT_FAIL;
       status = FALSE;
       printf("ftm_ant_qcomm_handle_event read fail len=%d is more than sizeof(res->evt)=%d\n", len, (int)sizeof(res->evt));
   }
   return status;
}
