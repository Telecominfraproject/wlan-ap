/*
*Copyright (c) 2018-2020 Qualcomm Technologies, Inc.
*
*All Rights Reserved.
*Confidential and Proprietary - Qualcomm Technologies, Inc.
*/

/* IPQ-QCA402X specific file */

#ifdef IPQ_AP_HOST_IOT

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

#include <sys/ioctl.h>
#include <mtd/mtd-user.h>
#include "comdef.h"
#include "diagcmd.h"
#include "ftm_wlan.h"
#include "ftm_dbg.h"
#include "ftm_iot.h"
#ifdef IPQ_AP_HOST_IOT_QCA402X
#include "diag_api.h"
#endif /* IPQ_AP_HOST_IOT_QCA402X */
#ifdef IPQ_AP_HOST_IOT_IPQ50XX
#include "btdaemon.h"
#endif /* IPQ_AP_HOST_IOT_IPQ50XX  */

#define NHDLC_TERM 126
#define NHDLC_VERSION 1
#define NHDLC_TERM_SIZE 1
#define FLASH_CMD_ID_POS 1
#define MAX_BUF_SIZE 2048
#define WAIT_TIME_MS 100
#define SUBSYS_CMD_ID_POS 2
#define RESERVED_CMD_ID 0
#define DUT_INTERFACE_SELECT 1
#define DUT_INTERFACE_ID_POS 4
#define DUT_INTERFACE_SELECT_POS 10
#define DIAG_HDR_LEN (sizeof(diag_nonhdlc_hdr_t) + NHDLC_TERM_SIZE)
#define FTM_IOT_LOG_HEADER_SIZE  sizeof(ftm_iot_log_pkt_type)

/* Sempahore Timeout Period set for 2 seconds */
#define SEM_WAIT_TIMEOUT 5
#define MEMSET_RESET_VALUE 0
#define DIAG_HEADER_SIZE 12

extern void diagpkt_free(void *pkt);

void print_array(uint8_t *addr, int len)
{
    int i;
    int line = 1;
    for (i = 0; i < len; i++) {
        if (i == (line * 80)) {
           DPRINTF(FTM_DBG_TRACE, "\n");
           line++;
        }
        DPRINTF(FTM_DBG_TRACE, "%02X ", addr[i]);
    }
    DPRINTF(FTM_DBG_TRACE, "\n");
}
#ifdef IPQ_AP_HOST_IOT_QCA402X
/*===========================================================================
  FUNCTION iot_thr_func_qca402x

  DESCRIPTION
  Continously polls QCA402X for asynchronous data responses and
  logs receievd asynchronous data responses to Diag module using
  log-submit()

  DEPENDENCIES
  NIL

  RETURN VALUE
  Returns NULL on failure. Function also exits with NULL return value
  when main indicates that this thread should be stopped

  SIDE EFFECTS
  NONE

  ===========================================================================*/
void *iot_thr_func_qca402x(void *hdl)
{
    int bytes = 0;
    void *rsp2 = NULL;
    int diag_hdr_len = DIAG_HDR_LEN ;
    void *new_iot_ftm_rsp2_pkt = NULL;

    if (!hdl) {
        DPRINTF(FTM_DBG_ERROR, "Invalid iotd handle\n");
        return NULL;
    }

    new_iot_ftm_rsp2_pkt = malloc(MAX_BUF_SIZE);
    if (!new_iot_ftm_rsp2_pkt) {
        DPRINTF(FTM_DBG_ERROR, "Could not allocate response packet \n");
        return NULL;
    }

    while(1) {

        if (thread_stop == 1) {
            DPRINTF(FTM_DBG_TRACE, "FTMd: Exiting thread.\n");
            break;
        }

        memset(new_iot_ftm_rsp2_pkt, MEMSET_RESET_VALUE, MAX_BUF_SIZE);
        sem_wait(&iot_sem);

        /*If we recieve a response from QCA402X, allocate a buffer using diag alloc with correct
          subsystem code and length */
        while ((bytes = diag_recv(hdl, (uint8_t *)new_iot_ftm_rsp2_pkt,
                                                      MAX_BUF_SIZE,
                                                      WAIT_TIME_MS)) >= 0) {
            if (bytes > MAX_BUF_SIZE || bytes <= diag_hdr_len) {
                DPRINTF(FTM_DBG_ERROR, "Could not allocate async log response packet\n");
                free (new_iot_ftm_rsp2_pkt);
                return NULL;
            }

            rsp2 = diagpkt_subsys_alloc(DIAG_SUBSYS_FTM, ftm_iot_cmd_code, (bytes - diag_hdr_len));
            if (!rsp2) {
                DPRINTF(FTM_DBG_ERROR, "Could not allocate async log response packet\n");
                free (new_iot_ftm_rsp2_pkt);
                return NULL;
            }

            /* Remove NHDLC header from recieved packet and store contents in
               buffer allocated above */
            memcpy(rsp2, (new_iot_ftm_rsp2_pkt + diag_hdr_len - NHDLC_TERM_SIZE),
                                                         (bytes - diag_hdr_len));

            DPRINTF(FTM_DBG_TRACE, "FTMd: Asynchronous Data response has been sent.\n");
            print_array((uint8_t *)rsp2, (bytes - diag_hdr_len) );

            /*Remove an additional 4 bytes of header and log packet to diag module
              asynchronously for further processing*/
            log_submit(rsp2 + diag_hdr_len - NHDLC_TERM_SIZE);
            diagpkt_free (rsp2);
            memset(new_iot_ftm_rsp2_pkt, MEMSET_RESET_VALUE, MAX_BUF_SIZE);
        }

	sem_post(&iot_sem_async);
    }

    free (new_iot_ftm_rsp2_pkt);
    diagpkt_free (rsp2);
    pthread_exit(NULL);
}

