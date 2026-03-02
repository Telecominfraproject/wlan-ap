/*
 * Copyright (c) 2012, 2017-2019 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 *
 */

#include "msg.h"
#include "diag_lsm.h"
#include "stdio.h"
#include "diagpkt.h"
#include "diagcmd.h"
#include "string.h"
#include <unistd.h>
#include <stdlib.h>
#include <pthread.h>
#include <fcntl.h>
#include <errno.h>
#define DEFAULT_SMD_PORT QMI_PORT_RMNET_0
#define LOGI(...) fprintf(stderr, "I:" __VA_ARGS__)
#define DIAG_CONTROL_F 41
#define EDL_RESET_CMD_CODE 1

#define PACKED __attribute__ ((__packed__))

#ifdef USE_GLIB
#include <glib.h>
#define strlcat g_strlcat
#define strlcpy g_strlcpy
#endif

/* string to send to reboot_daemon to reboot */
#define REBOOT_STR "REBOOT"
/* string to send to reboot_daemon to edl-reboot */
#define EDL_REBOOT_STR "EDL-REBOOT"
/* size to make write buffer */
#define MAX_BUF 64
/* name of pipe to write to */
#define FIFO_NAME "/dev/rebooterdev"

/* ID's for diag */
#define DIAG_UID 53
#define DIAG_GID 53
#define REBOOTERS_GID 1301
#define SDCARD_GID 1015

/* QDSS defines */
#define QDSSDIAG_PROCESSOR_APPS   0x0100
#define QDSS_DIAG_PROC_ID QDSSDIAG_PROCESSOR_APPS

#define QDSS_QUERY_STATUS              0x00
#define QDSS_TRACE_SINK                0x01
#define QDSS_FILTER_ETM                0x02
#define QDSS_FILTER_STM                0x03
#define QDSS_FILTER_HWEVENT_ENABLE     0x04

#define QDSS_FILTER_HWEVENT_CONFIGURE  0x31
#define QDSS_QTIMER_TS_SYNC            0x60

#define QDSS_DIAG_MAC_PROC_ID	0x4200
#define QDSS_MAC0		0x00
#define QDSS_MAC1		0x01
#define QDSS_MAC2		0x02
#define QDSS_UMAC		0x03

#define   TMC_TRACESINK_ETB   0
#define   TMC_TRACESINK_RAM   1
#define   TMC_TRACESINK_TPIU  2
#define   TMC_TRACESINK_USB   3
#define   TMC_TRACESINK_STREAM   4
#define   TMC_TRACESINK_USB_BUFFERED   4
#define   TMC_TRACESINK_SD    6

#define QDSS_RSP_SUCCESS  0
#define QDSS_RSP_FAIL  1

#define QDSS_CLK_FREQ_HZ 19200000
#define QTIMER_CLK_FREQ_HZ 19200000

#define QDSS_ETB_SINK_FILE "/sys/bus/coresight/devices/coresight-tmc-etf/curr_sink"
#define QDSS_ETB_SINK_FILE_2 "/sys/bus/coresight/devices/coresight-tmc-etf/enable_sink"
#define QDSS_ETR_SINK_FILE "/sys/bus/coresight/devices/coresight-tmc-etr/curr_sink"
#define QDSS_ETR_SINK_FILE_2 "/sys/bus/coresight/devices/coresight-tmc-etr/enable_sink"
#define QDSS_ETR_OUTMODE_FILE "/sys/bus/coresight/devices/coresight-tmc-etr/out_mode"
#define QDSS_TPIU_SINK_FILE "/sys/bus/coresight/devices/coresight-tpiu/curr_sink"
#define QDSS_TPIU_OUTMODE_FILE "/sys/bus/coresight/devices/coresight-tpiu/out_mode"
#define QDSS_STM_FILE "/sys/bus/coresight/devices/coresight-stm/enable"
#define QDSS_STM_FILE_2 "/sys/bus/coresight/devices/coresight-stm/enable_source"
#define QDSS_HWEVENT_FILE "/sys/bus/coresight/devices/coresight-hwevent/enable"
#define QDSS_STM_HWEVENT_FILE "/sys/bus/coresight/devices/coresight-stm/hwevent_enable"
#define QDSS_HWEVENT_SET_REG_FILE "/sys/bus/coresight/devices/coresight-hwevent/setreg"
#define QDSS_SWAO_CSR_TIMESTAMP "/sys/bus/coresight/devices/coresight-swao-csr/timestamp"
#define QDSS_MACEVENT_SET_CFG_FILE "/data/vendor/wifi/qdss_trace_config.bin"
#define QDSS_CNSCLI_CMD_FILE "/tmp/cnsscli_cmds.txt"

enum mac_trace_sub_enum
{
	SW,
	HWSCH,
	PGD,
	TXDMA,
	RXDMA,
	TXOLE,
	RXOLE,
	CRYPTO,
	TXPCU,
	RXPCU,
	RRI,
	AMPI,
	C,
	MXI,
	MCMN,
	RXDMA1,
	LEPC,
	RXOLEB1,
	SFM,
	LPECEB1
};

enum umac_trace_sub_enum
{
	USW,
	CE,
	CXC,
	REO,
	TQM,
	WBM,
	TCL,
	CXC2,
	TCL_1,
	WBM2,
	REO2,
	TQM2,
	PHY_A,
	PHY_B,
};


typedef struct _mac_diag_cfg_pkt_tag
{
  uint8 mac_id;		//0 or 1
			//VID	Val1	Val2	Val3	Val4
  /*mac0 & mac1*/
  uint32 sw[5];
  uint32 hwsch[5];
  uint32 pdg[5];
  uint32 txdma[5];
  uint32 rxdma[5];
  uint32 txole[5];
  uint32 rxole[5];
  uint32 crypto[5];
  uint32 txpcu[5];
  uint32 rxpcu[5];
  uint32 rri[5];
  uint32 ampi[5];
  uint32 c[5];
  uint32 mxi[5];
  uint32 mcmn[5];
  uint32 rxdma1[5];
  uint32 lepc[5];
  uint32 rxoleb1[5];
  uint32 sfm[5];
  uint32 lpeceb1[5];
}__attribute__((packed))_mac_diag_cfg_pkt_t;

