#ifndef _PKTLOG_H_
#define _PKTLOG_H_

#ifdef CPTCFG_ATH11K_PKTLOG
#define CUR_PKTLOG_VER          10010  /* Packet log version */
#define PKTLOG_MAGIC_NUM        7735225
#define PKTLOG_NEW_MAGIC_NUM	2453506

/* Masks for setting pktlog events filters */
#define ATH_PKTLOG_RX		0x000000001
#define ATH_PKTLOG_TX		0x000000002
#define ATH_PKTLOG_RCFIND	0x000000004
#define ATH_PKTLOG_RCUPDATE	0x000000008

#define ATH_DEBUGFS_PKTLOG_SIZE_DEFAULT (8 * 1024 * 1024)
#define ATH_PKTLOG_FILTER_DEFAULT (ATH_PKTLOG_TX | ATH_PKTLOG_RX | \
				   ATH_PKTLOG_RCFIND | ATH_PKTLOG_RCUPDATE)

enum {
	PKTLOG_FLG_FRM_TYPE_LOCAL_S = 0,
	PKTLOG_FLG_FRM_TYPE_REMOTE_S,
	PKTLOG_FLG_FRM_TYPE_CLONE_S,
	PKTLOG_FLG_FRM_TYPE_UNKNOWN_S
};

struct ath_pktlog_hdr_arg {
	u16 log_type;
	u8 *payload;
	u16 payload_size;
	u8 *pktlog_hdr;
};

struct ath_pktlog_bufhdr {
	u32 magic_num;  /* Used by post processing scripts */
	u32 version;    /* Set to CUR_PKTLOG_VER */
};

struct ath_pktlog_buf {
	struct ath_pktlog_bufhdr bufhdr;
	int rd_offset;
	int wr_offset;
	char log_data[0];
};

struct ath_pktlog {
	struct ath_pktlog_buf *buf;
	u32 filter;
	u32 buf_size;           /* Size of buffer in bytes */
	spinlock_t lock;
	u8 hdr_size;
	u8 hdr_size_field_offset;
};

#endif /* CONFIG_ATH11K_PKTLOG */

#endif /* _PKTLOG_H_ */