/*===========================================================================
  FUNCTION ftm_iot_dispatch_qca402x

  DESCRIPTION
  Function processes WIN IOT specific requests and relays to
  QCA402x FTM layer for further processing. Recieves response
  buffer from QCA402x and returns buffer meant for diag call back

  This function handles NHDLC to HDLC translation and vice-versa
  before sending and receivng buffers to QCA402X FTM layer

  DEPENDENCIES
  NIL

  RETURN VALUE
  Returns back buffer that is meant for diag callback

  SIDE EFFECTS
  NONE

  ===========================================================================*/

void *ftm_iot_dispatch_qca402x(void *iot_ftm_pkt, int pkt_len, void *hdl)
{
    int diag_hdr_len = DIAG_HDR_LEN;
    int ret = 0;
    byte *payload_ptr = NULL;
    void *rsp1 = NULL;
    ftm_iot_req_pkt_type *new_iot_ftm_pkt = NULL;
    void *new_iot_ftm_rsp_pkt = NULL;
    char command[50] = {'\0'};
    uint16_t *ftm_iot_flash_ptr = NULL;
    uint16 ftm_iot_flash_cmd_code = 0;
    /* The new packet length will be length of original request packet
       + size of NHDLC header + 1 byte of termination character */
    int new_pkt_len = pkt_len + diag_hdr_len;

    if (!iot_ftm_pkt || !pkt_len || !hdl) {
        DPRINTF(FTM_DBG_ERROR, "Invalid ftm iot request packet or iotd handle\n");
        return NULL;
    }

    new_iot_ftm_pkt = malloc(sizeof(ftm_iot_req_pkt_type) + pkt_len + NHDLC_TERM_SIZE);
    if (!new_iot_ftm_pkt) {
        DPRINTF(FTM_DBG_ERROR, "Could not create new ftm iot request packet\n");
        return NULL;
    }
    memset(new_iot_ftm_pkt, MEMSET_RESET_VALUE, (sizeof(ftm_iot_req_pkt_type) + pkt_len + NHDLC_TERM_SIZE));

    new_iot_ftm_rsp_pkt = malloc(MAX_BUF_SIZE);
    if (!new_iot_ftm_rsp_pkt) {
        DPRINTF(FTM_DBG_ERROR, "Could not create new ftm iot response packet\n");
        free (new_iot_ftm_pkt);
        return NULL;
    }
    memset(new_iot_ftm_rsp_pkt, MEMSET_RESET_VALUE, MAX_BUF_SIZE);

    /* Add Non-HDLC header to request packet
       and populate NHDLC header*/
    new_iot_ftm_pkt->hdr.start = NHDLC_TERM;
    new_iot_ftm_pkt->hdr.version = NHDLC_VERSION;
    new_iot_ftm_pkt->hdr.length = pkt_len;
    memcpy(&(new_iot_ftm_pkt->payload), iot_ftm_pkt, pkt_len);
    payload_ptr = (byte *) &(new_iot_ftm_pkt->payload);
    *( payload_ptr + pkt_len) = NHDLC_TERM;
    ftm_iot_cmd_code = *(payload_ptr + SUBSYS_CMD_ID_POS);
    ftm_iot_dut_interface_code = *(payload_ptr + DUT_INTERFACE_ID_POS);
    ftm_iot_reserved_code = *(payload_ptr + SUBSYS_CMD_ID_POS + 1);
    ftm_iot_flash_ptr = (uint16_t *) &(new_iot_ftm_pkt->payload);
    ftm_iot_flash_cmd_code = *(ftm_iot_flash_ptr + FLASH_CMD_ID_POS);
    /*Print packet after adding headers */
    DPRINTF(FTM_DBG_TRACE, "FTMd: Request Packet of size %d bytes sent:\n", new_pkt_len);
    print_array((uint8_t *)new_iot_ftm_pkt, new_pkt_len);

    /*If the request packet it a DUT interface selection command,
      update interface number and return a response packet that
      is an encho of the request packet. ( In the case of multiple
      QCA402x DUT attaches on IPQ platforms) */
    if (((ftm_iot_cmd_code == MFG_CMD_ID_BLE_HCI) || (ftm_iot_cmd_code == MFG_CMD_ID_I15P4_HMI))
                                          && (ftm_iot_dut_interface_code == DUT_INTERFACE_SELECT)
                                          && (ftm_iot_reserved_code == RESERVED_CMD_ID)){
        interface = *(payload_ptr + DUT_INTERFACE_SELECT_POS) - 1;
        if (interface < 0) {
            DPRINTF(FTM_DBG_ERROR, "Invalid DUT interface selection command\n");
            free (new_iot_ftm_pkt);
            free (new_iot_ftm_rsp_pkt);
            return NULL;
         }
        rsp1 = diagpkt_subsys_alloc(DIAG_SUBSYS_FTM, ftm_iot_cmd_code, pkt_len);
        if (!rsp1){
            DPRINTF(FTM_DBG_ERROR, "Could not allocate response packet for interface selection\n");
            free (new_iot_ftm_pkt);
            free (new_iot_ftm_rsp_pkt);
            return NULL;
        }

        memcpy(rsp1, iot_ftm_pkt, pkt_len);

        DPRINTF(FTM_DBG_TRACE, "FTMd: The DUT interface selected is %d \n",interface);
        DPRINTF(FTM_DBG_TRACE, "FTMd: DUT interface resp packet of size %d bytes sent:\n",pkt_len);
        print_array((uint8_t *)rsp1, pkt_len);

        free (new_iot_ftm_pkt);
        free (new_iot_ftm_rsp_pkt);

        /*This resp pointer will be freed by diag later*/
        return rsp1;
    }

    /*If the request packet is a MFG PROG command,
      launch flash script and return a response packet that indicates
      flashing mode of QCA402x is enabled or disabled */
    if ((ftm_iot_flash_cmd_code == MFG_CMD_ID_MISC_PROG_MODE)){

        if (ftm_iot_dut_interface_code == MFG_FLASH_ON){
            strlcpy(command, "/usr/bin/qca402x_flash.sh flash on", sizeof(command));
        }

        if (ftm_iot_dut_interface_code == MFG_FLASH_OFF){
            strlcpy(command, "/usr/bin/qca402x_flash.sh flash off", sizeof(command));
        }

        if (ftm_iot_dut_interface_code == MFG_USB_OFF){
            strlcpy(command, "/usr/bin/qca402x_flash.sh usb-select off", sizeof(command));
        }

        if (ftm_iot_dut_interface_code == MFG_USB_ON){
           strlcpy(command, "/usr/bin/qca402x_flash.sh usb-select on", sizeof(command));
        }

        if (ftm_iot_dut_interface_code == MFG_EDL_OFF){
           strlcpy(command, "/usr/bin/qca402x_flash.sh edl off", sizeof(command));
        }

        if (ftm_iot_dut_interface_code == MFG_EDL_ON){
           strlcpy(command, "/usr/bin/qca402x_flash.sh edl on", sizeof(command));
        }

        /*Return with NULL if string is empty or packet length is less than
          10 for a DUT interface selection command to make sure there will be
          no out of bound access */
        if ( (command[0] == '\0') || (pkt_len <= DUT_INTERFACE_ID_POS) ) {
            DPRINTF(FTM_DBG_ERROR, "Error: Invalid MFG Program command\n");
            free (new_iot_ftm_pkt);
            free (new_iot_ftm_rsp_pkt);
            return NULL;
        }

        system(command);
        DPRINTF(FTM_DBG_TRACE, "\n FTMd: Sent system command: %s \n", command);

        /* Check of size for packet pointed to by payload_ptr has been done above
           using pkt_len to make sure there is no out of bound access */

        *(payload_ptr + DUT_INTERFACE_ID_POS) = MFG_PROG_RESP;

        rsp1 = diagpkt_subsys_alloc(DIAG_SUBSYS_FTM, ftm_iot_cmd_code, pkt_len);
        if (!rsp1){
            DPRINTF(FTM_DBG_ERROR, "Could not allocate response packet for MFG flash commands\n");
            free (new_iot_ftm_pkt);
            free (new_iot_ftm_rsp_pkt);
            return NULL;
        }

        memcpy(rsp1, payload_ptr, pkt_len);

        DPRINTF(FTM_DBG_TRACE, "FTMd: MFG Flash resp packet of size %d bytes sent:\n",pkt_len);
        print_array((uint8_t *)rsp1, pkt_len);

        free (new_iot_ftm_pkt);
        free (new_iot_ftm_rsp_pkt);

        /*This resp pointer will be freed by diag later*/
        return rsp1;
    }

    sem_wait(&iot_sem_async);
    /* Call IPQ-QCA402x diag APIs */
    ret = diag_send(hdl, interface, (uint8_t *)new_iot_ftm_pkt, new_pkt_len);
    if ((ret < 0) || (ret > MAX_BUF_SIZE)) {
        DPRINTF(FTM_DBG_ERROR, "Could not send the request packet to QCA402x \n");
        free (new_iot_ftm_pkt);
        free (new_iot_ftm_rsp_pkt);
        return NULL;
    }

    ret = diag_recv(hdl, (uint8_t *)new_iot_ftm_rsp_pkt, MAX_BUF_SIZE, WAIT_TIME_MS);
    if ((ret < 0) || (ret > MAX_BUF_SIZE) || (ret <= diag_hdr_len)) {
        DPRINTF(FTM_DBG_ERROR, "Could not recieve packet from QCA402x\n");
        free (new_iot_ftm_pkt);
        free (new_iot_ftm_rsp_pkt);
        return NULL;
    }

    DPRINTF(FTM_DBG_TRACE,"Received Command Response of %d bytes\n",ret);
    print_array((uint8_t *)new_iot_ftm_rsp_pkt, ret);

    rsp1 = diagpkt_subsys_alloc(DIAG_SUBSYS_FTM, ftm_iot_cmd_code, (ret - diag_hdr_len));
    if (!rsp1){
        DPRINTF(FTM_DBG_ERROR, "Could not allocate response packet\n");
        free (new_iot_ftm_pkt);
        free (new_iot_ftm_rsp_pkt);
        return NULL;
    }

    memcpy(rsp1, (new_iot_ftm_rsp_pkt + diag_hdr_len - NHDLC_TERM_SIZE), (ret - diag_hdr_len));

    free (new_iot_ftm_pkt);
    free (new_iot_ftm_rsp_pkt);
    sem_post(&iot_sem);

    /*This resp pointer will be freed by diag module later*/
    return (void *)rsp1;
}
#endif /* IPQ_AP_HOST_IOT_QCA402X */

