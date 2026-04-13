/*
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Copyright (c) 2023 Realtek Semiconductor Corp. All rights reserved.
 */

#include <linux/phy.h>
#include <crypto/aes.h>
#include <net/macsec.h>
#include "rtk_phylib_macsec.h"
#include "rtk_phylib_rtl826xb.h"
#include "rtk_phylib.h"
#include "rtk_phy.h"

static int rtk_macsec_del_txsa(struct macsec_context *ctx);
static int rtk_macsec_del_rxsa(struct macsec_context *ctx);

static int __rtk_macsec_clear_txsc(struct macsec_context *ctx, uint32 sc_id)
{
    int32 ret = 0;
    RTK_PHYLIB_ERR_CHK(rtk_phylib_macsec_sc_del(ctx->phydev, RTK_MACSEC_DIR_EGRESS, sc_id));
    return ret;
}

static int __rtk_macsec_clear_rxsc(struct macsec_context *ctx, uint32 sc_id)
{
    int32 ret = 0;
    RTK_PHYLIB_ERR_CHK(rtk_phylib_macsec_sc_del(ctx->phydev, RTK_MACSEC_DIR_INGRESS, sc_id));
    return ret;
}

static int rtk_macsec_dev_open(struct macsec_context *ctx)
{

    return rtk_phylib_macsec_enable_set(ctx->phydev, 1);
}

static int rtk_macsec_dev_stop(struct macsec_context *ctx)
{

    return rtk_phylib_macsec_enable_set(ctx->phydev, 0);
}

static int rtk_macsec_add_secy(struct macsec_context *ctx)
{
    int32 ret = 0;
    struct rtk_phy_priv *priv = ctx->phydev->priv;
    rtk_macsec_port_info_t *macsec_db = priv->macsec;


    if(MACSEC_SC_IS_USED(macsec_db, RTK_MACSEC_DIR_EGRESS, 0))
    {
        PR_ERR("[%s]MACSEC_SC_IS_USED\n",__FUNCTION__);
        return -EEXIST;
    }

    /* create TX SC */
    {
        uint32 sc_id = 0;
        rtk_macsec_sc_t sc;
        memset(&sc, 0x0, sizeof(rtk_macsec_sc_t));

        switch (ctx->secy->key_len)
        {
            case 16:
                sc.tx.cipher_suite = (ctx->secy->xpn) ? RTK_MACSEC_CIPHER_GCM_ASE_XPN_128 : RTK_MACSEC_CIPHER_GCM_ASE_128;
                break;
            case 32:
                sc.tx.cipher_suite = (ctx->secy->xpn) ? RTK_MACSEC_CIPHER_GCM_ASE_XPN_256 : RTK_MACSEC_CIPHER_GCM_ASE_256;
                break;
            default:
                PR_ERR("Not support key_len %d\n", ctx->secy->key_len);
                return -EINVAL;
        }

        RTK_PHYLIB_VAL_TO_BYTE_ARRAY(ctx->secy->sci, 8, sc.tx.sci, 0, 8);
        PR_DBG("[%s]secy->sci: 0x%llX\n", __FUNCTION__, ctx->secy->sci);
        sc.tx.flow_match =    RTK_MACSEC_MATCH_NON_CTRL;
        sc.tx.protect_frame = ctx->secy->protect_frames;
        sc.tx.include_sci =   ctx->secy->tx_sc.send_sci;
        sc.tx.use_es =        ctx->secy->tx_sc.end_station;
        sc.tx.use_scb =       ctx->secy->tx_sc.scb;
        sc.tx.conf_protect =  ctx->secy->tx_sc.encrypt;

        RTK_PHYLIB_ERR_CHK(rtk_phylib_macsec_sc_create(ctx->phydev, RTK_MACSEC_DIR_EGRESS, &sc, &sc_id, (ctx->secy->tx_sc.active) ? 1 : 0));
    }

    return ret;
}

static int rtk_macsec_del_secy(struct macsec_context *ctx)
{
    int32 ret = 0;
    struct rtk_phy_priv *priv = ctx->phydev->priv;
    rtk_macsec_port_info_t *macsec_db = priv->macsec;
    uint32 i = 0;


    for (i = 0; i < MACSEC_SC_MAX(macsec_db); i++)
    {
        if(MACSEC_SC_IS_USED(macsec_db, RTK_MACSEC_DIR_EGRESS, i))
        {
            PR_DBG("[%s] clear TX SC %u\n", __FUNCTION__, i);
            RTK_PHYLIB_ERR_CHK(__rtk_macsec_clear_txsc(ctx, i));
        }

        if(MACSEC_SC_IS_USED(macsec_db, RTK_MACSEC_DIR_INGRESS, i))
        {
            PR_DBG("[%s] clear RX SC %u\n", __FUNCTION__, i);
            RTK_PHYLIB_ERR_CHK(__rtk_macsec_clear_rxsc(ctx, i));
        }
    }
    memset(&(macsec_db->port_stats), 0x0, sizeof(rtk_macsec_sc_t));
    return ret;
}

