/*==========================================================================

                     FTM Platform specfic HCI UART/SMD File

Description
  Platform specific routines to program the UART/SMD descriptors

# Copyright (c) 2010-2011, 2013 by Qualcomm Technologies, Inc.  All Rights Reserved.
# Qualcomm Technologies Proprietary and Confidential.

===========================================================================*/

/*===========================================================================

                         Edit History


when       who     what, where, why
--------   ---     ----------------------------------------------------------
06/07/11   bneti       Add support smd support for msm8960
06/18/10   rakeshk  Created a source file to implement platform specific
                    routines for UART
07/07/10   rakeshk  Removed the conversion of 3.2 Mbps baud rate
01/07/10   rakeshk  Added support for verbose logging of Cmd and events
===========================================================================*/

#include <errno.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <sys/select.h>
#include <termios.h>
#include <pthread.h>
#include <stdio.h>
#include <dlfcn.h>
#include "bt_vendor_lib.h"
#include "ftm_bt_hci_pfal.h"
#include "ftm_common.h"
#include <string.h>
#include "log.h"
#include <cutils/properties.h>
#include "hidl_client.h"

#ifdef ANDROID
#define VENDOR_LIB "libbt-vendor.so"
#else
#define VENDOR_LIB "libbt-vendor.so.0"
#endif

uint8_t is_slim_bus_test = 0;
#define UNUSED(x) (void)(x)

/*identify the transport type*/
static char *transport_dev;

typedef enum {
    BT_SOC_DEFAULT = 0,
    BT_SOC_SMD = BT_SOC_DEFAULT,
    BT_SOC_AR3K,
    BT_SOC_ROME,
    BT_SOC_CHEROKEE,
    BT_SOC_NAPIER,
    /* Add chipset type here */
    BT_SOC_RESERVED
} bt_soc_type;

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


bt_vendor_interface_t *vendor_interface=NULL;
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


/*BT HS UART TTY DEVICE */
#define BT_HS_UART_DEVICE "/dev/ttyHS0"

/*BT RIVA-SMD CHANNELS */
#define APPS_RIVA_BT_ACL_CH  "/dev/smd2"
#define APPS_RIVA_BT_CMD_CH  "/dev/smd3"

/* Variables to identify the platform */
char transport_type[PROPERTY_VALUE_MAX];
static boolean is_transportSMD;

extern int soc_type;

/* Reader thread handle */
pthread_t hci_cmd_thread_hdl;
/* Pipe file descriptors for cancelling read operation */
int pipefd[2];
/* Transport file descriptor */
int fd_transport;
/* Starting baud rate to init the tty device */
int starting_baud = 115200;
/* Verbose output monitoring variable */
int verbose = 1;
/* Defintion to convert integer baud rate to the
 * Data type understood by tty device
 */
#define BAUDCLAUS(i) case (i): return ( B##i )

/*===========================================================================
FUNCTION   convert_baud

DESCRIPTION
  Routine to convert the integer baud rate to type speed_t

DEPENDENCIES
  NIL

RETURN VALUE
  RETURN VALUE
  Converted Baud rate, else default 0

SIDE EFFECTS
  None

===========================================================================*/
static speed_t convert_baud(uint32 baud_rate)
{
  switch (baud_rate)
  {
    BAUDCLAUS(50);
    BAUDCLAUS(75);
    BAUDCLAUS(110);
    BAUDCLAUS(134);
    BAUDCLAUS(150);
    BAUDCLAUS(200);
    BAUDCLAUS(300);
    BAUDCLAUS(600);
    BAUDCLAUS(1200);
    BAUDCLAUS(1800);
    BAUDCLAUS(2400);
    BAUDCLAUS(4800);
    BAUDCLAUS(9600);
    BAUDCLAUS(19200);
    BAUDCLAUS(38400);
    BAUDCLAUS(57600);
    BAUDCLAUS(115200);
    BAUDCLAUS(230400);
    BAUDCLAUS(460800);
    BAUDCLAUS(500000);
    BAUDCLAUS(576000);
    BAUDCLAUS(921600);
    BAUDCLAUS(1000000);
    BAUDCLAUS(1152000);
    BAUDCLAUS(1500000);
    BAUDCLAUS(2000000);
    BAUDCLAUS(2500000);
    BAUDCLAUS(3000000);
    BAUDCLAUS(3500000);
    BAUDCLAUS(4000000);

    default: return 0;
  }
}

