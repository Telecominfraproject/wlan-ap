// SPDX-License-Identifier: GPL-2.0
#ifndef __RTL8373_VLAN_H__
#define __RTL8373_VLAN_H__

#ifndef RTL8373_C2SIDXMAX
#define RTL8373_C2SIDXMAX      (127)
#endif

#ifndef RTL8373_VIDMAX
#define RTL8373_VIDMAX         (0xFFF)
#endif

#ifndef RTL8373_FIDMAX
#define RTL8373_FIDMAX         (15)
#endif

#ifndef RTL8373_MSTIMAX
#define RTL8373_MSTIMAX        (15)
#endif
#ifndef RTL8373_PRIMAX
#define RTL8373_PRIMAX                       (7)
#endif

#ifndef RTL8373_DSCPMAX
#define RTL8373_DSCPMAX                    (63)
#endif
typedef struct rtl8373_vlan_4k
{
    u16  vid;
    u16  mbr;
    u16  untag;
    u16  fid_msti;
    u16  svlan_chk_ivl_svl;
    u16  ivl_svl;
}dal_rtl8373_user_vlan4kentry;

typedef enum rtk_vlan_egressTagMode_e
{
    VLAN_EGRESS_TAG_MODE_ORIGINAL = 0,
    VLAN_EGRESS_TAG_MODE_KEEP_FORMAT,
    VLAN_EGRESS_TAG_MODE_PRI,
    VLAN_EGRESS_TAG_MODE_REAL_KEEP,
    VLAN_EGRESS_TAG_MODE_END
} rtk_vlan_egressTagMode_t;

typedef u32 rtk_svlan_index_t;

typedef struct rtk_svlan_memberCfg_s{
    u32 svid;
    rtk_portmask_t memberport;
    rtk_portmask_t untagport;
    u32 fiden;
    u32 fid;
    u32 priority;
    u32 efiden;
    u32 efid;
    u32 chk_ivl_svl;	
    u32 ivl_svl;		
}rtk_svlan_memberCfg_t;

typedef enum rtk_svlan_pri_ref_e
{
    REF_INTERNAL_PRI = 0,
    REF_CTAG_PRI,
    REF_RSV,
    REF_PB_PRI,
    REF_PRI_END
} rtk_svlan_pri_ref_t;


typedef u32 rtk_svlan_tpid_t;

typedef enum rtk_svlan_untag_action_e
{
    UNTAG_DROP = 0,
    UNTAG_TRAP,
    UNTAG_ASSIGN,
    UNTAG_END
} rtk_svlan_untag_action_t;

typedef enum rtk_svlan_unassign_action_e
{
    UNASSIGN_PBSVID = 0,
    UNASSIGN_TRAP,
    UNASSIGN_END
} rtk_svlan_unassign_action_t;

typedef enum rtk_svlan_trapCpuMsk_e
{
    TRAP_TO_NONE = 0,   
    TRAP_TO_INT_CPU,
    TRAP_TO_EXT_CPU,
    TRAP_TO_BOTH_INT_EXT_CPU,
    TRAP_TO_END
} rtk_svlan_trapCpuMsk_t;

int rtl8373_init_vlan(struct realtek_smi *smi);
int rtl8373_port_vlan_filtering(struct dsa_switch *ds, int port,
				bool vlan_filtering,
				struct netlink_ext_ack *extack);
int rtl8373_port_vlan_add(struct dsa_switch *ds, int port,
		     const struct switchdev_obj_port_vlan *vlan,
		     struct netlink_ext_ack *extack);
int rtl8373_port_vlan_del(struct dsa_switch *ds, int port,
			  const struct switchdev_obj_port_vlan *vlan);

bool rtl8373_is_vlan_valid(struct realtek_smi *priv, unsigned int vlan);

int rtl8366u_set_vlan(struct realtek_smi *priv, int vid, u32 member,
		     u32 untag, u32 fid_msti, u32 svlan_chk_ivl_svl, u32 ivl_svl);
int rtl8366u_set_pvid(struct realtek_smi *priv, unsigned int port,
		     unsigned int vid);
int rtl8366u_enable_vlan4k(struct realtek_smi *priv, bool enable);
int rtl8366u_enable_vlan(struct realtek_smi *smi, bool enable);
int rtl8366u_reset_vlan(struct realtek_smi *smi);

/* Initialize SVLAN Configuration */
rtk_api_ret_t dal_rtl8373_svlanInit(struct realtek_smi *smi);

/* Add one service port in the specified device */
rtk_api_ret_t dal_rtl8373_svlanServicePort_add(struct realtek_smi *smi, rtk_port_t port);