static int rtk_macsec_upd_secy(struct macsec_context *ctx)
{
    int32 ret = 0;
    struct rtk_phy_priv *priv = ctx->phydev->priv;
    rtk_macsec_port_info_t *macsec_db = priv->macsec;

    rtk_macsec_cipher_t    cipher_suite;


    if(MACSEC_SC_IS_CLEAR(macsec_db, RTK_MACSEC_DIR_EGRESS, 0))
    {
        PR_ERR("[%s]MACSEC_SC_IS_CLEAR\n",__FUNCTION__);
        return -ENOENT;
    }

    /* create TX SC */
    {
        uint32 sc_id = 0;
        rtk_macsec_sc_t sc;
        rtk_macsec_sc_t cur_sc;
        RTK_PHYLIB_ERR_CHK(rtk_phylib_macsec_sc_get(ctx->phydev, RTK_MACSEC_DIR_EGRESS, 0, &sc));
        memcpy(&cur_sc, &sc, sizeof(rtk_macsec_sc_t));

        switch (ctx->secy->key_len)
        {
            case 16:
                cipher_suite = (ctx->secy->xpn) ? RTK_MACSEC_CIPHER_GCM_ASE_XPN_128 : RTK_MACSEC_CIPHER_GCM_ASE_128;
                break;
            case 32:
                cipher_suite = (ctx->secy->xpn) ? RTK_MACSEC_CIPHER_GCM_ASE_XPN_256 : RTK_MACSEC_CIPHER_GCM_ASE_256;
                break;
            default:
                PR_ERR("Not support key_len %d\n", ctx->secy->key_len);
                return -EINVAL;
        }

        RTK_PHYLIB_VAL_TO_BYTE_ARRAY(ctx->secy->sci, 8, sc.tx.sci, 0, 8);
      #ifdef MACSEC_DBG_PRINT
        PR_DBG("[%s]secy->sci: 0x%llX\n", __FUNCTION__, ctx->secy->sci);
        PR_DBG("cipher_suite  %u => %u\n", sc.tx.cipher_suite, cipher_suite);
        PR_DBG("flow_match    %u => %u\n", sc.tx.flow_match, RTK_MACSEC_MATCH_NON_CTRL);
        PR_DBG("protect_frame %u => %u\n", sc.tx.protect_frame, ctx->secy->protect_frames);
        PR_DBG("include_sci   %u => %u\n", sc.tx.include_sci, ctx->secy->tx_sc.send_sci);
        PR_DBG("use_es        %u => %u\n", sc.tx.use_es, ctx->secy->tx_sc.end_station);
        PR_DBG("use_scb       %u => %u\n", sc.tx.use_scb, ctx->secy->tx_sc.scb);
        PR_DBG("conf_protect  %u => %u\n", sc.tx.conf_protect, ctx->secy->tx_sc.encrypt);
      #endif
        sc.tx.cipher_suite = cipher_suite;
        sc.tx.flow_match =    RTK_MACSEC_MATCH_NON_CTRL;
        sc.tx.protect_frame = ctx->secy->protect_frames;
        sc.tx.include_sci =   ctx->secy->tx_sc.send_sci;
        sc.tx.use_es =        ctx->secy->tx_sc.end_station;
        sc.tx.use_scb =       ctx->secy->tx_sc.scb;
        sc.tx.conf_protect =  ctx->secy->tx_sc.encrypt;

        if(memcmp(&cur_sc, &sc, sizeof(rtk_macsec_sc_t)) != 0)
            RTK_PHYLIB_ERR_CHK(rtk_phylib_macsec_sc_update(ctx->phydev, RTK_MACSEC_DIR_EGRESS, &sc, &sc_id, (ctx->secy->tx_sc.active) ? 1 : 0));
    }

    return ret;
}

static int rtk_macsec_add_rxsc(struct macsec_context *ctx)
{
    int32 ret = 0;
    uint32 sc_id = 0;
    rtk_macsec_sc_t sc;
    memset(&sc, 0x0, sizeof(rtk_macsec_sc_t));


    ret = rtk_phylib_macsec_sci_to_scid(ctx->phydev, RTK_MACSEC_DIR_INGRESS, ctx->rx_sc->sci, &sc_id);
    if(ret != RTK_PHYLIB_ERR_ENTRY_NOTFOUND) //sc is existed
    {
        PR_DBG("[%s] ret:%d sc_id:%d is existed \n", __FUNCTION__, ret, sc_id);
        return -EEXIST;
    }
    PR_DBG("[%s]rx_sc->sci: 0x%llX\n", __FUNCTION__, ctx->rx_sc->sci);

    switch (ctx->secy->key_len)
    {
        case 16:
            sc.rx.cipher_suite = (ctx->secy->xpn) ? RTK_MACSEC_CIPHER_GCM_ASE_XPN_128 : RTK_MACSEC_CIPHER_GCM_ASE_128;
            break;
        case 32:
            sc.rx.cipher_suite = (ctx->secy->xpn) ? RTK_MACSEC_CIPHER_GCM_ASE_XPN_256 : RTK_MACSEC_CIPHER_GCM_ASE_256;
            break;
        default:
            PR_ERR("Not support key_len %d\n", ctx->secy->key_len);
            return -EINVAL;
    }

    PR_DBG("[%s]rx_sc->sci: 0x%llX\n", __FUNCTION__, ctx->rx_sc->sci);
    RTK_PHYLIB_VAL_TO_BYTE_ARRAY(ctx->rx_sc->sci, 8, sc.rx.sci, 0, 8);
    sc.rx.flow_match =    RTK_MACSEC_MATCH_SCI;

    switch (ctx->secy->validate_frames)
    {
        case MACSEC_VALIDATE_DISABLED:
            sc.rx.validate_frames = RTK_MACSEC_VALIDATE_DISABLE;
            break;
        case MACSEC_VALIDATE_CHECK:
            sc.rx.validate_frames = RTK_MACSEC_VALIDATE_CHECK;
            break;
        case MACSEC_VALIDATE_STRICT:
            sc.rx.validate_frames = RTK_MACSEC_VALIDATE_STRICT;
            break;
        default:
            PR_ERR("Not support validate_frames %d\n", ctx->secy->validate_frames);
            return -EINVAL;
    }

    sc.rx.replay_protect =        ctx->secy->replay_protect;
    sc.rx.replay_window =       ctx->secy->replay_window;

    RTK_PHYLIB_ERR_CHK(rtk_phylib_macsec_sc_create(ctx->phydev, RTK_MACSEC_DIR_INGRESS, &sc, &sc_id, (ctx->rx_sc->active) ? 1 : 0));
    return ret;
}

