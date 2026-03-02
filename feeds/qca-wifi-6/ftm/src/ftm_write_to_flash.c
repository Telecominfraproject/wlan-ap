/*
*Copyright (c) 2017-2019 Qualcomm Technologies, Inc.
*
*All Rights Reserved.
*Confidential and Proprietary - Qualcomm Technologies, Inc.
*/

#ifdef WIN_AP_HOST

#include "ftm_wlan_win.h"
#include <string.h>

/*===========================================================================
FUNCTION bt_setmac_flash_write

DESCRIPTION
    Write MAC and XTAL to flash region

DEPENDENCIES
    NIL

RETURN VALUE
   Returns status success or failure

SIDE EFFECTS
    NONE

===========================================================================*/
int bt_setmac_flash_write(uint8_t *mac, unsigned int len)
{
    int fd;
    int offset;
    int i;
    uint8_t *flashdata;

    fd = open(VIRTUAL_FLASH_PARTITION, O_RDWR);

    if (fd < 0) {
        perror("Could not open flash. Returning without write\n");
        return -1;
    }

    DPRINTF(FTM_DBG_TRACE,"\nNumber of bytes = %d\n",len);

    offset = BT_MAC_OFFSET;
    lseek(fd, offset, SEEK_SET);
    if (write(fd, mac, len) < 1) {
        DPRINTF(FTM_DBG_TRACE,"flash write error. Returning\n");
        close(fd);
        return -1;
    }
    
    DPRINTF(FTM_DBG_TRACE,
      "BT mac written successfully to flash @ offset %X size %X\n",
      offset, len);

    flashdata = (uint8_t *)malloc(len);
    if (!flashdata){
        DPRINTF(FTM_DBG_TRACE, "Write verification failed. Unable to allocate memory.\n");
        close(fd);
        return -1;
    }

    lseek(fd, offset, SEEK_SET);
    if (read(fd, flashdata, len) < 1){
        DPRINTF(FTM_DBG_TRACE, "Flashdata read failed\n");
        free(flashdata);
        return -1;
    }
    for(i = 0; i < len; i++){
        DPRINTF(FTM_DBG_TRACE, "input mac = 0x%x, flashdata = 0x%x\n", mac[i], flashdata[i]);
    }

    free(flashdata);
    close(fd);
    return 1;
}


/*===========================================================================
FUNCTION win_bt_mac_flash_write

DESCRIPTION
    Call bt_setmac_flash_write function and populate response to Qdart

DEPENDENCIES
    NIL

RETURN VALUE
   Returns resp to qdart

SIDE EFFECTS
    NONE

===========================================================================*/
ftm_wlan_rsp_pkt_type *win_bt_mac_flash_write(ftm_wlan_req_pkt_type *wlan_ftm_pkt, int pkt_len)
{
    int i;
    int status;
    ftm_wlan_rsp_pkt_type *rsp;
    unsigned char BtDiagMAC[MAC_XTAL_LENGTH];
    unsigned int dataLen = 0;
    uint8_t *input_msg = (uint8_t*)wlan_ftm_pkt;
    TLV2_Specific_byte = wlan_ftm_pkt->cmd.common_ops.rsvd;

    dataLen = input_msg[MAC_LENGTH_POS];
    for(i=0; i<dataLen; i++)
        BtDiagMAC[i]= input_msg[MAC_POS + i];
    rsp = (ftm_wlan_rsp_pkt_type*)diagpkt_subsys_alloc(DIAG_SUBSYS_FTM,
                                                       FTM_WLAN_CMD_CODE,
                                                       (sizeof(rsp->common_header) +
                                                       sizeof(rsp->cmd.common_ops)+
                                                       BT_TLV1_RESP_LEN ));
    if (!rsp)
        return rsp;

    rsp->common_header.cmd_rsp_pkt_size = BT_RESP_LEN;
    rsp->common_header.cmd_data_len = 0;
    rsp->cmd.win_resp.data_len = BT_TLV1_RESP_LEN;
    rsp->cmd.win_resp.win_cmd_specific = TLV2_Specific_byte;

    status = bt_setmac_flash_write(BtDiagMAC, dataLen);
    if (status > 0) {
        memcpy(rsp->cmd.win_resp.data, BTsetmacResponse, BT_TLV1_RESP_LEN);
        DPRINTF(FTM_DBG_TRACE,"Response sent to Qdart\n");
        /*print_uchar_array((uint8_t*)(rsp->cmd.win_resp.data), BT_TLV1_RESP_LEN);*/
        return rsp;
    }
    else
        return rsp;
}

