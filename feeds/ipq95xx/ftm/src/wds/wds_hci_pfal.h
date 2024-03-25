/*
 * Copyright (c) 2016 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 *
 * Copyright (c) 2012 by Qualcomm Atheros, Inc..
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */

#ifndef DEBUG
#define DEBUG   printf
#endif

#ifndef ERROR
#define ERROR   printf
#endif

#include "bt_vendor_qcom.h"

/* error codes */
enum {
    STATUS_SUCCESS,
    STATUS_ERROR,
    STATUS_INVALID_LENGTH,
    STATUS_NO_MEMORY,
    STATUS_NULL_POINTER,
    STATUS_CLIENT_ERROR,
};

enum {
    RX_ERROR = -1,
    RX_BT_EVT_IND = 1,
    RX_BT_HDR,
    RX_BT_DATA,
    RX_ANT_HDR,
    RX_ANT_DATA,
    RX_FM_EVT_IND,
    RX_FM_HDR,
    RX_FM_DATA,
    RX_PKT_IND
};

enum pkt_type {
    BT_PKT_TYPE = 1,
    FM_PKT_TYPE,
    ANT_PKT_TYPE
};
/* device to communicate between PC and DUT */
#define BT_HS_NMEA_DEVICE     "/dev/ttyGS0"
#define BT_HSLITE_UART_DEVICE "/dev/ttyHSL0"

/* interface between PC-DUT */
typedef struct pc_uart_interafce {
    unsigned char *intf;
    int uart_fd;
} pc_uart_interface;

typedef union pc_interface {
    pc_uart_interface uart;
} pc_interface;

/* device to communicate between DUT and BTSOC */
#define APPS_RIVA_FM_CMD_CH  "/dev/smd1"
#define APPS_RIVA_BT_ACL_CH  "/dev/smd2"
#define APPS_RIVA_BT_CMD_CH  "/dev/smd3"
#define APPS_RIVA_ANT_CMD    "/dev/smd5"
#define APPS_RIVA_ANT_DATA   "/dev/smd6"
#define BT_HS_UART_DEVICE    "/dev/ttyHS0"

/* SMD interface between DUT-SOC */
typedef struct soc_smd_interface {
    unsigned char *fm_cmd;
    unsigned char *bt_acl;
    unsigned char *bt_cmd;
    unsigned char *ant_cmd;
    unsigned char *ant_data;
    int fm_cmd_fd;
    int bt_acl_fd;
    int bt_cmd_fd;
    int ant_cmd_fd;
    int ant_data_fd;
} soc_smd_interface;

/* UART interface between DUT-SOC */
typedef struct soc_uart_interface {
    unsigned char *intf;
    int uart_fd;
} soc_uart_interface;

typedef union soc_interface {
    soc_smd_interface smd;
    soc_uart_interface uart;
} soc_interface;

/* context for wdsdaemon */
typedef struct wdsdaemon {
    int mode;
    int soc_type;
    bool pcinit_mask;
    pc_interface pc_if;
    soc_interface soc_if;
    bool is_server_enabled;
    int server_socket_fd;
    pthread_t soc_rthread;
} wdsdaemon;

/* packet types */
#define PACKET_TYPE_INVALID    (0)
#define PACKET_TYPE_BT_CMD     (1)
#define PACKET_TYPE_FM_CMD     (2)
#define PACKET_TYPE_BT_ACL     (3)
#define PACKET_TYPE_ANT_CMD    (4)
#define PACKET_TYPE_ANT_DATA   (5)

/* operation modes for wdsdaemon */
#define MODE_BT_SMD          (0)
#define MODE_FM_SMD          (1)
#define MODE_ANT_SMD         (2)
#define MODE_ALL_SMD         (3)
#define MODE_BT_UART         (4)
#define MODE_ANT_UART        (5)
#define MODE_FM_UART         (6)

/* Bluetooth Header */
#define BT_CMD_PKT_HDR_LEN       (2)
#define BT_EVT_PKT_HDR_LEN       (2)
#define BT_FM_PKT_UART_HDR_LEN   (4)
#define BT_ACL_PKT_HDR_LEN       (4)
#define BT_ACL_PKT_UART_HDR_LEN  (5)

/* FM Header */
#define FM_CMD_PKT_HDR_LEN       (3)    //Opcode(2byte) + Param len(1 byte)
#define FM_EVT_PKT_HDR_LEN       (2)    //Opcode(1 byte) + Param len(1 byte)

/* ANT Header */
#define ANT_CMD_PKT_HDR_LEN           (1)
#define ANT_DATA_PKT_HDR_LEN          (1)
#define ANT_CMD_DATA_PKT_UART_HDR_LEN (2)

#define BT_EVT_PKT_HDR_LEN_UART  (BT_CMD_PKT_HDR_LEN+1)
#define BT_ACL_PKT_HDR_LEN_UART  (BT_ACL_PKT_HDR_LEN+1)

/* ANT data packet type */
#define ANT_DATA_TYPE_BROADCAST     (0x4E)
#define ANT_DATA_TYPE_ACKNOWLEDGED  (0x4F)
#define ANT_DATA_TYPE_BURST         (0x50)
#define ANT_DATA_TYPE_ADV_BURST     (0x72)

/*Packet Identifiers */
#define BT_CMD_PKT_ID 0x01
#define FM_CMD_PKT_ID 0x11
#define BT_EVT_PKT_ID 0x04
#define FM_EVT_PKT_ID 0x14
#define ANT_CMD_PKT_ID  0x0C
#define ANT_EVT_PKT_ID  0x0C
#define ANT_DATA_PKT_ID 0x0E
#define BT_ACL_DATA_PKT_ID 0x02

#define SMD_BUF_SIZE (9000)
#define UART_BUF_SIZE (9000)

#define PC_TO_SOC       (1)
#define SOC_TO_PC       (2)

int get_acl_pkt_length(unsigned char, unsigned char);
unsigned short get_pkt_data_len(unsigned char type, unsigned char *buf);
int init_pc_interface(wdsdaemon *wds);
int init_soc_interface(wdsdaemon *wds);
int establish_server_socket(wdsdaemon *wds);
