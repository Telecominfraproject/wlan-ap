/*==========================================================================

                     FTM BT Task Source File

Description
  FTM state machine and platform independent routines for BT

# Copyright (c) 2010-2013 by Qualcomm Technologies, Inc.  All Rights Reserved.
# Qualcomm Technologies Proprietary and Confidential.

===========================================================================*/

/*===========================================================================

                         Edit History


when       who     what, where, why
--------   ---     ----------------------------------------------------------
02/29/12   rrr      Added/Modified LE & BR/EDR power class configuration
09/27/11   bneti    Added packet indicator for hci events for msm8960
09/28/11   rrr      Moved peristent NV item related APIs to CPP,
                    for having BD address being programmed twice if previous
                    BD address was random generated.
06/07/11   bneti    Add support smd support for msm8960
09/03/11   agaja    Added support for NV_READ and NV_WRITE Commands to write
                    onto Persist File system
02/08/11   braghave Reading the HCI commands from binary file
		    for non-Android case
01/19/11   rakeshk  Added the connectivity test implementation
01/07/11   rakeshk  Updated the debug log warnings related to typecasting
07/07/10   rakeshk  Updated the function name of BT power set routine
                    of pointers
06/18/10   rakeshk  Created a source file to implement routines for FTM
                    states and command processing
==========================================================================*/

#include <errno.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <stdio.h>
#include <cutils/properties.h>
#include <pthread.h>
#include "ftm_bt_power_hal.h"
#include "ftm_fm_common.h"
#include "ftm_bt_hci_hal.h"
#include "ftm_common.h"
#include <string.h>
#ifdef BT_NV_SUPPORT
#include "ftm_bt_persist.h"
#endif
#include "hidl_client.h"

#define ARRAY_SIZE(a) (sizeof(a) / sizeof(a[0]))
#define I2C_SLAVE_ADDR 0x0C
#define MAX_PIN_CONFIGS 8

#define NVM_PAYLOAD_MAXLENGTH (1024)
#define MAX_FILE_NAME         (255)
#define LOG_FTM_FM_C  ((uint16) 0x14CC)

extern uint8_t is_slim_bus_test;
int fd_pintest = -1;
FILE *fp;
unsigned char *nvm_cmd;
typedef struct pintest {
  char *gpiostring;
  /* GPIO number */
  int    gpionum;
  /* Pin control register */
  int    pinctrlreg;
  /* Data register */
  int    datareg;
  /* Bit position in Data register */
  int    bitpos;
  /* Direction of the Pin */
  char   direction;
} pintest;

typedef struct platformpintest {
  /* MSM number for pin test*/
  int platform;
  /* Pin test config */
  pintest pinconfig[MAX_PIN_CONFIGS];
  /* Pin direction config register 0 */
  int pinctloe0;
  /* Pin direction config register 1 */
  int pinctloe1;
  /* Data register 0 */
  int pinctldata0;
  /* Data register 1 */
  int pinctldata1;
  /* PIN CTL enable value */
  int pinctlenable;
  /* Output enable mask for UART pins  */
  int hcioeenablemask;
  /* Output enable mask for AUX PCM pins  */
  int pcmoeenablemask;
}platformpintest;

const platformpintest pintestconfigs[] = {
  {
  .platform = 8660,
  .pinconfig = {
    {
      .gpiostring = "53",/*UARTDM_TX*/
      .gpionum = 53,
      .pinctrlreg = 0x87,/*BT_HCI_0_MODE*/
      .datareg = 0x0D,/*PIN_CTL_DATA0*/
      .bitpos = 7,
      .direction = 1,
    },
    {
      .gpiostring = "54",/*UARTDM_RX*/
      .gpionum = 54,
      .pinctrlreg = 0x86,/*BT_HCI_1_MODE*/
      .datareg = 0x0D,/*PIN_CTL_DATA0*/
      .bitpos = 6,
      .direction = 0,
    },
    {
      .gpiostring = "55",/*UARTDM_CTS*/
      .gpionum = 55,
      .pinctrlreg = 0x84,/*BT_HCI_3_MODE*/
      .datareg = 0x0D,/*PIN_CTL_DATA0*/
      .bitpos = 4,
      .direction = 0,
    },
    {
      .gpiostring = "56",/*UARTDM_RFR*/
      .gpionum = 56,
      .pinctrlreg = 0x85,/*BT_HCI_2_MODE*/
      .datareg = 0x0D,/*PIN_CTL_DATA0*/
      .bitpos = 5,
      .direction = 1,
    },
    {
      .gpiostring = "111",/*AUX_PCM_DOUT_S*/
      .gpionum = 111,
      .pinctrlreg = 0x89,/*BT_PCM_DIN_MODE */
      .datareg = 0x0E,/*PIN_CTL_DATA1*/
      .bitpos = 1,
      .direction = 1,
    },
    {
      .gpiostring = "112",/*AUX_PCM_DIN_S*/
      .gpionum = 112,
      .pinctrlreg = 0x8A,/*BT_PCM_DOUT_MODE*/
      .datareg = 0x0E,/*PIN_CTL_DATA1*/
      .bitpos = 2,
      .direction = 0,
    },
    {
      .gpiostring = "113",/*AUX_PCM_SYNC_S*/
      .gpionum = 113,
      .pinctrlreg = 0x8B,/*BT_PCM_SYNC_MODE*/
      .datareg= 0x0E,/*PIN_CTL_DATA1*/
      .bitpos = 3,
      .direction = 1,
    },
    {
      .gpiostring = "114",/*AUX_PCM_CLK_S*/
      .gpionum = 114,
      .pinctrlreg = 0x88,/* BT_PCM_BCLK_MODE */
      .datareg = 0x0E,/*PIN_CTL_DATA1*/
      .bitpos = 0,
      .direction = 1,
    },
  },
  .pinctloe0 = 0xA,
  .pinctloe1 = 0xB,
  .pinctldata0 = 0xD,
  .pinctldata1 = 0xE,
  .pinctlenable = 0x15,
  .hcioeenablemask = 0x50,
  .pcmoeenablemask = 0x04,
  },
};
platformpintest *pintestconfig= (platformpintest *)&pintestconfigs[0];
/* I2C bus and GPIO mux entry drivers */
#define I2C_PATH "/dev/i2c-4"
#define PINTEST_ENABLE_PATH "/sys/kernel/debug/btpintest/enable"

const char HIGH = '1';
const char LOW = '0';

/* -------------------------------------------------------------------------
** Definitions and Declarations
** ------------------------------------------------------------------------- */

/*Flag to manage the verbose output */
extern int verbose;
/* HCI Command buffer */
static uint8 bt_ftm_buffer[BT_FTM_CMD_RSP_LEN];
/* HCI Event buffer */
static uint8 event_buf[HC_VS_MAX_ACL];
/* Varibale to handle the stages of Sleep disable cmds */
static int sleep_stage =0;
/* FTM Global state variable */
static ftm_state global_state = FTM_SOC_NOT_INITIALISED;
/* pointer to the SOC version string */
static uint8 *bt_soc_app_version_string = NULL;
/*variable to identify the msm type*/
static boolean is_transportSMD = 0;
/* Variables to identify the platform */
extern char transport_type[PROPERTY_VALUE_MAX];
/* Default hw version register contents for 4020BD B0, if it's different than it's 4020BD B1 */
static uint8 bt_soc_hw_version[] =
{
    0x05, 0x00, 0x00, 0x00
};
/* Default Bluetooth address if read from NV fails, same in AMSS */
static const uint8  default_bt_bd_addr[] =
{
    0x34, 0x12, 0x78, 0x56, 0xBC, 0x9A
};
/* Uart Protocol config tag*/
static const bt_qsoc_cfg_tbl_struct_type bt_qsoc_tag17 =
{
    0x8,   {0x01, 0x11, 0x05, 0x0A, 0x01, 0x00, 0x00, 0x00,}
};

/* Uart Protocol config tag for latest hw */
static const bt_qsoc_cfg_tbl_struct_type bt_qsoc_tag17_latest_hw =
{
   0x0B,   {0x01, 0x11, 0x08, 0x02,0x01,0x0E,0x08,0x04,0x32,0x0A,0x00}
};