#ifdef IPQ_AP_HOST_IOT_IPQ50XX
/*===========================================================================
 FUNCTION iot_thr_func_ipq50xx

 DESCRIPTION
 Continously polls IPQ50XX BTSS for asynchronous data responses and
 logs received asynchronous data responses to Diag module using
 log-submit()

 DEPENDENCIES
 NIL

 RETURN VALUE
 Returns NULL on failure. Function also exits with NULL return value
 when main indicates that this thread should be stopped

 SIDE EFFECTS
 NONE

===========================================================================*/

void *iot_thr_func_ipq50xx(void *hdl)
{
    int bytes_read = 0, handle = 0;
    void *buffer = NULL;
    void *rsp = NULL;
    struct timespec ts;
    ftm_bt_rsp_pkt_type *ftm_async_pkt;

    buffer = malloc(MAX_BUF_SIZE);
    if (!buffer)
    {
        DPRINTF(FTM_DBG_ERROR, "Could not allocate memory to the buffer \n");
        return NULL;
    }

    memset(buffer, MEMSET_RESET_VALUE, MAX_BUF_SIZE);

    if(hdl == NULL || *((int*)hdl) < 0)
    {
        DPRINTF(FTM_DBG_ERROR, "\n Invalid Handle received from BTSS \n");
        free(buffer);
        return NULL;
    }

    handle = *((int*)hdl);
    while(1)
    {
        if (thread_stop == 1) {
            DPRINTF(FTM_DBG_TRACE, "FTMd: Exiting thread.\n");
            break;
        }

        if (clock_gettime(CLOCK_REALTIME, &ts) == -1)
        {
            DPRINTF(FTM_DBG_ERROR, "clock_gettime");
            free(buffer);
            return NULL;
        }
        ts.tv_sec += SEM_WAIT_TIMEOUT;
        sem_timedwait(&iot_sem, &ts);
        while((bytes_read = bt_daemon_receive(handle, &buffer)) > 0)
        {
            rsp = log_alloc(LOG_BT_HCI_EV_C, (DIAG_HEADER_SIZE + bytes_read));
            if (!rsp)
            {
                DPRINTF(FTM_DBG_ERROR, "Could not allocate rsp packet \n");
                free(buffer);
                return NULL;
            }

            ftm_async_pkt = (ftm_bt_rsp_pkt_type*)rsp;
            memcpy(ftm_async_pkt->buf, buffer, bytes_read);
            DPRINTF(FTM_DBG_TRACE, "\n Printing the Async Packet sent to QDART\n");
            print_array((uint8_t *)rsp, (DIAG_HEADER_SIZE + bytes_read));

            log_submit(rsp);
            log_free(rsp);
            memset(buffer, MEMSET_RESET_VALUE, MAX_BUF_SIZE);
        }
        sem_post(&iot_sem_async);
    }
    free(buffer);
    pthread_exit(NULL);
}
/*===========================================================================
  FUNCTION ftm_iot_dispatch_ipq50xx

  DESCRIPTION
  Function processes WIN IOT specific requests and relays to
  IPQ50XX BTSS for further processing. Constructs response packet
  and returns buffer meant for callback.

  DEPENDENCIES
  NIL

  RETURN VALUE
  Returns back buffer that is meant for diag callback

  SIDE EFFECTS
  NONE

  ===========================================================================*/