typedef struct _umac_diag_cfg_pkt_tag
{
  /*umac*/
  uint8 mac_id;		// 3
  uint32 usw[5];
  uint32 ce[5];
  uint32 cxc[5];
  uint32 reo[5];
  uint32 tqm[5];
  uint32 wbm[5];
  uint32 tcl[5];
  uint32 cxc2[5];
  uint32 tcl_1[5];
  uint32 wbm2[5];
  uint32 reo2[5];
  uint32 tqm2[5];
  uint32 phy_a[5];
  uint32 phy_b[5];
}__attribute__((packed))_umac_diag_cfg_pkt_t;

/* QDSS */
typedef struct
{
  uint8 cmdCode;        // Diag Message ID
  uint8 subsysId;       // Subsystem ID (DIAG_SUBSYS_QDSS)
  uint16 subsysCmdCode; // Subsystem command code
}__attribute__((packed))qdss_diag_pkt_hdr;

typedef struct
{
  qdss_diag_pkt_hdr hdr;
}__attribute__((packed))qdss_diag_pkt_req;

typedef struct
{
  qdss_diag_pkt_hdr hdr; // Header
  uint8 result;          //See QDSS_CMDRESP_... definitions
}__attribute__((packed))qdss_diag_pkt_rsp;

typedef struct
{
  qdss_diag_pkt_hdr hdr;
  uint8 trace_sink;
}__attribute__((packed))qdss_trace_sink_req;

typedef qdss_diag_pkt_rsp qdss_trace_sink_rsp; //generic response

typedef struct
{
  qdss_diag_pkt_hdr hdr;
  _mac_diag_cfg_pkt_t mac_pkt;
}__attribute__((packed))qdss_trace_mac_req;

typedef qdss_diag_pkt_rsp qdss_trace_mac_rsp; //generic mac response

typedef struct
{
  qdss_diag_pkt_hdr hdr;
  _umac_diag_cfg_pkt_t umac_pkt;
}__attribute__((packed))qdss_trace_umac_req;

typedef qdss_diag_pkt_rsp qdss_trace_umac_rsp; //generic umac response

typedef struct
{
  qdss_diag_pkt_hdr hdr;
  uint8  state;
}__attribute__((packed))qdss_filter_etm_req;

typedef qdss_diag_pkt_rsp qdss_filter_etm_rsp;

typedef struct
{
  qdss_diag_pkt_hdr hdr;
  uint8  state;
}__attribute__((packed))qdss_filter_stm_req;

typedef qdss_diag_pkt_rsp qdss_filter_stm_rsp;

typedef struct
{
  qdss_diag_pkt_hdr hdr;
  uint8  state;
}__attribute__((packed))qdss_filter_hwevents_req;

typedef qdss_diag_pkt_rsp qdss_filter_hwevents_rsp;

typedef struct
{
  qdss_diag_pkt_hdr hdr;
  uint32 register_addr;
  uint32 register_value;
}__attribute__((packed))qdss_filter_hwevents_configure_req;

typedef qdss_diag_pkt_rsp qdss_filter_hwevents_configure_rsp;

typedef struct
{
  qdss_diag_pkt_hdr hdr;
}__attribute__((packed))qdss_query_status_req;

typedef struct
{
  qdss_diag_pkt_hdr hdr;
  uint8 trace_sink;
  uint8 stm_enabled;
  uint8 hw_events_enabled;
}__attribute__((packed))qdss_query_status_rsp;

typedef struct
{
  qdss_diag_pkt_hdr hdr;
}__attribute__((packed))qdss_qtimer_ts_sync_req;

typedef struct
{
  qdss_diag_pkt_hdr hdr;
  uint32 status;
  uint64 qdss_ticks;
  uint64 qtimer_ticks;
  uint64 qdss_freq;
  uint64 qtimer_freq;
}__attribute__((packed))qdss_qtimer_ts_sync_rsp;

//enable the line below if you want to turn on debug messages
//#define DIAG_REBOOT_DEBUG

PACK(void *) qdss_diag_pkt_handler(PACK(void *) pReq, uint16 pkt_len);

/*qdss dispatch table*/
static const diagpkt_user_table_entry_type qdss_diag_pkt_tbl[] =
{
    {QDSS_DIAG_PROC_ID | QDSS_QUERY_STATUS,
    QDSS_DIAG_PROC_ID | QDSS_QTIMER_TS_SYNC,
    qdss_diag_pkt_handler}
};


PACK(void *) qdss_diag_mac_pkt_handler(PACK(void *) pReq, uint16 pkt_len);
static const diagpkt_user_table_entry_type qdss_diag_mac_pkt_tbl[] =
{
    {QDSS_DIAG_MAC_PROC_ID | QDSS_MAC0,
    QDSS_DIAG_MAC_PROC_ID | QDSS_UMAC,
    qdss_diag_mac_pkt_handler}
};


static char qdss_sink = 0, qdss_hwevent = 0, qdss_stm = 0;

void drop_privileges() {

    /* Start as root */
    /* Update primary group */
    setgid(DIAG_GID);

    /* Update secondary groups */
    gid_t *groups;
    int numGroups = 2;
    groups = (gid_t *) malloc(numGroups * sizeof(gid_t));
    if (groups){
        groups[0] = REBOOTERS_GID;
        groups[1] = SDCARD_GID;
    }
    setgroups(numGroups, groups);

    /* Update UID -- from root to diag */
    setuid(DIAG_UID);

    free(groups);
}