/* FTM status log size*/
const uint8 logsize = 2;
/* HCI user Cmd pass Log Packet */
const uint8 event_buf_user_cmd_pass[2] = {0x0f,FTM_BT_DRV_NO_ERR};
/* HCI user Cmd fail Log Packet */
const uint8 event_buf_user_cmd_fail[2] = {0x0f,FTM_BT_DRV_CONN_TEST_FAILS};
/* HCI user Cmd timed out Log Packet */
const uint8 event_buf_user_cmd_timeout[2] = {0x0f,FTM_BT_DRV_NO_SOC_RSP_TOUT};
/* HCI user Cmd Unknown error Log Packet */
const uint8 event_buf_user_unknown_err[2] = {0x0f,FTM_BT_DRV_UNKNOWN_ERR};

struct first_cmd ptr_powerup;
#ifdef USE_LIBSOCCFG
/* Run time SoC Cfg paramters */
ftm_bt_soc_runtime_cfg_type soc_cfg_parameters;
#endif
/* Peek table Loop count for 4025 R3 SoC */
uint loopCount;
/* Command Queue front pointer */
cmdQ *front = NULL;
/* Command Queue rear pointer */
cmdQ *rear = NULL;
/* cmd count for unprocessed cmds in queue */
uint32 num_pending_cmds = 0;
/* Descriptors for connectivity test */
static int fd_i2c;
static char ctime_buf[10];

char *get_current_time(void)
{
    struct timeval tv;
    time_t ctime;

    gettimeofday(&tv, NULL);
    ctime = tv.tv_sec;
    strftime(ctime_buf, 10, "%T", localtime(&ctime));
    return ctime_buf;
}
/*===========================================================================
FUNCTION   qinsert_cmd

DESCRIPTION
  Command Queue insert routine. Add the FTM BT packet to the Queue

DEPENDENCIES
  NIL

RETURN VALUE
  RETURNS FALSE without adding queue entry in failure
  to allocate a new Queue item
  else returns TRUE

SIDE EFFECTS
  increments the number of commands queued

===========================================================================*/
boolean qinsert_cmd(ftm_bt_pkt_type *ftm_bt_pkt)
{
  cmdQ *newitem;
#ifdef FTM_DEBUG
  printf("qinsert_cmd > rear = 0x%x front = 0x%x\n",
			(unsigned int)rear,(unsigned int)front);
#endif
  if(num_pending_cmds == 20)
  {
     ftm_log_send_msg(&event_buf_user_unknown_err[0],logsize);
     return FALSE;
  }
  newitem = (cmdQ*)malloc(sizeof(cmdQ));
  if(newitem == NULL)
  {
    ftm_log_send_msg(&event_buf_user_unknown_err[0],logsize);
    return FALSE;
  }
  newitem->next=NULL;
  newitem->data = (void *)malloc(ftm_bt_pkt->ftm_hdr.cmd_data_len);
  if(newitem->data == NULL)
  {
    free(newitem);
    ftm_log_send_msg(&event_buf_user_unknown_err[0],logsize);
    return FALSE;
  }
  /* Copy the data into the queue buffer */
  memcpy(newitem->data,(void*)ftm_bt_pkt->data, ftm_bt_pkt->ftm_hdr.cmd_data_len);
  /* Set Flag to notify BT command*/
  newitem->bt_command = 1;
  newitem->cmd_len = ftm_bt_pkt->ftm_hdr.cmd_data_len;

  if(front==NULL && rear==NULL)
  {
    front=newitem;
    rear=newitem;
  }
  else
  {
    (rear)->next=newitem;
    rear=newitem;
  }
  num_pending_cmds++;
#ifdef FTM_DEBUG
  printf("qinsert_cmd < rear = 0x%x front = 0x%x\n",
			(unsigned int)rear,(unsigned int)front);
#endif
  return TRUE;
}

/*===========================================================================
FUNCTION   dequeue_send

DESCRIPTION
  Command Queue delete and calls HCI send routine. Dequeues the HCI data from
  the queue and sends it to HCI HAL layer.

DEPENDENCIES
  NIL

RETURN VALUE
  RETURN NIL

SIDE EFFECTS
  decrements the number of command queued

===========================================================================*/
void dequeue_send()
{
  cmdQ *delitem;      /* Node to be deleted */
#ifdef FTM_DEBUG
  printf("dequeue_send > rear = 0x%x front = 0x%x\n",
			(unsigned int)rear,(unsigned int)front);
#endif
  if((front)==NULL && (rear)==NULL)
    printf("\nQueue is empty to delete any element\n");
  else
  {
    delitem=front;
    if(delitem)
    {
      ftm_bt_dispatch(delitem->data,delitem->cmd_len);
      front=front->next;
      if(front == NULL)
      {
        rear = NULL;
        num_pending_cmds = 0;
      }
      free(delitem->data);
      free(delitem);
      num_pending_cmds--;
    }
  }
#ifdef FTM_DEBUG
  printf("dequeue_send < rear = 0x%x front = 0x%x\n",
			(unsigned int)rear,(unsigned int)front);
#endif
}

/*===========================================================================
FUNCTION   cleanup_pending_cmd_queue

DESCRIPTION
  Command Queue delete routine. Dequeues the HCI cmds from the
  queue. This routine is useful in case the HCI interface has
  hung up and a FTM module restart is immimnent.

DEPENDENCIES
  NIL

RETURN VALUE
  RETURN NIL

SIDE EFFECTS
  NONE

===========================================================================*/

void cleanup_pending_cmd_queue()
{
  cmdQ *delitem;      /* Node to be deleted */
#ifdef FTM_DEBUG
  printf("cleanup_pending_cmd_queue > rear = 0x%x front = 0x%x\n",
			(unsigned int)rear,(unsigned int)front);
#endif
  if((front==NULL) && (rear==NULL))
  {
    printf("\nQueue is empty to delete any element\n");
  }
  else
  {
    while(front != NULL)
    {
      delitem=front;
      front=front->next;
      if(front == NULL)
        rear = NULL;
      free(delitem->data);
      free(delitem);
    }
  }
#ifdef FTM_DEBUG
  printf("cleanup_pending_cmd_queue < rear = 0x%x front = 0x%x\n",
			(unsigned int)rear,(unsigned int)front);
#endif
}

/*===========================================================================
FUNCTION   ftm_bt_err_timedout

DESCRIPTION
  This routine triggers the shutdown of the HCI and Power resources in case
  a HCI command previously sent times out.

DEPENDENCIES
  NIL

RETURN VALUE
  RETURN NIL

SIDE EFFECTS
  NONE

===========================================================================*/
void ftm_bt_err_timedout()
{
  ftm_bt_hci_hal_deinit_transport();
  if(!is_transportSMD)
     ftm_bt_power_hal_set(BT_OFF);
#ifdef FTM_DEBUG
  printf("\nTimed out \n");
#endif
  global_state = FTM_SOC_NOT_INITIALISED;
  cleanup_pending_cmd_queue();
  ftm_log_send_msg(&event_buf_user_cmd_timeout[0],logsize);
}

