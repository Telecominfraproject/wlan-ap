/*
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Copyright (c) 2023 Realtek Semiconductor Corp. All rights reserved.
 */

#ifndef __RTK_PHYLIB_H
#define __RTK_PHYLIB_H

#if defined(RTK_PHYDRV_IN_LINUX)
  #include <linux/phy.h>
  #include "type.h"
  #include "rtk_phylib_def.h"
#else
  //#include SDK headers
#endif

#if defined(DEBUG)
  #define MACSEC_DBG_PRINT 1
#endif

#if defined(RTK_PHYDRV_IN_LINUX)
  #define PR_INFO(_fmt, _args...) pr_info(_fmt, ##_args)
  #define PR_DBG(_fmt, _args...)  pr_debug(_fmt, ##_args)
  #define PR_ERR(_fmt, _args...)  pr_err("ERROR: "_fmt, ##_args)

  #define RTK_PHYLIB_ERR_FAILED             (-EPERM)
  #define RTK_PHYLIB_ERR_INPUT              (-EINVAL)
  #define RTK_PHYLIB_ERR_EXCEEDS_CAPACITY   (-ENOSPC)
  #define RTK_PHYLIB_ERR_TIMEOUT            (-ETIME)
  #define RTK_PHYLIB_ERR_ENTRY_NOTFOUND     (-ENODATA)
  #define RTK_PHYLIB_ERR_OPER_DENIED        (-EACCES)
#else
  #define PR_INFO(_fmt, _args...) RT_LOG(LOG_INFO, (MOD_HAL|MOD_PHY), _fmt, ##_args)
  #define PR_DBG(_fmt, _args...)  RT_LOG(LOG_DEBUG, (MOD_HAL|MOD_PHY), _fmt, ##_args)
  #define PR_ERR(_fmt, _args...)  RT_LOG(LOG_MAJOR_ERR, (MOD_HAL|MOD_PHY), _fmt, ##_args)

  #define RTK_PHYLIB_ERR_FAILED              (RT_ERR_FAILED)
  #define RTK_PHYLIB_ERR_INPUT               (RT_ERR_INPUT)
  #define RTK_PHYLIB_ERR_EXCEEDS_CAPACITY    (RT_ERR_EXCEEDS_CAPACITY)
  #define RTK_PHYLIB_ERR_TIMEOUT             (RT_ERR_BUSYWAIT_TIMEOUT)
  #define RTK_PHYLIB_ERR_ENTRY_NOTFOUND      (RT_ERR_ENTRY_NOTFOUND)
  #define RTK_PHYLIB_ERR_OPER_DENIED         (RT_ERR_OPER_DENIED)
#endif

typedef enum rtk_phylib_phy_e
{
    RTK_PHYLIB_NONE,
    RTK_PHYLIB_RTL8261N,
    RTK_PHYLIB_RTL8264B,
    RTK_PHYLIB_RTL8251L,
    RTK_PHYLIB_RTL8254B,
    RTK_PHYLIB_END
} rtk_phylib_phy_t;

#if defined(RTK_PHYDRV_IN_LINUX)
    typedef struct phy_device rtk_phydev;
#else
    struct rtk_phy_dev_s
    {
        uint32 unit;
        rtk_port_t port;

        struct rtk_phy_priv *priv;
    };
    typedef struct rtk_phy_dev_s rtk_phydev;
#endif

#define RTK_PHYLIB_ERR_CHK(op)\
do {\
    if ((ret = (op)) != 0)\
        return ret;\
} while(0)

#define RTK_PHYLIB_VAL_TO_BYTE_ARRAY(_val, _valbytes, _array, _start, _bytes)\
do{\
    uint32 _i = 0;\
    for (_i = 0; _i < _bytes; _i++)\
        _array[(_bytes-1)-_i] = (_val >> (8* (_valbytes - _i - 1)));\
}while(0)

