// SPDX-License-Identifier: GPL-2.0
#ifndef _DAL_RTL8373_PORT_H_
#define _DAL_RTL8373_PORT_H_

#include <linux/regmap.h>
#include "rtl8373_core.h"

#ifdef __cplusplus
extern "C" {
#endif
#ifndef RTL8373_PKTLENMAX
#define RTL8373_PKTLENMAX                   (0x3FFF)
#endif
typedef struct  rtk_port_status_s{
    u32 txpause;
    u32 rxpause;
    u32 link;
    u32 duplex;
    u32 speed;
    u32 media;
    u32 eee;
    u32 master;
    u32 master_slave;
}rtk_port_status_t;

ret_t dal_rtl8373_portFrcAbility_set(
    struct realtek_smi *smi,
    rtk_port_t port,
    rtk_port_ability_t *ability);

ret_t dal_rtl8373_portFrcAbility_get(
    struct realtek_smi *smi,
    rtk_port_t port,
    rtk_port_ability_t *ability);

ret_t dal_rtl8373_portStatus_get(
    struct realtek_smi *smi,
    rtk_port_t port,
    rtk_port_status_t *status);

ret_t dal_rtl8373_portMaxLen_set(
    struct realtek_smi *smi,
    rtk_port_t port,
    u32 type,
    u32 len);

ret_t dal_rtl8373_portMaxLen_get(
    struct realtek_smi *smi,
    rtk_port_t port,
    u32 type,
    u32 *pLen);

ret_t dal_rtl8373_portMaxLenIncTag_set(
    struct realtek_smi *smi,
    rtk_port_t port,
    u32 enable);

ret_t dal_rtl8373_portMaxLenIncTag_get(
    struct realtek_smi *smi,
    rtk_port_t port,
    u32 *pEnable);

ret_t dal_rtl8373_portLoopbackEn_set(
    struct realtek_smi *smi,
    rtk_port_t port,
    rtk_enable_t enable);

ret_t dal_rtl8373_portLoopbackEn_get(
    struct realtek_smi *smi,
    rtk_port_t port,
    rtk_enable_t *pEnable);

ret_t dal_rtl8373_portBackpressureEn_set(
    struct realtek_smi *smi,
    rtk_port_t port,
    rtk_enable_t enable);

ret_t dal_rtl8373_portBackpressureEn_get(
    struct realtek_smi *smi,
    rtk_port_t port,
    rtk_enable_t *pEnable);

ret_t dal_rtl8373_port_extphyid_set(
    struct realtek_smi *smi,
    u32 sdsid,
    u32 phyid);

ret_t dal_rtl8373_port_extphyid_get(
    struct realtek_smi *smi,
    u32 sdsid,
    u32 *phyid);

rtk_api_ret_t dal_rtl8373_fiber_fc_set(
    struct realtek_smi *smi,
    u32 sdsId,
    rtk_sds_mode_t sdsMode,
    rtk_enable_t fc_en);

rtk_api_ret_t dal_rtl8373_fiber_fc_get(
    struct realtek_smi *smi,
    u32 sdsId,
    rtk_sds_mode_t sdsMode,
    rtk_enable_t *fc_en);

rtk_api_ret_t dal_rtl8373_port_sdsNway_set(
    struct realtek_smi *smi,
    u32 sdsId,
    rtk_sds_mode_t sdsMode,
    rtk_enable_t enable);

rtk_api_ret_t dal_rtl8373_port_sdsNway_get(
    struct realtek_smi *smi,
    u32 sdsId,
    rtk_sds_mode_t sdsMode,
    rtk_enable_t *pState);

rtk_api_ret_t dal_rtl8373_sdsMode_set(
    struct realtek_smi *smi,
    u32 sdsid,
    rtk_sds_mode_t mode);

rtk_api_ret_t dal_rtl8373_sdsMode_get(
    struct realtek_smi *smi,
    u32 sdsid,
    rtk_sds_mode_t *pMode);

rtk_api_ret_t dal_rtl8373_port_txPktMaxLen_set(
    struct realtek_smi *smi,
    rtk_port_t port,
    u32 type,
    u32 pktLen);

rtk_api_ret_t dal_rtl8373_port_txPktMaxLen_get(
    struct realtek_smi *smi,
    rtk_port_t port,
    u32 type,
    u32 *pPktLen);

#ifdef __cplusplus
}
#endif

#endif /* _DAL_RTL8373_PORT_H_ */