/*===========================================================================
FUNCTION   export_gpio

DESCRIPTION
  Writes the gpio number passed in the argumnet to export a sysfs entry

DEPENDENCIES
  NIL

RETURN VALUE
  number of bytes written

SIDE EFFECTS
  None

===========================================================================*/
int export_gpio(int fd,char *gpionum)
{
  int sz;
  sz = write(fd,gpionum,strlen(gpionum));
  return sz;
}
/*===========================================================================
FUNCTION   ftm_bt_conn_init

DESCRIPTION
  Initialises the connectivity test settings
	1. Exports the sysfs entries for MSM GPIOs
	2. Configures the TLMM settings on BT SoC to be in I2C PIN
	   control mode and directions.

DEPENDENCIES
  NIL

RETURN VALUE
  FALSE,if the init fails
  TRUE,if it passes

SIDE EFFECTS
  None

===========================================================================*/
boolean ftm_bt_conn_init(void)
{
  char out[] = "out";
  char in[] = "in";
  int sz,i;
  unsigned char buffer;
  char direction_path[64];
  int fd_gpio = -1;
  int fd = -1;
#ifdef FTM_DEBUG
  printf("ftm_bt_conn_init start \n");
#endif
  fd_gpio = open("/sys/class/gpio/export", O_WRONLY);

  if(fd_gpio < 0)
    return FALSE;

  for(i = 0; i < MAX_PIN_CONFIGS; i++)
  {
#ifdef FTM_DEBUG
    printf("pintestconfig->gpiostring = %s\n",pintestconfig->pinconfig[i].gpiostring);
#endif
    sz = export_gpio(fd_gpio,pintestconfig->pinconfig[i].gpiostring);
    if (sz < 0)
    {
      goto out;
    }
  }
#ifdef FTM_DEBUG
  printf("Enabling pin test path\n");
#endif
  /* Configure the TLMM settings for the GPIOs requested using export */
  fd = open(PINTEST_ENABLE_PATH, O_WRONLY);
  if(fd < 0)
    goto out;

  buffer = HIGH;
  sz = write(fd,&buffer,sizeof(buffer));
  if (sz < 0)
  {
    goto out;
  }
  close(fd);
#ifdef FTM_DEBUG
  printf("open I2C_PATH\n");
#endif
  fd_i2c = open(I2C_PATH,O_RDWR);

  if(fd_i2c < 0)
  {
    goto out;
  }

  buffer = pintestconfig->pinctlenable;
  for(i = 0; i < MAX_PIN_CONFIGS ; i++)
  {
#ifdef FTM_DEBUG
    printf("pintestconfig->pinconfig[i]pinctrlreg = 0x%x\n",pintestconfig->pinconfig[i].pinctrlreg);
#endif
    sz = i2c_write(fd_i2c,pintestconfig->pinconfig[i].pinctrlreg,&buffer,sizeof(buffer),I2C_SLAVE_ADDR);
    if (sz < 0)
    {
      goto out;
    }
  }

  buffer = pintestconfig->hcioeenablemask;
  sz = i2c_write(fd_i2c,pintestconfig->pinctloe0,&buffer,sizeof(buffer),I2C_SLAVE_ADDR);
  if (sz < 0)
  {
    goto out;
  }
  for(i = 0; i < MAX_PIN_CONFIGS; i++)
  {
    snprintf(direction_path,sizeof(direction_path),
          "/sys/class/gpio/gpio%d/direction", pintestconfig->pinconfig[i].gpionum);
    fd = open(direction_path,O_WRONLY);
    if(fd < 0)
       goto out;
    if(pintestconfig->pinconfig[i].direction)
      sz = write(fd,&out,sizeof(out));
    else
      sz = write(fd,&in,sizeof(in));
    if (sz < 0)
    {
      goto out;
    }
    close(fd);
  }

  buffer = pintestconfig->pcmoeenablemask;
  sz = i2c_write(fd_i2c,pintestconfig->pinctloe1,&buffer,sizeof(buffer),I2C_SLAVE_ADDR);

out :
#ifdef FTM_DEBUG
  printf("ftm_bt_conn_init end\n");
#endif
  if(fd >= 0)
    close(fd);
  if(fd_gpio >= 0)
    close(fd_gpio);

  if ((sz < 0) || (fd < 0) || (fd_gpio < 0))
  {
    return FALSE;
  }
  return TRUE;

}

/*===========================================================================
FUNCTION   ftm_bt_conn_outputpin_test

DESCRIPTION
  Executes the connectivity test for a output pin on
  MSM 8660 to input pin on BT SOC

DEPENDENCIES
  NIL

RETURN VALUE
  FALSE,if the connectivity test fails
  TRUE,if it passes for all pins combination

SIDE EFFECTS
  None

===========================================================================*/
boolean ftm_bt_conn_outputpin_test(const pintest *config)
{
  int fd_gpioN = -1;
  int bit;
  unsigned char wr_gpio_value = HIGH,rd_reg_value = 0;
  char value_path[64];
#ifdef FTM_DEBUG
  printf("ftm_bt_conn_outputpin_test\n");
#endif
  /* Test UART Tx --> HCI0(RX on SoC) */
  snprintf(value_path,sizeof(value_path),
        "/sys/class/gpio/gpio%d/value", config->gpionum);
  fd_gpioN = open(value_path,O_WRONLY);
  wr_gpio_value = HIGH;
  /* Write a HIGH on the GPIO line on MSM */
  write(fd_gpioN,&wr_gpio_value,sizeof(wr_gpio_value));

  i2c_read(fd_i2c,config->datareg,&rd_reg_value,sizeof(rd_reg_value),I2C_SLAVE_ADDR);

  bit = ((rd_reg_value & (1 << config->bitpos)) >> config->bitpos);
  /* Check the Bit position in the Pin's Data register on SoC for a 1 */
  if(bit != 1)
  {
    close(fd_gpioN);
    printf("OUT :HIGH Test GPIO %d FAIL data reg = %d\n",config->gpionum,config->datareg);
    return FALSE;
  }

  wr_gpio_value = LOW;
  /* Write a LOW on the GPIO line on MSM*/
  write(fd_gpioN,&wr_gpio_value,sizeof(wr_gpio_value));

  i2c_read(fd_i2c,config->datareg,&rd_reg_value,sizeof(rd_reg_value),I2C_SLAVE_ADDR);

  bit = ((rd_reg_value & (1 << config->bitpos)) >> config->bitpos);
  /* Check if the bit position in the Pin's data register on SoC is cleared */
  if(bit != 0)
  {
    close(fd_gpioN);
    printf("OUT : HIGH Test GPIO %d FAIL data reg = %d\n",config->gpionum,config->datareg);
    return FALSE;
  }
  close(fd_gpioN);

  printf("OUT : Test GPIO %d PASS \n",config->gpionum);

  return TRUE;

}

/*===========================================================================
FUNCTION   ftm_bt_conn_inputpin_test

DESCRIPTION
  Executes the connectivity test for a input pin on
  MSM 8660 from output pin on BT SOC

DEPENDENCIES
  NIL

RETURN VALUE
  FALSE,if the connectivity test fails
  TRUE,if it passes for all pins combination

SIDE EFFECTS
  None

===========================================================================*/
boolean ftm_bt_conn_inputpin_test(const pintest *config)
{
  int fd_gpioN = -1;
  unsigned char wr_reg_value = HIGH,rd_gpio_value;
  char value_path[64];
#ifdef FTM_DEBUG
  printf("ftm_bt_conn_inputpin_test\n");
#endif

  snprintf(value_path,sizeof(value_path),
        "/sys/class/gpio/gpio%d/value", config->gpionum);
  fd_gpioN = open(value_path,O_RDONLY);

  /* Write a 1 to the Pins bit position on SoC */
  wr_reg_value = (1 << config->bitpos);
  i2c_write(fd_i2c,config->datareg,&wr_reg_value,sizeof(wr_reg_value),I2C_SLAVE_ADDR);

  read(fd_gpioN,&rd_gpio_value,sizeof(rd_gpio_value));
  /* Check if the value is high on the MSM GPIO line */
  if(rd_gpio_value != HIGH)
  {
    close(fd_gpioN);
    printf("IN : HIGH Test GPIO %d FAIL data reg = %d\n",config->gpionum,config->datareg);
    return FALSE;
  }

  close(fd_gpioN);
  fd_gpioN = open(value_path,O_RDONLY);
  /* Clear the Pins bit position on SoC */
  wr_reg_value = wr_reg_value & ~(1 << config->bitpos);
  i2c_write(fd_i2c,config->datareg,&wr_reg_value,sizeof(wr_reg_value),I2C_SLAVE_ADDR);

  read(fd_gpioN,&rd_gpio_value,sizeof(rd_gpio_value));

  /* Check if the value is low on the MSM GPIO line */
  if(rd_gpio_value != LOW)
  {
    close(fd_gpioN);
    printf("IN : LOW Test GPIO %d FAIL data reg = %d\n",config->gpionum,config->datareg);
    return FALSE;
  }
  close(fd_gpioN);

  printf("IN : Test GPIO %d  PASS \n",config->gpionum);

  return TRUE;
}


/*===========================================================================
FUNCTION   ftm_bt_conn_test_execute

DESCRIPTION
  Executes the connectivity test for MSM 8660-Bahama SOC
	MSM 8660	 	Bahama SoC
	========   	 	==========
	UART Tx  	------> UART Rx
	UART Rx  	<------ UART Tx
	UART RTS 	------> UART CTS
	UART CTS 	<------ UART RTS

	AUX_PCM_CLK	------> PCM_BCLK
	AUX_PCM_SYNC	------> PCM_SYNC
	AUX_PCM_DOUT	------> PCM_DIN
	AUX_PCM_DIN	<------ PCM_DOUT

DEPENDENCIES
  NIL

RETURN VALUE
  FALSE,if the connectivity test fails
  TRUE,if it passes for all pins combination

SIDE EFFECTS
  None

===========================================================================*/
boolean ftm_bt_conn_test_execute(void)
{

  int i = 0,ret = 0;

  printf("Conn test begin \n");

  for(i = 0; i < MAX_PIN_CONFIGS; i++)
  {
    if(pintestconfig->pinconfig[i].direction)
      ret = ftm_bt_conn_outputpin_test(&pintestconfig->pinconfig[i]);
    else
      ret = ftm_bt_conn_inputpin_test(&pintestconfig->pinconfig[i]);

    if(ret != TRUE)
      return FALSE;
  }
  printf("Conn test successfully done \n");
  return TRUE;
}