/*===========================================================================
FUNCTION win_host_handle_bdf_req

DESCRIPTION
    Function to process WIN specific bdf requests.
    This handles the requests related to device identify,
    boarddata capture and flash write requests, and sends
    the response to QDART.


DEPENDENCIES
    NIL

RETURN VALUE
   Returns back buffer that is meant for diag callback

SIDE EFFECTS
    NONE

===========================================================================*/
ftm_wlan_rsp_pkt_type *win_host_handle_bdf_req(ftm_wlan_req_pkt_type *wlan_ftm_pkt, int pkt_len)
{
    uint8_t *input_msg = (uint8_t*)wlan_ftm_pkt;
    int error = 0;
    int fd;
    int file_offset;
    int art_slot_id;
    ftm_wlan_rsp_pkt_type *rsp;
    deviceno = wlan_ftm_pkt->cmd.common_ops.wlandeviceno;
    art_slot_id = wlan_ftm_pkt->cmd.common_ops.wlanslotno;
    TLV2_Specific_byte = wlan_ftm_pkt->cmd.common_ops.rsvd;
    rsp = (ftm_wlan_rsp_pkt_type*)diagpkt_subsys_alloc(DIAG_SUBSYS_FTM,
                                                       FTM_WLAN_CMD_CODE,
                                                       (sizeof(rsp->common_header) +
                                                       sizeof(rsp->cmd.common_ops)+
                                                       TLV1_RESP_LEN));

   if (rsp == NULL) {
           DPRINTF(FTM_DBG_ERROR, "Failed to allocate Diag packet: %p\n", rsp);
           return rsp;
   }
    rsp->cmd.win_resp.win_cmd_specific = htole16(TLV2_Specific_byte);
    rsp->common_header.cmd_rsp_pkt_size = htole16(TLV1_CMD_RESP_SIZE);
    rsp->common_header.cmd_data_len = 0;
    rsp->cmd.win_resp.data_len = htole16(TLV1_RESP_LEN);
    rsp->cmd.win_resp.rsvd = 0;
    rsp->cmd.win_resp.wlandeviceno = deviceno;

    DPRINTF(FTM_DBG_TRACE, "Received FTM daemon specific TLV\n");

    /* Byte 80 in request signifies type of request, 101 is used for BD capture*/
    if(input_msg[TLV_PAYLOAD_PARAM_1] == BD_CAPTURE_REQ) {
       DPRINTF(FTM_DBG_TRACE, "BOARD_DATA_CAPTURE\n");
       if (input_msg[TLV_PAYLOAD_PARAM_2] == 1) {
           setBoardDataCaptureFlag(1);
           } else if (input_msg[TLV_PAYLOAD_PARAM_2] == 0) {
               setBoardDataCaptureFlag(0);
            }

        } else if (input_msg[TLV_PAYLOAD_PARAM_1] == FLASH_WRITE_REQ) {
	      fd = open(VIRTUAL_FLASH_PARTITION, O_RDWR);
              if (fd < 0) {
                  DPRINTF(FTM_DBG_TRACE, "FILE OPEN FAILED\n");
                  error = WLAN_BDF_FILE_OPEN_FAIL;
              } else {
                  DPRINTF(FTM_DBG_TRACE, "FILE OPEN SUCCESSFULL\n");
                  if (!deviceid) {
                      file_offset = FLASH_BASE_CALDATA_OFFSET_SOC_0;

                      if(art_slot_id == 1)
                          file_offset = FLASH_BASE_CALDATA_OFFSET_PCI_1;
                      else if(art_slot_id == 2)
                          file_offset = FLASH_BASE_CALDATA_OFFSET_PCI_2;

                  } else if (deviceid == QC9887_DEVICE_ID ||
                             deviceid == QC9888_DEVICE_ID) {
                      file_offset = FLASH_BASE_CALDATA_OFFSET_SOC_1;
                  } else if (deviceid == QC99xx_DEVICE_ID) {
                      file_offset = FLASH_BASE_CALDATA_OFFSET_SOC_1;
                  } else  if (deviceid == QCN9000_DEVICE_ID) {
                      if(art_slot_id == 1)
                          file_offset = FLASH_BASE_CALDATA_OFFSET_PCI_1;
                      else if(art_slot_id == 2)
                          file_offset = FLASH_BASE_CALDATA_OFFSET_PCI_2;
                      else {
                          close(fd);
                          return rsp;
                      }
                  } else {
                      DPRINTF(FTM_DBG_TRACE, "%s:%d - Invalid Device ID %d\n",
                              __func__, __LINE__, deviceid);
                      close(fd);
                      return rsp;
                  }
                  lseek(fd, file_offset, SEEK_SET);
                  if (write(fd, BDbuffer , BDbuffer_offset) < 1) {
                      DPRINTF(FTM_DBG_TRACE, "%s:%d - Flash write error\n",
                              __func__, __LINE__);
                      error = WLAN_BDF_WRITE_FAILED;
                      close(fd);
                  } else {
                      fsync(fd);
                      close(fd);
                      system(WRITE_ART);

                      DPRINTF(FTM_DBG_INFO,
                              "Flash commit success @ offset %0x Size %d\n",
                      file_offset, BDbuffer_offset);
                  }
              }

        } else if (input_msg[TLV_PAYLOAD_PARAM_1] == DEVICE_IDENTIFY) {
            DPRINTF(FTM_DBG_TRACE, "Device Identify \n");
            setDeviceId(input_msg[TLV_PAYLOAD_PARAM_2]);
        }
    if (error!=NO_ERROR)
        /* These commands will not go to firmware */
        return rsp;
    else {
        memcpy(rsp->cmd.win_resp.data, ftm_wlan_tlvRespMsg, TLV1_RESP_LEN);
        return rsp;
    }
}

