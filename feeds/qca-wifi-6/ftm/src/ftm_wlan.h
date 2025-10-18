/*==========================================================================

                     FTM WLAN Header File

Description
  The header file includes enums, struct definitions for WLAN FTM packets

# Copyright (c) 2010-2011, 2014 by Qualcomm Technologies, Inc.
# All Rights Reserved.
# Qualcomm Technologies Proprietary and Confidential.

===========================================================================*/

/*===========================================================================

                         Edit History


when       who       what, where, why
--------   ---       ----------------------------------------------------------
07/11/11   karthikm  Created header file to include enums, struct for WLAN FTM
                     for Atheros support
========================================================================*/

#ifndef  FTM_WLAN_H_
#define  FTM_WLAN_H_

#ifdef CONFIG_FTM_WLAN

#include "diagpkt.h"
#include <sys/types.h>

#define FTM_WLAN_CMD_CODE 22

/* TODO: For LE platforms only - need to extend it for BE platform too*/
#define cpu32_to_le32(buf, val)     \
    do { \
            buf[0] = val & 0xff;            \
            buf[1] = (val >> 8) & 0xff;     \
            buf[2] = (val >> 16) & 0xff;    \
            buf[3] = (val >> 24) & 0xff;    \
    } while(0)

/* TODO: For LE platforms only - need to extend it for BE platform too*/
#define le_to_cpu16(buf, uint16_val)                                   \
    do {                                                                   \
            uint16_val = (buf[0] | buf[1] << 8);  \
    } while(0)

/* TODO: For LE platforms only - need to extend it for BE platform too*/
#define le_to_cpu32(buf, uint32_val)                                   \
    do {                                                                   \
            uint32_val = (buf[0] | buf[1] << 8 | buf[2] << 16 | buf[3] << 24); \
    } while(0)

extern char g_ifname[];

/* Various ERROR CODES supported by the FTM WLAN module*/
typedef enum {
    FTM_ERR_CODE_PASS = 0,
    FTM_ERR_CODE_IOCTL_FAIL,
    FTM_ERR_CODE_SOCK_FAIL,
    FTM_ERR_CODE_UNRECOG_FTM
}FTM_WLAN_LOAD_ERROR_CODES;


#define CONFIG_HOST_TCMD_SUPPORT  1
#define AR6000_IOCTL_SUPPORTED    1

#define ATH_MAC_LEN               6

typedef enum {
    FTM_WLAN_COMMON_OP,
    FTM_WLAN_BDF_GET_MAX_TRANSFER_SIZE,
    FTM_WLAN_BDF_READ,
    FTM_WLAN_BDF_WRITE,
    FTM_WLAN_BDF_GET_FNAMEPATH,
    FTM_WLAN_BDF_SET_FNAMEPATH
}FTM_WLAN_CMD;

typedef enum {
    WLAN_BDF_READ_SUCCESS,
    WLAN_BDF_READ_FAILED,
    WLAN_BDF_WRITE_SUCCESS,
    WLAN_BDF_WRITE_FAILED,
    WLAN_BDF_INVALID_SIZE = 5,
    WLAN_BDF_BAD_OFFSET,
    WLAN_BDF_FILE_OPEN_FAIL,
    WLAN_BDF_FILE_SEEK_FAIL,
    WLAN_BDF_FILE_STAT_FAIL,
    WLAN_BDF_PATH_GET_SUCCESS,
    WLAN_BDF_PATH_GET_FAILED,
    WLAN_BDF_PATH_SET_SUCCESS,
    WLAN_BDF_PATH_SET_FAILED
}FTM_WLAN_ERROR_CODES;

#ifdef WIN_AP_HOST
#define PACKED_STRUCT __attribute__((__packed__))
#else
#define PACKED_STRUCT __attribute__((packed))
#endif

/*FTM WLAN request type*/