void setup_qdss_sysfs_nodes() {
    chown(QDSS_ETB_SINK_FILE, DIAG_UID, -1);
    chown(QDSS_ETB_SINK_FILE_2, DIAG_UID, -1);
    chown(QDSS_ETR_SINK_FILE, DIAG_UID, -1);
    chown(QDSS_ETR_SINK_FILE_2, DIAG_UID, -1);
    chown(QDSS_ETR_OUTMODE_FILE, DIAG_UID, -1);
    chown(QDSS_TPIU_SINK_FILE, DIAG_UID, -1);
    chown(QDSS_TPIU_OUTMODE_FILE, DIAG_UID, -1);
    chown(QDSS_STM_FILE, DIAG_UID, -1);
    chown(QDSS_STM_FILE_2, DIAG_UID, -1);
    chown(QDSS_HWEVENT_FILE, DIAG_UID, -1);
    chown(QDSS_STM_HWEVENT_FILE, DIAG_UID, -1);
    chown(QDSS_HWEVENT_SET_REG_FILE, DIAG_UID, -1);
    chown(QDSS_MACEVENT_SET_CFG_FILE, DIAG_UID, -1);
}

int main (void) {

    boolean bInit_Success = FALSE;
    bInit_Success = Diag_LSM_Init(NULL);
    FILE *qdss_fd;

    if (!bInit_Success) {
        printf("Diag_LSM_Init() failed.\n");
        return -1;
    }

    DIAGPKT_DISPATCH_TABLE_REGISTER (DIAG_SUBSYS_QDSS, qdss_diag_pkt_tbl);
    DIAGPKT_DISPATCH_TABLE_REGISTER (DIAG_SUBSYS_QDSS, qdss_diag_mac_pkt_tbl);

    /*Header writing*/
    qdss_fd = fopen(QDSS_MACEVENT_SET_CFG_FILE, "w");
    if (qdss_fd == NULL) {
	LOGI("qdss open file: %s error: %s", QDSS_MACEVENT_SET_CFG_FILE, strerror(errno));
	exit(1);
    }
    fprintf(qdss_fd, "seq_start;\nseq_type:mac_event_trace;\nsink:etr_ddr;\n");
    fclose(qdss_fd);

    /*cnscli writing*/
    qdss_fd = fopen(QDSS_CNSCLI_CMD_FILE, "w");
    if (qdss_fd == NULL) {
	LOGI("qdss open file: %s error: %s", QDSS_CNSCLI_CMD_FILE, strerror(errno));
	exit(1);
    }
    fprintf(qdss_fd, "qdss_trace_load_config\nqdss_trace_start\nquit\n");
    fclose(qdss_fd);

    do {
        sleep (1);
    } while (1);

    Diag_LSM_DeInit();

    return 0;
}

static int qdss_file_write_str(const char *qdss_file_path, const char *str)
{
    int qdss_fd, ret;

    if (!qdss_file_path || !str) {
        return QDSS_RSP_FAIL;
    }

    qdss_fd = open(qdss_file_path, O_RDWR | O_APPEND);
    if (qdss_fd < 0) {
        LOGI("qdss open file: %s error: %s", qdss_file_path, strerror(errno));
        return QDSS_RSP_FAIL;
    }

    ret = write(qdss_fd, str, strlen(str));
    if (ret < 0) {
        LOGI("qdss write file: %s error: %s", qdss_file_path, strerror(errno));
        close(qdss_fd);
        return QDSS_RSP_FAIL;
    }

    close(qdss_fd);

    return QDSS_RSP_SUCCESS;
}

static int qdss_file_write_byte(const char *qdss_file_path, unsigned char val)
{
    int qdss_fd, ret;

    if (!qdss_file_path) {
        return QDSS_RSP_FAIL;
    }

    qdss_fd = open(qdss_file_path, O_WRONLY);
    if (qdss_fd < 0) {
        LOGI("qdss open file: %s error: %s", qdss_file_path, strerror(errno));
        return QDSS_RSP_FAIL;
    }

    ret = write(qdss_fd, &val, 1);
    if (ret < 0) {
        LOGI("qdss write file: %s error: %s", qdss_file_path, strerror(errno));
        close(qdss_fd);
        return QDSS_RSP_FAIL;
    }

    close(qdss_fd);

    return QDSS_RSP_SUCCESS;
}

/* Sets the Trace Sink */
static int qdss_trace_sink_handler(qdss_trace_sink_req *pReq, int req_len, qdss_trace_sink_rsp *pRsp, int rsp_len)
{
    int ret = 0;

    if (!pReq || !pRsp) {
        return QDSS_RSP_FAIL;
    }

    pRsp->result = QDSS_RSP_FAIL;
    if (pReq->trace_sink == TMC_TRACESINK_ETB) {
        /* For enabling writing ASCII value of 1 i.e. 0x31 */
        ret = qdss_file_write_byte(QDSS_ETB_SINK_FILE, 0x31);
        if (ret) {
            ret = qdss_file_write_byte(QDSS_ETB_SINK_FILE_2, 0x31);
            if (ret)
                return QDSS_RSP_FAIL;
        }
    } else if (pReq->trace_sink == TMC_TRACESINK_RAM) {
        ret = qdss_file_write_byte(QDSS_STM_FILE, 0x30);
        if (ret) {
            return QDSS_RSP_FAIL;
        }
        ret = qdss_file_write_str(QDSS_ETR_OUTMODE_FILE, "mem");
        if (ret) {
            return QDSS_RSP_FAIL;
        }

    } else if (pReq->trace_sink == TMC_TRACESINK_STREAM) {
        ret = qdss_file_write_byte(QDSS_STM_FILE, 0x30);
        if (ret) {
            return QDSS_RSP_FAIL;
        }
        ret = qdss_file_write_str(QDSS_ETR_OUTMODE_FILE, "q6mem_stream");
        if (ret) {
            return QDSS_RSP_FAIL;
        }

    } else if (pReq->trace_sink == TMC_TRACESINK_USB) {
        ret = qdss_file_write_byte(QDSS_STM_FILE, 0x30);
        if (ret) {
            return QDSS_RSP_FAIL;
        }
        ret = qdss_file_write_str(QDSS_ETR_OUTMODE_FILE, "usb");
        if (ret) {
            return QDSS_RSP_FAIL;
        }

    } else if (pReq->trace_sink == TMC_TRACESINK_TPIU) {
        ret = qdss_file_write_byte(QDSS_TPIU_SINK_FILE, 0x31);
        if (ret) {
            return QDSS_RSP_FAIL;
        }

        ret = qdss_file_write_str(QDSS_TPIU_OUTMODE_FILE, "mictor");
        if (ret) {
            return QDSS_RSP_FAIL;
        }
    } else if (pReq->trace_sink == TMC_TRACESINK_SD) {
        ret = qdss_file_write_byte(QDSS_TPIU_SINK_FILE, 0x31);
        if (ret) {
            return QDSS_RSP_FAIL;
        }

        ret = qdss_file_write_str(QDSS_TPIU_OUTMODE_FILE, "sdc");
        if (ret) {
            return QDSS_RSP_FAIL;
        }
    } else {
        qdss_sink = 0;
        return QDSS_RSP_FAIL;
    }

    qdss_sink = pReq->trace_sink;
    pRsp->result = QDSS_RSP_SUCCESS;

    return QDSS_RSP_SUCCESS;
}