/*===========================================================================
FUNCTION   ftm_bt_conn_deinit

DESCRIPTION
  Deinitialise the resources allocated for connectivity tests which includes
	1. unexport the GPIO sysfs entries
	2. Reset the settings in I2C registers of Bahama top level module

DEPENDENCIES
  NIL

RETURN VALUE
  NIL

SIDE EFFECTS
  None

===========================================================================*/
void ftm_bt_conn_deinit(void)
{
  int sz,fd,i;
  int fd_gpio;
  unsigned char buffer;

#ifdef FTM_DEBUG
  printf("ftm_bt_conn_deinit start\n");
#endif
  /* Restore the Mux settings of the requested GPIOs*/
  fd = open(PINTEST_ENABLE_PATH, O_WRONLY);
  buffer = '0';
  sz = write(fd,&buffer,sizeof(buffer));
  close(fd);
  /* Unexport all the GPIOs */
  fd_gpio = open("/sys/class/gpio/unexport", O_WRONLY);

  if(fd_gpio < 0)
    return ;

  for(i = 0; i < MAX_PIN_CONFIGS; i++)
  {
    sz = export_gpio(fd_gpio,pintestconfig->pinconfig[i].gpiostring);
    if (sz < 0)
    {
      break;
    }
  }

  for(i = 0; i < MAX_PIN_CONFIGS; i++)
  {
    if(pintestconfig->pinconfig[i].gpionum >= 53 && pintestconfig->pinconfig[i].gpionum <= 56)
      buffer = 0x40;
    else
      buffer = 0xC0;

    sz = i2c_write(fd_i2c,pintestconfig->pinconfig[i].pinctrlreg,&buffer,sizeof(buffer),I2C_SLAVE_ADDR);
    if (sz < 0)
    {
      break;
    }
  }
  buffer = 0x00;
  i2c_write(fd_i2c,pintestconfig->pinctloe0,&buffer,sizeof(buffer),I2C_SLAVE_ADDR);
  i2c_write(fd_i2c,pintestconfig->pinctloe1,&buffer,sizeof(buffer),I2C_SLAVE_ADDR);
  i2c_write(fd_i2c,pintestconfig->pinctldata0,&buffer,sizeof(buffer),I2C_SLAVE_ADDR);
  i2c_write(fd_i2c,pintestconfig->pinctldata1,&buffer,sizeof(buffer),I2C_SLAVE_ADDR);

  close(fd_gpio);
  close(fd_i2c);
  fd_i2c = -1;
#ifdef FTM_DEBUG
  printf("ftm_bt_conn_deinit end\n");
#endif
  return;
}

/*===========================================================================
FUNCTION   ftm_bt_conn_test

DESCRIPTION
  Executes the connectivity test for MSM 8660-Bahama SOC

DEPENDENCIES
  NIL

RETURN VALUE
  FALSE,if the connectivity test fails
  TRUE,if it passes

SIDE EFFECTS
  None

===========================================================================*/
boolean ftm_bt_conn_test(void)
{
  boolean ret;
  unsigned int i = 0;
#ifdef FTM_DEBUG
  printf("ftm_bt_conn_test = %d\n",pintestconfigs[i].platform);
#endif
  /* Walk through the avaialble pin test configs*/
  for(i = 0; i < ARRAY_SIZE(pintestconfigs);i++)
  {
    printf("board type = %d stored type = %d \n",boardtype,pintestconfigs[i].platform);
    if(boardtype == pintestconfigs[i].platform)
    {
      pintestconfig = (platformpintest *)&pintestconfigs[i];
      break;
    }
  }
  /* If we dont find a matching test config return here itself */
  if(i == ARRAY_SIZE(pintestconfigs))
  {
    printf("Board type not supported %d\n",boardtype);
    return FALSE;
  }
  /* Initialise the connectivity test
  *  related settings
  */
  if((ret = ftm_bt_conn_init()))
  {
    /* Execute the test */
    ret = ftm_bt_conn_test_execute();
  }
  /* Deinitliase the pin settings and i2c resources/mux
   * settings
   */
  ftm_bt_conn_deinit();
  return ret;
}

/*===========================================================================
FUNCTION   ftm_bt_hci_hal_vs_event

DESCRIPTION
  Processes the VS event buffer and stores the App version and HW version

DEPENDENCIES
  NIL

RETURN VALUE
  NIL, Error in the event buffer will mean a NULL App version and Zero HW
  version

SIDE EFFECTS
  None

===========================================================================*/
void ftm_bt_hci_hal_vs_event
(
  const uint8* pEventBuffer,
  uint8 nLength
)
{
  const uint8 poke_reg_addr[] = { 0xFF,0x0B,BT_QSOC_EDL_CMD_CODE,0x01,
				0x34,0x00,0x00,0x8C,0x04};
  if ( nLength > 3 )
  {
    if ( ( pEventBuffer[ 0]  ==  0xFF)  /* VS Event */
	    && (pEventBuffer[1] > 5)   /* VS Length > 5*/
	    && (pEventBuffer[2] == BT_QSOC_EDL_CMD_CODE)
	    && (pEventBuffer[3] == BT_QSOC_VS_EDL_APPVER_RESP)
       )
    {
      if( NULL != bt_soc_app_version_string )
      {
	free(bt_soc_app_version_string);
        bt_soc_app_version_string = NULL;
      }
      bt_soc_app_version_string = (uint8 *)malloc(nLength-5);
      if( NULL != bt_soc_app_version_string )
      {
        memmove(bt_soc_app_version_string,
          &pEventBuffer[5],nLength-5);
      }
   }
   else if ( (nLength > 12)  // make sure we have enough event bytes
            && (!memcmp(pEventBuffer,poke_reg_addr,sizeof(poke_reg_addr))))
    {
       bt_soc_hw_version[0] = pEventBuffer[9];
       bt_soc_hw_version[1] = pEventBuffer[10];
       bt_soc_hw_version[2] = pEventBuffer[11];
       bt_soc_hw_version[3] = pEventBuffer[12];
    }
  }
} /* ftm_bt_hci_hal_vs_event */