static int rtk_macsec_del_rxsc(struct macsec_context *ctx)
{
    int32 ret = 0;
    uint32 sc_id = 0;


    RTK_PHYLIB_ERR_CHK(rtk_phylib_macsec_sci_to_scid(ctx->phydev, RTK_MACSEC_DIR_INGRESS, ctx->rx_sc->sci, &sc_id));
    RTK_PHYLIB_ERR_CHK(__rtk_macsec_clear_rxsc(ctx, sc_id));

    return ret;
}

static int rtk_macsec_upd_rxsc(struct macsec_context *ctx)
{
    int32 ret = 0;
    uint32 sc_id = 0;
    rtk_macsec_sc_t sc;
    rtk_macsec_sc_t cur_sc;


    ret = rtk_phylib_macsec_sci_to_scid(ctx->phydev, RTK_MACSEC_DIR_INGRESS, ctx->rx_sc->sci, &sc_id);
    if(ret == RTK_PHYLIB_ERR_ENTRY_NOTFOUND) //sc is not existed
    {
        PR_DBG("[%s] ret:%d sc_id:%d is not existed \n", __FUNCTION__, ret, sc_id);
        return -ENOENT;
    }
    PR_DBG("[%s]rx_sc->sci: 0x%llX\n", __FUNCTION__, ctx->rx_sc->sci);

    RTK_PHYLIB_ERR_CHK(rtk_phylib_macsec_sc_get(ctx->phydev, RTK_MACSEC_DIR_INGRESS, sc_id, &sc));
    memcpy(&cur_sc, &sc, sizeof(rtk_macsec_sc_t));

    switch (ctx->secy->key_len)
    {
        case 16:
            sc.rx.cipher_suite = (ctx->secy->xpn) ? RTK_MACSEC_CIPHER_GCM_ASE_XPN_128 : RTK_MACSEC_CIPHER_GCM_ASE_128;
            break;
        case 32:
            sc.rx.cipher_suite = (ctx->secy->xpn) ? RTK_MACSEC_CIPHER_GCM_ASE_XPN_256 : RTK_MACSEC_CIPHER_GCM_ASE_256;
            break;
        default:
            PR_ERR("Not support key_len %d\n", ctx->secy->key_len);
            return -EINVAL;
    }

    PR_DBG("[%s]rx_sc->sci: 0x%llX\n", __FUNCTION__, ctx->rx_sc->sci);
    RTK_PHYLIB_VAL_TO_BYTE_ARRAY(ctx->rx_sc->sci, 8, sc.rx.sci, 0, 8);
    sc.rx.flow_match =    RTK_MACSEC_MATCH_SCI;

    switch (ctx->secy->validate_frames)
    {
        case MACSEC_VALIDATE_DISABLED:
            sc.rx.validate_frames = RTK_MACSEC_VALIDATE_DISABLE;
            break;
        case MACSEC_VALIDATE_CHECK:
            sc.rx.validate_frames = RTK_MACSEC_VALIDATE_CHECK;
            break;
        case MACSEC_VALIDATE_STRICT:
            sc.rx.validate_frames = RTK_MACSEC_VALIDATE_STRICT;
            break;
        default:
            PR_ERR("Not support validate_frames %d\n", ctx->secy->validate_frames);
            return -EINVAL;
    }

    sc.rx.replay_protect = ctx->secy->replay_protect;
    sc.rx.replay_window = ctx->secy->replay_window;

    if(memcmp(&cur_sc, &sc, sizeof(rtk_macsec_sc_t)) != 0)
        RTK_PHYLIB_ERR_CHK(rtk_phylib_macsec_sc_update(ctx->phydev, RTK_MACSEC_DIR_INGRESS, &sc, &sc_id, (ctx->rx_sc->active) ? 1 : 0));

    return ret;
}