#define RTK_PHYLIB_BYTE_ARRAY_TO_VAL(_val, _array, _start, _bytes)\
do{\
    uint32 _i = 0;\
    _val = 0;\
    for (_i = 0; _i < _bytes; _i++)\
        _val = (_val << 8) | _array[(_bytes-1)-_i];\
}while(0)


/* RTCT */
#define RTK_PHYLIB_CABLE_STATUS_NORMAL              (0)
#define RTK_PHYLIB_CABLE_STATUS_UNKNOWN             (1u << 0)
#define RTK_PHYLIB_CABLE_STATUS_SHORT               (1u << 1)
#define RTK_PHYLIB_CABLE_STATUS_OPEN                (1u << 2)
#define RTK_PHYLIB_CABLE_STATUS_MISMATCH            (1u << 3)
#define RTK_PHYLIB_CABLE_STATUS_CROSS               (1u << 4)
#define RTK_PHYLIB_CABLE_STATUS_INTER_PAIR_SHORT    (1u << 5)

typedef struct rtk_rtct_channel_result_s
{
    uint32 cable_status;
    uint32 length_cm;
} rtk_rtct_channel_result_t;

/* MACsec */
typedef struct rtk_macsec_sa_info_s
{
    uint8   ssci[4];
} rtk_macsec_sa_info_t;

#define MACSEC_SA_IS_USED(macsec_port_info_ptr, dir, sa_id)     (macsec_port_info_ptr->sa_used[dir][sa_id])
#define MACSEC_SC_IS_USED(macsec_port_info_ptr, dir, sc_id)     (macsec_port_info_ptr->sc_used[dir][sc_id])

#define MACSEC_SA_IS_CLEAR(macsec_port_info_ptr, dir, sa_id)    (!MACSEC_SA_IS_USED(macsec_port_info_ptr, dir, sa_id))
#define MACSEC_SC_IS_CLEAR(macsec_port_info_ptr, dir, sc_id)    (!MACSEC_SC_IS_USED(macsec_port_info_ptr, dir, sc_id))

#define MACSEC_SA_SET_USED(macsec_port_info_ptr, dir, sa_id)    do { macsec_port_info_ptr->sa_used[dir][sa_id] = 1; }while(0)
#define MACSEC_SC_SET_USED(macsec_port_info_ptr, dir, sc_id)    do { macsec_port_info_ptr->sc_used[dir][sc_id] = 1; }while(0)

#define MACSEC_SA_UNSET_USED(macsec_port_info_ptr, dir, sa_id)  do { macsec_port_info_ptr->sa_used[dir][sa_id] = 0; }while(0)
#define MACSEC_SC_UNSET_USED(macsec_port_info_ptr, dir, sc_id)  do { macsec_port_info_ptr->sc_used[dir][sc_id] = 0; }while(0)

#define MACSEC_SA_MAX(macsec_port_info_ptr)                 macsec_port_info_ptr->max_sa_num
#define MACSEC_SC_MAX(macsec_port_info_ptr)                 macsec_port_info_ptr->max_sc_num
#define MACSEC_SC_CS(macsec_port_info_ptr, dir, sc_id)      macsec_port_info_ptr->cipher_suite[dir][sc_id]
#define MACSEC_SC_MATCH(macsec_port_info_ptr, dir, sc_id)   macsec_port_info_ptr->flow_match[dir][sc_id]
#define MACSEC_SA_SSCI(macsec_port_info_ptr, sa_id)         macsec_port_info_ptr->sa_info[sa_id].ssci


typedef struct rtk_macsec_port_stats_s
{
    uint64 InPktsUntagged;
    uint64 InPktsNoTag;
    uint64 InPktsBadTag;
    uint64 InPktsUnknownSCI;
    uint64 InPktsNoSCI;
    uint64 OutPktsUntagged;
}rtk_macsec_port_stats_t;

typedef struct rtk_macsec_txsa_stats_s
{
    uint64 OutPktsTooLong;
    uint64 OutOctetsProtectedEncrypted;
    uint64 OutPktsProtectedEncrypted;
}rtk_macsec_txsa_stats_t;