/* Enable/Disable STM */
static int qdss_filter_stm_handler(qdss_filter_stm_req *pReq, int req_len, qdss_filter_stm_rsp *pRsp, int rsp_len)
{
    char ret = 0, stm_state = 0;

    if (!pReq || !pRsp) {
        return QDSS_RSP_FAIL;
    }

    pRsp->result = QDSS_RSP_FAIL;

    if (pReq->state) {
        stm_state = 1;
    } else {
        stm_state = 0;
    }

    ret = qdss_file_write_byte(QDSS_STM_FILE, stm_state + 0x30);
    if (ret) {
        ret = qdss_file_write_byte(QDSS_STM_FILE_2, stm_state + 0x30);
        if (ret)
            return QDSS_RSP_FAIL;
    }

    qdss_stm = stm_state;
    pRsp->result = QDSS_RSP_SUCCESS;

    return QDSS_RSP_SUCCESS;
}

/* Enable/Disable HW Events */
static int qdss_filter_hwevents_handler(qdss_filter_hwevents_req *pReq, int req_len, qdss_filter_hwevents_rsp *pRsp, int rsp_len)
{
    int ret = 0;

    if (!pReq || !pRsp) {
        return QDSS_RSP_FAIL;
    }

    pRsp->result = QDSS_RSP_FAIL;

    if (pReq->state) {

        qdss_hwevent = 1;
        /* For disabling writing ASCII value of 0 i.e. 0x30 */
        ret = qdss_file_write_byte(QDSS_HWEVENT_FILE, 0x30);
        if (ret) {
            return QDSS_RSP_FAIL;
        }

        ret = qdss_file_write_byte(QDSS_STM_HWEVENT_FILE, 0x30);
        if (ret) {
            return QDSS_RSP_FAIL;
        }

        ret = qdss_file_write_byte(QDSS_HWEVENT_FILE, 0x31);
        if (ret) {
            return QDSS_RSP_FAIL;
        }

        ret = qdss_file_write_byte(QDSS_STM_HWEVENT_FILE, 0x31);
        if (ret) {
            return QDSS_RSP_FAIL;
        }

    } else {

        qdss_hwevent = 0;

        ret = qdss_file_write_byte(QDSS_HWEVENT_FILE, 0x30);
        if (ret) {
            return QDSS_RSP_FAIL;
        }

        ret = qdss_file_write_byte(QDSS_STM_HWEVENT_FILE, 0x30);
        if (ret) {
            return QDSS_RSP_FAIL;
        }
    }

    pRsp->result = QDSS_RSP_SUCCESS;
    return QDSS_RSP_SUCCESS;
}

/* Programming registers to generate HW events */
static int qdss_filter_hwevents_configure_handler(qdss_filter_hwevents_configure_req *pReq, int req_len, qdss_filter_hwevents_configure_rsp *pRsp, int rsp_len)
{
    char reg_buf[100];
    int ret = 0, qdss_fd;

    if (!pReq || !pRsp) {
        return QDSS_RSP_FAIL;
    }

    pRsp->result = QDSS_RSP_FAIL;

    snprintf(reg_buf, sizeof(reg_buf), "%x %x", pReq->register_addr, pReq->register_value);

    qdss_fd = open(QDSS_HWEVENT_SET_REG_FILE, O_WRONLY);
    if (qdss_fd < 0) {
        LOGI("qdss open file: %s error: %s", QDSS_HWEVENT_SET_REG_FILE, strerror(errno));
        return QDSS_RSP_FAIL;
    }

    ret = write(qdss_fd, reg_buf, strlen(reg_buf));
    if (ret < 0) {
        LOGI("qdss write file: %s error: %s", QDSS_HWEVENT_SET_REG_FILE, strerror(errno));
        close(qdss_fd);
        return QDSS_RSP_FAIL;
    }

    close(qdss_fd);

    pRsp->result = QDSS_RSP_SUCCESS;
    return QDSS_RSP_SUCCESS;
}

/* Get the status of sink, stm and HW events */
static int qdss_query_status_handler(qdss_query_status_req *pReq, int req_len, qdss_query_status_rsp *pRsp, int rsp_len)
{

    if (!pReq || !pRsp) {
        return QDSS_RSP_FAIL;
    }

    pRsp->trace_sink = qdss_sink;
    pRsp->stm_enabled = qdss_stm;
    pRsp->hw_events_enabled = qdss_hwevent;

    return QDSS_RSP_SUCCESS;
}