static int __rtk_macsec_set_rxsa(struct macsec_context *ctx, bool update)
{
    int32 ret = 0;
    uint32 sc_id = 0;
    rtk_macsec_sa_t sa;
    memset(&sa, 0x0, sizeof(rtk_macsec_sa_t));

    RTK_PHYLIB_ERR_CHK(rtk_phylib_macsec_sci_to_scid(ctx->phydev, RTK_MACSEC_DIR_INGRESS, ctx->sa.rx_sa->sc->sci, &sc_id));

    if (update)
    {
        RTK_PHYLIB_ERR_CHK(rtk_phylib_macsec_sa_get(ctx->phydev, RTK_MACSEC_DIR_INGRESS, sc_id, ctx->sa.assoc_num, &sa));
    }
    else
    {
        sa.key_bytes = ctx->secy->key_len;
        memcpy(sa.key, ctx->sa.key, sa.key_bytes);
        if(ctx->secy->xpn)
        {
            memcpy(sa.salt, ctx->sa.rx_sa->key.salt.bytes, MACSEC_SALT_LEN);
            RTK_PHYLIB_VAL_TO_BYTE_ARRAY(ctx->sa.rx_sa->ssci, 4, sa.ssci, 0, 4);
        }
    }

    sa.pn = ctx->sa.tx_sa->next_pn_halves.lower;
    sa.pn_h = ctx->sa.tx_sa->next_pn_halves.upper;

    #ifdef MACSEC_DBG_PRINT
    {
        PR_DBG("[%s,%d] update:%u \n", __FUNCTION__, __LINE__, update);
        PR_DBG("  KEY 0-15 = 0x%02X%02X%02X%02X%02X%02X%02X%02X %02X%02X%02X%02X%02X%02X%02X%02X\n",
            sa.key[0], sa.key[1], sa.key[2], sa.key[3],
            sa.key[4], sa.key[5], sa.key[6], sa.key[7],
            sa.key[8], sa.key[9], sa.key[10],sa.key[11],
            sa.key[12],sa.key[13],sa.key[14],sa.key[15]);
        if (ctx->secy->key_len == 32)
        {
            PR_DBG("  KEY16-31 = 0x%02X%02X%02X%02X%02X%02X%02X%02X %02X%02X%02X%02X%02X%02X%02X%02X\n",
                sa.key[16],sa.key[17],sa.key[18],sa.key[19],
                sa.key[20],sa.key[21],sa.key[22],sa.key[23],
                sa.key[24],sa.key[25],sa.key[26],sa.key[27],
                sa.key[28],sa.key[29],sa.key[30],sa.key[31]);
        }
        PR_DBG("  ctx->sa.rx_sa->active: %u\n", ctx->sa.rx_sa->active);
        PR_DBG("  ctx->secy->xpn: %u\n", ctx->secy->xpn);
        PR_DBG("  sa.pn: 0x%X, sa.pn_h: 0x%X\n", sa.pn, sa.pn_h);
        if(ctx->secy->xpn)
        {
            PR_DBG("  ctx->sa.tx_sa->key.salt = 0x%02X%02X%02X%02X %02X%02X%02X%02X %02X%02X%02X%02X\n",
                sa.salt[0], sa.salt[1], sa.salt[2],  sa.salt[3],
                sa.salt[4], sa.salt[5], sa.salt[6],  sa.salt[7],
                sa.salt[8], sa.salt[9], sa.salt[10], sa.salt[11]);
        }
    }
    #endif

    RTK_PHYLIB_ERR_CHK(rtk_phylib_macsec_sa_create(ctx->phydev, RTK_MACSEC_DIR_INGRESS, sc_id, ctx->sa.assoc_num, &sa));

    if (ctx->sa.rx_sa->active)
    {
        RTK_PHYLIB_ERR_CHK(rtk_phylib_macsec_sa_activate(ctx->phydev, RTK_MACSEC_DIR_INGRESS, sc_id, ctx->sa.assoc_num));
    }
    else
    {
        RTK_PHYLIB_ERR_CHK(rtk_phylib_macsec_rxsa_disable(ctx->phydev, sc_id, ctx->sa.assoc_num));
    }

    return ret;
}

static int rtk_macsec_add_rxsa(struct macsec_context *ctx)
{
    int32 ret = 0;
    uint32 sc_id = 0, sa_id = 0;
    struct rtk_phy_priv *priv = ctx->phydev->priv;
    rtk_macsec_port_info_t *macsec_db = priv->macsec;

    RTK_PHYLIB_ERR_CHK(__rtk_macsec_set_rxsa(ctx, false));

    RTK_PHYLIB_ERR_CHK(rtk_phylib_macsec_sci_to_scid(ctx->phydev, RTK_MACSEC_DIR_INGRESS, ctx->sa.rx_sa->sc->sci, &sc_id));
    sa_id = PHY_MACSEC_HW_SA_ID(sc_id, ctx->sa.assoc_num);
    macsec_db->rxsa_stats[sa_id] = kzalloc(sizeof(rtk_macsec_rxsa_stats_t), GFP_KERNEL);
    if (macsec_db->rxsa_stats[sa_id] == NULL)
    {
        rtk_macsec_del_rxsa(ctx);
        return -ENOMEM;
    }

    return ret;
}

static int rtk_macsec_del_rxsa(struct macsec_context *ctx)
{
    int32 ret = 0;
    uint32 sc_id = 0, sa_id = 0;
    struct rtk_phy_priv *priv = ctx->phydev->priv;
    rtk_macsec_port_info_t *macsec_db = priv->macsec;

    RTK_PHYLIB_ERR_CHK(rtk_phylib_macsec_sci_to_scid(ctx->phydev, RTK_MACSEC_DIR_INGRESS, ctx->sa.rx_sa->sc->sci, &sc_id));
    RTK_PHYLIB_ERR_CHK(rtk_phylib_macsec_sa_del(ctx->phydev, RTK_MACSEC_DIR_INGRESS, sc_id, ctx->sa.assoc_num));

    sa_id = PHY_MACSEC_HW_SA_ID(sc_id, ctx->sa.assoc_num);
    if (macsec_db->rxsa_stats[sa_id] != NULL)
    {
        kfree(macsec_db->rxsa_stats[sa_id]);
        macsec_db->rxsa_stats[sa_id] = NULL;
    }

    return ret;
}