typedef struct rtk_macsec_rxsa_stats_s
{
    uint64 InPktsUnusedSA;
    uint64 InPktsNotUsingSA;
    uint64 InPktsUnchecked;
    uint64 InPktsDelayed;
    uint64 InPktsLate;
    uint64 InPktsOK;
    uint64 InPktsInvalid;
    uint64 InPktsNotValid;
    uint64 InOctetsDecryptedValidated;
}rtk_macsec_rxsa_stats_t;

typedef struct rtk_macsec_port_info_s
{
    int32 (*macsec_reg_get)(rtk_phydev *phydev, rtk_macsec_dir_t dir, uint32 reg, uint8 msb, uint8 lsb, uint32 *pData);
    int32 (*macsec_reg_set)(rtk_phydev *phydev, rtk_macsec_dir_t dir, uint32 reg, uint8 msb, uint8 lsb, uint32 data);

    uint16 sa_gen_seq;
    uint32 max_sa_num;
    uint32 max_sc_num;
    rtk_macsec_cipher_t cipher_suite[RTK_MACSEC_DIR_END][RTK_MAX_MACSEC_SC_PER_PORT];
    uint32 flow_match[RTK_MACSEC_DIR_END][RTK_MAX_MACSEC_SC_PER_PORT];
    uint8 sc_used[RTK_MACSEC_DIR_END][RTK_MAX_MACSEC_SC_PER_PORT];
    uint8 sa_used[RTK_MACSEC_DIR_END][RTK_MAX_MACSEC_SA_PER_PORT];
    rtk_macsec_sa_info_t sa_info[RTK_MAX_MACSEC_SA_PER_PORT];
    uint64 sci[RTK_MACSEC_DIR_END][RTK_MAX_MACSEC_SC_PER_PORT];

    rtk_macsec_port_stats_t port_stats;
    rtk_macsec_txsa_stats_t *txsa_stats[RTK_MAX_MACSEC_SA_PER_PORT];
    rtk_macsec_rxsa_stats_t *rxsa_stats[RTK_MAX_MACSEC_SA_PER_PORT];
} rtk_macsec_port_info_t;

struct rtk_phy_priv {
    rtk_phylib_phy_t phytype;
    uint8 isBasePort;
    rt_phy_patch_db_t *patch;
    rtk_macsec_port_info_t *macsec;
};

/* OSAL */
void rtk_phylib_mdelay(uint32 msec);
void rtk_phylib_udelay(uint32 usec);
#define rtk_phylib_strlen   strlen
#define rtk_phylib_strcmp   strcmp
#define rtk_phylib_strcpy   strcpy
#define rtk_phylib_strncpy  strncpy
#define rtk_phylib_strcat   strcat
#define rtk_phylib_strchr   strchr
#define rtk_phylib_memset   memset
#define rtk_phylib_memcpy   memcpy
#define rtk_phylib_memcmp   memcmp
#define rtk_phylib_strdup   strdup
#define rtk_phylib_strncmp  strncmp
#define rtk_phylib_strstr   strstr
#define rtk_phylib_strtok   strtok
#define rtk_phylib_strtok_r   strtok_r
#define rtk_phylib_toupper  toupper

int rtk_phylib_time_usecs_get(uint32 *pUsec);
#ifndef WAIT_COMPLETE_VAR
#define WAIT_COMPLETE_VAR() \
    uint32    _t, _now, _t_wait=0, _timeout;  \
    int32           _chkCnt=0;

#define WAIT_COMPLETE(_timeout_us)     \
    _timeout = _timeout_us;  \
    for(rtk_phylib_time_usecs_get(&_t),rtk_phylib_time_usecs_get(&_now),_t_wait=0,_chkCnt=0 ; \
        (_t_wait <= _timeout); \
        rtk_phylib_time_usecs_get(&_now), _chkCnt++, _t_wait += ((_now >= _t) ? (_now - _t) : (0xFFFFFFFF - _t + _now)),_t = _now \
       )