/* Handling of qdss qtimer ts sync */
static int qdss_qtimer_ts_sync_handler(qdss_qtimer_ts_sync_req *pReq, int req_len, qdss_qtimer_ts_sync_rsp *pRsp, int rsp_len)
{
    int ret = 0, qdss_ts_fd = 0;
    uint64_t qdss_ticks = 0, qtimer_ticks = 0;
    char qdss_ts_val[17];

    if (!pReq || !pRsp) {
        return QDSS_RSP_FAIL;
    }

    memset(qdss_ts_val, '\0', sizeof(qdss_ts_val));

    qdss_ts_fd = open(QDSS_SWAO_CSR_TIMESTAMP, O_RDONLY);
    if (qdss_ts_fd < 0) {
        LOGI("qdss open file: %s error: %s", QDSS_SWAO_CSR_TIMESTAMP, strerror(errno));
        goto fail;
    }

    ret = read(qdss_ts_fd, qdss_ts_val, sizeof(qdss_ts_val)-1);
    if (ret < 0) {
        LOGI("qdss read file: %s error: %s", QDSS_SWAO_CSR_TIMESTAMP, strerror(errno));
        close(qdss_ts_fd);
        goto fail;
    }

    qdss_ticks = atoll(qdss_ts_val);

    close(qdss_ts_fd);

#if defined __aarch64__ && __aarch64__ == 1
    asm volatile("mrs %0, cntvct_el0" : "=r" (qtimer_ticks));
#else
    asm volatile("mrrc p15, 1, %Q0, %R0, c14" : "=r" (qtimer_ticks));
#endif

    pRsp->status = 0;
    pRsp->qdss_ticks = qdss_ticks;
    pRsp->qtimer_ticks = qtimer_ticks;
    pRsp->qdss_freq = QDSS_CLK_FREQ_HZ;
    pRsp->qtimer_freq = QTIMER_CLK_FREQ_HZ;
    return QDSS_RSP_SUCCESS;
fail:
    pRsp->status = 1;
    pRsp->qdss_ticks = 0;
    pRsp->qtimer_ticks = 0;
    pRsp->qdss_freq = 0;
    pRsp->qtimer_freq = 0;
    return QDSS_RSP_FAIL;
}