static int rtk_macsec_upd_rxsa(struct macsec_context *ctx)
{
    int32 ret = 0;

    RTK_PHYLIB_ERR_CHK(__rtk_macsec_set_rxsa(ctx, true));
    return ret;
}

static int __rtk_macsec_set_txsa(struct macsec_context *ctx, bool update)
{
    int32 ret = 0;
    uint32 sc_id = 0;
    rtk_macsec_sa_t sa;
    memset(&sa, 0x0, sizeof(rtk_macsec_sa_t));

    RTK_PHYLIB_ERR_CHK(rtk_phylib_macsec_sci_to_scid(ctx->phydev, RTK_MACSEC_DIR_EGRESS, ctx->secy->sci, &sc_id));

    if (update)
    {
        RTK_PHYLIB_ERR_CHK(rtk_phylib_macsec_sa_get(ctx->phydev, RTK_MACSEC_DIR_EGRESS, sc_id, ctx->sa.assoc_num, &sa));
    }
    else
    {
        sa.key_bytes = ctx->secy->key_len;
        memcpy(sa.key, ctx->sa.key, sa.key_bytes);
        if(ctx->secy->xpn)
        {
            memcpy(sa.salt, ctx->sa.tx_sa->key.salt.bytes, MACSEC_SALT_LEN);
            RTK_PHYLIB_VAL_TO_BYTE_ARRAY(ctx->sa.tx_sa->ssci, 4, sa.ssci, 0, 4);
        }
    }

    sa.pn = ctx->sa.tx_sa->next_pn_halves.lower;
    sa.pn_h = ctx->sa.tx_sa->next_pn_halves.upper;

    #ifdef MACSEC_DBG_PRINT
    {
        PR_DBG("[%s,%d] update:%u \n", __FUNCTION__, __LINE__, update);
        PR_DBG("  KEY 0-15 = 0x%02X%02X%02X%02X%02X%02X%02X%02X %02X%02X%02X%02X%02X%02X%02X%02X\n",
            sa.key[0], sa.key[1], sa.key[2], sa.key[3],
            sa.key[4], sa.key[5], sa.key[6], sa.key[7],
            sa.key[8], sa.key[9], sa.key[10],sa.key[11],
            sa.key[12],sa.key[13],sa.key[14],sa.key[15]);
        if (ctx->secy->key_len == 32)
        {
            PR_DBG("  KEY16-31 = 0x%02X%02X%02X%02X%02X%02X%02X%02X %02X%02X%02X%02X%02X%02X%02X%02X\n",
                sa.key[16],sa.key[17],sa.key[18],sa.key[19],
                sa.key[20],sa.key[21],sa.key[22],sa.key[23],
                sa.key[24],sa.key[25],sa.key[26],sa.key[27],
                sa.key[28],sa.key[29],sa.key[30],sa.key[31]);
        }
        PR_DBG("  ctx->sa.tx_sa->active: %u\n", ctx->sa.tx_sa->active);
        PR_DBG("  ctx->secy->xpn: %u\n", ctx->secy->xpn);
        PR_DBG("  sa.pn: 0x%X, sa.pn_h: 0x%X\n", sa.pn, sa.pn_h);
        if(ctx->secy->xpn)
        {
            PR_DBG("  ctx->sa.tx_sa->key.salt = 0x%02X%02X%02X%02X %02X%02X%02X%02X %02X%02X%02X%02X\n",
                sa.salt[0], sa.salt[1], sa.salt[2],  sa.salt[3],
                sa.salt[4], sa.salt[5], sa.salt[6],  sa.salt[7],
                sa.salt[8], sa.salt[9], sa.salt[10], sa.salt[11]);
        }
    }
    #endif

    RTK_PHYLIB_ERR_CHK(rtk_phylib_macsec_sa_create(ctx->phydev, RTK_MACSEC_DIR_EGRESS, sc_id, ctx->sa.assoc_num, &sa));

    if (ctx->sa.tx_sa->active)
    {
        RTK_PHYLIB_ERR_CHK(rtk_phylib_macsec_sa_activate(ctx->phydev, RTK_MACSEC_DIR_EGRESS, sc_id, ctx->sa.assoc_num));
    }
    else
    {
        RTK_PHYLIB_ERR_CHK(rtk_phylib_macsec_txsa_disable(ctx->phydev, sc_id));

    }
    return ret;
}

static int rtk_macsec_add_txsa(struct macsec_context *ctx)
{
    int32 ret = 0;
    uint32 sc_id = 0, sa_id = 0;
    struct rtk_phy_priv *priv = ctx->phydev->priv;
    rtk_macsec_port_info_t *macsec_db = priv->macsec;


    RTK_PHYLIB_ERR_CHK(__rtk_macsec_set_txsa(ctx, false));

    RTK_PHYLIB_ERR_CHK(rtk_phylib_macsec_sci_to_scid(ctx->phydev, RTK_MACSEC_DIR_EGRESS, ctx->secy->sci, &sc_id));
    sa_id = PHY_MACSEC_HW_SA_ID(sc_id, ctx->sa.assoc_num);
    macsec_db->txsa_stats[sa_id] = kzalloc(sizeof(rtk_macsec_txsa_stats_t), GFP_KERNEL);
    if (macsec_db->txsa_stats[sa_id] == NULL)
    {
        rtk_macsec_del_txsa(ctx);
        return -ENOMEM;
    }

    return ret;
}

