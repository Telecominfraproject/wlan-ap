// SPDX-License-Identifier: GPL-2.0
#ifndef __RTL8373_LUT_H__
#define __RTL8373_LUT_H__

#define RTL8373_LUT_IPMCGRP_TABLE_MAX  (0x3F)

#define RTL8373_LUT_BUSY_CHECK_NO      (10)

#define RTL8373_LUT_TABLE_SIZE         (3)

typedef u32                  ipaddr_t;
typedef u32                  memaddr;
typedef u32  rtk_pri_t;      /* priority vlaue */
typedef u32  rtk_qid_t;      /* queue id type */
typedef u32  rtk_data_t;
typedef u32  rtk_dscp_t;     /* dscp vlaue */
typedef u32  rtk_fid_t;      /* filter id type */
typedef u32  rtk_vlan_t;     /* vlan id type */
typedef u32  rtk_mac_cnt_t;  /* MAC count type  */
typedef u32  rtk_meter_id_t; /* meter id type  */
typedef u32  rtk_rate_t;     /* rate type  */

#define RTL8373_LUTREADMETHOD_MAC            0
#define RTL8373_LUTREADMETHOD_ADDRESS        1
#define RTL8373_LUTREADMETHOD_NEXT_ADDRESS   2
#define RTL8373_LUTREADMETHOD_NEXT_L2UC      3
#define RTL8373_LUTREADMETHOD_NEXT_L2MC      4
#define RTL8373_LUTREADMETHOD_NEXT_L3MC      5
#define RTL8373_LUTREADMETHOD_NEXT_L2L3MC    6
#define RTL8373_LUTREADMETHOD_NEXT_L2UCSPA   7

#define RTK_TOTAL_NUM_OF_WORD_FOR_1BIT_PORT_LIST    1

#ifndef ETHER_ADDR_LEN
#define ETHER_ADDR_LEN      6
#endif
typedef struct ether_addr_s {
    u8 octet[ETHER_ADDR_LEN];
} ether_addr_t;

/* ethernet address type */
typedef struct  rtk_mac_s
{
    u8 octet[ETHER_ADDR_LEN];
} rtk_mac_t;

typedef struct rtk_portmask_s
{
    u32  bits[RTK_TOTAL_NUM_OF_WORD_FOR_1BIT_PORT_LIST];
} rtk_portmask_t;

enum L2_NEWSA_BEHAVE
{
    L2_NEWSA_FWD = 0,
    L2_NEWSA_DROP,
    L2_NEWSA_TRAP,
    L2_BEHAVE_SA_END
};


enum RTL8373_FLUSHMODE
{
    FLUSHMDOE_PORT = 0,
    FLUSHMDOE_VID,
    FLUSHMDOE_FID,
    FLUSHMDOE_END,
};

enum RTL8373_L2_TRAPPORT
{
    TRAP_NONE = 0,
    TRAP_8051,
    TRAP_EXTERNAL,
    TRAP_BOTH,
};

typedef enum rtk_l2_read_method_e{

    READMETHOD_MAC = 0,
    READMETHOD_ADDRESS,
    READMETHOD_NEXT_ADDRESS,
    READMETHOD_NEXT_L2UC, ///3
    READMETHOD_NEXT_L2MC,
    READMETHOD_NEXT_L3MC,
    READMETHOD_NEXT_L2L3MC,
    READMETHOD_NEXT_L2UCSPA, //7
    READMETHOD_END
}rtk_l2_read_method_t;

/* l2 limit learning count action */
typedef enum rtk_l2_limitLearnCntAction_e
{
    LIMIT_LEARN_CNT_ACTION_DROP = 0,
    LIMIT_LEARN_CNT_ACTION_FORWARD,
    LIMIT_LEARN_CNT_ACTION_TO_CPU,
    LIMIT_LEARN_CNT_ACTION_END
} rtk_l2_limitLearnCntAction_t;

typedef enum rtk_l2_ipmc_lookup_type_e
{
    LOOKUP_MAC = 0,
    LOOKUP_IP,
    LOOKUP_END
} rtk_l2_ipmc_lookup_type_t;



/* l2 address table - multicast data structure */
typedef struct rtk_l2_mcastAddr_s
{
    u32      vid_fid;
    rtk_mac_t       mac;
    rtk_portmask_t  portmask;
    u32      ivl;
    u32      igmp_asic;
    u32      igmp_index;
    u32      address;
}rtk_l2_mcastAddr_t;