/* QDSS commands handler */
PACK(void *) qdss_diag_pkt_handler(PACK(void *) pReq, uint16 pkt_len)
{
/*
 * 1) Checks the request command size. If it fails send error response.
 * 2) If request command size is valid then allocates response packet
 *    based on request.
 * 3) Invokes the respective command handler
 */

#define QDSS_HANDLE_DIAG_CMD(cmd)                              \
   if (pkt_len < sizeof(cmd##_req)) {                          \
      pRsp = diagpkt_err_rsp(DIAG_BAD_LEN_F, pReq, pkt_len);   \
   }                                                           \
   else {                                                      \
      pRsp =  diagpkt_subsys_alloc(DIAG_SUBSYS_QDSS,           \
                                   pHdr->subsysCmdCode,        \
                                   sizeof(cmd##_rsp));         \
      if (NULL != pRsp) {                                      \
         cmd##_handler((cmd##_req *)pReq,                      \
                       pkt_len,                                \
                       (cmd##_rsp *)pRsp,                      \
                       sizeof(cmd##_rsp));                     \
      }                                                        \
   }

    qdss_diag_pkt_hdr *pHdr;
    PACK(void *)pRsp = NULL;

    if (NULL != pReq) {
        pHdr = (qdss_diag_pkt_hdr *)pReq;

        switch (pHdr->subsysCmdCode & 0x0FF) {
        case QDSS_QUERY_STATUS:
            QDSS_HANDLE_DIAG_CMD(qdss_query_status);
            break;
        case QDSS_TRACE_SINK:
            QDSS_HANDLE_DIAG_CMD(qdss_trace_sink);
            break;
        case QDSS_FILTER_STM:
            QDSS_HANDLE_DIAG_CMD(qdss_filter_stm);
            break;
        case QDSS_FILTER_HWEVENT_ENABLE:
            QDSS_HANDLE_DIAG_CMD(qdss_filter_hwevents);
            break;
        case QDSS_FILTER_HWEVENT_CONFIGURE:
            QDSS_HANDLE_DIAG_CMD(qdss_filter_hwevents_configure);
            break;
        case QDSS_QTIMER_TS_SYNC:
            QDSS_HANDLE_DIAG_CMD(qdss_qtimer_ts_sync);
            break;
        default:
            pRsp = diagpkt_err_rsp(DIAG_BAD_CMD_F, pReq, pkt_len);
            break;
        }

        if (NULL != pRsp) {
            diagpkt_commit(pRsp);
            pRsp = NULL;
        }
    }
    return (pRsp);
}

static int qdss_trace_mac_handler(qdss_trace_mac_req *pReq, int req_len, qdss_trace_mac_rsp *pRsp, int rsp_len)
{
    int i;
    FILE *qdss_fd;

    if (!pReq || !pRsp) {
        return QDSS_RSP_FAIL;
    }

	qdss_fd = fopen(QDSS_MACEVENT_SET_CFG_FILE, "a");
	if (qdss_fd == NULL) {
		LOGI("qdss open file: %s error: %s", QDSS_MACEVENT_SET_CFG_FILE, strerror(errno));
		return QDSS_RSP_FAIL;
    }

    fprintf(qdss_fd, "subsys_cfg_start:mac%d;\n", pReq->mac_pkt.mac_id);

    for (i = SW; i <= LPECEB1; i++) {
		switch (i) {
		case SW:
			if (pReq->mac_pkt.sw[0]) {
				fprintf(qdss_fd, "sw:0x%x, 0x%x, 0x%x, 0x%x, 0x%x;\n",
						pReq->mac_pkt.sw[0], pReq->mac_pkt.sw[1],
						pReq->mac_pkt.sw[2],pReq->mac_pkt.sw[3],
						pReq->mac_pkt.sw[4]);

		    }
		break;

		case HWSCH:
			if (pReq->mac_pkt.hwsch[0]) {
				fprintf(qdss_fd, "hwsch:0x%x, 0x%x, 0x%x, 0x%x, 0x%x;\n",
						pReq->mac_pkt.hwsch[0], pReq->mac_pkt.hwsch[1],
						pReq->mac_pkt.hwsch[2], pReq->mac_pkt.hwsch[3],
						pReq->mac_pkt.hwsch[4]);
			}
			break;


		case PGD:
			if (pReq->mac_pkt.pdg[0]) {
				fprintf(qdss_fd, "pdg:0x%x, 0x%x, 0x%x, 0x%x, 0x%x;\n",
						pReq->mac_pkt.pdg[0], pReq->mac_pkt.pdg[1],
						pReq->mac_pkt.pdg[2], pReq->mac_pkt.pdg[3],
						pReq->mac_pkt.pdg[4]);
			}
			break;

		case TXDMA:
			if (pReq->mac_pkt.txdma[0]) {
				fprintf(qdss_fd, "txdma:0x%x, 0x%x, 0x%x, 0x%x, 0x%x;\n",
						pReq->mac_pkt.txdma[0], pReq->mac_pkt.txdma[1],
						pReq->mac_pkt.txdma[2], pReq->mac_pkt.txdma[3],
						pReq->mac_pkt.txdma[4]);
			}
			break;

		case RXDMA:
			if (pReq->mac_pkt.rxdma[0]) {
				fprintf(qdss_fd, "rxdma:0x%x, 0x%x, 0x%x, 0x%x, 0x%x;\n",
						pReq->mac_pkt.rxdma[0], pReq->mac_pkt.rxdma[1],
						pReq->mac_pkt.rxdma[2], pReq->mac_pkt.rxdma[3],
						pReq->mac_pkt.rxdma[4]);
			}
			break;

		case TXOLE:
			if (pReq->mac_pkt.txole[0]) {
				fprintf(qdss_fd, "txole:0x%x, 0x%x, 0x%x, 0x%x, 0x%x;\n",
						pReq->mac_pkt.txole[0], pReq->mac_pkt.txole[1],
						pReq->mac_pkt.txole[2], pReq->mac_pkt.txole[3],
						pReq->mac_pkt.txole[4]);
			}
			break;

		case RXOLE:
			if (pReq->mac_pkt.rxole[0]) {
				fprintf(qdss_fd, "rxole:0x%x, 0x%x, 0x%x, 0x%x, 0x%x;\n",
						pReq->mac_pkt.rxole[0], pReq->mac_pkt.rxole[1],
						pReq->mac_pkt.rxole[2], pReq->mac_pkt.rxole[3],
						pReq->mac_pkt.rxole[4]);
			}
			break;

		case CRYPTO:
			if (pReq->mac_pkt.crypto[0]) {
				fprintf(qdss_fd, "crypto:0x%x, 0x%x, 0x%x, 0x%x, 0x%x;\n",
						pReq->mac_pkt.crypto[0], pReq->mac_pkt.crypto[1],
						pReq->mac_pkt.crypto[2], pReq->mac_pkt.crypto[3],
						pReq->mac_pkt.crypto[4]);
			}
			break;

		case TXPCU:
			if (pReq->mac_pkt.txpcu[0]) {
				fprintf(qdss_fd, "txpcu:0x%x, 0x%x, 0x%x, 0x%x, 0x%x;\n",
						pReq->mac_pkt.txpcu[0], pReq->mac_pkt.txpcu[1],
						pReq->mac_pkt.txpcu[2], pReq->mac_pkt.txpcu[3],
						pReq->mac_pkt.txpcu[4]);
			}
			break;

		case RXPCU:
			if (pReq->mac_pkt.rxpcu[0]) {
				fprintf(qdss_fd, "rxpcu:0x%x, 0x%x, 0x%x, 0x%x, 0x%x;\n",
						pReq->mac_pkt.rxpcu[0], pReq->mac_pkt.rxpcu[1],
						pReq->mac_pkt.rxpcu[2], pReq->mac_pkt.rxpcu[3],
						pReq->mac_pkt.rxpcu[4]);
			}
			break;

		case RRI:
			if (pReq->mac_pkt.rri[0]) {
				fprintf(qdss_fd, "rri:0x%x, 0x%x, 0x%x, 0x%x, 0x%x;\n",
						pReq->mac_pkt.rri[0], pReq->mac_pkt.rri[1],
						pReq->mac_pkt.rri[2], pReq->mac_pkt.rri[3],
						pReq->mac_pkt.rri[4]);
			}
			break;

		case AMPI:
			if (pReq->mac_pkt.ampi[0]) {
				fprintf(qdss_fd, "ampi:0x%x, 0x%x, 0x%x, 0x%x, 0x%x;\n",
						pReq->mac_pkt.ampi[0], pReq->mac_pkt.ampi[1],
						pReq->mac_pkt.ampi[2], pReq->mac_pkt.ampi[3],
						pReq->mac_pkt.ampi[4]);
			}
			break;

		case C:
			if (pReq->mac_pkt.c[0]) {
				fprintf(qdss_fd, "c:0x%x, 0x%x, 0x%x, 0x%x, 0x%x;\n",
						pReq->mac_pkt.c[0], pReq->mac_pkt.c[1],
						pReq->mac_pkt.c[2], pReq->mac_pkt.c[3],
						pReq->mac_pkt.c[4]);
			}
			break;

		case MXI:
			if (pReq->mac_pkt.mxi[0]) {
				fprintf(qdss_fd, "mxi:0x%x, 0x%x, 0x%x, 0x%x, 0x%x;\n",
						pReq->mac_pkt.mxi[0], pReq->mac_pkt.mxi[1],
						pReq->mac_pkt.mxi[2], pReq->mac_pkt.mxi[3],
						pReq->mac_pkt.mxi[4]);
			}
			break;

		case MCMN:
			if (pReq->mac_pkt.mcmn[0]) {
				fprintf(qdss_fd, "mcmn:0x%x, 0x%x, 0x%x, 0x%x, 0x%x;\n",
						pReq->mac_pkt.mcmn[0], pReq->mac_pkt.mcmn[1],
						pReq->mac_pkt.mcmn[2], pReq->mac_pkt.mcmn[3],
						pReq->mac_pkt.mcmn[4]);
			}
			break;

		case RXDMA1:
			if (pReq->mac_pkt.rxdma1[0]) {
				fprintf(qdss_fd, "rxdma1:0x%x, 0x%x, 0x%x, 0x%x, 0x%x;\n",
						pReq->mac_pkt.rxdma1[0], pReq->mac_pkt.rxdma1[1],
						pReq->mac_pkt.rxdma1[2], pReq->mac_pkt.rxdma1[3],
						pReq->mac_pkt.rxdma1[4]);
			}
			break;

		case LEPC:
			if (pReq->mac_pkt.lepc[0]) {
				fprintf(qdss_fd, "lepc:0x%x, 0x%x, 0x%x, 0x%x, 0x%x;\n",
						pReq->mac_pkt.lepc[0], pReq->mac_pkt.lepc[1],
						pReq->mac_pkt.lepc[2], pReq->mac_pkt.lepc[3],
						pReq->mac_pkt.lepc[4]);
			}
			break;

		case RXOLEB1:
			if (pReq->mac_pkt.rxoleb1[0]) {
				fprintf(qdss_fd, "rxoleb1:0x%x, 0x%x, 0x%x, 0x%x, 0x%x;\n",
						pReq->mac_pkt.rxoleb1[0], pReq->mac_pkt.rxoleb1[1],
						pReq->mac_pkt.rxoleb1[2], pReq->mac_pkt.rxoleb1[3],
						pReq->mac_pkt.rxoleb1[4]);
			}
			break;

		case SFM:
			if (pReq->mac_pkt.sfm[0]) {
				fprintf(qdss_fd, "sfm:0x%x, 0x%x, 0x%x, 0x%x, 0x%x;\n",
						pReq->mac_pkt.sfm[0], pReq->mac_pkt.sfm[1],
						pReq->mac_pkt.sfm[2], pReq->mac_pkt.sfm[3],
						pReq->mac_pkt.sfm[4]);
			}
			break;

		case LPECEB1:
			if (pReq->mac_pkt.lpeceb1[0]) {
				fprintf(qdss_fd, "lpeceb1:0x%x, 0x%x, 0x%x, 0x%x, 0x%x;\n",
						pReq->mac_pkt.lpeceb1[0], pReq->mac_pkt.lpeceb1[1],
						pReq->mac_pkt.lpeceb1[2], pReq->mac_pkt.lpeceb1[3],
						pReq->mac_pkt.lpeceb1[4]);
			}
			break;
		}
	}

	fprintf(qdss_fd, "subsys_cfg_end:mac%d;\n",pReq->mac_pkt.mac_id);

    fclose(qdss_fd);

    pRsp->result = QDSS_RSP_SUCCESS;
    return QDSS_RSP_SUCCESS;

}

static int qdss_trace_umac_handler(qdss_trace_umac_req *pReq, int req_len, qdss_trace_umac_rsp *pRsp, int rsp_len)
{
    int i;
    FILE *qdss_fd;

    if (!pReq || !pRsp) {
        return QDSS_RSP_FAIL;
    }

	qdss_fd = fopen(QDSS_MACEVENT_SET_CFG_FILE, "a");
	if (qdss_fd == NULL) {
		LOGI("qdss open file: %s error: %s", QDSS_MACEVENT_SET_CFG_FILE, strerror(errno));
		return QDSS_RSP_FAIL;
	}

	fprintf(qdss_fd, "subsys_cfg_start:umac;\n");

	for (i = USW; i <= PHY_B; i++) {
		switch (i) {
		case USW:
			if (pReq->umac_pkt.usw[0]) {
				fprintf(qdss_fd, "usw:0x%x, 0x%x, 0x%x, 0x%x, 0x%x;\n",
						pReq->umac_pkt.usw[0], pReq->umac_pkt.usw[1],
						pReq->umac_pkt.usw[2],pReq->umac_pkt.usw[3],
						pReq->umac_pkt.usw[4]);
			}
			break;

		case CE:
			if (pReq->umac_pkt.ce[0]) {
				fprintf(qdss_fd, "ce:0x%x, 0x%x, 0x%x, 0x%x, 0x%x;\n",
						pReq->umac_pkt.ce[0], pReq->umac_pkt.ce[1],
						pReq->umac_pkt.ce[2], pReq->umac_pkt.ce[3],
						pReq->umac_pkt.ce[4]);
			}
			break;

		case CXC:
			if (pReq->umac_pkt.cxc[0]) {
				fprintf(qdss_fd, "cxc:0x%x, 0x%x, 0x%x, 0x%x, 0x%x;\n",
						pReq->umac_pkt.cxc[0], pReq->umac_pkt.cxc[1],
						pReq->umac_pkt.cxc[2], pReq->umac_pkt.cxc[3],
						pReq->umac_pkt.cxc[4]);
			}
			break;

		case REO:
			if (pReq->umac_pkt.reo[0]) {
				fprintf(qdss_fd, "reo:0x%x, 0x%x, 0x%x, 0x%x, 0x%x;\n",
						pReq->umac_pkt.reo[0], pReq->umac_pkt.reo[1],
						pReq->umac_pkt.reo[2], pReq->umac_pkt.reo[3],
						pReq->umac_pkt.reo[4]);
			}
			break;

		case TQM:
			if (pReq->umac_pkt.tqm[0]) {
				fprintf(qdss_fd, "tqm:0x%x, 0x%x, 0x%x, 0x%x, 0x%x;\n",
						pReq->umac_pkt.tqm[0], pReq->umac_pkt.tqm[1],
						pReq->umac_pkt.tqm[2], pReq->umac_pkt.tqm[3],
						pReq->umac_pkt.tqm[4]);
			}
			break;

		case WBM:
			if (pReq->umac_pkt.wbm[0]) {
				fprintf(qdss_fd, "wbm:0x%x, 0x%x, 0x%x, 0x%x, 0x%x;\n",
						pReq->umac_pkt.wbm[0], pReq->umac_pkt.wbm[1],
						pReq->umac_pkt.wbm[2], pReq->umac_pkt.wbm[3],
						pReq->umac_pkt.wbm[4]);
			}
			break;

		case TCL:
			if (pReq->umac_pkt.tcl[0]) {
				fprintf(qdss_fd, "tcl:0x%x, 0x%x, 0x%x, 0x%x, 0x%x;\n",
						pReq->umac_pkt.tcl[0], pReq->umac_pkt.tcl[1],
						pReq->umac_pkt.tcl[2], pReq->umac_pkt.tcl[3],
						pReq->umac_pkt.tcl[4]);
			}
			break;

		case CXC2:
			if (pReq->umac_pkt.cxc2[0]) {
				fprintf(qdss_fd, "cxc2:0x%x, 0x%x, 0x%x, 0x%x, 0x%x;\n",
						pReq->umac_pkt.cxc2[0], pReq->umac_pkt.cxc2[1],
						pReq->umac_pkt.cxc2[2], pReq->umac_pkt.cxc2[3],
						pReq->umac_pkt.cxc2[4]);
			}
			break;

		case TCL_1:
			if (pReq->umac_pkt.tcl_1[0]) {
				fprintf(qdss_fd, "tcl_1:0x%x, 0x%x, 0x%x, 0x%x, 0x%x;\n",
						pReq->umac_pkt.tcl_1[0], pReq->umac_pkt.tcl_1[1],
						pReq->umac_pkt.tcl_1[2], pReq->umac_pkt.tcl_1[3],
						pReq->umac_pkt.tcl_1[4]);
			}
			break;

		case WBM2:
			if (pReq->umac_pkt.wbm2[0]) {
				fprintf(qdss_fd, "wbm2:0x%x, 0x%x, 0x%x, 0x%x, 0x%x;\n",
						pReq->umac_pkt.wbm2[0], pReq->umac_pkt.wbm2[1],
						pReq->umac_pkt.wbm2[2], pReq->umac_pkt.wbm2[3],
						pReq->umac_pkt.wbm2[4]);
			}
			break;

		case REO2:
			if (pReq->umac_pkt.reo2[0]) {
				fprintf(qdss_fd, "reo2:0x%x, 0x%x, 0x%x, 0x%x, 0x%x;\n",
						pReq->umac_pkt.reo2[0], pReq->umac_pkt.reo2[1],
						pReq->umac_pkt.reo2[2], pReq->umac_pkt.reo2[3],
						pReq->umac_pkt.reo2[4]);
			}
			break;

		case TQM2:
			if (pReq->umac_pkt.tqm2[0]) {
				fprintf(qdss_fd, "tqm2:0x%x, 0x%x, 0x%x, 0x%x, 0x%x;\n",
						pReq->umac_pkt.tqm2[0], pReq->umac_pkt.tqm2[1],
						pReq->umac_pkt.tqm2[2], pReq->umac_pkt.tqm2[3],
						pReq->umac_pkt.tqm2[4]);
			}
			break;

		case PHY_A:
			if (pReq->umac_pkt.phy_a[0]) {
				fprintf(qdss_fd, "phy_a:0x%x, 0x%x, 0x%x, 0x%x, 0x%x;\n",
						pReq->umac_pkt.phy_a[0], pReq->umac_pkt.phy_a[1],
						pReq->umac_pkt.phy_a[2], pReq->umac_pkt.phy_a[3],
						pReq->umac_pkt.phy_a[4]);
			}
			break;

		case PHY_B:
			if (pReq->umac_pkt.phy_b[0]) {
				fprintf(qdss_fd, "phy_b:0x%x, 0x%x, 0x%x, 0x%x, 0x%x;\n",
						pReq->umac_pkt.phy_b[0], pReq->umac_pkt.phy_b[1],
						pReq->umac_pkt.phy_b[2], pReq->umac_pkt.phy_b[3],
						pReq->umac_pkt.phy_b[4]);
			}
			break;
		}
	}
	fprintf(qdss_fd, "subsys_cfg_end:umac;\n");

	char rept[50] = "0xFFFFFFFF,0xFFFFFFFF,0xFFFFFFFF,0xFFFFFFFF";
	fprintf(qdss_fd, "swap:%.*s;\ntrigger_start:trc;\nwfi:%s;\nts0:%s;\nts1:%s;\nts2:%s;\nts3:%s;\nts4:%s;\ntrigger_end:trc;\nmemw:%.*s;\nseq_end;\n",10,rept,rept,rept,rept,rept,rept,rept,21,rept);

    fclose(qdss_fd);

	system("cd /data/vendor/wifi; cnsscli < /tmp/cnsscli_cmds.txt");

    pRsp->result = QDSS_RSP_SUCCESS;
    return QDSS_RSP_SUCCESS;
}

/* QDSS mac command handler */
PACK(void *) qdss_diag_mac_pkt_handler(PACK(void *) pReq, uint16 pkt_len)
{
/*
 * 1) Checks the request command size. If it fails send error response.
 * 2) If request command size is valid then allocates response packet
 *    based on request.
 * 3) Invokes the respective command handler
 */

#define QDSS_HANDLE_DIAG_CMD(cmd)                              \
   if (pkt_len < sizeof(cmd##_req)) {                          \
      pRsp = diagpkt_err_rsp(DIAG_BAD_LEN_F, pReq, pkt_len);   \
   }                                                           \
   else {                                                      \
      pRsp =  diagpkt_subsys_alloc(DIAG_SUBSYS_QDSS,           \
                                   pHdr->subsysCmdCode,        \
                                   sizeof(cmd##_rsp));         \
      if (NULL != pRsp) {                                      \
         cmd##_handler((cmd##_req *)pReq,                      \
                       pkt_len,                                \
                       (cmd##_rsp *)pRsp,                      \
                       sizeof(cmd##_rsp));                     \
      }                                                        \
   }
	qdss_diag_pkt_hdr *pHdr;
    PACK(void *)pRsp = NULL;

    if (NULL != pReq) {
        pHdr = (qdss_diag_pkt_hdr *)pReq;

        switch (pHdr->subsysCmdCode & 0x0FF) {
        case QDSS_MAC0:
            QDSS_HANDLE_DIAG_CMD(qdss_trace_mac);
            break;
        case QDSS_MAC1:
            QDSS_HANDLE_DIAG_CMD(qdss_trace_mac);
            break;
	case QDSS_MAC2:
	    QDSS_HANDLE_DIAG_CMD(qdss_trace_mac);
	    break;
        case QDSS_UMAC:
            QDSS_HANDLE_DIAG_CMD(qdss_trace_umac);
            break;
        default:
            pRsp = diagpkt_err_rsp(DIAG_BAD_CMD_F, pReq, pkt_len);
            break;
        }

        if (NULL != pRsp) {
            diagpkt_commit(pRsp);
            pRsp = NULL;
        }
    }
    return (pRsp);
}