static int rtk_macsec_del_txsa(struct macsec_context *ctx)
{
    int32 ret = 0;
    uint32 sc_id = 0, sa_id = 0;
    struct rtk_phy_priv *priv = ctx->phydev->priv;
    rtk_macsec_port_info_t *macsec_db = priv->macsec;


    RTK_PHYLIB_ERR_CHK(rtk_phylib_macsec_sci_to_scid(ctx->phydev, RTK_MACSEC_DIR_EGRESS, ctx->secy->sci, &sc_id));
    RTK_PHYLIB_ERR_CHK(rtk_phylib_macsec_sa_del(ctx->phydev, RTK_MACSEC_DIR_EGRESS, sc_id, ctx->sa.assoc_num));

    sa_id = PHY_MACSEC_HW_SA_ID(sc_id, ctx->sa.assoc_num);
    if (macsec_db->txsa_stats[sa_id] != NULL)
    {
        kfree(macsec_db->txsa_stats[sa_id]);
        macsec_db->txsa_stats[sa_id] = NULL;
    }

    return ret;
}

static int rtk_macsec_upd_txsa(struct macsec_context *ctx)
{
    int32 ret = 0;

    RTK_PHYLIB_ERR_CHK(__rtk_macsec_set_txsa(ctx, true));
    return ret;
}


static int rtk_macsec_get_dev_stats(struct macsec_context *ctx)
{
    int32 ret = 0;
    uint64 cnt = 0;
    uint32 sc_id = 0, an = 0;

    RTK_PHYLIB_ERR_CHK(rtk_phylib_macsec_stat_port_get(ctx->phydev, RTK_MACSEC_STAT_OutPktsUntagged, &cnt));
    ctx->stats.dev_stats->OutPktsUntagged = cnt;

    RTK_PHYLIB_ERR_CHK(rtk_phylib_macsec_stat_port_get(ctx->phydev, RTK_MACSEC_STAT_InPktsUntagged, &cnt));
    ctx->stats.dev_stats->InPktsUntagged = cnt;

    RTK_PHYLIB_ERR_CHK(rtk_phylib_macsec_stat_port_get(ctx->phydev, RTK_MACSEC_STAT_InPktsNoTag, &cnt));
    ctx->stats.dev_stats->InPktsNoTag = cnt;

    RTK_PHYLIB_ERR_CHK(rtk_phylib_macsec_stat_port_get(ctx->phydev, RTK_MACSEC_STAT_InPktsBadTag, &cnt));
    ctx->stats.dev_stats->InPktsBadTag = cnt;

    RTK_PHYLIB_ERR_CHK(rtk_phylib_macsec_stat_port_get(ctx->phydev, RTK_MACSEC_STAT_InPktsUnknownSCI, &cnt));
    ctx->stats.dev_stats->InPktsUnknownSCI = cnt;

    RTK_PHYLIB_ERR_CHK(rtk_phylib_macsec_stat_port_get(ctx->phydev, RTK_MACSEC_STAT_InPktsNoSCI, &cnt));
    ctx->stats.dev_stats->InPktsNoSCI = cnt;

    ctx->stats.dev_stats->InPktsOverrun = 0; /* not support */

    /* accumulate over each egress SA */
    RTK_PHYLIB_ERR_CHK(rtk_phylib_macsec_sci_to_scid(ctx->phydev, RTK_MACSEC_DIR_EGRESS, ctx->secy->sci, &sc_id));
    for (an = 0; an < MACSEC_NUM_AN; an++)
    {
        if (0 == rtk_phylib_macsec_stat_txsa_get(ctx->phydev, sc_id, an, RTK_MACSEC_TXSA_STAT_OutPktsTooLong, &cnt))
        {
            ctx->stats.dev_stats->OutPktsTooLong += cnt;
        }
    }

    return ret;
}

static int rtk_macsec_get_txsc_stats(struct macsec_context *ctx)
{
    int32 ret = 0;
    uint64 cnt = 0;
    uint32 sc_id = 0, an = 0;

    RTK_PHYLIB_ERR_CHK(rtk_phylib_macsec_sci_to_scid(ctx->phydev, RTK_MACSEC_DIR_EGRESS, ctx->secy->sci, &sc_id));

    /* accumulate over each egress SA */
    for (an = 0; an < MACSEC_NUM_AN; an++)
    {
        if (0 == rtk_phylib_macsec_stat_txsa_get(ctx->phydev, sc_id, an, RTK_MACSEC_TXSA_STAT_OutPktsProtectedEncrypted, &cnt))
        {
            if (ctx->secy->tx_sc.encrypt)
            {
                ctx->stats.tx_sc_stats->OutPktsEncrypted += cnt;
            }
            else
            {
                ctx->stats.tx_sc_stats->OutPktsProtected += cnt;
            }
        }

        if (0 == rtk_phylib_macsec_stat_txsa_get(ctx->phydev, sc_id, an, RTK_MACSEC_TXSA_STAT_OutOctetsProtectedEncrypted, &cnt))
        {
            if (ctx->secy->tx_sc.encrypt)
            {
                ctx->stats.tx_sc_stats->OutOctetsEncrypted += cnt;
            }
            else
            {
                ctx->stats.tx_sc_stats->OutOctetsProtected += cnt;
            }
        }

    }

    return ret;
}