int is_snoop_log_enabled ()
{
  char value[PROPERTY_VALUE_MAX] = {'\0'};
  property_get("persist.service.bdroid.snooplog", value, "false");
  return (strcmp(value, "true") == 0);
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


void ftm_log_send_msg(const uint8 *pEventBuf,int event_bytes)
{
  if (strcasecmp(transport_type, "smd") || ((pEventBuf != NULL) && (pEventBuf[0] == FTM_BT_CMD_NV_READ)))
  {
    /* ftmdaemon uses log opcode 0x1366 to send the HCI Event logs to QRCT via DIAG
     * but Riva/Pronto also uses the same opcode to send the events over DIAG.
     * So this is causing the QRCT to recv some time 2 events for one cmd and some
     * time pkt corruption.
     * With this change ftmdaemon wont send any logs to QRCT in case of Riva/Pronto
     */
    ftm_bt_log_pkt_type*  ftm_bt_log_pkt_ptr = NULL;

    if((pEventBuf == NULL) || (event_bytes == 0))
      return;

    if(!is_snoop_log_enabled())
    {
       if(pEventBuf[0] == BT_HCI_ACL_PKT && log_status(LOG_BT_HCI_RX_ACL_C))
       {
           ftm_bt_log_pkt_ptr = (ftm_bt_log_pkt_type *)log_alloc(LOG_BT_HCI_RX_ACL_C,
             FTM_BT_LOG_HEADER_SIZE + (event_bytes-1));
       }
       else if(pEventBuf[0] == BT_HCI_EVT_PKT && log_status(LOG_BT_HCI_EV_C))
       {
           ftm_bt_log_pkt_ptr = (ftm_bt_log_pkt_type *)log_alloc(LOG_BT_HCI_EV_C,
             FTM_BT_LOG_HEADER_SIZE + (event_bytes-1));
       }
    }
    if(pEventBuf[0] == FM_HCI_EVT_PKT )
    {
       ftm_bt_log_pkt_ptr = (ftm_bt_log_pkt_type *)log_alloc(LOG_FTM_FM_C,
         FTM_BT_LOG_HEADER_SIZE + (event_bytes-1));
    }
    if(ftm_bt_log_pkt_ptr != NULL)
    {
       /* We should not send HCI event code 0x04 for the log opcode 0x1366. Hence
        * move the pointer to the next location to skip first byte in HCI event.
        */
       pEventBuf++;
       memcpy((void *)ftm_bt_log_pkt_ptr->data,(void *)pEventBuf, event_bytes-1);
       log_commit( ftm_bt_log_pkt_ptr );
    }
    else
    {
       printf("ftm_log_send_msg: Dropping packet\n");
       return;
    }
  }
}
/*===========================================================================
FUNCTION   ftm_bt_hci_hal_send_reset_cmd

DESCRIPTION
  Sends the HCI packet to reset the BT SoC

DEPENDENCIES
  NIL

RETURN VALUE
  FALSE: If write Fails, else TRUE

SIDE EFFECTS
  None

===========================================================================*/

boolean ftm_bt_hci_hal_send_reset_cmd()
{
  const uint8   ResetCmd[4] = {0x01, 0x03, 0x0C, 0x00};
  global_state = FTM_SOC_RESET;
  return ((ftm_bt_hci_hal_nwrite((uint8*)(&ResetCmd[0]), 4) == STATUS_SUCCESS)
	? TRUE: FALSE);
}

/*===========================================================================
FUNCTION   ftm_bt_hci_hal_retrieve_send_nvm

DESCRIPTION
  Retrieves the NVM commands from the NVM parser module
  and packages the VS HCI packet before calling ftm_bt_hci_hal_vs_sendcmd
  When NVM entries are exhausted it Calls the next stage of Init to
  disable sleep

DEPENDENCIES
  NIL

RETURN VALUE
  FALSE: If Failure, else TRUE

SIDE EFFECTS
  None

===========================================================================*/
#ifdef USE_LIBSOCCFG
boolean  ftm_bt_hci_hal_retrieve_send_nvm()
{
  bt_qsoc_nvm_status nvm_status;
  uint8 * nvm_ptr=NULL;
  static const bt_qsoc_cfg_tbl_struct_type bt_qsoc_tag27 =
  {
    0x04,   {0x01, 0x1B, 0x01, 0x00}
  };

  /* Get the next NVM "string" */
  nvm_status = bt_qsoc_nvm_get_next_cmd(&nvm_ptr);
  if(nvm_status == BT_QSOC_NVM_STATUS_SUCCESS)
  {
    if( nvm_ptr != NULL )
    {
      return ftm_bt_hci_hal_vs_sendcmd( BT_QSOC_NVM_ACCESS_OPCODE,
	(uint8 *)(&nvm_ptr[1]),(uint8)(nvm_ptr[0]) );
    }
  }
  else
  {
    bt_qsoc_nvm_close();
    sleep_stage = 0;
    global_state = FTM_SOC_SLEEP_DISABLE;
    if ( (ftm_bt_hci_hal_vs_sendcmd (
		BT_QSOC_NVM_ACCESS_OPCODE,
		(uint8 *)(bt_qsoc_tag27.vs_cmd_data),
		bt_qsoc_tag27.vs_cmd_len )
	      ) != TRUE )
      return FALSE;
    sleep_stage++;
  }

  return TRUE;
}
#endif
/*===========================================================================
FUNCTION   ftm_bt_hci_hal_vs_sendcmd

DESCRIPTION
 Helper Routine to process the VS HCI cmd and constucts the HCI packet before
 calling ftm_bt_hci_send_cmd routine

DEPENDENCIES
  NIL

RETURN VALUE
  RETURN VALUE
  FALSE = failure, else TRUE

SIDE EFFECTS
  None

===========================================================================*/

boolean ftm_bt_hci_hal_vs_sendcmd
(
  uint16 opcode,
  uint8  *pCmdBuffer,
  uint8  nSize
)
{
  uint8 cmd[HC_VS_MAX_CMD_EVENT];   //JN: change this
  request_status status = FALSE;

  int nwrite;
  cmd[0] = BT_HCI_CMD_PKT; // JN: bluetooth header files in linux has a define
                      // HCI_COMMAND_PKT for this but do we want to use
                      // something thats command between QC platforms.
  cmd[1] = (uint8)(opcode & 0xFF);
  cmd[2] = (uint8)( (opcode>>8) & 0xFF);
  cmd[3] = (uint8)(nSize);

  memcpy(&cmd[HCI_CMD_HDR_SIZE], pCmdBuffer, nSize);

  status = ftm_bt_hci_hal_nwrite((&cmd[0]), (HCI_CMD_HDR_SIZE+nSize));

  if (status != STATUS_SUCCESS)
  {
    printf("Error->Send Header failed : %d\n",status);
    return FALSE;
  }
  return TRUE;
}
/*===========================================================================
FUNCTION   ftm_bt_hci_hal_read_app_version

DESCRIPTION
 Helper Routine to package the VS HCI cmd to read the Application version
 and calls the ftm_bt_hci_hal_vs_sendcmd

DEPENDENCIES
  NIL

RETURN VALUE
  RETURN VALUE
  FALSE = failure, else TRUE

SIDE EFFECTS
  None

===========================================================================*/

boolean ftm_bt_hci_hal_read_app_version()
{
  const uint8 getAppVerCmd[] = {0x06};
  global_state = FTM_SOC_READ_APP_VER;
  ALOGV("ftm_bt_hci_hal_read_app_version:global_state = %d\n",global_state);
  return ftm_bt_hci_hal_vs_sendcmd(BT_QSOC_EDL_CMD_OPCODE,(uint8 *)&getAppVerCmd[0],1);
}

/*===========================================================================
FUNCTION   ftm_bt_hci_hal_read_hw_version

DESCRIPTION
 Helper Routine to package the VS HCI cmd to read the HW version
 and calls the ftm_bt_hci_hal_vs_sendcmd

DEPENDENCIES
  NIL

RETURN VALUE
  RETURN VALUE
  FALSE = failure, else TRUE

SIDE EFFECTS
  None

===========================================================================*/

boolean ftm_bt_hci_hal_read_hw_version()
{
  const uint8 getHWVerRegCmd[] = {0x0D, 0x34, 0x00, 0x00, 0x8C, 0x04 };
  global_state = FTM_SOC_READ_HW_VER;
  return ftm_bt_hci_hal_vs_sendcmd(BT_QSOC_EDL_CMD_OPCODE,(uint8 *)&getHWVerRegCmd[0],6);
}

/*===========================================================================
FUNCTION   ftm_bt_hci_hal_nvm_download_init

DESCRIPTION
 Routine to lookup the Soc type and initiate a Poke Table in case of a R3
 Soc type or else go ahead and proceed with the NvM open with runtime parameters
 in AUTO MODE
DEPENDENCIES
  NIL

RETURN VALUE
  RETURN VALUE
  FALSE = failure, else TRUE

SIDE EFFECTS
  None

===========================================================================*/
#ifdef USE_LIBSOCCFG
boolean ftm_bt_hci_hal_nvm_download_init()
{
  bt_qsoc_config_params_struct_type run_time_params;
  bt_qsoc_lookup_param soc_data;
  bt_qsoc_nvm_status nvm_status;
  bt_qsoc_enum_type soc_type;
  boolean returnStatus = TRUE;
  int loopCount;
  bt_qsoc_enum_nvm_mode nvm_mode = NVM_AUTO_MODE;

  soc_data.app_ver_str = (char *)bt_soc_app_version_string;
  soc_data.hw_ver_str = (char *)bt_soc_hw_version;
  soc_type = bt_qsoc_type_look_up(&soc_data);

  if (soc_type == BT_QSOC_R2B)
  {
    printf("bt_hci_qcomm_init Failed R2B Not supported\n");
    returnStatus = FALSE;
  }
  else if (soc_type == BT_QSOC_R2C)
  {
    printf("bt_hci_qcomm_init Failed R2C Not supported");
    returnStatus = FALSE;
  }
  else
  {
#ifdef FTM_DEBUG
    printf("\nbt_hci_qcomm_init : Found QSoC type %d.\n", soc_type);
#endif
  }
  // default run-time parameters for SOC
  memmove((uint8*)(&run_time_params.bd_address[0]),
          (const uint8 *)(&(default_bt_bd_addr[0])),
          BT_QSOC_MAX_BD_ADDRESS_SIZE);
  run_time_params.refclock_type = BT_SOC_REFCLOCK_19P2MHZ;
  run_time_params.clock_sharing =BT_SOC_CLOCK_SHARING_ENABLED;
  run_time_params.soc_logging = 0;
  run_time_params.bt_2_1_lisbon_disabled = 0;
  /* ROM defualt LE & BR/EDR SoC power class configurations
   */
  run_time_params.bt_qsoc_bredr_dev_class = BT_QSOC_DEV_CLASS1;
  run_time_params.bt_qsoc_le_dev_class = BT_QSOC_DEV_CLASS2;

  /* After the Firmware is detected, start intializing the Poke table */
  if ( returnStatus != FALSE)
  {
     /* Patch: Pokes Only specific to R3 */
     global_state = FTM_SOC_POKE8_TBL_INIT;
     if ( soc_type == BT_QSOC_R3 )
     {
       printf("bt_hci_qcomm_init  - Initialize R3 Poke table");
       loopCount = 0;
       if ( (ftm_bt_hci_hal_vs_sendcmd(
                BT_QSOC_EDL_CMD_OPCODE,
                (uint8 *)bt_qsoc_vs_poke8_tbl_r3[loopCount].vs_poke8_data,
                 bt_qsoc_vs_poke8_tbl_r3[loopCount].vs_poke8_data_len)
                      ) !=  TRUE)
        {
          printf("bt_hci_qcomm_init Failed Poke VS Set Cmds");
          returnStatus = FALSE;
          return returnStatus;
        }
       loopCount++;
     }
    memcpy(&soc_cfg_parameters.run_time_params,&run_time_params,
           sizeof(bt_qsoc_config_params_struct_type));
    soc_cfg_parameters.soc_type = soc_type;
    soc_cfg_parameters.nvm_mode = nvm_mode;
  }
  else
  {
    nvm_status = bt_qsoc_nvm_open(soc_type, nvm_mode, &run_time_params);

    if(nvm_status != BT_QSOC_NVM_STATUS_SUCCESS)
      return FALSE;
    global_state = FTM_SOC_DOWNLOAD_NVM;
    /* Send all the NVM data to the SOC */
    returnStatus = ftm_bt_hci_hal_retrieve_send_nvm();
  }

  return returnStatus;
}
#endif
/*===========================================================================
FUNCTION   ftm_bt_hal_soc_init

DESCRIPTION
 Opens the  handle to UART/SMD, configures the BT SoC high level power,
 and initiates a read for application version

DEPENDENCIES
  NIL

RETURN VALUE
  RETURN VALUE
  FALSE = failure, else TRUE

SIDE EFFECTS
  None

===========================================================================*/
boolean ftm_bt_hal_soc_init(int mode)
{
  request_status ret = 0;
  int i,init_success = 0;
  char value;

  if(!is_transportSMD && !isLatestTarget()){
    if(ftm_bt_power_hal_check() != BT_ON)
    {
      ret = ftm_bt_power_hal_set(BT_ON);
      if(ret != STATUS_SUCCESS)
      {
          return FALSE;
      }
    }
    else
      return FALSE;
  }

  ret = ftm_bt_hci_hal_init_transport(mode) ;
#ifdef FTM_DEBUG
  printf("Transport open ret = %d\n",ret);
#endif
  if(ret != STATUS_SUCCESS)
  {
    return FALSE;
  }
  if(mode != MODE_FM) {
  /*ToDo:  this can be featuturized under USE_LIBSOCCFG */
    return ftm_bt_hci_hal_read_app_version();
  }
  return TRUE;
}

/*===========================================================================
FUNCTION   ftm_bt_dispatch

DESCRIPTION
  Processes the BT FTM packet and dispatches the command to FTM HCI driver

DEPENDENCIES
  NIL

RETURN VALUE
  NIL,The error in the Command Processing is sent to the DIAG App on PC via
  log packets

SIDE EFFECTS
  None

===========================================================================*/
void ftm_bt_dispatch(void *ftm_bt_pkt ,int cmd_len )
{
  int ret;
  memcpy(bt_ftm_buffer, (void*)ftm_bt_pkt, cmd_len);
  ret = ftm_bt_hci_send_cmd((uint8 *) bt_ftm_buffer, cmd_len);
  if (ret != TRUE)
  {
    ftm_log_send_msg(&event_buf_user_unknown_err[0],logsize);
    printf("Error->Send FTM command failed:: %d\n", ret);
    /** We had a premature exit here even before the command is Queued
    * So notify the semaphore to wait for the next command
    */
    sem_post(&semaphore_cmd_complete);
    return ;
  }
  return ;
}

/*===========================================================================
FUNCTION   ftm_bt_hci_send_cmd

DESCRIPTION
 Helper Routine to process the HCI cmd and invokes the sub routines to intialise
 /deinitialise the SoC if needed based on the state of the FTM module

DEPENDENCIES
  NIL

RETURN VALUE
  RETURN VALUE
  FALSE = failure, else TRUE

SIDE EFFECTS
  None

===========================================================================*/
boolean ftm_bt_hci_send_cmd
(
  uint8 * cmd_buf,   /* pointer to Cmd */
  uint16 cmd_len     /* Cmd length */
)
{
  request_status ret = 0;
  boolean status = FALSE;
  if(NULL == cmd_buf)
  {
    return FALSE;
  }
 #ifdef BT_NV_SUPPORT
  if (*cmd_buf == FTM_BT_CMD_NV_READ)
  {
    status = ftm_bt_send_nv_read_cmd(cmd_buf, cmd_len);
    return status;
  }
  if (*cmd_buf == FTM_BT_CMD_NV_WRITE)
  {
    status = ftm_bt_send_nv_write_cmd(cmd_buf, cmd_len);
    return status;
  }
#endif /* End of BT_NV_SUPPORT */

  if (*cmd_buf == FTM_BT_DRV_START_TEST)
  {
  /** Deinit the queue only if we are not initialised */
    if(global_state != FTM_SOC_NOT_INITIALISED)
    {
      ftm_bt_hci_hal_deinit_transport();
      if(!is_transportSMD && !isLatestTarget() )
        ftm_bt_power_hal_set(BT_OFF);
    }
    if(ftm_bt_conn_test() != TRUE)
       return FALSE;
#ifdef FTM_DEBUG
    printf("\nBT Soc Shutdown\n");
#endif
    global_state = FTM_SOC_NOT_INITIALISED;
  }

  if(global_state == FTM_SOC_NOT_INITIALISED)
  {
#ifdef HAS_BLUEZ_BUILDCFG
    // BT disabled for FTM to procceed
    // BT test is only aplicable for BLUEZ stack
    if(system("/system/xbin/bttest disable") !=  0)
    {
      printf("\nbttest disable failed");
    }
    else
    {
      /* Bluetooth resources like bluetoothd & hciattach (if applicable
      * based on transport) are asynchronously cleaned.
      * This delay ensures that the transport device is released before
      * being used by BT-FTM module.
      * Note: This delay as expected is < 5 seconds timeout set up for
      *       the command complete of the received BT FTM commmand. */
      printf("\nsleep for 2 seconds");
      usleep(2000000);
    }
#endif
    ptr_powerup.cmd_buf = cmd_buf;
    ptr_powerup.cmd_len = cmd_len;
    /*To identify the transport based on the target name*/
    is_transportSMD = ftm_bt_hci_hal_set_transport();
    /* Creating power up thread for asynchronous completion of request */
    status = ftm_bt_hal_soc_init(MODE_BT);
    if (*cmd_buf == FM_HCI_CMD_PKT)
      status = ftm_bt_hal_soc_init(MODE_FM);
  }
  else
  {
    ret = ftm_bt_hci_hal_nwrite((uint8 *) cmd_buf, cmd_len);
    if(ret == STATUS_SUCCESS)
      status = TRUE;
  }
  return status;
}
/*===========================================================================
FUNCTION   ftm_bt_hci_hal_read_event

DESCRIPTION
 Helper Routine to read the HCI event by invoking the UART/SMD HAL read routines
 and returns the event in the pointer passed

DEPENDENCIES
  NIL

RETURN VALUE
  RETURN VALUE
  FALSE = failure, else TRUE

SIDE EFFECTS
  None

===========================================================================*/
boolean ftm_bt_hci_hal_read_event (uint8 * event_buf_ptr)
{
  boolean status = FALSE;
  boolean long_event = FALSE;
  request_status   rx_status;
  int event_bytes;
  int i, ret_val;
 /*hci packet is not required to carry the Packet indicator (for UART interfaces) for msm8960
     as it is using share memory interface */
  rx_status = ftm_bt_hci_hal_nread(event_buf_ptr , PROTOCOL_BYTE_SIZE);
  if (rx_status == STATUS_SHORT_READ)
  {
    printf("ftm_bt_hci_qcomm_handle_event: VERY SHORT READ!\n");
    return status;
  }

  printf("%s:protocol byte: %02X\n", __FUNCTION__, event_buf_ptr[0]);
  /* else get rest of the packet */
  if(event_buf_ptr[0] == BT_HCI_ACL_PKT)
  {
    rx_status = ftm_bt_hci_hal_nread(event_buf_ptr + PROTOCOL_BYTE_SIZE, HCI_ACL_HDR_SIZE - PROTOCOL_BYTE_SIZE);
    if (rx_status == STATUS_SHORT_READ)
    {
      printf("ftm_bt_hci_qcomm_handle_event: VERY SHORT READ!\n");
      return status;
    }

    event_bytes = ( event_buf_ptr[HCI_ACL_HDR_SIZE - 1 ] << 8 ) | ( event_buf_ptr[HCI_ACL_HDR_SIZE - 2 ] ) ;
    if (HC_VS_MAX_ACL < event_bytes)
    {
      printf("ftm_bt_hci_qcomm_handle_event: LONG ACL PKT!\n");
      long_event = TRUE;
      event_bytes = HC_VS_MAX_ACL;
    }

    rx_status = ftm_bt_hci_hal_nread(&(event_buf_ptr[HCI_ACL_HDR_SIZE]), event_bytes);

    if (rx_status == STATUS_SUCCESS)
    {
      status = TRUE;
    }
    else
    {
      printf("ftm_bt_hci_qcomm_handle_event: SHORT READ!\n");
      fflush (stderr);
    }

    event_bytes += HCI_ACL_HDR_SIZE;
  }
  else if((event_buf_ptr[0] == BT_HCI_EVT_PKT) || (event_buf_ptr[0] == FM_HCI_EVT_PKT))
  {
    rx_status = ftm_bt_hci_hal_nread(event_buf_ptr + PROTOCOL_BYTE_SIZE, HCI_EVT_HDR_SIZE - PROTOCOL_BYTE_SIZE);
    if (rx_status == STATUS_SHORT_READ)
    {
      printf("ftm_bt_hci_qcomm_handle_event: VERY SHORT READ!\n");
      return status;
    }

    event_bytes = event_buf_ptr[HCI_EVT_HDR_SIZE - 1 ];
    if (HC_VS_MAX_CMD_EVENT < event_bytes)
    {
      printf("ftm_bt_hci_qcomm_handle_event: LONG EVENT!\n");
      long_event = TRUE;
      event_bytes = HC_VS_MAX_CMD_EVENT;
    }
    rx_status = ftm_bt_hci_hal_nread(&(event_buf_ptr[HCI_EVT_HDR_SIZE]), event_bytes);

    if (rx_status == STATUS_SUCCESS)
    {
      status = TRUE;
    }
    else
    {
      printf("ftm_bt_hci_qcomm_handle_event: SHORT READ!\n");
      fflush (stderr);
    }

    event_bytes += HCI_EVT_HDR_SIZE;
  }
  else
  {
    printf("ftm_bt_hci_qcomm_handle_event: Unknown packet type!\n");
    return status;
  }

  /*
  ** Validate if the loopback command event has arrived and has  succsfull
  ** response from FW, if yes enable slimbus to validate pinc connectivity
  ** test
  */

  if ( event_buf_ptr[0] == BT_HCI_EVT_PKT &&
       event_buf_ptr[LOOP_BACK_EVT_OGF_BIT] == LOOP_BACK_EVT_OGF &&
       event_buf_ptr[LOOP_BACK_EVT_OCF_BIT] == LOOP_BACK_EVT_OCF &&
       event_buf_ptr[LOOP_BACK_EVT_STATUS_BIT] == LOOP_BACK_EVT_STATUS &&
       is_slim_bus_test == 1)
  {
      printf("\nInitializing slim bus for pin-connectivity\n");
      fd_pintest = open("/dev/pintest",O_RDONLY, O_NONBLOCK);
      if(fd_pintest < 0)
          printf("\nfailed to open\n");
      ret_val = ioctl(fd_pintest, BT_CMD_SLIM_TEST, NULL);
      event_buf_ptr[PIN_CON_EVENT_LEN_BIT] = PIN_CON_EVENT_LEN;
      event_buf_ptr[PIN_CON_EVT_OCF_BIT] = PIN_CON_CMD_OCF;
      event_buf_ptr[PIN_CON_EVT_OGF_BIT] = PIN_CON_CMD_OGF;
      event_buf_ptr[PIN_CON_EVT_SUB_OP_BIT] = PIN_CON_CMD_SUB_OP;
      event_buf_ptr[PIN_CON_INTERFACE_ID_EVT_BIT] = PIN_CON_INTERFACE_ID;
      event_bytes += EXT_PIN_CON_LEN;
      if( ret_val < 0) {
          event_buf_ptr[PIN_CON_EVT_STATUS_BIT] = ret_val;
          printf("\nFailed to initialise slim bus %d\n", ret_val);
          status = FALSE;
      } else {
          event_buf_ptr[PIN_CON_EVT_STATUS_BIT] = 0;
          printf("\nSlim bus initiazed succesfully\n");
      }
  }

  if (verbose == 1)
  {
    if((event_buf_ptr[0] == BT_HCI_EVT_PKT) || (event_buf_ptr[0] == FM_HCI_EVT_PKT))
    {
      printf("[%s] %s: EVT:", get_current_time(), __FUNCTION__);
      for (i = 0; i < event_bytes; i++)
      {
        printf(" %02X", event_buf_ptr[i]);
      }

      printf(long_event? " ...\n": "\n");
    }
    else if (event_buf_ptr[0] == BT_HCI_ACL_PKT)
    {
      printf("[%s] %s: ACL packet: %d bytes\n", get_current_time(), __FUNCTION__, event_bytes);

      printf(long_event? " ...\n": "\n");
    }
  }
  ftm_log_send_msg(event_buf_ptr,event_bytes);
  return status;
}

boolean ftm_bt_hci_hal_retrieve_nvm_and_send_efs(FILE* fp)
{
  unsigned char payload[NVM_PAYLOAD_MAXLENGTH];
  unsigned char header[HCI_CMD_HDR_SIZE];
  int n = 0;
  int len=0;
  static const bt_qsoc_cfg_tbl_struct_type bt_qsoc_tag27 =
  {
    0x04,   {0x01, 0x1B, 0x01, 0x00}
  };

  n = fread(header, 1, HCI_CMD_HDR_SIZE, fp);

  if(feof(fp))
  {
    if(nvm_cmd)
      free(nvm_cmd);
    fclose(fp);
    sleep_stage = 0;
    if(!isLatestTarget()){//Do not disable sleep for ROME
    global_state = FTM_SOC_SLEEP_DISABLE;
    if ( (ftm_bt_hci_hal_vs_sendcmd (
      BT_QSOC_NVM_ACCESS_OPCODE,(uint8 *)(bt_qsoc_tag27.vs_cmd_data),
      bt_qsoc_tag27.vs_cmd_len )) != TRUE )
      return FALSE;
    sleep_stage++;
    }
    return TRUE;
  }

  /*Last byte gives the length*/
  len = (int)header[3];

   printf("PayLoad length: %d\n",  len);
   n = fread(payload, 1, len, fp);

   /*Form the resultant buffer*/
   if(nvm_cmd)
   {
     /*Delete the previous buffer*/
     free(nvm_cmd);
   }
   nvm_cmd = (unsigned char*)malloc(HCI_CMD_HDR_SIZE+len);
   if(nvm_cmd)
   {
      memcpy(nvm_cmd, header, HCI_CMD_HDR_SIZE);
      memcpy(nvm_cmd+HCI_CMD_HDR_SIZE, payload, len);

      global_state = FTM_SOC_DOWNLOAD_NVM_EFS;

      if(ftm_bt_hci_hal_nwrite((uint8 *) &nvm_cmd[0],HCI_CMD_HDR_SIZE+len) == STATUS_SUCCESS)
        return TRUE;
   }
   return FALSE;
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

boolean ftm_bt_hci_qcomm_handle_event ()
{
  boolean status = TRUE;
  char filename[MAX_FILE_NAME];
#ifdef USE_LIBSOCCFG
  bt_qsoc_nvm_status nvm_status;
#endif
  int ret = 0;
#ifdef FTM_DEBUG
  printf("ftm_bt_hci_qcomm_handle_event global_state --> %d\n",global_state);
#endif
  switch(global_state)
  {
    case FTM_SOC_READ_APP_VER:
      /* Read the VS event*/
      if(ftm_bt_hci_hal_read_event(event_buf) != TRUE) {
         printf("Failed to read VS event");
         return FALSE;
      }
      ftm_bt_hci_hal_vs_event(&event_buf[1], 2 + event_buf[2]);
      /* Read out the HCI event*/
      if(ftm_bt_hci_hal_read_event(event_buf) != TRUE) {
         printf("Failed to read HCI event");
         return FALSE;
      }
      /* Until libsoccfg is part of the Android system
       * we will use the system call to perform the
       * soc initialisation
       */
#ifndef USE_LIBSOCCFG
      if(!isLatestTarget())
      {
        ret = system("/system/bin/hci_qcomm_init -e -H");
        if(ret !=  0)
        {
          return FALSE;
        }
        else
        {
          global_state = FTM_SOC_INITIALISED;
        }
      }
      else
      {
#if 0//Do not Disable Sleep for ROME
        global_state = FTM_SOC_SLEEP_DISABLE;
        if ( (ftm_bt_hci_hal_vs_sendcmd(BT_QSOC_NVM_ACCESS_OPCODE,
                (uint8 *)(bt_qsoc_tag17_latest_hw.vs_cmd_data),
                bt_qsoc_tag17.vs_cmd_len )) != TRUE )
        {
          return FALSE;
        }
#endif
      if(ftm_bt_hci_hal_send_reset_cmd() != TRUE)
         return FALSE;
      }
      printf("\nFTM Global state = %d\n",global_state);
      // Dont send HCI reset before inband sleep disable
      if (!isLatestTarget())
      {
        if(ptr_powerup.cmd_buf[0] == FTM_BT_DRV_START_TEST)
        {
          ftm_log_send_msg(&event_buf_user_cmd_pass[0],logsize);
          sem_post(&semaphore_cmd_complete);
          return TRUE;
        }
        if(ftm_bt_hci_hal_nwrite((uint8 *) ptr_powerup.cmd_buf,
                        ptr_powerup.cmd_len) != STATUS_SUCCESS)
          return FALSE;
      }
#else
      if(ftm_bt_hci_hal_read_hw_version() != TRUE)
        return FALSE;
#ifdef FTM_DEBUG
      printf("FTM_SOC_READ_APP_VER Done\n");
#endif
#endif
      break;
#ifdef USE_LIBSOCCFG
    case FTM_SOC_READ_HW_VER:
      /* Read the VS event*/
      if(ftm_bt_hci_hal_read_event(event_buf) != TRUE)
        return FALSE;
      ftm_bt_hci_hal_vs_event(&event_buf[1], 2 + event_buf[2]);
      /* Read out the HCI event*/
      if(ftm_bt_hci_hal_read_event(event_buf) != TRUE)
        return FALSE;

      if(ftm_bt_hci_hal_nvm_download_init() != TRUE)
        return FALSE;
#ifdef FTM_DEBUG
      printf("FTM_SOC_READ_HW_VER Done\n");
#endif
      break;
    case FTM_SOC_POKE8_TBL_INIT :
      /* Read the VS event*/
      if(ftm_bt_hci_hal_read_event(event_buf) != TRUE)
        return FALSE;
       /* Read out the HCI event*/
      if(ftm_bt_hci_hal_read_event(event_buf) != TRUE)
        return FALSE;
      if(loopCount < BT_QSOC_R3_POKETBL_COUNT)
      {
        if ( (ftm_bt_hci_hal_vs_sendcmd(
                    BT_QSOC_EDL_CMD_OPCODE,
                    (uint8 *)bt_qsoc_vs_poke8_tbl_r3[loopCount].vs_poke8_data,
                    bt_qsoc_vs_poke8_tbl_r3[loopCount].vs_poke8_data_len)
                    ) !=  TRUE)
        {
          printf("bt_hci_qcomm_init Failed Poke VS Set Cmds");
          return FALSE;
        }
        loopCount++;
     }
     else
     {
       nvm_status = bt_qsoc_nvm_open(soc_cfg_parameters.soc_type,
                                    soc_cfg_parameters.nvm_mode,
                                    &soc_cfg_parameters.run_time_params);

       if(nvm_status != BT_QSOC_NVM_STATUS_SUCCESS)
          return FALSE;
        global_state = FTM_SOC_DOWNLOAD_NVM;
        /* Send all the NVM data to the SOC */
        if(ftm_bt_hci_hal_retrieve_send_nvm() != TRUE)
          return FALSE;
      }
      break;
    case FTM_SOC_DOWNLOAD_NVM:
      /* Read the VS event*/
      if(ftm_bt_hci_hal_read_event(event_buf) != TRUE)
        return FALSE;
       /* Read out the HCI event*/
      if(ftm_bt_hci_hal_read_event(event_buf) != TRUE)
        return FALSE;
      if(ftm_bt_hci_hal_retrieve_send_nvm()!= TRUE)
        return FALSE;
#ifdef FTM_DEBUG
      printf("FTM_SOC_DOWNLOAD_NVM in progress\n");
#endif
      break;
#endif //USE_LIBSOCCFG

     case FTM_SOC_DOWNLOAD_NVM_EFS:
      /* Read the VS event*/
      if(ftm_bt_hci_hal_read_event(event_buf) != TRUE)
        return FALSE;
       /* Read out the HCI event*/
      if(ftm_bt_hci_hal_read_event(event_buf) != TRUE)
        return FALSE;
      if(ftm_bt_hci_hal_retrieve_nvm_and_send_efs(fp) != TRUE)
        return FALSE;
      printf("FTM_SOC_DOWNLOAD_NVM in progress\n");
      break;

    case FTM_SOC_SLEEP_DISABLE:
      /* Read the VS event*/
      if(ftm_bt_hci_hal_read_event(event_buf) != TRUE)
        return FALSE;
      /* Read out the HCI event*/
      if(ftm_bt_hci_hal_read_event(event_buf) != TRUE)
        return FALSE;
      if(sleep_stage == 1)
      {
#ifdef FTM_DEBUG
        printf("Sleep Stage tag 17 set\n");
#endif
        if ( (ftm_bt_hci_hal_vs_sendcmd(BT_QSOC_NVM_ACCESS_OPCODE,
		(uint8 *)(bt_qsoc_tag17.vs_cmd_data),
		bt_qsoc_tag17.vs_cmd_len )) != TRUE )
        {
          return FALSE;
        }
        sleep_stage++;
        return TRUE;
      }
     if(ftm_bt_hci_hal_send_reset_cmd() != TRUE)
        return FALSE;
#ifdef FTM_DEBUG
      printf("FTM_SOC_SLEEP_DISABLE done\n");
#endif
      break;
    case FTM_SOC_RESET:
       /* Read out the HCI event*/
      if(ftm_bt_hci_hal_read_event(event_buf) != TRUE)
        return FALSE;
      global_state = FTM_SOC_INITIALISED;
      if(ptr_powerup.cmd_buf[0] == FTM_BT_DRV_START_TEST)
      {
        ftm_log_send_msg(&event_buf_user_cmd_pass[0],logsize);
        sem_post(&semaphore_cmd_complete);
        return TRUE;
      }
      if(ftm_bt_hci_hal_nwrite((uint8 *) ptr_powerup.cmd_buf,
                               ptr_powerup.cmd_len) != STATUS_SUCCESS)
        return FALSE;
#ifdef FTM_DEBUG
      printf("FTM_SOC_RESET done queued the Command\n");
#endif
      break;
    case FTM_SOC_INITIALISED :
      /* Read out the HCI event*/
      if(ftm_bt_hci_hal_read_event(event_buf) != TRUE)
        return FALSE;
      sem_post(&semaphore_cmd_complete);
      break;
    default :
      return FALSE;
  }
  return status;
}

/*===========================================================================
FUNCTION   isLatestTarget

DESCRIPTION
For all the target/solution which has Bluedroid as stack and libbt-vendor as
vendor initialization component considered as latest target

DEPENDENCIES
  NIL

RETURN VALUE
  RETURN VALUE
  FALSE = failure, else TRUE

SIDE EFFECTS
  None

===========================================================================*/
boolean isLatestTarget()
{
#ifdef ANDROID
    int ret = 0;
    char bt_soc_type[PROPERTY_VALUE_MAX];
    ret = property_get("qcom.bluetooth.soc", bt_soc_type, NULL);
    if (ret != 0)
    {
      if (!strncasecmp(bt_soc_type, "ath3k", sizeof("ath3k")))
      {
        return FALSE;
      }
    }
    return TRUE;
#else
    return TRUE;
#endif
}