typedef struct
{
    diagpkt_cmd_code_type              cmd_code;
    diagpkt_subsys_id_type             subsys_id;
    diagpkt_subsys_cmd_code_type       subsys_cmd_code;
    uint16                             cmd_id; /* command id (required) */
    uint16                             cmd_data_len;
    uint16                             cmd_rsp_pkt_size;
    union {
        struct {
             uint16                    rsvd;
             byte                      rsvd1;
             byte                      rsvd2;
             byte                      wlanslotno;
             byte                      wlandeviceno;
             byte                      data[0];
        }PACKED_STRUCT common_ops;
        struct {
             byte                       rsvd[6];
             byte                       data[0];
        }PACKED_STRUCT get_max_transfer_size;
        struct {
            uint32                     offset;
            byte                       rsvd[2];
            byte                       data[0];
        }PACKED_STRUCT read_file;
        struct {
            uint16                     size;
            uint8                      append_flag;
            byte                       rsvd[3];
            byte                       data[0];
        }PACKED_STRUCT write_file;
        struct {
            byte                       rsvd[6];
            byte                       data[0];
        }PACKED_STRUCT get_fname;
        struct {
            byte                       rsvd[6];
            byte                       data[0];
        }PACKED_STRUCT set_fname;
    }cmd;
}PACKED_STRUCT ftm_wlan_req_pkt_type;

/*FTM WLAM response type */
typedef struct
{
    struct {
        diagpkt_subsys_header_type         header; /*diag header*/
        uint16                             cmd_id; /* command id (required) */
        uint16                             cmd_data_len;
        uint16                             cmd_rsp_pkt_size;
    }PACKED_STRUCT common_header;
    union {
        struct {
            uint16                             rsvd;
            uint32                             result ;/* error_code */
            union {
                struct {
                    byte                              data[0]; /*rxReport*/
                }rxReport;
                struct {
                    byte                              data[0];  /*ThermValReport*/
                }thermval_report;
            }rx_and_therm;
        }PACKED_STRUCT common_ops;
        struct {
            uint16                             result; /*error_code*/
            byte                               rsvd[4];
            uint16                             max_size;
        }PACKED_STRUCT get_max_transfer_size;
        struct {
            byte                               result; /*error_code*/
            uint16                             size;
            byte                               bytes_remaining[3];
            byte                               data[0];
        }PACKED_STRUCT read_file;
        struct {
            byte                               result;
            byte                               rsvd[5];
            byte                               data[0];
        }PACKED_STRUCT write_file;
        struct {
            byte                              result;
            byte                              rsvd[5];
            byte                              data[0];
        }PACKED_STRUCT get_fname;
        struct {
            byte                              result;
            byte                              rsvd[5];
            byte                              data[0];
        }PACKED_STRUCT set_fname;
        struct {
            uint16                            win_cmd_specific;
            uint16                            data_len;
            uint8                             rsvd;
            uint8                             wlandeviceno;
            byte                              data[0];
        }PACKED_STRUCT win_resp;
    }cmd;
}PACKED_STRUCT ftm_wlan_rsp_pkt_type;

void* ftm_wlan_dispatch(ftm_wlan_req_pkt_type *wlan_ftm_pkt, int pkt_len);

#ifdef WIN_AP_HOST
void setBoardDataCaptureFlag (int flag);
void setDeviceId(int id);
extern ftm_wlan_rsp_pkt_type *win_bt_mac_flash_write(
       ftm_wlan_req_pkt_type *wlan_ftm_pkt,
       int pkt_len);

extern void win_host_handle_fw_resp (ftm_wlan_rsp_pkt_type *rsp, void *data, uint32_t data_len);
extern ftm_wlan_rsp_pkt_type *win_host_handle_bdf_req(
       ftm_wlan_req_pkt_type *wlan_ftm_pkt, int pkt_len);
#endif

#endif /* CONFIG_FTM_WLAN */
#endif /* FTM_WLAN_H_ */