/* l2 address table - ip multicast data structure */
typedef struct rtk_l2_ipMcastAddr_s
{
    ipaddr_t        dip;
    ipaddr_t        sip;
    rtk_portmask_t  portmask;
    u32      igmp_asic;
    u32      igmp_index;
    u32      address;
}rtk_l2_ipMcastAddr_t;


typedef struct rtk_l2_addr_table_s
{
    u32  index;
    ipaddr_t    sip;
    ipaddr_t    dip;
    rtk_mac_t   mac;
    u32  auth;
    rtk_portmask_t  portmask;
    u32  age;
    u32  ivl;
    u32  vid_fid;
    u32  is_ipmul;
    u32  is_static;
    u32  igmp_idx;
    u32  igmp_asic;
	u32	srcport;
}rtk_l2_addr_table_t;

typedef enum rtk_l2_clearStatus_e
{
    L2_CLEAR_STATE_FINISH = 0,
    L2_CLEAR_STATE_BUSY,
    L2_CLEAR_STATE_END
}rtk_l2_clearStatus_t;

typedef struct LUTTABLE{

    ipaddr_t sip;
    ipaddr_t dip;
    ether_addr_t mac;
    u16 ivl_svl;
    u16 cvid_fid;
    u16 nosalearn;
    u16 mbr;
    u16 spa;
    u16 age;
    u16 l3lookup;
    u16 auth;
    u16 igmp_idx;
    u16 igmp_asic;

    u16 lookup_hit;
    u16 lookup_busy;
    u16 address;
    u16 wait_time;

}rtl8373_luttb;


typedef struct  IPMCTBL{
    u32 ipaddr;
    u32 portmask;
    u32 index;
}rtl8373_ipmctbl;


int
rtl8373_port_fdb_add(struct dsa_switch *ds, int port,
		       const unsigned char *addr, u16 vid,
		       struct dsa_db db);

int
rtl8373_port_fdb_del(struct dsa_switch *ds, int port,
		       const unsigned char *addr, u16 vid,
		       struct dsa_db db);

int
rtl8373_port_fdb_dump(struct dsa_switch *ds, int port,
				 dsa_fdb_dump_cb_t *cb, void *data);

int rtl8373_port_mdb_del(struct dsa_switch *ds, int port,
		       const struct switchdev_obj_port_mdb *mdb,
		       struct dsa_db db);

int rtl8373_port_mdb_add(struct dsa_switch *ds, int port,
		       const struct switchdev_obj_port_mdb *mdb,
		       struct dsa_db db);   

rtk_api_ret_t dal_rtl8373_l2_limitLearningCnt_set(struct realtek_smi *smi, rtk_port_t port, rtk_mac_cnt_t mac_cnt);
rtk_api_ret_t dal_rtl8373_l2_limitLearningCnt_get(struct realtek_smi *smi, rtk_port_t port, rtk_mac_cnt_t *pMac_cnt);
rtk_api_ret_t dal_rtl8373_l2_limitSystemLearningCnt_set(struct realtek_smi *smi, rtk_mac_cnt_t mac_cnt);
rtk_api_ret_t dal_rtl8373_l2_limitSystemLearningCnt_get(struct realtek_smi *smi, rtk_mac_cnt_t *pMac_cnt);
rtk_api_ret_t dal_rtl8373_l2_limitLearningCntAction_set(struct realtek_smi *smi, rtk_l2_limitLearnCntAction_t action);
rtk_api_ret_t dal_rtl8373_l2_limitLearningCntAction_get(struct realtek_smi *smi, rtk_l2_limitLearnCntAction_t *pAction);
rtk_api_ret_t dal_rtl8373_l2_limitSystemLearningCntAction_set(struct realtek_smi *smi, rtk_l2_limitLearnCntAction_t action);
rtk_api_ret_t dal_rtl8373_l2_limitSystemLearningCntAction_get(struct realtek_smi *smi, rtk_l2_limitLearnCntAction_t *pAction);
rtk_api_ret_t dal_rtl8373_l2_limitSystemLearningCntPortMask_set(struct realtek_smi *smi, rtk_portmask_t *pPortmask);
rtk_api_ret_t dal_rtl8373_l2_limitSystemLearningCntPortMask_get(struct realtek_smi *smi, rtk_portmask_t *pPortmask);
rtk_api_ret_t dal_rtl8373_l2_learningCnt_get(struct realtek_smi *smi, rtk_port_t port, rtk_mac_cnt_t *pMac_cnt);
       
#endif