static int rtk_macsec_get_rxsc_stats(struct macsec_context *ctx)
{
    int32 ret = 0;
    uint64 cnt = 0;
    uint32 sc_id = 0, an = 0;

    RTK_PHYLIB_ERR_CHK(rtk_phylib_macsec_sci_to_scid(ctx->phydev, RTK_MACSEC_DIR_INGRESS, ctx->rx_sc->sci, &sc_id));

    /* accumulate over each ingress SA */
    for (an = 0; an < MACSEC_NUM_AN; an++)
    {
        if (0 == rtk_phylib_macsec_stat_rxsa_get(ctx->phydev, sc_id, an, RTK_MACSEC_RXSA_STAT_InOctetsDecryptedValidated, &cnt))
        {
            ctx->stats.rx_sc_stats->InOctetsValidated += cnt;
            ctx->stats.rx_sc_stats->InOctetsDecrypted = ctx->stats.rx_sc_stats->InOctetsValidated;
        }
        if (0 == rtk_phylib_macsec_stat_rxsa_get(ctx->phydev, sc_id, an, RTK_MACSEC_RXSA_STAT_InPktsUnchecked, &cnt))
        {
            ctx->stats.rx_sc_stats->InPktsUnchecked += cnt;
        }
        if (0 == rtk_phylib_macsec_stat_rxsa_get(ctx->phydev, sc_id, an, RTK_MACSEC_RXSA_STAT_InPktsDelayed, &cnt))
        {
            ctx->stats.rx_sc_stats->InPktsDelayed += cnt;
        }
        if (0 == rtk_phylib_macsec_stat_rxsa_get(ctx->phydev, sc_id, an, RTK_MACSEC_RXSA_STAT_InPktsOK, &cnt))
        {
            ctx->stats.rx_sc_stats->InPktsOK += cnt;
        }
        if (0 == rtk_phylib_macsec_stat_rxsa_get(ctx->phydev, sc_id, an, RTK_MACSEC_RXSA_STAT_InPktsInvalid, &cnt))
        {
            ctx->stats.rx_sc_stats->InPktsInvalid += cnt;
        }
        if (0 == rtk_phylib_macsec_stat_rxsa_get(ctx->phydev, sc_id, an, RTK_MACSEC_RXSA_STAT_InPktsLate, &cnt))
        {
            ctx->stats.rx_sc_stats->InPktsLate += cnt;
        }
        if (0 == rtk_phylib_macsec_stat_rxsa_get(ctx->phydev, sc_id, an, RTK_MACSEC_RXSA_STAT_InPktsNotValid, &cnt))
        {
            ctx->stats.rx_sc_stats->InPktsNotValid += cnt;
        }
        if (0 == rtk_phylib_macsec_stat_rxsa_get(ctx->phydev, sc_id, an, RTK_MACSEC_RXSA_STAT_InPktsNotUsingSA, &cnt))
        {
            ctx->stats.rx_sc_stats->InPktsNotUsingSA += cnt;
        }
        if (0 == rtk_phylib_macsec_stat_rxsa_get(ctx->phydev, sc_id, an, RTK_MACSEC_RXSA_STAT_InPktsUnusedSA, &cnt))
        {
            ctx->stats.rx_sc_stats->InPktsUnusedSA += cnt;
        }
    }

    return ret;
}

static int rtk_macsec_get_txsa_stats(struct macsec_context *ctx)
{
    int32 ret = 0;
    uint64 cnt = 0;
    uint32 sc_id = 0;
    rtk_macsec_sa_t sa;

    RTK_PHYLIB_ERR_CHK(rtk_phylib_macsec_sci_to_scid(ctx->phydev, RTK_MACSEC_DIR_EGRESS, ctx->secy->sci, &sc_id));

    RTK_PHYLIB_ERR_CHK(rtk_phylib_macsec_stat_txsa_get(ctx->phydev, sc_id, ctx->sa.assoc_num, RTK_MACSEC_TXSA_STAT_OutPktsProtectedEncrypted, &cnt));
    if (ctx->secy->tx_sc.encrypt)
    {
        ctx->stats.tx_sa_stats->OutPktsEncrypted = (uint32)cnt;
    }
    else
    {
        ctx->stats.tx_sa_stats->OutPktsProtected = (uint32)cnt;
    }

    RTK_PHYLIB_ERR_CHK(rtk_phylib_macsec_sa_get(ctx->phydev, RTK_MACSEC_DIR_EGRESS, sc_id, ctx->sa.assoc_num, &sa));
    ctx->sa.tx_sa->next_pn_halves.lower = sa.pn;
    ctx->sa.tx_sa->next_pn_halves.upper = sa.pn_h;

    return ret;
}