/*===========================================================================
FUNCTION   setBordDataCaptureFlag

DESCRIPTION
  Sets the BDCapture variable

DEPENDENCIES
  NIL

RETURN VALUE
  NONE

SIDE EFFECTS
  NONE

===========================================================================*/
void setBoardDataCaptureFlag (int flag)
{
    DPRINTF(FTM_DBG_TRACE, "Setting BDCapture flag to %d\n", flag);
    if (flag == 1) {
        BDbuffer_offset = 0;
        resp_counter = 0;
        start_capture = 1;
    }

    // when board data capture is turned off, set the device id to 0
    if (flag == 0){
        start_capture = 0;
        setDeviceId(0);
    }
}

/*===========================================================================
FUNCTION setDeviceId

DESCRIPTION
    Sets the global variable device id, upon requests
    according to the parameter 2 of the TLV command

DEPENDENCIES
  NIL

RETURN VALUE
  NONE

SIDE EFFECTS
  NONE
============================================================================*/
void setDeviceId(int id)
{
    DPRINTF(FTM_DBG_TRACE, "Setting Device id to %d\n", id);
    deviceid = id;
}

/*===========================================================================
FUNCTION    win_host_handle_fw_resp

DESCRIPTION
    WIN specific handler for responses from FW.
    FW responses for board data are snooped here
    and stored in a global buffer which is then
    accessed by the WIN request handler for flash
    write requests.

DEPENDENCIES
    NIL

RETURN VALUE
    0 on success
    1 on failure

SIDE EFFECTS
    NONE
===========================================================================*/