void *ftm_iot_dispatch_ipq50xx(void *iot_ftm_pkt, int pkt_len, int *hdl)
{
    void *rsp = NULL;
    struct timespec ts;
    int bytes_sent = -1;

    if(hdl == NULL || *hdl < 0)
    {
        DPRINTF(FTM_DBG_ERROR, "\n Invalid Handle received from BTSS \n");
        return NULL;
    }

    if (!iot_ftm_pkt)
    {
        DPRINTF(FTM_DBG_ERROR, "Invalid iot_ftm_pkt received \n");
        return NULL;
    }

    if (clock_gettime(CLOCK_REALTIME, &ts) == -1)
    {
        perror("clock_gettime");
        return NULL;
    }
    ts.tv_sec += SEM_WAIT_TIMEOUT;
    sem_timedwait(&iot_sem_async, &ts);

    DPRINTF(FTM_DBG_TRACE, "\n Request Packet received for IPQ50xx BTSS\n");
    print_array((uint8_t *)iot_ftm_pkt, pkt_len);

    bytes_sent = bt_daemon_send(*hdl, iot_ftm_pkt);
    if(bytes_sent < 0)
    {
        perror("Unable to send Request Packet to IPQ50xx BTSS");
        return NULL;
    }

    /* Constructing ACK Packet */
    rsp = diagpkt_subsys_alloc(DIAG_SUBSYS_FTM, ftm_iot_cmd_code, pkt_len);
    if (!rsp)
    {
        DPRINTF(FTM_DBG_ERROR, "\n Unable to allocate diag response packet \n");
        return NULL;
    }

    memcpy(rsp, iot_ftm_pkt, pkt_len);

    DPRINTF(FTM_DBG_TRACE, "\n ACK Packet constructed in FTM layer\n");
    print_array((uint8_t *)rsp, pkt_len);

    sem_post(&iot_sem);

    /*This rsp pointer will be freed by diag later */
    return rsp;
}
#endif /* IPQ_AP_HOST_IOT_IPQ50XX */

void *ftm_iot_dispatch(void *iot_ftm_pkt, int pkt_len, void *hdl)
{
    void* retValue = NULL;
#ifdef IPQ_AP_HOST_IOT_QCA402X
    retValue = ftm_iot_dispatch_qca402x(iot_ftm_pkt, pkt_len ,hdl);
#endif
#ifdef IPQ_AP_HOST_IOT_IPQ50XX
    retValue = ftm_iot_dispatch_ipq50xx(iot_ftm_pkt, pkt_len ,(int *)hdl);
#endif
    return retValue;
}

#endif /*ifdef IPQ_AP_HOST_IOT*/