static int rtk_macsec_get_rxsa_stats(struct macsec_context *ctx)
{
    int32 ret = 0;
    uint64 cnt = 0;
    uint32 sc_id = 0;
    rtk_macsec_sa_t sa;

    RTK_PHYLIB_ERR_CHK(rtk_phylib_macsec_sci_to_scid(ctx->phydev, RTK_MACSEC_DIR_INGRESS, ctx->rx_sc->sci, &sc_id));

    RTK_PHYLIB_ERR_CHK(rtk_phylib_macsec_stat_rxsa_get(ctx->phydev, sc_id, ctx->sa.assoc_num, RTK_MACSEC_RXSA_STAT_InPktsOK, &cnt));
    ctx->stats.rx_sa_stats->InPktsOK = (uint32)cnt;

    RTK_PHYLIB_ERR_CHK(rtk_phylib_macsec_stat_rxsa_get(ctx->phydev, sc_id, ctx->sa.assoc_num, RTK_MACSEC_RXSA_STAT_InPktsInvalid, &cnt));
    ctx->stats.rx_sa_stats->InPktsInvalid = (uint32)cnt;

    RTK_PHYLIB_ERR_CHK(rtk_phylib_macsec_stat_rxsa_get(ctx->phydev, sc_id, ctx->sa.assoc_num, RTK_MACSEC_RXSA_STAT_InPktsNotValid, &cnt));
    ctx->stats.rx_sa_stats->InPktsNotValid = (uint32)cnt;

    RTK_PHYLIB_ERR_CHK(rtk_phylib_macsec_stat_rxsa_get(ctx->phydev, sc_id, ctx->sa.assoc_num, RTK_MACSEC_RXSA_STAT_InPktsNotUsingSA, &cnt));
    ctx->stats.rx_sa_stats->InPktsNotUsingSA = (uint32)cnt;

    RTK_PHYLIB_ERR_CHK(rtk_phylib_macsec_stat_rxsa_get(ctx->phydev, sc_id, ctx->sa.assoc_num, RTK_MACSEC_RXSA_STAT_InPktsUnusedSA, &cnt));
    ctx->stats.rx_sa_stats->InPktsUnusedSA = (uint32)cnt;

    RTK_PHYLIB_ERR_CHK(rtk_phylib_macsec_sa_get(ctx->phydev, RTK_MACSEC_DIR_INGRESS, sc_id, ctx->sa.assoc_num, &sa));
    ctx->sa.rx_sa->next_pn_halves.lower = sa.pn;
    ctx->sa.rx_sa->next_pn_halves.upper = sa.pn_h;

    return ret;
}

static const struct macsec_ops rtk_macsec_ops = {
    /* Device wide */
    .mdo_dev_open = rtk_macsec_dev_open,
    .mdo_dev_stop = rtk_macsec_dev_stop,
    /* SecY */
    .mdo_add_secy = rtk_macsec_add_secy,
    .mdo_upd_secy = rtk_macsec_upd_secy,
    .mdo_del_secy = rtk_macsec_del_secy,
    /* Security channels */
    .mdo_add_rxsc = rtk_macsec_add_rxsc,
    .mdo_upd_rxsc = rtk_macsec_upd_rxsc,
    .mdo_del_rxsc = rtk_macsec_del_rxsc,
    /* Security associations */
    .mdo_add_rxsa = rtk_macsec_add_rxsa,
    .mdo_upd_rxsa = rtk_macsec_upd_rxsa,
    .mdo_del_rxsa = rtk_macsec_del_rxsa,
    .mdo_add_txsa = rtk_macsec_add_txsa,
    .mdo_upd_txsa = rtk_macsec_upd_txsa,
    .mdo_del_txsa = rtk_macsec_del_txsa,
    /* Statistics */
    .mdo_get_dev_stats = rtk_macsec_get_dev_stats,
    .mdo_get_tx_sc_stats = rtk_macsec_get_txsc_stats,
    .mdo_get_rx_sc_stats = rtk_macsec_get_rxsc_stats,
    .mdo_get_tx_sa_stats = rtk_macsec_get_txsa_stats,
    .mdo_get_rx_sa_stats = rtk_macsec_get_rxsa_stats,
};

int rtk_macsec_init(struct phy_device *phydev)
{
    int32 ret = 0;
    struct rtk_phy_priv *priv = phydev->priv;

    priv->macsec = kzalloc(sizeof(*(priv->macsec)), GFP_KERNEL);
    if (!priv->macsec)
        return -ENOMEM;
    memset(priv->macsec, 0, sizeof(*(priv->macsec)));

    switch (phydev->drv->phy_id)
    {
        case REALTEK_PHY_ID_RTL8261N:
        case REALTEK_PHY_ID_RTL8264B:
            phydev->macsec_ops = &rtk_macsec_ops;
            priv->macsec->max_sa_num = 64;
            priv->macsec->max_sc_num = 64/4;
            ret = rtk_phylib_826xb_macsec_init(phydev);
            if (ret != 0)
            {
                phydev_err(phydev, "[%s]rtk_phylib_826xb_macsec_init failed!! 0x%X\n", __FUNCTION__, ret);
                return ret;
            }
            break;
        default:
            PR_ERR("[%s]phy_id: 0x%X not support!\n", __FUNCTION__, phydev->drv->phy_id);
            kfree(priv->macsec);
            priv->macsec = NULL;
            return -EOPNOTSUPP;
    }

    RTK_PHYLIB_ERR_CHK(rtk_phylib_macsec_init(phydev));

    return 0;
}