/*===========================================================================
FUNCTION   ftm_readerthread

DESCRIPTION
  Thread Routine to perfom asynchrounous handling of events coming on Uart/Smd
  descriptor. It invokes a callback to the FTM BT layer to intiate a request
  to read event bytes.

DEPENDENCIES
  The LifeTime of ReaderThraad is dependent on the status returned by the
  call to ftm_bt_hci_qcomm_handle_event

RETURN VALUE
  RETURN NIL

SIDE EFFECTS
  None

===========================================================================*/
void *ftm_readerthread(void *ptr)
{
  UNUSED(ptr);
  boolean status = FALSE;
  int retval;
  fd_set readfds;
  int buf;

  do
  {
    FD_ZERO(&readfds);
    FD_SET(fd_transport, &readfds);
    FD_SET(pipefd[0],&readfds);
    retval = select((pipefd[0] > fd_transport? pipefd[0] : fd_transport) + 1,
                   &readfds, NULL, NULL, NULL);
    if(retval == -1)
    {
      printf("select failed\n");
      break;
    }
    if(FD_ISSET(pipefd[0],&readfds))
    {
#ifdef FTM_DEBUG
       printf("Pipe descriptor set\n");
#endif
       read(pipefd[0],&buf,1);
       if(buf == 1)
         break;
    }
    if(FD_ISSET(fd_transport,&readfds))
    {
#ifdef FTM_DEBUG
      printf("Read descriptor set\n");
#endif
      status = ftm_bt_hci_qcomm_handle_event();
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
FUNCTION   ftm_bt_pfal_set_transport

DESCRIPTION
 sets the type of transport based on the msm type

DEPENDENCIES
  NIL

RETURN VALUE
returns the type of transport
SIDE EFFECTS
  None

===========================================================================*/
boolean ftm_bt_hci_pfal_set_transport(void)
{
    if (soc_type == BT_SOC_ROME || soc_type == BT_SOC_CHEROKEE || soc_type == BT_SOC_NAPIER) {
        strlcpy(transport_type, "uart", sizeof(transport_type));
        printf("[%s]: Transport type is: %s\n", __FUNCTION__, transport_type);
        is_transportSMD = 0;
        transport_dev = BT_HS_UART_DEVICE;
    } else {
        strlcpy(transport_type, "smd", sizeof(transport_type));
        printf("[%s]: Transport type is: %s\n", __FUNCTION__, transport_type);
        is_transportSMD = 1;
        transport_dev = APPS_RIVA_BT_CMD_CH;
    }
    return is_transportSMD;
}


int init_transport_bdroid(boolean on) {

    void *so_handle;
    unsigned char bdaddr[] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06};
    request_status st;
    int  fd[CH_MAX], powerstate, ret;

    if (on) {
        so_handle = dlopen(VENDOR_LIB, RTLD_NOW);
        if (!so_handle)
        {
           ALOGE("Failed to load vendor component %s", dlerror());
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
        ret = vendor_interface->op(BT_VND_OP_USERIAL_OPEN, fd);
        ALOGE("ret value: %d", ret);
        /* This is just a hack; needs to be removed */
        ret = 1;
        ALOGE("setting ret value to 1 manually");
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
            int ret = vendor_interface->op(BT_VND_OP_USERIAL_CLOSE, NULL);

            ALOGE("ret value: %d", ret);
            vendor_interface->cleanup();
            return 0;
        } else {

            ALOGE("Not able to find vendor interface handle");
            return -1;
        }
    }
}

/*===========================================================================
FUNCTION   ftm_bt_hci_pfal_deinit_transport

DESCRIPTION
  Platform specific routine to de-intialise the UART/SMD resource.

PLATFORM SPECIFIC DESCRIPTION
  Closes the TTY/SMD file descriptor and sets the descriptor value to -1

DEPENDENCIES
  NIL

RETURN VALUE
  RETURN VALUE
  STATUS_SUCCESS if SUCCESS, else other reasons

SIDE EFFECTS
  The Close of the descriptor will trigger a failure in the Reader Thread
  and hence cause a Deinit of the ReaderThread

===========================================================================*/
request_status ftm_bt_hci_pfal_deinit_transport()
{
  int buf = 1;
  write(pipefd[1],&buf,1);
  if(!isLatestTarget())
  {
    close(fd_transport);
    fd_transport = -1;
  }
  else
  {
    //Use libbt-vendor for chip de-initialization
    init_transport_bdroid(FALSE);
  }
  return STATUS_SUCCESS;
}

/*===========================================================================
FUNCTION   ftm_bt_hci_pfal_init_uart

DESCRIPTION
  Platform specific routine to intialise the UART/SMD resources.

PLATFORM SPECIFIC DESCRIPTION
  Opens the TTY/SMD device file descriptor, congiures the TTY/SMD device for CTS/RTS
  flow control,sets 115200 for TTY as the default baudrate and starts the Reader
  Thread

DEPENDENCIES
  NIL

RETURN VALUE
  RETURN VALUE
  STATUS_SUCCESS if SUCCESS, else other reasons

SIDE EFFECTS
  None

===========================================================================*/
request_status ftm_bt_hci_pfal_init_transport(int mode)
{
  struct termios   term;
  if(isLatestTarget())
  {
    printf("%s: ",__func__ );
    //Use hidl_client_initialize for chip initialization
    if (hidl_client_initialize(mode, &fd_transport) == false) {
        printf("%s: HIDL client initialization failed \n", __func__);
        return STATUS_NO_RESOURCES;
    }
    printf("%s: , fd:%d: ", __func__, fd_transport);
  }
  else
  {
    fd_transport = open(transport_dev, (O_RDWR | O_NOCTTY));

    if (-1 == fd_transport)
    {
      return STATUS_NO_RESOURCES;
    }

    if (tcflush(fd_transport, TCIOFLUSH) < 0)
    {
      close(fd_transport);
      return STATUS_FAIL;
    }

    if (tcgetattr(fd_transport, &term) < 0)
    {
      close(fd_transport);
      return STATUS_FAIL;
    }

    cfmakeraw(&term);
    /* Set RTS/CTS HW Flow Control*/
    term.c_cflag |= (CRTSCTS | CLOCAL);

    if (tcsetattr(fd_transport, TCSANOW, &term) < 0)
    {
      close(fd_transport);
      return STATUS_FAIL;
    }

    /* Configure the /dev/ttyHS0 device to operate at 115200.
     no need for msm8960 as it is using smd as transport
     */
    if (!is_transportSMD)
       if (ftm_bt_hci_pfal_changebaudrate(starting_baud) == FALSE)
       {
          close(fd_transport);
          return STATUS_FAIL;
       }
  }
  if (pipe(pipefd) == -1)
  {
    printf("pipe create error");
    return STATUS_FAIL;
  }
  if(mode != MODE_FM) {
  /* Creating read thread which listens for various masks & pkt requests */
  pthread_create( &hci_cmd_thread_hdl, NULL, ftm_readerthread, NULL);
  }
  return STATUS_SUCCESS;
}

/*===========================================================================
FUNCTION   ftm_bt_hci_pfal_nwrite

DESCRIPTION
  Platform specific routine to write the data in the argument to the UART/SMD
  port intialised.

PLATFORM SPECIFIC DESCRIPTION
  Write the buffer to the tty device and ensure it is completely written
  In case of short write report error to the BT FTM layer.

DEPENDENCIES
  NIL

RETURN VALUE
  RETURN VALUE
  STATUS_SUCCESS if SUCCESS, else other reasons

SIDE EFFECTS
  None

===========================================================================*/
request_status ftm_bt_hci_pfal_nwrite(uint8 *buf, int size)
{
  int tx_bytes = 0, nwrite;
  int i = 0, buf_size = size;
  uint8 loop_back_cmd[6] = {0x1, 0x02, 0x18, 0x01, 0x01};
  /*hci packet is not required to carry the Packet indicator (for UART interfaces) for msm8960
     as it is using share memory interface */
  int hci_uart_pkt_ind = 0;

  if(fd_transport < 0)
    return STATUS_NO_RESOURCES;
  if ( buf[PIN_CON_CMD_OGF_BIT] == PIN_CON_CMD_OGF &&
       buf[PIN_CON_CMD_OCF_BIT] == PIN_CON_CMD_OCF &&
      (size > PIN_CON_CMD_SUBOP_BIT) &&
       buf[PIN_CON_CMD_SUBOP_BIT] == PIN_CON_CMD_SUB_OP &&
      (size > PIN_CON_CMD_INTER_BIT) &&
       buf[PIN_CON_CMD_INTER_BIT] == PIN_CON_INTERFACE_ID)
  {
     is_slim_bus_test = 1;
     printf("\nPinConnectivityTest: Sending loopback command to SOC before initiasing slimbus\n");
     strlcpy(buf, loop_back_cmd, size);
  }
  do
  {
    nwrite = write(fd_transport, (buf + hci_uart_pkt_ind + tx_bytes), (size - hci_uart_pkt_ind - tx_bytes));

    if (nwrite < 0)
    {
      printf("Error while writing ->\n");
      return STATUS_SHORT_WRITE;
    }
    if (nwrite == 0)
    {
      printf("ftm_bt_hci_pfal_nwrite: zero-length write\n");
      return STATUS_SHORT_WRITE;
    }

    tx_bytes += nwrite;
    size     -= nwrite;
  } while (tx_bytes < size - hci_uart_pkt_ind);

  if (verbose == 1)
  {
    printf("[%s] %s: CMD:", get_current_time(), __FUNCTION__);
    for (i = 0; i < buf_size; i++)
    {
      printf(" %02X", buf[i]);
    }
    printf("\n");
  }

  return STATUS_SUCCESS;
}

/*===========================================================================
FUNCTION   ftm_bt_hci_pfal_nread

DESCRIPTION
  Platform specific routine to read data from the UART/SMD port intialised into
  the buffer passed in argument.

PLATFORM SPECIFIC DESCRIPTION
  Read from the tty device into the buffer and ensure  the read request is
  completed, in case of short read report error to the BT FTM layer.


DEPENDENCIES
  NIL

RETURN VALUE
  RETURN VALUE
  STATUS_SUCCESS if SUCCESS, else other reasons

SIDE EFFECTS
  None

===========================================================================*/
request_status ftm_bt_hci_pfal_nread(uint8 *buf, int size)
{
  int rx_bytes = 0, nread;

  if(fd_transport < 0)
    return STATUS_NO_RESOURCES;

  do
  {
    nread = read(fd_transport, (buf + rx_bytes), (size - rx_bytes));
    if (nread < 0)
    {
      printf("Error while reading ->\n");
      return STATUS_SHORT_READ;
    }

    rx_bytes += nread;

  } while (rx_bytes < size);

  return STATUS_SUCCESS;
}

/*===========================================================================
FUNCTION   ftm_bt_hci_pfal_changebaudrate

DESCRIPTION
  Platform specific routine to intiate a change in baud rate

PLATFORM SPECIFIC DESCRIPTION
  Convert the Baud rate passed to the speed_t type and program the
  Baud rate change after ensuring all transmit is drained at the
  current baud rate

DEPENDENCIES
  It is expected that the Upper layer will intiate a Flow Off to the
  BT SoC, to signal the stop of receive if the baud rate change is
  initiated while SoC init is in progress

RETURN VALUE
  RETURN VALUE
  TRUE if SUCCESS, else FALSE

SIDE EFFECTS
  None

===========================================================================*/
boolean ftm_bt_hci_pfal_changebaudrate (uint32 new_baud)
{
  struct termios term;
  boolean status = TRUE;
  speed_t baud_code;
  speed_t actual_baud_code;

  if (tcgetattr(fd_transport, &term) < 0)
  {
    printf("Can't get port settings\n");
    status = FALSE;
  }
  else
  {
    baud_code = convert_baud(new_baud);
    (void) cfsetospeed(&term, baud_code);
    if (tcsetattr(fd_transport, TCSADRAIN, &term) < 0) /* don't change speed until last write done */
    {
      printf("bt_hci_qcomm_pfal_changebaudrate: tcsetattr:\n");
      status = FALSE;
    }
    /* make sure that we reportedly got the speed we tried to set */
    if (1 < verbose)
    {
      if (tcgetattr(fd_transport, &term) < 0)
      {
        printf("bt_hci_qcomm_pfal_changebaudrate: tcgetattr:\n");
        status = FALSE;
      }
      if (baud_code != (actual_baud_code = cfgetospeed(&term)))
      {
        printf("bt_hci_qcomm_pfal_changebaudrate: new baud %u FAILED, got 0x%x\n", new_baud, actual_baud_code);
      }
      else
      {
        printf("bt_hci_qcomm_pfal_changebaudrate: new baud %u SUCCESS, got 0x%x\n", new_baud, actual_baud_code);
      }
    }
  }

  return status;
}
