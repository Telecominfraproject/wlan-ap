/*=========================================================================
                        NFC FTM Source File
Description
   This file contains the routines to communicate with the NFCC in FTM mode.

Copyright (c) 2015 Qualcomm Technologies, Inc.
All Rights Reserved.
Confidential and Proprietary - Qualcomm Technologies, Inc.
===========================================================================*/
/*===========================================================================
                         Edit History
when        who              what,              where,     why
--------   ---     ----------------------------------------------------------
===========================================================================*/
/*==========================================================================*
*                             INCLUDE FILES                                *
*==========================================================================*/
#include "ftm_nfcqti.h"

#define UNUSED(x) (void)(x)

/*=========================================================================*
*                            file scope local defnitions                   *
*==========================================================================*/
const hw_module_t* hw_module = NULL;
nfc_nci_device_t* dev = NULL;
uint8 hal_state = NCI_HAL_INIT, nfc_ftmthread = FALSE;
uint8 *nfc_cmd_buff = NULL, len = 0;
uint16 res_len = 0, async_msg_cnt = 0;
uint8  *response_buff = NULL;
static uint8 hal_opened = FALSE, wait_rsp = FALSE;
static uint8 async_msg_available = FALSE, ftm_data_rsp_pending = FALSE;
asyncdata *buff = NULL;
asyncdata *start = NULL;
/*I2C read/write*/
static uint8 i2c_cmd_cnt = 0;
uint8 i2c_status=0,i2c_req_write = FALSE, i2c_req_read = FALSE;
uint8 i2c_num_of_reg_to_read = 0, i2c_reg_read_data[40]={0}, ii = 0;
pthread_mutex_t nfcftm_mutex = PTHREAD_MUTEX_INITIALIZER;
/*===================================================================================*
*                             Function Defnitions                                    *
*===================================================================================*/
/*====================================================================================
FUNCTION   nfc_ftm_hal_cback
DESCRIPTION
  This is the call back function which will indicate if the nfc hal open is successful
  or failed.
DEPENDENCIES
  NIL
RETURN VALUE
none
SIDE EFFECTS
  NONE
=====================================================================================*/
static void nfc_ftm_cback(uint8 event, uint8 status)
{
    switch(event)
    {
        case HAL_NFC_OPEN_CPLT_EVT:
             if(status == HAL_NFC_STATUS_OK)
             {
                 /* Release semaphore to indicate that hal open is done
                 and change the state to write.*/
                 hal_state = NCI_HAL_WRITE;
                 hal_opened = TRUE;
                 printf("HAL Open Success..state changed to Write  \n");
             }
             else
             {
                 printf("HAL Open Failed \n");
                 hal_state = NCI_HAL_ERROR;
                 hal_opened = FALSE;
             }
             sem_post(&semaphore_halcmd_complete);
             break;
        case HAL_NFC_CLOSE_CPLT_EVT:
             printf("HAL_NFC_CLOSE_CPLT_EVT recieved..\n");
             break;
        default:
             printf ("nfc_ftm_hal_cback unhandled event %x \n", event);
             break;
    }
}
/*==========================================================================================
FUNCTION   fill_async_data
DESCRIPTION
This function will store all the incoming async msgs( like ntfs and data from QCA1990)
in to a list to be committed further.
DEPENDENCIES
  NIL
RETURN VALUE
  NONE
SIDE EFFECTS
  NONE
==============================================================================================*/
void fill_async_data(uint16 data_len, uint8 *p_data)
{
    uint16 i = 0;
    asyncdata *next_node = NULL;
    printf("fill_async_data() function \n");
    /* Initialize a list which will store all async message untill they are sent*/
    if(buff == NULL)
    {
        /* first node creation*/
        buff = (asyncdata*)malloc(sizeof(asyncdata));
        if(buff)
        {
            start = buff;
            buff->response_buff = (uint8*)malloc(data_len);
            if(buff->response_buff)
            {
                memcpy(buff->response_buff, p_data, data_len);
                buff->async_datalen = data_len;
                buff->next = NULL;
                async_msg_cnt = 0;
                async_msg_cnt++;
            }
            else
            {
              printf("mem allocation failed while storing asysnc msg \n");
            }
        }
        else
        {
            printf("mem allocation failed while trying to make the async list \n");
        }
    }
    else
    {
        /* this is the case when some data is already present in the list which has not been sent yet*/
        next_node = (asyncdata*)malloc(sizeof(asyncdata));
        if(next_node)
        {
            next_node->response_buff = (uint8*)malloc(data_len);
            if(next_node->response_buff)
            {
                memcpy(next_node->response_buff, p_data,data_len);
                next_node->async_datalen = data_len;
                next_node->next = NULL;
                async_msg_cnt++;
                while(buff->next != NULL)
                {
                   buff = buff->next;
                }
                buff->next = next_node;
            }
            else
            {
                printf("mem allocation failed while storing asysnc msg \n");
            }
        }
        else
        {
           printf("mem allocation failed while trying to make the async list \n");
        }
    }
}
/*======================================================================================================
FUNCTION   nfc_ftm_data_cback
DESCRIPTION
  This is the call back function which will provide back incoming data from the QCA1990
  to nfc ftm.
DEPENDENCIES
  NIL
RETURN VALUE
  NONE
SIDE EFFECTS
  NONE
========================================================================================================*/
static void nfc_ftm_data_cback(uint16 data_len, uint8 *p_data)
{
    uint8 i = 0;
    if(hal_opened == FALSE)
    {
        /* Reject data call backs untill HAL in initialized */
        return;
    }
    if((data_len == 0x00) || (p_data == NULL))
    {
        printf("Error case : wrong data lentgh or buffer revcieved \n");
        return;
    }
    if((i2c_req_write == TRUE) || (i2c_req_read == TRUE))
    {
        if(i2c_req_write)
        {
            /*check the incoming status*/
            if(p_data[0] != 0x00) /* 0x00 = Command executed successfully*/
            {
                /* some error has occured in I2C write.Send the status code back now to pc app*/
                i2c_status = p_data[0];
                printf("Error occured in I2C write .. reporting to application..Error Code = %X \n", i2c_status);
                hal_state = NCI_HAL_READ;
                sem_post(&semaphore_halcmd_complete);
            }
            else
            {
                /*status is fine. Complete further requests as ftmdaemon is writing one by one*/
                if(len)
                {
                    /*send further addr and value pair*/
                    printf("I2C write status correct..sending next..\n");
                    hal_state = NCI_HAL_WRITE;
                }
                else
                {
                    /*All I2C write completed .Send final status to app*/
                     i2c_status = p_data[0];
                     printf(" All I2C write completed i2c_status = %X \n", i2c_status);
                     hal_state = NCI_HAL_READ;
                }
                sem_post(&semaphore_halcmd_complete);
            }
        }
        else
        {
            /*I2C read rsp arrived . fill it in buffer if correct or report error if wrong*/
            if(p_data[0] != 0x00)
            {
                /* some error has occured in I2C read.Send the status code to app*/
                i2c_status = p_data[0];
                printf("Error occured in I2C read .. reporting to application..Error Code = %X \n", i2c_status);
                hal_state = NCI_HAL_READ;
                memset(nfc_cmd_buff, 0, len);
                sem_post(&semaphore_halcmd_complete);
            }
            else
            {
                if(len)
                {
                    /*send further addr to read*/
                    i2c_status = p_data[0];
                    i2c_reg_read_data[ii++] = p_data[1];
                    hal_state = NCI_HAL_WRITE;
                }
                else
                {
                    /*All I2C read completed .Send the read data back to pc app*/
                    i2c_status = p_data[0];
                    i2c_reg_read_data[ii++] = p_data[1];
                    hal_state = NCI_HAL_READ;
                    ii = 0;
                 }
                 sem_post(&semaphore_halcmd_complete);
             }
        }
    }
    else
    {
        if(((p_data[0] & 0xF0) == 0x60 /*ntf packets*/) || ((p_data[0] & 0xF0) == 0x00)/*data packet rsps*/)
        {
            async_msg_available = TRUE;
            pthread_mutex_lock(&nfcftm_mutex);
            fill_async_data(data_len, p_data);
            pthread_mutex_unlock(&nfcftm_mutex);
            if(ftm_data_rsp_pending == TRUE)
            {
                printf("Sending data rsp \n");
                hal_state = NCI_HAL_READ;
                sem_post(&semaphore_halcmd_complete);
                ftm_data_rsp_pending = FALSE;
            }
            else
            {
                if((wait_rsp == FALSE) || ((p_data[0] == 0x60) && (p_data[1] == 0x00)))
                {
                    /*This is the case when ntf receieved after rsp is logged to pc app*/
                    printf("Sending async msg to logging subsystem \n");
                    hal_state = NCI_HAL_ASYNC_LOG;
                    sem_post(&semaphore_halcmd_complete);
                }
            }
        }
        else
        {
            if(response_buff || res_len)
            {
                printf("nfc_ftm_data_cback : response_buff = %p, res_len = %d", response_buff, res_len);
                return;
            }
            response_buff = (uint8*)malloc(data_len);
            if(response_buff)
            {
                memcpy(response_buff, p_data, data_len);
                res_len = data_len;
                printf("nfc_ftm_data_cback: res_len=%d data_len=%d  response_buff= %X %X %X %X %X %X  \n", res_len,data_len, \
                         response_buff[0],response_buff[1],response_buff[2],response_buff[3],response_buff[4],response_buff[5]);
                hal_state = NCI_HAL_READ;
                sem_post(&semaphore_halcmd_complete);
            }
            else
            {
                printf("Mem allocation failed in nfc_ftm_data_cback \n");
            }
        }
    }
}
/*===========================================================================
FUNCTION   ftm_nfc_hal_open
DESCRIPTION
   This function will open the nfc hal for ftm nfc command processing.
DEPENDENCIES
  NIL
RETURN VALUE
  void
SIDE EFFECTS
  NONE
===============================================================================*/
uint8 ftm_nfc_hal_open(void)
{
    uint8 ret = 0;
    ret = hw_get_module(NFC_NCI_HARDWARE_MODULE, &hw_module);
    if(ret == 0)
    {
        dev = (nfc_nci_device_t*)malloc(sizeof(nfc_nci_device_t));
        if(!dev)
        {
            printf("NFC FTM : mem allocation failed \n");
            return FALSE;
        }
        else
        {
            ret = nfc_nci_open (hw_module, &dev);
            if(ret != 0)
            {
                printf("NFC FTM : nfc_nci_open fail \n");
                free(dev);
                return FALSE;
            }
            else
            {
                printf("NFC FTM : opening NCI HAL \n");
                dev->common.reserved[0] = FTM_MODE;
                dev->open (dev, nfc_ftm_cback, nfc_ftm_data_cback);
                sem_wait(&semaphore_halcmd_complete);
            }
        }
    }
    else
    {
       printf("NFC FTM : hw_get_module() call failed \n");
       return FALSE;
    }
    return TRUE;
}
/*=================================================================================================
FUNCTION   ftm_nfc_log_send_msg
DESCRIPTION
This function will log the asynchronous messages(NTFs and data packets) to the logging subsystem
 of DIAG.
DEPENDENCIES
RETURN VALUE
TRUE if data logged successfully and FALSE if failed.
SIDE EFFECTS
  None
==================================================================================================*/
int ftm_nfc_log_send_msg(void)
{
    uint16 i = 0;
    ftm_nfc_log_pkt_type* ftm_nfc_log_pkt_ptr = NULL;
    asyncdata* node = NULL;
    uint8 arr[1]= {'\n'};
    if(log_status(LOG_NFC_FTM))
    {
        buff = start;
        if(buff != NULL)
        {
            do{
                  printf("buff->async_datalen : %d \n", buff->async_datalen);
                  ftm_nfc_log_pkt_ptr = (ftm_nfc_log_pkt_type *)log_alloc(LOG_NFC_FTM, (FTM_NFC_LOG_HEADER_SIZE + (buff->async_datalen)));
                  if(ftm_nfc_log_pkt_ptr)
                  {
                      memcpy((void *)ftm_nfc_log_pkt_ptr->data, (void *)buff->response_buff, buff->async_datalen);
                      printf("Async msg is = ");
                      for(i=0; i<buff->async_datalen; i++)
                      {
                          printf("%X ", ftm_nfc_log_pkt_ptr->data[i]);
                      }
                      printf("%c",arr[0]);
                      node = buff;
                      buff = buff->next;
                      free(node);
                      printf("Commiting the log message(async msg) \n");
                      log_commit(ftm_nfc_log_pkt_ptr);
                  }
                  else
                  {
                      printf("\nmem alloc failed in log_alloc \n");
                      return FALSE;
                  }
              }while(buff != NULL);
              printf("all msgs committed \n");
              async_msg_available = FALSE;
              return TRUE;
          }
          else
          {
              printf("No async message left to be logged \n");
          }
     }
     else
     {
         printf("LOG_NFC_FTM code is not enabled in logging subsystem \n");
     }
     return FALSE;
}
/*===========================================================================
FUNCTION   nfc_ftm_readerthread
DESCRIPTION
  Thread Routine to perfom asynchrounous handling of events coming from
  NFCC. It will perform read and write for all type of commands/data.
DEPENDENCIES
RETURN VALUE
  RETURN NIL
SIDE EFFECTS
  None
===========================================================================*/
void* nfc_ftm_thread(void *ptr)
{
    uint8 i2c_buff[3] = {0};

    UNUSED(ptr);

    while(1)
    {
       printf("Waiting for Cmd/Rsp \n");
       sem_wait (&semaphore_halcmd_complete);
       switch(hal_state)
       {
           case NCI_HAL_INIT:
                printf("NFC FTM : HAL Open request recieved..\n");
                if(ftm_nfc_hal_open() == FALSE)
                {
                    hal_state = NCI_HAL_ERROR;
                    hal_opened = FALSE;
                }
                else
                {
                    break;
                }
           case NCI_HAL_ERROR:
                /* HAL open failed.Post sem and handle error case*/
                sem_post(&semaphore_nfcftmcmd_complete);
                break;
           case NCI_HAL_WRITE:
                if(dev != NULL)
                {
                    printf("NFC FTM : Cmd recieved for nfc ftm..sending.\n");
                    if((!i2c_req_write) && (!i2c_req_read))
                    {
                        /* send data to the NFCC*/
                        if(nfc_cmd_buff[0] == 0x00 /*data req*/)
                        {
                            printf("Data send request arrived \n");
                            ftm_data_rsp_pending = TRUE;
                        }
                        else
                        {
                            printf("cmd request arrived \n");
                            wait_rsp = TRUE;
                        }
                        dev->write(dev, len, nfc_cmd_buff);
                    }
                    else
                    {
                        if(i2c_req_write)
                        {
                            i2c_buff[0] = 0xFF;
                            i2c_buff[1] = nfc_cmd_buff[i2c_cmd_cnt++]; /* addr*/
                            i2c_buff[2] = nfc_cmd_buff[i2c_cmd_cnt++]; /*value*/
                            len -=2;
                            dev->write(dev, 3, i2c_buff);
                        }
                        else
                        {
                            /* I2c Read req*/
                            i2c_buff[0] = 0xFF;
                            i2c_buff[1] = nfc_cmd_buff[i2c_cmd_cnt++]; /* I2C addr to read*/
                            i2c_reg_read_data[ii++] = i2c_buff[1]; /* store address to send in response.*/
                            len -= 1;
                            dev->write(dev, 2, i2c_buff);
                        }
                    }
                }
                else
                {
                    printf("dev is null \n");
                }
                break;
           case NCI_HAL_READ:
                /* indicate to ftm that response is avilable now*/
                sem_post(&semaphore_nfcftmcmd_complete);
                 printf("NFC FTM : State changed to READ  i2c_req_read: %d\n",i2c_req_read);
                break;
           case NCI_HAL_ASYNC_LOG:
                /* indicate to ftm that response is avilable now*/
                printf("NFC FTM : State changed to NCI_HAL_ASYNC_LOG.Logging aysnc message \n");
                pthread_mutex_lock(&nfcftm_mutex);
                if(ftm_nfc_log_send_msg())
                {
                    printf("async msgs commited to the log system..changing HAL state to write \n");
                }
                else
                {
                   printf("async msgs commit failed..changing HAL state to write \n");
                }
                hal_state = NCI_HAL_WRITE;
                pthread_mutex_unlock(&nfcftm_mutex);
                break;
           default:
                break;
       }
    }
}
/*===========================================================================
FUNCTION   ftm_nfc_dispatch
DESCRIPTION
This is the function which will be called by the NFC FTM layer callback function
registered with the DIAG service./
DEPENDENCIES
RETURN VALUE
  RETURN rsp pointer(containing the NFCC rsp packets) to the callback function
  (subsequently for DIAG service)
SIDE EFFECTS
  None
===========================================================================*/
void* ftm_nfc_dispatch_qti(ftm_nfc_pkt_type *nfc_ftm_pkt, uint16 pkt_len)
{
    ftm_nfc_i2c_write_rsp_pkt_type *i2c_write_rsp = NULL;
    ftm_nfc_i2c_read_rsp_pkt_type *i2c_read_rsp = NULL;
    ftm_nfc_pkt_type *rsp = NULL;
    ftm_nfc_data_rsp_pkt_type *nfc_data_rsp = NULL;
    struct timespec time_sec;
    int sem_status;

    UNUSED(pkt_len);

    printf("NFC FTM : nfc ftm mode requested \n");
    if(nfc_ftm_pkt == NULL)
    {
        printf("Error : NULL packet recieved from DIAG \n");
        goto error_case;
    }
    /* Start nfc_ftm_thread which will process all requests as per
       state machine flow. By Default First state will be NCI_HAL_INIT*/
    if(!nfc_ftmthread)
    {
        if(sem_init(&semaphore_halcmd_complete, 0, 1) != 0)
        {
            printf("NFC FTM :semaphore_halcmd_complete creation failed \n");
            goto error_case;
        }
        if(sem_init(&semaphore_nfcftmcmd_complete, 0, 0) != 0)
        {
            printf("NFC FTM :semaphore_nfcftmcmd_complete creation failed \n");
            goto error_case;
        }
        printf("NFC FTM : nfc ftm thread is being started \n");
        pthread_create(&nfc_thread_handle, NULL, nfc_ftm_thread, NULL);
        nfc_ftmthread = TRUE;
    }
    /* parse the diag packet to identify the NFC FTM command which needs to be sent
       to QCA 1990*/
    if(nfc_ftm_pkt->ftm_nfc_hdr.nfc_cmd_len > 2)
    {
        len = nfc_ftm_pkt->ftm_nfc_hdr.nfc_cmd_len-2;
    }
    else
    {
        /*Wrong nfc ftm packet*/
        goto error_case;
    }
    switch(nfc_ftm_pkt->ftm_nfc_hdr.nfc_cmd_id)
    {
        case FTM_NFC_I2C_SLAVE_WRITE:
             i2c_req_write = TRUE;
             break;
        case FTM_NFC_I2C_SLAVE_READ:
             i2c_num_of_reg_to_read = len;
             i2c_req_read = TRUE;
             break;
        case FTM_NFC_NFCC_COMMAND:
        case FTM_NFC_SEND_DATA:
             break;
        default :
             goto error_case;
             break;
    }
    /*copy command to send it further to QCA1990*/
    nfc_cmd_buff = (uint8 *)malloc(len+1);
    if(nfc_cmd_buff)
    {
        memcpy(nfc_cmd_buff, nfc_ftm_pkt->nci_data, len);
    }
    else
    {
        printf("Mem allocation failed for cmd storage");
        goto error_case;
    }
    /*send the command */
    sem_post(&semaphore_halcmd_complete);
    printf("\nwaiting for nfc ftm response \n");
    if (clock_gettime(CLOCK_REALTIME, &time_sec) == -1)
    {
        printf("get clock_gettime error");
    }
    time_sec.tv_sec += FTM_NFC_CMD_CMPL_TIMEOUT;
    sem_status = sem_timedwait(&semaphore_nfcftmcmd_complete,&time_sec);
    if(sem_status == -1)
    {
        printf("nfc ftm command timed out\n");
        goto error_case;
    }
    if(!hal_opened)
    {
        /*Hal open is failed */
        free(nfc_cmd_buff);
        hal_state = NCI_HAL_INIT;
        goto error_case;
    }
    printf("\n\n *****Framing the response to send back to Diag service******** \n\n");
    /* Frame the response as per the cmd request*/
    switch(nfc_ftm_pkt->ftm_nfc_hdr.nfc_cmd_id)
    {
        case FTM_NFC_I2C_SLAVE_WRITE:
             printf("Framing the response for FTM_NFC_I2C_SLAVE_WRITE cmd \n");
             i2c_write_rsp = (ftm_nfc_i2c_write_rsp_pkt_type*)diagpkt_subsys_alloc(DIAG_SUBSYS_FTM,
                                                                                   FTM_NFC_CMD_CODE,
                                                                                   sizeof(ftm_nfc_i2c_write_rsp_pkt_type));
             if(i2c_write_rsp)
             {
                 i2c_write_rsp->nfc_i2c_slave_status = i2c_status;
                 i2c_status = 0;
                 i2c_cmd_cnt = 0;
                 i2c_req_write = FALSE;
             }
             break;
       case  FTM_NFC_I2C_SLAVE_READ:
             printf("Framing the response for FTM_NFC_I2C_SLAVE_READ cmd \n");
             i2c_read_rsp = (ftm_nfc_i2c_read_rsp_pkt_type*)diagpkt_subsys_alloc(DIAG_SUBSYS_FTM,
                                                                                 FTM_NFC_CMD_CODE,
                                                                                 sizeof(ftm_nfc_i2c_read_rsp_pkt_type));
             if(i2c_read_rsp)
             {
                 i2c_read_rsp->ftm_nfc_hdr.nfc_cmd_id = FTM_NFC_I2C_SLAVE_READ;
                 i2c_read_rsp->ftm_nfc_hdr.nfc_cmd_len = 2+(2*i2c_num_of_reg_to_read);
                 i2c_read_rsp->nfc_i2c_slave_status = i2c_status;
                 if(i2c_status == 0x00)
                 {
                     i2c_read_rsp->nfc_nb_reg_reads = i2c_num_of_reg_to_read;
                 }
                 else
                 {
                     i2c_read_rsp->nfc_nb_reg_reads = 0x00; // error case so return num of read as 0x00.
                 }
                 memcpy(i2c_read_rsp->i2c_reg_read_rsp, i2c_reg_read_data, (i2c_num_of_reg_to_read*2));
                 i2c_cmd_cnt = 0;
             }
             break;
        case FTM_NFC_NFCC_COMMAND:
             printf("Framing the response for FTM_NFC_NFCC_COMMAND cmd \n");
             if(response_buff && res_len)
             {
                 rsp = (ftm_nfc_pkt_type*)diagpkt_subsys_alloc(DIAG_SUBSYS_FTM,
                                                               FTM_NFC_CMD_CODE,
                                                               sizeof(ftm_nfc_pkt_type));
                 if(rsp)
                 {
                     rsp->ftm_nfc_hdr.nfc_cmd_id = FTM_NFC_NFCC_COMMAND;
                     rsp->ftm_nfc_hdr.nfc_cmd_len = 2+res_len;
                     rsp->nfc_nci_pkt_len = res_len;
                     memcpy(rsp->nci_data, response_buff, res_len);
                     free(response_buff);
                     response_buff = 0;
                     res_len = 0;
                 }
             }
             else
                 printf("ftm_nfc_dispatch : response_buff = %p, res_len = %d", response_buff, res_len);
             break;
        case FTM_NFC_SEND_DATA:
             printf("Framing the response for FTM_NFC_SEND_DATA cmd \n");
             nfc_data_rsp = (ftm_nfc_data_rsp_pkt_type*)diagpkt_subsys_alloc(DIAG_SUBSYS_FTM,
                                                                             FTM_NFC_CMD_CODE,
                                                                             sizeof(ftm_nfc_data_rsp_pkt_type));
             if(nfc_data_rsp)
             {
                 nfc_data_rsp->ftm_nfc_hdr.nfc_cmd_id = FTM_NFC_SEND_DATA;
                 nfc_data_rsp->ftm_nfc_hdr.nfc_cmd_len = 0;/*Rsp as per the NFC FTM data rsp req*/
             }
             break;
        default:
             goto error_case;
             break;
    }
    free(nfc_cmd_buff);
    hal_state = NCI_HAL_WRITE;
    if(async_msg_available)
    {
        printf(" Some async message available.. committing now.\n");
        hal_state = NCI_HAL_ASYNC_LOG;
        sem_post(&semaphore_halcmd_complete);
    }
    wait_rsp = FALSE;
    if(nfc_ftm_pkt->ftm_nfc_hdr.nfc_cmd_id == FTM_NFC_I2C_SLAVE_WRITE)
    {
        return(void*)i2c_write_rsp;
    }
    else if(nfc_ftm_pkt->ftm_nfc_hdr.nfc_cmd_id == FTM_NFC_I2C_SLAVE_READ)
    {
        i2c_req_read = FALSE;
        return(void*)i2c_read_rsp;
    }
    else if(nfc_ftm_pkt->ftm_nfc_hdr.nfc_cmd_id == FTM_NFC_NFCC_COMMAND)
    {
        return(void*)rsp;
    }
    else
    {
        return(void*)nfc_data_rsp;
    }
error_case:
    return NULL;
}