void win_host_handle_fw_resp (ftm_wlan_rsp_pkt_type *rsp, void *data, uint32_t data_len)
{
    uint32_t sequence_id;
    int legacy_dataSize;
    if (!rsp || !data || !data_len) {
        DPRINTF(FTM_DBG_ERROR, "rsp/data is NULL\n");
        rsp = NULL;
        return;
    }
    rsp->cmd.win_resp.win_cmd_specific = htole16(TLV2_Specific_byte);
    rsp->common_header.cmd_rsp_pkt_size = htole16(data_len + DIAG_HDR_LEN);
    rsp->cmd.win_resp.data_len = htole16(data_len);
    rsp->cmd.win_resp.wlandeviceno = deviceno;
    rsp->cmd.win_resp.rsvd = 0;
    memcpy(rsp->cmd.win_resp.data, data, data_len);

    /*TODO: Need better identification method for BDF responses*/
    /* Check if the deviceid is set */

    switch (deviceid) {

        /* DeviceId is 0 for HK and lithium family targets */
        case 0:
        {   /* Check if request is for BD_get size. */
            if (((uint8_t *)data)[BD_SIZE_REQ_POS] == BD_SIZE_REQ_ID ) {
                bd_size = 0 ;
                BDbuffer_offset = 0;
                resp_counter = 0;
                le_to_cpu32(((uint8_t *)data + BD_SIZE_VAL), bd_size);
                DPRINTF(FTM_DBG_INFO, "bd_size = %d \n", bd_size);
                total_4K_responses = bd_size / REQ_SEG_SIZE;
                remaining_bytes = bd_size % REQ_SEG_SIZE;
                DPRINTF(FTM_DBG_INFO, "Total_responses= %d \n",
                        total_4K_responses);
                DPRINTF(FTM_DBG_INFO, "Remaining_bytes = %d \n",
                        remaining_bytes);
            }

            if (data_len > BD_LEN_EXPECTED && start_capture == 1) {
                sequence_id = ((uint8_t *)data)[SEQUENCE_ID_POS];
                DPRINTF(FTM_DBG_INFO, "Sequence_ID= %d\n", sequence_id);

            if (sequence_id == FIRST_SEG)
                resp_counter ++;
                DPRINTF(FTM_DBG_INFO, "Response counter == %d\n",
                        resp_counter);
                DPRINTF(FTM_DBG_INFO, "Buffer offset == %d\n",
                        BDbuffer_offset);

            if (resp_counter < total_4K_responses + 1) {
            /* Handle 3 segments of BDF resonses
             * 1st segment - BDF is from offset 84
             * 2nd segment - BDF is from offset 28
             * 3rd segment - BDF is from offset 28
             */
                switch (sequence_id) {

                    case FIRST_SEG:
                        memcpy(BDbuffer + BDbuffer_offset,
                               (void *)((uint8_t*)data + FIRST_SEG_TLV_HDR),
                               data_len - FIRST_SEG_TLV_HDR);
                        BDbuffer_offset += (data_len - FIRST_SEG_TLV_HDR);
                    break;

                    case SECOND_SEG:
                        memcpy(BDbuffer + BDbuffer_offset,
                               (void *)((uint8_t *)data + SECOND_SEG_TLV_HDR),
                               data_len - SECOND_SEG_TLV_HDR);
                        BDbuffer_offset += (data_len - SECOND_SEG_TLV_HDR);
                    break;

                    case THIRD_SEG:
                        memcpy(BDbuffer + BDbuffer_offset,
                               (void *)((uint8_t*)data + THIRD_SEG_TLV_HDR),
                               data_len - THIRD_SEG_TLV_HDR);
                        BDbuffer_offset += (data_len - THIRD_SEG_TLV_HDR);
                    break;

                    default:
                         /* error */
                    break;

                }
             /* In case of total bdf data size not being a multiple
              * of 4k, extra remaining bdf data is handled based on
              * which segment of 4k it belongs to.
              */
             } else {
                   switch (sequence_id) {
                     case FIRST_SEG:
                         if (remaining_bytes < CALDATA_SIZE_FIRST_SEG) {
                             memcpy(BDbuffer + BDbuffer_offset,
                                    data + FIRST_SEG_TLV_HDR,
                                    remaining_bytes);
                             BDbuffer_offset += remaining_bytes;
                         } else {
                             memcpy(BDbuffer + BDbuffer_offset,
                                    data + FIRST_SEG_TLV_HDR,
                                    data_len - FIRST_SEG_TLV_HDR);
                             BDbuffer_offset += (data_len - FIRST_SEG_TLV_HDR);
                             remaining_bytes -= CALDATA_SIZE_FIRST_SEG;
                         }
                     break;

                     case SECOND_SEG:
                         if (remaining_bytes < CALDATA_SIZE_SECOND_SEG) {
                             memcpy(BDbuffer + BDbuffer_offset,
                                    data + SECOND_SEG_TLV_HDR,
                                    remaining_bytes);
                             BDbuffer_offset += remaining_bytes;
                         } else {
                             memcpy(BDbuffer + BDbuffer_offset,
                                    data + SECOND_SEG_TLV_HDR,
                                    data_len - SECOND_SEG_TLV_HDR);
                             BDbuffer_offset += (data_len - SECOND_SEG_TLV_HDR);
                             remaining_bytes -= CALDATA_SIZE_SECOND_SEG;
                         }
                     break;

                     case THIRD_SEG:
                          memcpy(BDbuffer + BDbuffer_offset,
                                 (void *)((uint8_t*)data + THIRD_SEG_TLV_HDR),
                                 remaining_bytes);
                          BDbuffer_offset += remaining_bytes;
                     break;
                   }
               }
            }
        break;
        }
        case QC9887_DEVICE_ID:
        case QC9888_DEVICE_ID:
        {
            if ((((uint8_t *)data)[BD_READ_CMD_ID_POS] == M_EEEPROM_BLOCK_READ_ID_QC98XX) &&
                (((uint8_t *)data)[BD_READ_RESP_PARAM_POS] == BD_READ_RESP_PARAM)){
                le_to_cpu32(((uint8_t *)data + QC98XX_BLOCK_SIZE_VAL),
                            legacy_dataSize);
                DPRINTF(FTM_DBG_TRACE,
                        "Capturing Caldata for QC98xx by FTM :: Size %d\n",
                        legacy_dataSize);
                DPRINTF(FTM_DBG_TRACE,
                        "BDbuffer Offset : %d \n",
                        (resp_counter * QC98XX_BLOCK_SIZE));
                memcpy(BDbuffer + BDbuffer_offset, (void *)((uint8_t *)data +
                       QC98XX_BLOCK_START_POS),
                       legacy_dataSize);
                resp_counter = resp_counter + 1;
                BDbuffer_offset += legacy_dataSize;
            }
        break;
        }
        case QC99xx_DEVICE_ID:
        {
            if ((((uint8_t *)data)[BD_READ_CMD_ID_POS] == M_EEEPROM_BLOCK_READ_ID_LEGACY) &&
                (((uint8_t *)data)[BD_READ_RESP_PARAM_POS] == BD_READ_RESP_PARAM) &&
                ((uint8_t *)data)[LEGACY_RADIO_PARAM_POS] < LEGACY_RADIO_PARAM_THRESHOLD) {
                le_to_cpu16(((uint8_t *)data + LEGACY_BLOCK_SIZE_VAL), legacy_dataSize);
                DPRINTF(FTM_DBG_TRACE,
                        "Capturing Caldata by FTM :: Size %d\n",
                        legacy_dataSize);
                DPRINTF(FTM_DBG_TRACE,
                        "BDbuffer Offset : %d \n",
                        (resp_counter * BD_BLOCK_SIZE));
                memcpy(BDbuffer + BDbuffer_offset,
                       (void *)((uint8_t *)data + LEGACY_BLOCK_START_POS),
                       legacy_dataSize);
                resp_counter = resp_counter + 1;
                BDbuffer_offset+=legacy_dataSize;
            }
        break;
        }
    }
}
#endif