#define WAIT_COMPLETE_IS_TIMEOUT()   (_t_wait > _timeout)
#endif


/* Register Access APIs */
int32 rtk_phylib_mmd_write(rtk_phydev *phydev, uint32 mmd, uint32 reg, uint8 msb, uint8 lsb, uint32 data);
int32 rtk_phylib_mmd_read(rtk_phydev *phydev, uint32 mmd, uint32 reg, uint8 msb, uint8 lsb, uint32 *pData);

/* Function Driver */
int32 rtk_phylib_c45_power_normal(rtk_phydev *phydev);
int32 rtk_phylib_c45_power_low(rtk_phydev *phydev);
int32 rtk_phylib_c45_pcs_loopback(rtk_phydev *phydev, uint32 enable);

/* MACsec*/
int32 rtk_phylib_macsec_init(rtk_phydev *phydev);
int32 rtk_phylib_macsec_enable_get(rtk_phydev *phydev, uint32 *pEna);
int32 rtk_phylib_macsec_enable_set(rtk_phydev *phydev, uint32 ena);

int32 rtk_phylib_macsec_sc_create(rtk_phydev *phydev, rtk_macsec_dir_t dir, rtk_macsec_sc_t *pSc, uint32 *pSc_id, uint8 active);
int32 rtk_phylib_macsec_sc_update(rtk_phydev *phydev, rtk_macsec_dir_t dir, rtk_macsec_sc_t *pSc, uint32 *pSc_id, uint8 active);
int32 rtk_phylib_macsec_sc_del(rtk_phydev *phydev, rtk_macsec_dir_t dir, uint32 sc_id);
int32 rtk_phylib_macsec_sci_to_scid(rtk_phydev *phydev, rtk_macsec_dir_t dir, uint64 sci, uint32 *sc_id);
int32 rtk_phylib_macsec_sc_status_get(rtk_phydev *phydev, rtk_macsec_dir_t dir,uint32 sc_id, rtk_macsec_sc_status_t *pSc_status);
int32 rtk_phylib_macsec_sc_get(rtk_phydev *phydev, rtk_macsec_dir_t dir, uint32 sc_id, rtk_macsec_sc_t *pSc);

int32 rtk_phylib_macsec_sa_activate(rtk_phydev *phydev, rtk_macsec_dir_t dir, uint32 sc_id, rtk_macsec_an_t an);
int32 rtk_phylib_macsec_rxsa_disable(rtk_phydev *phydev, uint32 rxsc_id, rtk_macsec_an_t an);
int32 rtk_phylib_macsec_txsa_disable(rtk_phydev *phydev, uint32 txsc_id);
int32 rtk_phylib_macsec_sa_create(rtk_phydev *phydev, rtk_macsec_dir_t dir, uint32 sc_id, rtk_macsec_an_t an, rtk_macsec_sa_t *pSa);
int32 rtk_phylib_macsec_sa_get(rtk_phydev *phydev, rtk_macsec_dir_t dir, uint32 sc_id, rtk_macsec_an_t an, rtk_macsec_sa_t *pSa);
int32 rtk_phylib_macsec_sa_del(rtk_phydev *phydev, rtk_macsec_dir_t dir, uint32 sc_id, rtk_macsec_an_t an);

int32 rtk_phylib_macsec_stat_port_get(rtk_phydev *phydev, rtk_macsec_stat_t stat, uint64 *pCnt);
int32 rtk_phylib_macsec_stat_txsa_get(rtk_phydev *phydev, uint32 sc_id, rtk_macsec_an_t an, rtk_macsec_txsa_stat_t stat, uint64 *pCnt);
int32 rtk_phylib_macsec_stat_rxsa_get(rtk_phydev *phydev, uint32 sc_id, rtk_macsec_an_t an, rtk_macsec_rxsa_stat_t stat, uint64 *pCnt);


#endif /* __RTK_PHYLIB_H */