/* Get service ports in the specified device */
rtk_api_ret_t dal_rtl8373_svlanServicePort_get(struct realtek_smi *smi, rtk_portmask_t *pSvlanPmsk);

/* Delete one service port in the specified device */
rtk_api_ret_t dal_rtl8373_svlanServicePort_del(struct realtek_smi *smi, rtk_port_t port);

/* Configure accepted S-VLAN ether type */
rtk_api_ret_t dal_rtl8373_svlanTpid_set(struct realtek_smi *smi, u32 svlanTpid);

/* Get accepted S-VLAN ether type setting */
rtk_api_ret_t dal_rtl8373_svlanTpid_get(struct realtek_smi *smi, u32 *pSvlanTpid);

/* Set S-VLAN upstream priority reference setting */
rtk_api_ret_t dal_rtl8373_svlanPriRef_set(struct realtek_smi *smi, rtk_svlan_pri_ref_t ref);

/* Get S-VLAN upstream priority reference setting */
rtk_api_ret_t dal_rtl8373_svlanPriRef_get(struct realtek_smi *smi, rtk_svlan_pri_ref_t *pRef);

rtk_api_ret_t dal_rtl8373_svlanMbrPortEntry_set(struct realtek_smi *smi,
						rtk_vlan_t svid,
						rtk_svlan_memberCfg_t *pSvlan_cfg);

rtk_api_ret_t dal_rtl8373_svlanMbrPortEntry_get(struct realtek_smi *smi,
						rtk_vlan_t svid,
						rtk_svlan_memberCfg_t *pSvlan_cfg);
/* Configure default egress SVLAN */
rtk_api_ret_t dal_rtl8373_svlanDfltSvlan_set(struct realtek_smi *smi, rtk_port_t port, rtk_vlan_t svid);

/* Get the configure default egress SVLAN */
rtk_api_ret_t dal_rtl8373_svlanDfltSvlan_get(struct realtek_smi *smi, rtk_port_t port, rtk_vlan_t *pSvid);

/* Configure SVLAN C2S table */
rtk_api_ret_t dal_rtl8373_svlanC2S_add(struct realtek_smi *smi, rtk_vlan_t vid, rtk_port_t srcPort, rtk_vlan_t svid);

/* Delete one C2S entry */
rtk_api_ret_t dal_rtl8373_svlanC2S_del(struct realtek_smi *smi, rtk_vlan_t vid, rtk_port_t srcPort);

/* Get configure SVLAN C2S table */
rtk_api_ret_t dal_rtl8373_svlanC2S_get(struct realtek_smi *smi, rtk_vlan_t vid, rtk_port_t srcPort, rtk_vlan_t *pSvid);

/* Configure Action of downstream Un-Stag packet */
rtk_api_ret_t dal_rtl8373_svlanUntagAction_set(struct realtek_smi *smi, rtk_svlan_untag_action_t action, rtk_vlan_t svid);

/* Get Action of downstream Un-Stag packet */
rtk_api_ret_t dal_rtl8373_svlanUntagAction_get(struct realtek_smi *smi, rtk_svlan_untag_action_t *pAction, rtk_vlan_t *pSvid);

/* Configure Action of upstream without svid assign action */
rtk_api_ret_t dal_rtl8373_svlanUnassignAction_set(struct realtek_smi *smi, rtk_svlan_unassign_action_t action);

/* Get action of upstream without svid assignment */
rtk_api_ret_t dal_rtl8373_svlanUnassignAction_get(struct realtek_smi *smi, rtk_svlan_unassign_action_t *pAction);

/* Set svlan trap priority */
rtk_api_ret_t dal_rtl8373_svlanTrapPri_set(struct realtek_smi *smi, rtk_pri_t priority);

/* Get svlan trap priority */
rtk_api_ret_t dal_rtl8373_svlanTrapPri_get(struct realtek_smi *smi, rtk_pri_t *pPriority);

rtk_api_ret_t dal_rtl8373_svlanTrapCpumsk_set(struct realtek_smi *smi,
					      u32 mask);

rtk_api_ret_t dal_rtl8373_svlanTrapCpumsk_get(struct realtek_smi *smi,
					      u32 *pMask);

rtk_api_ret_t dal_rtl8373_vlan_portFid_get(struct realtek_smi *smi, rtk_port_t port, rtk_enable_t *pEnable, rtk_fid_t *pFid);
rtk_api_ret_t dal_rtl8373_vlan_portFid_set(struct realtek_smi *smi, rtk_port_t port, rtk_enable_t enable, rtk_fid_t fid);
#endif