/*
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Copyright (c) 2023 Realtek Semiconductor Corp. All rights reserved.
 */

#if defined(RTK_PHYDRV_IN_LINUX)
  #include <crypto/aes.h>
  #include "rtk_phylib.h"
  #include "rtk_phylib_macsec.h"
  #include "rtk_phylib_rtl826xb.h"
#else
  //#include SDK headers
  #include <hal/phy/macsec/phy_macsec.h>
  #include <hal/common/halctrl.h>
  #include <osal/time.h>
  #include <osal/memory.h>
  #include <hal/phy/macsec/aes.h>
  #include <hal/phy/phy_probe.h>
  #include <hal/phy/macsec/aes.h>
#endif


#define MACSEC_REG_GET(phydev, dir, reg, pData) \
do\
{\
    struct rtk_phy_priv *_priv = phydev->priv;\
    RTK_PHYLIB_ERR_CHK(_priv->macsec->macsec_reg_get(phydev, dir, reg, 31, 0, pData));\
} while (0)

#define MACSEC_REG_SET(phydev, dir, reg, data) \
do\
{\
    struct rtk_phy_priv *_priv = phydev->priv;\
    RTK_PHYLIB_ERR_CHK(_priv->macsec->macsec_reg_set(phydev, dir, reg, 31, 0, data));\
} while (0)

#define MACSEC_REG_ARRAY_GET(phydev, dir, reg, pArray, cnt) \
do\
{\
    if((ret = __rtk_phylib_macsec_reg_array_get(phydev, dir, reg, pArray, cnt)) != 0)\
    {\
        return ret;\
    }\
} while(0)

#define MACSEC_REG_ARRAY_SET(phydev, dir, reg, pArray, cnt) \
do\
{\
    if((ret = __rtk_phylib_macsec_reg_array_set(phydev, dir, reg, pArray, cnt)) != 0)\
    {\
        return ret;\
    }\
} while(0)

static void __rtk_macsec_aes_encrypt(const uint8 * const In_p, uint8 * const Out_p,
        const uint8 * const Key_p, const unsigned int KeyByteCount)
{
    struct crypto_aes_ctx ctx;
    int ret = 0;
	ret = aes_expandkey(&ctx, Key_p, KeyByteCount);
	if (ret)
    {
        PR_ERR("aes_expandkey failed!");
		return;
    }
    aes_encrypt(&ctx, Out_p, In_p);
}

static int32 __rtk_phylib_macsec_reg_array_get(rtk_phydev *phydev, rtk_macsec_dir_t dir,
                         const uint32 reg, uint32* pArray, const uint32 cnt)
{
    int32  ret = 0;
    uint32 data = 0, i = 0;
    uint32 offset = reg;

    for (i = 0; i < cnt; i++)
    {
        MACSEC_REG_GET(phydev, dir, offset, &data);
        pArray[i] = data;
        offset += MACSEC_REG_OFFS;
    }
    return ret;
}

static int32 __rtk_phylib_macsec_reg_array_set(rtk_phydev *phydev, rtk_macsec_dir_t dir,
                         const uint32 reg, uint32* pArray, const uint32 cnt)
{
    int32  ret = 0;
    uint32 data = 0, i = 0;
    uint32 offset = reg;

    for (i = 0; i < cnt; i++)
    {
        data = pArray[i];
        MACSEC_REG_SET(phydev, dir, offset, data);
        offset += MACSEC_REG_OFFS;
    }
    return ret;
}

static int32 __rtk_phylib_macsec_hw_flow_action_write(
        rtk_phydev *phydev,
        rtk_macsec_dir_t dir, uint32 flow_index,
        const uint32 SAIndex,
        const uint8 FlowType,
        const uint8 DestPort,
        const uint8 fDropNonReserved,
        const uint8 fFlowCryptAuth,
        const uint8 DropAction,
        const uint8 fProtect,
        const uint8 fSAInUse,
        const uint8 fIncludeSCI,
        const uint8 ValidateFrames,
        const uint8 TagBypassSize,
        const uint8 fSaIndexUpdate,
        const uint8 ConfOffset,
        const uint8 fConfProtect)
{
    int32 ret = 0;
    uint32 data = 0;

    if(fDropNonReserved)
        data |= BIT_4;
    else
        data &= ~BIT_4;

    if(fFlowCryptAuth)
        data |= BIT_5;
    else
        data &= ~BIT_5;

    if(fProtect)
        data |= BIT_16;
    else
        data &= ~BIT_16;

    if(fSAInUse)
        data |= BIT_17;
    else
        data &= ~BIT_17;

    if(fIncludeSCI)
        data |= BIT_18;
    else
        data &= ~BIT_18;

    if(fSaIndexUpdate)
        data |= BIT_23;
    else
        data &= ~BIT_23;

    if(fConfProtect)
        data |= BIT_31;
    else
        data &= ~BIT_31;

    data |= (uint32)((((uint32)ConfOffset)     & MASK_7_BITS) << 24);
    data |= (uint32)((((uint32)TagBypassSize)  & MASK_2_BITS) << 21);
    data |= (uint32)((((uint32)ValidateFrames) & MASK_2_BITS) << 19);
    data |= (uint32)((((uint32)SAIndex)        & MASK_8_BITS) << 8);
    data |= (uint32)((((uint32)DropAction)     & MASK_2_BITS) << 6);
    data |= (uint32)((((uint32)DestPort)       & MASK_2_BITS) << 2);
    data |= (uint32)((((uint32)FlowType)       & MASK_2_BITS));

    MACSEC_REG_SET(phydev, dir, MACSEC_REG_SAM_FLOW_CTRL(flow_index), data);

    PR_DBG("FLOW[%u] = 0x%08X\n", flow_index, data);

    return ret;
}

static void __rtk_phylib_macsec_copy_key_to_raw(uint32 *pRaw, uint32 offset, uint8 *pKey, uint32 key_bytes)
{
    uint32_t *dst = pRaw + offset;
    const uint8_t *src = pKey;
    unsigned int i,j;
    uint32_t w;
    if (pRaw == NULL)
        return;
    for(i=0; i<(key_bytes+3)/4; i++)
    {
        w=0;
        for(j=0; j<4; j++)
            w=(w>>8)|(*src++ << 24);
        *dst++ = w;
    }
}

static void __rtk_phylib_macsec_copy_raw_to_key(uint32 *pRaw, uint32 offset ,uint8 *pKey, uint32 raw_words)
{
    uint32_t *src = pRaw + offset;
    uint8_t *dst = pKey;
    unsigned int i;
    if (pRaw == NULL)
        return;

    for (i = 0; i < raw_words; i++)
    {
        *dst++ = (uint8)((src[i] & 0xff));
        *dst++ = (uint8)((src[i] & 0xff00) >> 8) ;
        *dst++ = (uint8)((src[i] & 0xff0000) >> 16) ;
        *dst++ = (uint8)((src[i] & 0xff000000) >> 24) ;
    }
}

static void __rtk_phylib_macsec_hw_sa_offset_parse(phy_macsec_sa_params_t *pSa, phy_macsec_sa_offset_t *pOffs)
{
    unsigned int long_key;
    rtk_phylib_memset(pOffs, 0, sizeof(phy_macsec_sa_offset_t));

    pOffs->key_offs = 2;
    if (pSa->key_bytes == 16)
    {
        long_key = 0;
    }
    else
    {
        long_key = 4;
    }
    pOffs->hkey_offs = long_key + 6;
    pOffs->seq_offs = long_key + 10;
    if (pSa->direction == RTK_MACSEC_DIR_EGRESS)
    {
        if ((pSa->flags & RTK_PHY_MACSEC_SA_FLAG_XPN) != 0)
        {
            pOffs->ctx_salt_offs = long_key + 13;
            pOffs->iv_offs = long_key + 16;
            if (long_key)
                pOffs->upd_ctrl_offs = 16;
            else
                pOffs->upd_ctrl_offs = 19;
        }
        else
        {
            pOffs->iv_offs = long_key + 11;
            pOffs->upd_ctrl_offs = long_key + 15;
        }
    }
    else
    {
        if ((pSa->flags & RTK_PHY_MACSEC_SA_FLAG_XPN) != 0)
        {
            pOffs->mask_offs = long_key + 12;
            pOffs->ctx_salt_offs = long_key + 13;
            pOffs->upd_ctrl_offs = 0;
        }
        else
        {
            pOffs->mask_offs = long_key + 11;
            pOffs->iv_offs = long_key + 12;
            pOffs->upd_ctrl_offs = 0;
        }
    }
}

static uint32 __rtk_phylib_macsec_hw_context_id_gen(rtk_phydev *phydev, rtk_macsec_dir_t dir, uint32 sa_index)
{
    struct rtk_phy_priv *priv = phydev->priv;
    rtk_macsec_port_info_t *macsec_db = priv->macsec;

    uint32 context_id = 0;
    context_id = 0x10000 * (macsec_db->sa_gen_seq & (0xFFFF)) + 0x1000 * (dir)+ sa_index;
    macsec_db->sa_gen_seq++;
    return context_id;
}

static int32 __rtk_phylib_macsec_hw_sa_parse(rtk_phydev *phydev, uint32 sa_index, uint32 *pSa_raw, phy_macsec_sa_params_t *pSa)
{
    struct rtk_phy_priv *priv = phydev->priv;
    rtk_macsec_port_info_t *macsec_db = priv->macsec;

    int32 ret = 0;
    uint32 next_offs = 0, words = 0;
    uint8 tmp8 = 0;
    if (pSa == NULL || pSa_raw == NULL)
        return RTK_PHYLIB_ERR_INPUT;

    rtk_phylib_memset(pSa, 0, sizeof(phy_macsec_sa_params_t));

    if (pSa_raw[0] & BIT_29)
    {
        pSa->flags |= RTK_PHY_MACSEC_SA_FLAG_XPN;
    }

    if ((pSa_raw[0] & MASK_4_BITS) == 0b0110)
    {
        pSa->direction = RTK_MACSEC_DIR_EGRESS;

        pSa->an = (uint8)((pSa_raw[0] >> 26) & 0x3);
    }
    else
    {
        pSa->direction = RTK_MACSEC_DIR_INGRESS;
    }

    if (((pSa_raw[0] >> 17) & MASK_3_BITS) == 0b101)
    {
        pSa->key_bytes = 16;
    }
    else
    {
        pSa->key_bytes = 32;
    }
    next_offs = 2;

    pSa->context_id = pSa_raw[1];

    /* key */
    words = (pSa->key_bytes * 8) / 32;
    __rtk_phylib_macsec_copy_raw_to_key(pSa_raw, next_offs, pSa->key, words);
    next_offs += words + 4;

    /* seq */
    pSa->seq = pSa_raw[next_offs];
    words = 1;

    if (pSa->flags & RTK_PHY_MACSEC_SA_FLAG_XPN)
    {
        pSa->seq_h = pSa_raw[next_offs+1];
        words = 2;
    }
    next_offs += words;

    /* replay_window(ingress) */
    if (pSa->direction == RTK_MACSEC_DIR_EGRESS)
    {
        words = (pSa->flags & RTK_PHY_MACSEC_SA_FLAG_XPN) ? 1 : 0;
    }
    else
    {
        pSa->replay_window = pSa_raw[next_offs];
        words = 1;
    }
    next_offs += words;

    /* CtxSalt */
    if (pSa->flags & RTK_PHY_MACSEC_SA_FLAG_XPN)
    {
        words = 3;

        pSa->ssci[0] = macsec_db->sa_info[sa_index].ssci[0];
        pSa->ssci[1] = macsec_db->sa_info[sa_index].ssci[1];
        pSa->ssci[2] = macsec_db->sa_info[sa_index].ssci[2];
        pSa->ssci[3] = macsec_db->sa_info[sa_index].ssci[3];

        tmp8 = (uint8)(pSa_raw[next_offs] & 0xFF);
        pSa->salt[0] = (tmp8 ^ (pSa->ssci[0]));
        tmp8 = (uint8)((pSa_raw[next_offs] & 0xFF00) >> 8 );
        pSa->salt[1] = (tmp8 ^ (pSa->ssci[1]));
        tmp8 = (uint8)((pSa_raw[next_offs] & 0xFF0000) >> 16 );
        pSa->salt[2] = (tmp8 ^ (pSa->ssci[2]));
        tmp8 = (uint8)((pSa_raw[next_offs] & 0xFF000000) >> 24 );
        pSa->salt[3] = (tmp8 ^ (pSa->ssci[3]));

        __rtk_phylib_macsec_copy_raw_to_key(pSa_raw, next_offs + 1, &pSa->salt[4], 2);

    }
    else
    {
        words = 0;
    }
    next_offs += words;

    /* IV(SCI) */
    if (pSa->flags & RTK_PHY_MACSEC_SA_FLAG_XPN)
    {
        if (pSa->direction == RTK_MACSEC_DIR_EGRESS)
        {
            words = 3;
        }
        else
        {
            words = 0;
        }
    }
    else
    {
        words = 4;
    }

    if (words != 0)
    {
        __rtk_phylib_macsec_copy_raw_to_key(pSa_raw, next_offs, pSa->sci, 2);
    }
    next_offs += words;

    /* Update Control */
    if (pSa->direction == RTK_MACSEC_DIR_EGRESS)
    {

        pSa->flow_index = (uint32)((pSa_raw[next_offs] >> 16) & MASK_15_BITS);
        pSa->next_sa_index = (uint32)(pSa_raw[next_offs] & MASK_14_BITS);
        pSa->update_en = (pSa_raw[next_offs] & BIT_31) ? 1 : 0;
        pSa->next_sa_valid = (pSa_raw[next_offs] & BIT_15) ? 1 : 0;
        pSa->sa_expired_irq = (pSa_raw[next_offs] & BIT_14) ? 1 : 0;
    }

    return ret;
}

static int32 __rtk_phylib_macsec_hw_sa_build(rtk_phydev *phydev, uint32 sa_index, phy_macsec_sa_params_t *pSa, uint32 *pSa_raw)
{
    struct rtk_phy_priv *priv = phydev->priv;
    rtk_macsec_port_info_t *macsec_db = priv->macsec;

    int32 ret = 0;
    phy_macsec_sa_offset_t offs;

    uint8 hkey[16] = { 0 };
    uint32 tmp = 0;
    uint32_t seq = 0; // sequence number.
    uint32_t seq_h = 0; // High part of sequence number (64-bit sequence numbers)
    uint32 gen_id = 0;

    if (pSa == NULL || pSa_raw == NULL)
        return RTK_PHYLIB_ERR_INPUT;
    if (pSa->an > 3)
        return RTK_PHYLIB_ERR_INPUT;

    if ((pSa->direction != RTK_MACSEC_DIR_INGRESS) &&
        (pSa->direction != RTK_MACSEC_DIR_EGRESS))
    {
        PR_ERR("unknown direction:%d", pSa->direction);
        return RTK_PHYLIB_ERR_INPUT;
    }

    if (pSa->context_id == 0)
    {
        gen_id = __rtk_phylib_macsec_hw_context_id_gen(phydev, pSa->direction, sa_index);
    }
    else
    {
        gen_id = pSa->context_id;
    }

    // Compute offsets for various fields.
    __rtk_phylib_macsec_hw_sa_offset_parse(pSa, &offs);

    // Fill the entire SA record with zeros.
    rtk_phylib_memset(pSa_raw, 0, PHY_MACSEC_MAX_SA_SIZE * sizeof(uint32_t));

    if (pSa->direction == RTK_MACSEC_DIR_EGRESS)
    {
        if((pSa->flags & RTK_PHY_MACSEC_SA_FLAG_XPN) != 0)
        {
            pSa_raw[0] = MACSEC_SAB_CW0_MACSEC_EG64;
        }
        else
        {
            pSa_raw[0] = MACSEC_SAB_CW0_MACSEC_EG32;
        }
        seq = pSa->seq;
        seq_h = pSa->seq_h;
        pSa_raw[0] |= (pSa->an & 0x3) << 26;
    }
    else //RTK_MACSEC_DIR_INGRESS
    {
        if((pSa->flags & RTK_PHY_MACSEC_SA_FLAG_XPN) != 0)
        {
            pSa_raw[0] = MACSEC_SAB_CW0_MACSEC_IG64;
        }
        else
        {
            pSa_raw[0] = MACSEC_SAB_CW0_MACSEC_IG32;
        }
        seq = (pSa->seq == 0 && pSa->seq_h == 0) ? 1 : pSa->seq;
        seq_h = pSa->seq_h;
    }

    switch (pSa->key_bytes)
    {
        case 16:
            pSa_raw[0] |= MACSEC_SAB_CW0_AES128;
            break;
        case 32:
            pSa_raw[0] |= MACSEC_SAB_CW0_AES256;
            break;
        default:
            PR_ERR("unsupported AES key size:%d", pSa->key_bytes);
            return RTK_PHYLIB_ERR_INPUT;
    }

    // Fill in ID
    pSa_raw[1] = gen_id;

    // Fill Key and HKey
    __rtk_phylib_macsec_copy_key_to_raw(pSa_raw, offs.key_offs, pSa->key, pSa->key_bytes);
    /* generate hkey from key, encrypt a single all-zero block */
    __rtk_macsec_aes_encrypt((uint8_t *)(pSa_raw + offs.hkey_offs),  hkey, pSa->key, pSa->key_bytes);
    __rtk_phylib_macsec_copy_key_to_raw(pSa_raw, offs.hkey_offs, hkey, 16);

    // Fill in sequence number/seqmask.
    pSa_raw[offs.seq_offs] = seq;
    if ((pSa->flags & RTK_PHY_MACSEC_SA_FLAG_XPN) != 0)
        pSa_raw[offs.seq_offs + 1] = seq_h;

    if (pSa->direction == RTK_MACSEC_DIR_INGRESS)
        pSa_raw[offs.mask_offs] = pSa->replay_window;

    // Fill in CtxSalt field.
    if (offs.ctx_salt_offs > 0)
    {
        //__rtk_phylib_macsec_copy_key_to_raw(&phy_macsec_info[unit]->ssci[port], 0, pSa->ssci, 8);
        macsec_db->sa_info[sa_index].ssci[0] = pSa->ssci[0];
        macsec_db->sa_info[sa_index].ssci[1] = pSa->ssci[1];
        macsec_db->sa_info[sa_index].ssci[2] = pSa->ssci[2];
        macsec_db->sa_info[sa_index].ssci[3] = pSa->ssci[3];

        //[0] = most significant 32-bits Salt XOR-ed with SSCI
        tmp =  (pSa->salt[0] ^ pSa->ssci[0]) |
              ((pSa->salt[1] ^ pSa->ssci[1]) << 8)  |
              ((pSa->salt[2] ^ pSa->ssci[2]) << 16) |
              ((pSa->salt[3] ^ pSa->ssci[3]) << 24);
        pSa_raw[offs.ctx_salt_offs] = tmp;
        //[1:2] = lower 64-bits Salt
        __rtk_phylib_macsec_copy_key_to_raw(pSa_raw, offs.ctx_salt_offs + 1, pSa->salt + 4, 8);
    }

    // Fill in IV(SCI) fields.
    if (offs.iv_offs > 0)
    {
        __rtk_phylib_macsec_copy_key_to_raw(pSa_raw, offs.iv_offs, pSa->sci, 8);
    }

    // Fill in update control fields.
    if(offs.upd_ctrl_offs > 0)
    {
        tmp = (pSa->next_sa_index & MASK_14_BITS) |
            ((pSa->flow_index & MASK_15_BITS) << 16);

        if (pSa->update_en)
            tmp |= BIT_31;
        if (pSa->next_sa_valid)
            tmp |= BIT_15;
        if (pSa->sa_expired_irq)
            tmp |= BIT_14;

        pSa_raw[offs.upd_ctrl_offs] = tmp;
    }

    return ret;
}

int32 __rtk_phylib_macsec_hw_sa_del(rtk_phydev *phydev, rtk_macsec_dir_t dir, uint32 sa_index)
{
    int32 ret = 0;
    uint32 sa_raw[PHY_MACSEC_MAX_SA_SIZE] = {0};

    MACSEC_REG_ARRAY_SET(phydev, dir, MACSEC_REG_XFORM_REC_OFFS(sa_index, dir, 0), sa_raw, MACSEC_XFORM_REC_SIZE(dir));

    return ret;
}

static int32 __rtk_phylib_macsec_hw_sa_set(rtk_phydev *phydev, rtk_macsec_dir_t dir, uint32 sa_index, phy_macsec_sa_params_t *pSa)
{
    int32 ret = 0;
    uint32 sa_raw[PHY_MACSEC_MAX_SA_SIZE] = {0};

    if (pSa == NULL)
        return RTK_PHYLIB_ERR_INPUT;

    if (pSa->direction == RTK_MACSEC_DIR_EGRESS && dir == RTK_MACSEC_DIR_INGRESS)
    {
        PR_ERR("direction mismatch!(set egress entry on ingress)");
        return RTK_PHYLIB_ERR_INPUT;
    }
    if (pSa->direction == RTK_MACSEC_DIR_INGRESS && dir == RTK_MACSEC_DIR_EGRESS)
    {
        PR_ERR("direction mismatch!(set ingress entry on egress)");
        return RTK_PHYLIB_ERR_INPUT;
    }

    if ((ret = __rtk_phylib_macsec_hw_sa_build(phydev, sa_index, pSa, sa_raw)) != 0)
        return ret;

    MACSEC_REG_ARRAY_SET(phydev, dir, MACSEC_REG_XFORM_REC_OFFS(sa_index, dir, 0), sa_raw, MACSEC_XFORM_REC_SIZE(dir));

    #ifdef MACSEC_DBG_PRINT
    {
        uint32 __i = 0;
        PR_DBG("flow_index: %u\n", pSa->flow_index);
        PR_DBG("next_sa_index: %u\n", pSa->next_sa_index);
        PR_DBG("update_en: %u, next_sa_valid: %u, sa_expired_irq:%u\n", pSa->update_en, pSa->next_sa_valid,  pSa->sa_expired_irq);

        PR_DBG("SA offset = 0x%04X\n", MACSEC_REG_XFORM_REC_OFFS(sa_index, dir, 0));
        for (__i = 0; __i < MACSEC_XFORM_REC_SIZE(dir); __i++)
        {
            PR_DBG("SA[%02u] = 0x%08X\n", __i, sa_raw[__i]);
        }
    }
    #endif

    return ret;
}

static int32 __rtk_phylib_macsec_hw_sa_get(rtk_phydev *phydev, rtk_macsec_dir_t dir, uint32 sa_index, phy_macsec_sa_params_t *pSa)
{
    int32 ret = 0;
    uint32 sa_raw[PHY_MACSEC_MAX_SA_SIZE] = {0};

    if (pSa == NULL)
        return RTK_PHYLIB_ERR_INPUT;

    MACSEC_REG_ARRAY_GET(phydev, dir, MACSEC_REG_XFORM_REC_OFFS(sa_index, dir, 0), sa_raw, MACSEC_XFORM_REC_SIZE(dir));

    __rtk_phylib_macsec_hw_sa_parse(phydev, sa_index, sa_raw, pSa);

    return ret;
}

static int32 __rtk_phylib_macsec_hw_flow_action_rule_set(rtk_phydev *phydev, rtk_macsec_dir_t dir, uint32 flow_index, phy_macsec_flow_action_t *pAct)
{
    struct rtk_phy_priv *priv = phydev->priv;
    rtk_macsec_port_info_t *macsec_db = priv->macsec;

    int32  ret = 0;
    uint32 sa_index = pAct->sa_index;
    uint8  flow_type = 0;
    uint8  dest_port = 0;
    uint8  drop_non_reserved = 0; //do not drop
    uint8  flow_crypt_auth = 0;
    uint8  drop_action = 0; // CRC
    uint8  protect = 0;
    uint8  sa_in_use = 0;
    uint8  include_sci = 0;
    uint8  validate_frames = 0;
    uint8  tag_bypass_size = 0;
    uint8  sa_index_update = 0;
    uint8  conf_offset = 0;
    uint8  conf_protect = 0;

    if (flow_index >= MACSEC_SA_MAX(macsec_db))
    {
        PR_ERR("flow_index out of range!");
        return RTK_PHYLIB_ERR_INPUT;
    }
    if (pAct->flow_type == RTK_MACSEC_FLOW_EGRESS && dir == RTK_MACSEC_DIR_INGRESS)
    {
        PR_ERR("flow_type mismatch!(set egress entry on ingress)");
        return RTK_PHYLIB_ERR_INPUT;
    }
    if (pAct->flow_type == RTK_MACSEC_FLOW_INGRESS && dir == RTK_MACSEC_DIR_EGRESS)
    {
        PR_ERR("flow_type mismatch!(set ingress entry on egress)");
        return RTK_PHYLIB_ERR_INPUT;
    }

    dest_port = pAct->dest_port;
    switch (pAct->flow_type)
    {
        case RTK_MACSEC_FLOW_EGRESS:
            flow_type = 0b11;
            dest_port = RTK_MACSEC_PORT_COMMON;
            flow_crypt_auth = 0b0;
            protect = pAct->params.egress.protect_frame;
            sa_in_use = pAct->params.egress.sa_in_use;
            include_sci = pAct->params.egress.include_sci;
            validate_frames = pAct->params.egress.use_es | ( pAct->params.egress.use_scb << 1);
            tag_bypass_size = pAct->params.egress.tag_bypass_size;
            conf_offset = pAct->params.egress.confidentiality_offset;
            conf_protect = pAct->params.egress.conf_protect;
            break;

        case RTK_MACSEC_FLOW_INGRESS:
            flow_type = 0b10;
            dest_port = RTK_MACSEC_PORT_CONTROLLED;
            flow_crypt_auth = 0b0;
            protect = pAct->params.ingress.replay_protect;
            sa_in_use = pAct->params.ingress.sa_in_use;

            switch (pAct->params.ingress.validate_frames)
            {
               case RTK_MACSEC_VALIDATE_DISABLE:
                   validate_frames = 0b00;
                   break;
               case RTK_MACSEC_VALIDATE_CHECK:
                   validate_frames = 0b01;
                   break;
               case RTK_MACSEC_VALIDATE_STRICT:
                   validate_frames = 0b10;
                   break;
               default:
                   PR_ERR("unknown type of validate_frames!");
                   return RTK_PHYLIB_ERR_INPUT;
            }
            conf_offset = pAct->params.ingress.confidentiality_offset;
            break;

        case RTK_MACSEC_FLOW_BYPASS:
            flow_type = 0b00;
            flow_crypt_auth = 0b0;
            //sa_in_use = 1;
            sa_in_use = pAct->params.bypass_drop.sa_in_use;
            break;

        case RTK_MACSEC_FLOW_DROP:
            flow_type = 0b01;
            flow_crypt_auth = 0b0;
            //sa_in_use = 1;
            sa_in_use = pAct->params.bypass_drop.sa_in_use;
            break;

        default:
            return RTK_PHYLIB_ERR_INPUT;
    }

    ret = __rtk_phylib_macsec_hw_flow_action_write(
        phydev,
        dir, flow_index,
        sa_index,
        flow_type,
        dest_port,
        drop_non_reserved,
        flow_crypt_auth,
        drop_action,
        protect,
        sa_in_use,
        include_sci,
        validate_frames,
        tag_bypass_size,
        sa_index_update,
        conf_offset,
        conf_protect);

    return ret;
}

static int32 __rtk_phylib_macsec_hw_flow_action_rule_get(rtk_phydev *phydev, rtk_macsec_dir_t dir, uint32 flow_index, phy_macsec_flow_action_t *pAct)
{
    int32 ret = 0;
    uint32 reg = 0, reg_data = 0;
    uint8 flow_type = 0;

    if(pAct == NULL)
        return RTK_PHYLIB_ERR_INPUT;

    rtk_phylib_memset(pAct, 0, sizeof(phy_macsec_flow_action_t));

    reg = MACSEC_REG_SAM_FLOW_CTRL(flow_index);
    MACSEC_REG_GET(phydev, dir, reg, &reg_data);

    pAct->dest_port = (reg_data >> 2) & MASK_2_BITS;

    pAct->sa_index  = (reg_data >> 8) & MASK_8_BITS;

    flow_type = reg_data & MASK_2_BITS;
    switch (flow_type)
    {
        case 0b11:
            pAct->flow_type = RTK_MACSEC_FLOW_EGRESS;
            pAct->params.egress.protect_frame          = (reg_data >> 16) & MASK_1_BITS;
            pAct->params.egress.sa_in_use              = (reg_data >> 17) & MASK_1_BITS;
            pAct->params.egress.include_sci            = (reg_data >> 18) & MASK_1_BITS;
            pAct->params.egress.use_es                 = (reg_data >> 19) & MASK_1_BITS;
            pAct->params.egress.use_scb                = (reg_data >> 20) & MASK_1_BITS;
            pAct->params.egress.tag_bypass_size        = (reg_data >> 21) & MASK_2_BITS;
            pAct->params.egress.sa_index_update_by_hw  = (reg_data >> 23) & MASK_2_BITS;
            pAct->params.egress.confidentiality_offset = (reg_data >> 24) & MASK_7_BITS;
            pAct->params.egress.conf_protect           = (reg_data >> 31) & MASK_1_BITS;
            break;
        case 0b10:
            pAct->flow_type = RTK_MACSEC_FLOW_INGRESS;
            pAct->params.ingress.replay_protect         = (reg_data >> 16) & MASK_1_BITS;
            switch ((reg_data >> 19) & MASK_2_BITS)
            {
                case 0b00:
                    pAct->params.ingress.validate_frames = RTK_MACSEC_VALIDATE_DISABLE;
                    break;
                case 0b01:
                    pAct->params.ingress.validate_frames = RTK_MACSEC_VALIDATE_CHECK;
                    break;
                case 0b10:
                    pAct->params.ingress.validate_frames = RTK_MACSEC_VALIDATE_STRICT;
                    break;
                default:
                    return RTK_PHYLIB_ERR_FAILED;
            }

            pAct->params.ingress.confidentiality_offset = (reg_data >> 24) & MASK_7_BITS;
            break;
        case 0b01:
        case 0b00:
        default:
            pAct->flow_type = (flow_type == 0b01) ? RTK_MACSEC_FLOW_DROP : RTK_MACSEC_FLOW_BYPASS;
            pAct->params.bypass_drop.sa_in_use = (reg_data >> 17) & MASK_1_BITS;
            break;
    }

    return ret;
}

static int32 __rtk_phylib_macsec_hw_flow_action_rule_del(rtk_phydev *phydev, rtk_macsec_dir_t dir, uint32 flow_index)
{
    int32 ret = 0;

    ret = __rtk_phylib_macsec_hw_flow_action_write(
        phydev,
        dir, flow_index,
        0,
        0, 0, 0, 0, 0, 0,
        0, //SA not in use
        0, 0, 0, 0, 0, 0);
    return ret;
}


static int32 __rtk_phylib_macsec_hw_flow_match_rule_set(rtk_phydev *phydev, rtk_macsec_dir_t dir, uint32 flow_index, phy_macsec_flow_match_t *pMatch)
{
    struct rtk_phy_priv *priv = phydev->priv;
    rtk_macsec_port_info_t *macsec_db = priv->macsec;

    int32  ret = 0;
    uint32 reg_data = 0;
    uint32 srcPort = 0;
    uint16 tmp = 0;

    if (pMatch == NULL)
        return RTK_PHYLIB_ERR_INPUT;

    if (flow_index >= MACSEC_SA_MAX(macsec_db))
    {
        PR_ERR("flow_index out of range!");
        return RTK_PHYLIB_ERR_INPUT;
    }

    {
        reg_data = 0;
        reg_data |= (uint32)((((uint32)pMatch->mac_sa[3]) & MASK_8_BITS) << 24);
        reg_data |= (uint32)((((uint32)pMatch->mac_sa[2]) & MASK_8_BITS) << 16);
        reg_data |= (uint32)((((uint32)pMatch->mac_sa[1]) & MASK_8_BITS) << 8);
        reg_data |= (uint32)((((uint32)pMatch->mac_sa[0]) & MASK_8_BITS));
        MACSEC_REG_SET(phydev, dir, MACSEC_REG_SAM_MAC_SA_MATCH_LO(flow_index), reg_data);

        reg_data = 0;
        tmp = ((pMatch->etherType & 0xFF) << 8) | (pMatch->etherType >> 8);
        reg_data |= (uint32)((((uint32)tmp)  & MASK_16_BITS) << 16);

        reg_data |= (uint32)((((uint32)pMatch->mac_sa[5]) & MASK_8_BITS) << 8);
        reg_data |= (uint32)((((uint32)pMatch->mac_sa[4]) & MASK_8_BITS));
        MACSEC_REG_SET(phydev, dir, MACSEC_REG_SAM_MAC_SA_MATCH_HI(flow_index), reg_data);
    }

    {
        reg_data = 0;
        reg_data |= (uint32)((((uint32)pMatch->mac_da[3]) & MASK_8_BITS) << 24);
        reg_data |= (uint32)((((uint32)pMatch->mac_da[2]) & MASK_8_BITS) << 16);
        reg_data |= (uint32)((((uint32)pMatch->mac_da[1]) & MASK_8_BITS) << 8);
        reg_data |= (uint32)((((uint32)pMatch->mac_da[0]) & MASK_8_BITS));
        MACSEC_REG_SET(phydev, dir, MACSEC_REG_SAM_MAC_DA_MATCH_LO(flow_index), reg_data);

        reg_data = 0;
        reg_data |= (uint32)((((uint32)pMatch->vlan_id)   & MASK_12_BITS) << 16);
        reg_data |= (uint32)((((uint32)pMatch->mac_da[5]) & MASK_8_BITS) << 8);
        reg_data |= (uint32)((((uint32)pMatch->mac_da[4]) & MASK_8_BITS));
        MACSEC_REG_SET(phydev, dir, MACSEC_REG_SAM_MAC_DA_MATCH_HI(flow_index), reg_data);
    }

    {
        srcPort = pMatch->sourcePort;

        reg_data = 0;
        reg_data |= (pMatch->fVLANValid) ? (BIT_0) : 0;
        reg_data |= (pMatch->fQinQFound) ? (BIT_1) : 0;
        reg_data |= (pMatch->fSTagValid) ? (BIT_2) : 0;
        reg_data |= (pMatch->fQTagFound) ? (BIT_3) : 0;

        reg_data |= (pMatch->fControlPacket) ? (BIT_7)  : 0;
        reg_data |= (pMatch->fUntagged) ?      (BIT_8)  : 0;
        reg_data |= (pMatch->fTagged) ?        (BIT_9)  : 0;
        reg_data |= (pMatch->fBadTag) ?        (BIT_10) : 0;
        reg_data |= (pMatch->fKayTag) ?        (BIT_11) : 0;

        reg_data |= (uint32)((((uint32)pMatch->macsec_TCI_AN)    & MASK_8_BITS) << 24);
        reg_data |= (uint32)((((uint32)pMatch->matchPriority)    & MASK_4_BITS) << 16);
        reg_data |= (uint32)((((uint32)srcPort)                & MASK_2_BITS) << 12);
        reg_data |= (uint32)((((uint32)pMatch->vlanUserPriority) & MASK_3_BITS) << 4);
        MACSEC_REG_SET(phydev, dir, MACSEC_REG_SAM_MISC_MATCH(flow_index), reg_data);
    }

    {
        #ifdef MACSEC_DBG_PRINT
        {
            PR_DBG("[%s]SCI = 0x%02X%02X%02X%02X%02X%02X%02X%02X\n", __FUNCTION__,
                pMatch->sci[0], pMatch->sci[1], pMatch->sci[2], pMatch->sci[3],
                pMatch->sci[4], pMatch->sci[5], pMatch->sci[6], pMatch->sci[7]);
        }
        #endif
        reg_data = 0;
        reg_data |= (uint32)(((uint32)(pMatch->sci[3]) & MASK_8_BITS) << 24);
        reg_data |= (uint32)(((uint32)(pMatch->sci[2]) & MASK_8_BITS) << 16);
        reg_data |= (uint32)(((uint32)(pMatch->sci[1]) & MASK_8_BITS) << 8);
        reg_data |= (uint32)(((uint32)(pMatch->sci[0]) & MASK_8_BITS));
        MACSEC_REG_SET(phydev, dir, MACSEC_REG_SAM_SCI_MATCH_LO(flow_index), reg_data);
        #ifdef MACSEC_DBG_PRINT
        {
            PR_DBG("[%s]SCI_LO = 0x%08X\n", __FUNCTION__, reg_data);
            MACSEC_REG_GET(phydev, dir, MACSEC_REG_SAM_SCI_MATCH_LO(flow_index), &reg_data);
            PR_DBG("CHECK read[0x%04X] = 0x%08X\n", MACSEC_REG_SAM_SCI_MATCH_LO(flow_index), reg_data);
        }
        #endif

        reg_data = 0;
        reg_data |= (uint32)(((uint32)(pMatch->sci[7]) & MASK_8_BITS) << 24);
        reg_data |= (uint32)(((uint32)(pMatch->sci[6]) & MASK_8_BITS) << 16);
        reg_data |= (uint32)(((uint32)(pMatch->sci[5]) & MASK_8_BITS) << 8);
        reg_data |= (uint32)(((uint32)(pMatch->sci[4]) & MASK_8_BITS));
        MACSEC_REG_SET(phydev, dir, MACSEC_REG_SAM_SCI_MATCH_HI(flow_index), reg_data);
        #ifdef MACSEC_DBG_PRINT
        {
            PR_DBG("[%s]SCI_HI = 0x%08X\n", __FUNCTION__, reg_data);
            MACSEC_REG_GET(phydev, dir, MACSEC_REG_SAM_SCI_MATCH_HI(flow_index), &reg_data);
            PR_DBG("CHECK read[0x%04X] = 0x%08X\n", MACSEC_REG_SAM_SCI_MATCH_HI(flow_index), reg_data);
        }
        #endif
    }

    {
        reg_data = 0;
        reg_data |= pMatch->matchMask;
        MACSEC_REG_SET(phydev, dir, MACSEC_REG_SAM_MASK(flow_index), reg_data);
    }

    {
        reg_data = 0;
        reg_data |= (uint32)((((uint32)pMatch->flow_index) & MASK_8_BITS) << 16);
        reg_data |= (uint32)((((uint32)pMatch->vlanUpInner) & MASK_3_BITS) << 12);
        reg_data |= (uint32)((((uint32)pMatch->vlanIdInner) & MASK_12_BITS));
        MACSEC_REG_SET(phydev, dir, MACSEC_REG_SAM_EXT_MATCH(flow_index), reg_data);
    }
    return ret;
}

static int32 __rtk_phylib_macsec_hw_flow_match_rule_get(rtk_phydev *phydev, rtk_macsec_dir_t dir, uint32 flow_index, phy_macsec_flow_match_t *pMatch)
{
    int32 ret = 0;
    uint32 reg = 0, reg_data = 0;
    uint16 tmp = 0;

    if (pMatch == NULL)
        return RTK_PHYLIB_ERR_INPUT;

    rtk_phylib_memset(pMatch, 0, sizeof(phy_macsec_flow_match_t));

    reg = MACSEC_REG_SAM_MAC_SA_MATCH_LO(flow_index);
    MACSEC_REG_GET(phydev, dir, reg, &reg_data);
    PR_DBG("read[0x%04X] = 0x%08X\n", reg, reg_data);
    pMatch->mac_sa[3] = (uint8)((reg_data >> 24) & MASK_8_BITS);
    pMatch->mac_sa[2] = (uint8)((reg_data >> 16) & MASK_8_BITS);
    pMatch->mac_sa[1] = (uint8)((reg_data >> 8)  & MASK_8_BITS);
    pMatch->mac_sa[0] = (uint8)((reg_data)       & MASK_8_BITS);

    reg = MACSEC_REG_SAM_MAC_SA_MATCH_HI(flow_index);
    MACSEC_REG_GET(phydev, dir, reg, &reg_data);
    PR_DBG("read[0x%04X] = 0x%08X\n", reg, reg_data);
    tmp = (uint16)((reg_data >> 16) & MASK_16_BITS);
    pMatch->etherType = ((tmp & 0xFF) << 8) | ((tmp >> 8) & 0xFF);
    pMatch->mac_sa[5] = (uint8)((reg_data >> 8)  & MASK_8_BITS);
    pMatch->mac_sa[4] = (uint8)((reg_data)       & MASK_8_BITS);

    reg = MACSEC_REG_SAM_MAC_DA_MATCH_LO(flow_index);
    MACSEC_REG_GET(phydev, dir, reg, &reg_data);
    PR_DBG("read[0x%04X] = 0x%08X\n", reg, reg_data);
    pMatch->mac_da[3] = (uint8)((reg_data >> 24) & MASK_8_BITS);
    pMatch->mac_da[2] = (uint8)((reg_data >> 16) & MASK_8_BITS);
    pMatch->mac_da[1] = (uint8)((reg_data >> 8)  & MASK_8_BITS);
    pMatch->mac_da[0] = (uint8)((reg_data)       & MASK_8_BITS);

    reg = MACSEC_REG_SAM_MAC_DA_MATCH_HI(flow_index);
    MACSEC_REG_GET(phydev, dir, reg, &reg_data);
    PR_DBG("read[0x%04X] = 0x%08X\n", reg, reg_data);
    pMatch->vlan_id   = (uint16)((reg_data >> 16) & MASK_12_BITS);
    pMatch->mac_da[5] = (uint8)((reg_data >> 8)   & MASK_8_BITS);
    pMatch->mac_da[4] = (uint8)((reg_data)        & MASK_8_BITS);

    reg = MACSEC_REG_SAM_MISC_MATCH(flow_index);
    MACSEC_REG_GET(phydev, dir, reg, &reg_data);
    PR_DBG("read[0x%04X] = 0x%08X\n", reg, reg_data);
    pMatch->fVLANValid = (reg_data & BIT_0) ? 1 : 0;
    pMatch->fQinQFound = (reg_data & BIT_1) ? 1 : 0;
    pMatch->fSTagValid = (reg_data & BIT_2) ? 1 : 0;
    pMatch->fQTagFound = (reg_data & BIT_3) ? 1 : 0;

    pMatch->fControlPacket = (reg_data & BIT_7) ? 1 : 0;
    pMatch->fUntagged      = (reg_data & BIT_8) ? 1 : 0;
    pMatch->fTagged        = (reg_data & BIT_9) ? 1 : 0;
    pMatch->fBadTag        = (reg_data & BIT_10) ? 1 : 0;
    pMatch->fKayTag        = (reg_data & BIT_11) ? 1 : 0;

    pMatch->macsec_TCI_AN = (uint8)((reg_data >> 24) & MASK_8_BITS);
    pMatch->matchPriority = (uint8)((reg_data >> 16) & MASK_4_BITS);
    pMatch->sourcePort =    (uint8)((reg_data >> 12) & MASK_2_BITS);

    pMatch->vlanUserPriority = (uint8)((reg_data >> 4) & MASK_3_BITS);

    reg = MACSEC_REG_SAM_SCI_MATCH_LO(flow_index);
    MACSEC_REG_GET(phydev, dir, reg, &reg_data);
    PR_DBG("read[0x%04X] = 0x%08X\n", reg, reg_data);
    pMatch->sci[3] = (uint8)((reg_data >> 24) & MASK_8_BITS);
    pMatch->sci[2] = (uint8)((reg_data >> 16) & MASK_8_BITS);
    pMatch->sci[1] = (uint8)((reg_data >> 8)  & MASK_8_BITS);
    pMatch->sci[0] = (uint8)((reg_data)       & MASK_8_BITS);

    reg = MACSEC_REG_SAM_SCI_MATCH_HI(flow_index);
    MACSEC_REG_GET(phydev, dir, reg, &reg_data);
    PR_DBG("read[0x%04X] = 0x%08X\n", reg, reg_data);
    pMatch->sci[7] = (uint8)((reg_data >> 24) & MASK_8_BITS);
    pMatch->sci[6] = (uint8)((reg_data >> 16) & MASK_8_BITS);
    pMatch->sci[5] = (uint8)((reg_data >> 8)  & MASK_8_BITS);
    pMatch->sci[4] = (uint8)((reg_data)       & MASK_8_BITS);

    reg = MACSEC_REG_SAM_MASK(flow_index);
    MACSEC_REG_GET(phydev, dir, reg, &reg_data);
    PR_DBG("read[0x%04X] = 0x%08X\n", reg, reg_data);
    pMatch->matchMask = reg_data;

    reg = MACSEC_REG_SAM_EXT_MATCH(flow_index);
    MACSEC_REG_GET(phydev, dir, reg, &reg_data);
    PR_DBG("read[0x%04X] = 0x%08X\n", reg, reg_data);
    pMatch->flow_index  = (uint32) ((reg_data >> 16) & MASK_8_BITS);
    pMatch->vlanUpInner = (uint8)  ((reg_data >> 12) & MASK_3_BITS);
    pMatch->vlanIdInner = (uint16) ((reg_data) & MASK_12_BITS);

    return ret;
}

static int32 __rtk_phylib_macsec_hw_flow_match_rule_del(rtk_phydev *phydev, rtk_macsec_dir_t dir, uint32 flow_index)
{
    int32  ret = 0;
    uint32 reg_data = 0;

    MACSEC_REG_SET(phydev, dir, MACSEC_REG_SAM_MAC_SA_MATCH_LO(flow_index), reg_data);
    MACSEC_REG_SET(phydev, dir, MACSEC_REG_SAM_MAC_SA_MATCH_HI(flow_index), reg_data);
    MACSEC_REG_SET(phydev, dir, MACSEC_REG_SAM_MAC_DA_MATCH_LO(flow_index), reg_data);
    MACSEC_REG_SET(phydev, dir, MACSEC_REG_SAM_MAC_DA_MATCH_HI(flow_index), reg_data);
    MACSEC_REG_SET(phydev, dir, MACSEC_REG_SAM_MISC_MATCH(flow_index), reg_data);
    MACSEC_REG_SET(phydev, dir, MACSEC_REG_SAM_SCI_MATCH_LO(flow_index), reg_data);
    MACSEC_REG_SET(phydev, dir, MACSEC_REG_SAM_SCI_MATCH_HI(flow_index), reg_data);
    MACSEC_REG_SET(phydev, dir, MACSEC_REG_SAM_MASK(flow_index), reg_data);
    MACSEC_REG_SET(phydev, dir, MACSEC_REG_SAM_EXT_MATCH(flow_index), reg_data);

    return ret;
}

static int32 __rtk_phylib_macsec_hw_flow_enable_get(rtk_phydev *phydev, rtk_macsec_dir_t dir, uint32 flow_index, uint32 *pEnable)
{
    int32  ret = 0;
    uint32 reg_data;

    MACSEC_REG_GET(phydev, dir, MACSEC_REG_SAM_ENTRY_ENABLE(flow_index/32), &reg_data);
    *pEnable = (reg_data & (BIT_0 << (flow_index % 32))) ? ENABLED : DISABLED;

    return ret;
}

static int32 __rtk_phylib_macsec_hw_flow_enable_set(rtk_phydev *phydev, rtk_macsec_dir_t dir, uint32 flow_index, uint32 enable)
{
    int32        ret = 0;
    uint32       reg_data = 0;
    uint32       cur_ena = 0;
    WAIT_COMPLETE_VAR();

    if ((ret = __rtk_phylib_macsec_hw_flow_enable_get(phydev, dir, flow_index, &cur_ena)) != 0)
    {
        return ret;
    }

    if (enable != cur_ena)
    {
        MACSEC_REG_SET(phydev, dir, (enable == ENABLED)?
                   MACSEC_REG_SAM_ENTRY_SET(flow_index/32) : MACSEC_REG_SAM_ENTRY_CLEAR(flow_index/32),
                   BIT_0 << (flow_index % 32));

        if (enable == 0)
        {
            WAIT_COMPLETE(10000000)
            {
                MACSEC_REG_GET(phydev, dir, MACSEC_REG_SAM_IN_FLIGHT, &reg_data);
                if ((reg_data & 0x3F) == 0)
                    break;
            }
            if (WAIT_COMPLETE_IS_TIMEOUT())
            {
                ret = RTK_PHYLIB_ERR_TIMEOUT;
                PR_ERR("flow delete timeout, val=0x%x", (reg_data & 0x3F));
                return ret;
            }
        }

    }

    return ret;
}

int32 rtk_phylib_macsec_init(rtk_phydev *phydev)
{
    int32  ret = 0;
    uint32 data = 0;
    struct rtk_phy_priv *priv = phydev->priv;

    switch (priv->phytype)
    {
        case RTK_PHYLIB_RTL8261N:
        case RTK_PHYLIB_RTL8264B:
        case RTK_PHYLIB_RTL8251L:
        case RTK_PHYLIB_RTL8254B:
            priv->macsec->macsec_reg_get = rtk_phylib_826xb_macsec_read;
            priv->macsec->macsec_reg_set = rtk_phylib_826xb_macsec_write;
            break;
        default:
            return RTK_PHYLIB_ERR_FAILED;
    }

    //read HW version
    MACSEC_REG_GET(phydev, RTK_MACSEC_DIR_EGRESS, 0xFFFC, &data);
    if ((data & MASK_8_BITS) != 160)
        PR_ERR("[%s] HW ver: 0x%X, mismatch!\n", __FUNCTION__, (data & MASK_8_BITS));
    else
        PR_INFO("[%s] HW ver: 0x%X\n", __FUNCTION__, (data & MASK_8_BITS));

    MACSEC_REG_SET(phydev, RTK_MACSEC_DIR_EGRESS,  MACSEC_REG_COUNT_CTRL,     0x00000001);
    MACSEC_REG_SET(phydev, RTK_MACSEC_DIR_EGRESS,  MACSEC_REG_COUNT_CTRL,     0x0000000c);
    MACSEC_REG_SET(phydev, RTK_MACSEC_DIR_EGRESS,  MACSEC_REG_COUNT_SECFAIL1, 0x80fe0000);
    MACSEC_REG_SET(phydev, RTK_MACSEC_DIR_EGRESS,  MACSEC_REG_MISC_CONTROL,   0x02000046);
    MACSEC_REG_SET(phydev, RTK_MACSEC_DIR_EGRESS,  MACSEC_REG_CTX_CTRL,       0xe5880618);
    MACSEC_REG_SET(phydev, RTK_MACSEC_DIR_EGRESS,  MACSEC_REG_CTX_UPD_CTRL,   0x00000003);

    MACSEC_REG_SET(phydev, RTK_MACSEC_DIR_INGRESS, MACSEC_REG_COUNT_CTRL,     0x00000001);
    MACSEC_REG_SET(phydev, RTK_MACSEC_DIR_INGRESS, MACSEC_REG_COUNT_CTRL,     0x0000000c);
    MACSEC_REG_SET(phydev, RTK_MACSEC_DIR_INGRESS, MACSEC_REG_COUNT_SECFAIL1, 0x80fe0000);
    MACSEC_REG_SET(phydev, RTK_MACSEC_DIR_INGRESS, MACSEC_REG_MISC_CONTROL,   0x01001046);
    MACSEC_REG_SET(phydev, RTK_MACSEC_DIR_INGRESS, MACSEC_REG_CTX_CTRL,       0xe5880614);
    MACSEC_REG_SET(phydev, RTK_MACSEC_DIR_INGRESS, MACSEC_REG_CTX_UPD_CTRL,   0x00000003);
    MACSEC_REG_SET(phydev, RTK_MACSEC_DIR_INGRESS, MACSEC_REG_SAM_CP_TAG,     0xe0fac688);
    MACSEC_REG_SET(phydev, RTK_MACSEC_DIR_INGRESS, MACSEC_REG_IG_CC_CONTROL,  0x0000C000);
    MACSEC_REG_SET(phydev, RTK_MACSEC_DIR_INGRESS, MACSEC_REG_SAM_NM_PARAMS,  0xe588003f);

    /* default Egress non-match pkt action:
       bypass: NCP - KaY tag; CP - Untagged
       drop: others
       */
    MACSEC_REG_SET(phydev, RTK_MACSEC_DIR_EGRESS,  MACSEC_REG_SAM_NM_FLOW_NCP, 0x00010101);
    MACSEC_REG_SET(phydev, RTK_MACSEC_DIR_EGRESS,  MACSEC_REG_SAM_NM_FLOW_CP,  0x01010100);
    /* default Ingress non-match pkt action:
       bypass: NCP - KaY tag,Tagged; CP - Untagged
       drop: others
       */
    MACSEC_REG_SET(phydev, RTK_MACSEC_DIR_INGRESS, MACSEC_REG_SAM_NM_FLOW_NCP, 0x08090809);
    MACSEC_REG_SET(phydev, RTK_MACSEC_DIR_INGRESS, MACSEC_REG_SAM_NM_FLOW_CP,  0x09090908);

    MACSEC_REG_SET(phydev, RTK_MACSEC_DIR_EGRESS,  0xF810, 0x000003ff);
    MACSEC_REG_SET(phydev, RTK_MACSEC_DIR_EGRESS,  0xF808, 0x00000300);

    MACSEC_REG_SET(phydev, RTK_MACSEC_DIR_EGRESS, 0x780C, 0x8e880000);
    MACSEC_REG_SET(phydev, RTK_MACSEC_DIR_EGRESS, 0x78FC,  0x80200);
    MACSEC_REG_SET(phydev, RTK_MACSEC_DIR_INGRESS, 0x780C, 0x8e880000);
    MACSEC_REG_SET(phydev, RTK_MACSEC_DIR_INGRESS, 0x78FC,  0x80200);

    return 0;
}

int32 rtk_phylib_macsec_enable_get(rtk_phydev *phydev, uint32 *pEna)
{
    int32  ret = 0;
    struct rtk_phy_priv *priv = phydev->priv;
    uint32 val = 0;
    switch (priv->phytype)
    {
        case RTK_PHYLIB_RTL8261N:
        case RTK_PHYLIB_RTL8264B:
        case RTK_PHYLIB_RTL8251L:
        case RTK_PHYLIB_RTL8254B:
            RTK_PHYLIB_ERR_CHK(rtk_phylib_826xb_macsec_bypass_get(phydev, &val));
            *pEna = (val == 0) ? 1 : 0;
            break;
        default:
            return RTK_PHYLIB_ERR_FAILED;
    }
    return ret;
}
int32 rtk_phylib_macsec_enable_set(rtk_phydev *phydev, uint32 ena)
{
    int32  ret = 0;
    struct rtk_phy_priv *priv = phydev->priv;

    switch (priv->phytype)
    {
        case RTK_PHYLIB_RTL8261N:
        case RTK_PHYLIB_RTL8264B:
        case RTK_PHYLIB_RTL8251L:
        case RTK_PHYLIB_RTL8254B:
            ret = rtk_phylib_826xb_macsec_bypass_set(phydev, (ena == 0) ? 1 : 0);
            break;
        default:
            ret = RTK_PHYLIB_ERR_FAILED;
    }
    PR_DBG("[%s]ena=%u ret=%d\n", __FUNCTION__, ena, ret);
    return ret;
}

int32 rtk_phylib_macsec_sc_create(rtk_phydev *phydev, rtk_macsec_dir_t dir,
    rtk_macsec_sc_t *pSc, uint32 *pSc_id, uint8 active)
{
    struct rtk_phy_priv *priv = phydev->priv;
    rtk_macsec_port_info_t *macsec_db = priv->macsec;

    int32  ret = 0;
    uint8 an = 0, tci_an = 0;
    uint32 i = 0, sc_id = 0xFFFFFFFF;
    uint32 sa_id = 0;
    uint32 flow_base = 0;
    rtk_macsec_cipher_t cs = RTK_MACSEC_CIPHER_GCM_ASE_128;
    phy_macsec_flow_action_t flow;
    phy_macsec_flow_match_t match;
    phy_macsec_sa_params_t hwsa;

    if (pSc == NULL || pSc_id == NULL)
        return RTK_PHYLIB_ERR_INPUT;

    for (i = 0; i < MACSEC_SC_MAX(macsec_db); i++)
    {
        if(MACSEC_SC_IS_CLEAR(macsec_db, dir, i))
        {
            sc_id = i;
            break;
        }
    }
    if ( sc_id == 0xFFFFFFFF )
    {
        PR_ERR("no empty SC entry!");
        return RTK_PHYLIB_ERR_EXCEEDS_CAPACITY;
    }

    flow_base = PHY_MACSEC_HW_FLOW_ID(sc_id);

    rtk_phylib_memset(&flow, 0, sizeof(phy_macsec_flow_action_t));
    rtk_phylib_memset(&match, 0, sizeof(phy_macsec_flow_match_t));
    rtk_phylib_memset(&hwsa, 0, sizeof(phy_macsec_sa_params_t));

    cs = (dir == RTK_MACSEC_DIR_EGRESS) ? (pSc->tx.cipher_suite) : (pSc->rx.cipher_suite);
    switch (cs)
    {
        case RTK_MACSEC_CIPHER_GCM_ASE_128:
            hwsa.flags = 0;
            hwsa.key_bytes = 16;
            if(dir == RTK_MACSEC_DIR_INGRESS && pSc->rx.replay_window > 0xFFFFFFFF)
            {
                PR_ERR("PN replay_window %u out of range 0~%u", pSc->rx.replay_window, (0xFFFFFFFF));
                return RTK_PHYLIB_ERR_INPUT;
            }
            break;
        case RTK_MACSEC_CIPHER_GCM_ASE_256:
            hwsa.flags = 0;
            hwsa.key_bytes = 32;
            if(dir == RTK_MACSEC_DIR_INGRESS && pSc->rx.replay_window > 0xFFFFFFFF)
            {
                PR_ERR("PN replay_window %u out of range 0~%u", pSc->rx.replay_window, (0xFFFFFFFF));
                return RTK_PHYLIB_ERR_INPUT;
            }
            break;
        case RTK_MACSEC_CIPHER_GCM_ASE_XPN_128:
            hwsa.flags = RTK_PHY_MACSEC_SA_FLAG_XPN;
            hwsa.key_bytes = 16;
            if(dir == RTK_MACSEC_DIR_INGRESS && pSc->rx.replay_window > 0x40000000)
            {
                PR_ERR("XPN replay_window %u out of range 0~%u", pSc->rx.replay_window, (0x40000000));
                return RTK_PHYLIB_ERR_INPUT;
            }
            break;
        case RTK_MACSEC_CIPHER_GCM_ASE_XPN_256:
            hwsa.flags = RTK_PHY_MACSEC_SA_FLAG_XPN;
            hwsa.key_bytes = 32;
            if(dir == RTK_MACSEC_DIR_INGRESS && pSc->rx.replay_window > 0x40000000)
            {
                PR_ERR("XPN replay_window %u out of range 0~%u", pSc->rx.replay_window, (0x40000000));
                return RTK_PHYLIB_ERR_INPUT;
            }
            break;
        default:
            return RTK_PHYLIB_ERR_INPUT;
    }

    if (dir == RTK_MACSEC_DIR_EGRESS)
    {
        PR_DBG("[%s]SCI = 0x%02X%02X%02X%02X%02X%02X%02X%02X\n", __FUNCTION__,
                 pSc->tx.sci[0], pSc->tx.sci[1], pSc->tx.sci[2], pSc->tx.sci[3],
                 pSc->tx.sci[4], pSc->tx.sci[5], pSc->tx.sci[6], pSc->tx.sci[7]);
        PR_DBG("[%s]pf %u/is %u/es %u/scb %u/cp %u\n", __FUNCTION__,
                 pSc->tx.protect_frame, pSc->tx.include_sci, pSc->tx.use_es,  pSc->tx.use_scb, pSc->tx.conf_protect);

        /* match rule */
        match.fUntagged = 1;
        match.fControlPacket = 0;
        switch (pSc->tx.flow_match)
        {
            case RTK_MACSEC_MATCH_NON_CTRL:
                match.matchMask = MACSEC_SA_MATCH_MASK_CTRL_PKT;
                break;

            case RTK_MACSEC_MATCH_MAC_DA:
                rtk_phylib_memcpy(match.mac_da, pSc->tx.mac_da.octet, 6 * sizeof(uint8));
                match.matchMask = (MACSEC_SA_MATCH_MASK_MAC_DA_FULL
                    | MACSEC_SA_MATCH_MASK_CTRL_PKT);
                break;

            default:
                return RTK_PHYLIB_ERR_INPUT;
        }
        match.flow_index = flow_base;
        RTK_PHYLIB_ERR_CHK(__rtk_phylib_macsec_hw_flow_match_rule_set(phydev, dir, flow_base, &match));
        MACSEC_SC_MATCH(macsec_db, dir, sc_id) = pSc->tx.flow_match;

        /* flow ctrl */
        flow.flow_type = RTK_MACSEC_FLOW_EGRESS;
        flow.dest_port = RTK_MACSEC_PORT_COMMON;
        flow.sa_index = PHY_MACSEC_HW_SA_ID(sc_id, 0);
        flow.params.egress.protect_frame = pSc->tx.protect_frame;
        flow.params.egress.sa_in_use = 0;
        flow.params.egress.include_sci = pSc->tx.include_sci;
        flow.params.egress.use_es = pSc->tx.use_es;
        flow.params.egress.use_scb = pSc->tx.use_scb;
        flow.params.egress.confidentiality_offset = 0;
        flow.params.egress.conf_protect = pSc->tx.conf_protect;

        RTK_PHYLIB_ERR_CHK(__rtk_phylib_macsec_hw_flow_action_rule_set(phydev, dir, flow_base, &flow));

        /* TC */
        hwsa.direction = dir;
        hwsa.flow_index = flow_base;
        hwsa.sa_expired_irq = 1;
        hwsa.update_en = 1;
        hwsa.next_sa_valid = 0;
        rtk_phylib_memcpy(hwsa.sci, pSc->tx.sci, 8 * sizeof(uint8));
        for (an = 0; an < 4; an++)
        {
            hwsa.an = an;
            hwsa.next_sa_index = PHY_MACSEC_HW_SA_ID(sc_id, ((an + 1) % 4));
            RTK_PHYLIB_ERR_CHK(__rtk_phylib_macsec_hw_sa_set(phydev, dir, PHY_MACSEC_HW_SA_ID(sc_id, an), &hwsa));
        }
    }
    else /* RTK_MACSEC_DIR_INGRESS */
    {
        PR_DBG("[%s]SCI = 0x%02X%02X%02X%02X%02X%02X%02X%02X\n", __FUNCTION__,
                 pSc->rx.sci[0], pSc->rx.sci[1], pSc->rx.sci[2], pSc->rx.sci[3],
                 pSc->rx.sci[4], pSc->rx.sci[5], pSc->rx.sci[6], pSc->rx.sci[7]);
        PR_DBG("[%s]rp %u/rw %u\n", __FUNCTION__,
                 pSc->rx.replay_protect, pSc->rx.replay_window);
        /* match rule */
        match.fTagged = 1;
        match.fControlPacket = 0;
        rtk_phylib_memcpy(match.sci, pSc->rx.sci, 8 * sizeof(uint8));
        switch (pSc->rx.flow_match)
        {
            case RTK_MACSEC_MATCH_SCI:
                tci_an = 0x20;
                match.matchMask = (MACSEC_SA_MATCH_MASK_MACSEC_SCI
                    | MACSEC_SA_MATCH_MASK_MACSEC_TCI_AN_SC
                    | MACSEC_SA_MATCH_MASK_CTRL_PKT);
                break;

            case RTK_MACSEC_MATCH_MAC_SA:
                tci_an = 0x0;
                rtk_phylib_memcpy(match.mac_sa, pSc->rx.mac_sa.octet, 6 * sizeof(uint8));
                match.matchMask = (MACSEC_SA_MATCH_MASK_MAC_SA_FULL
                    | MACSEC_SA_MATCH_MASK_MACSEC_TCI_AN_SC
                    | MACSEC_SA_MATCH_MASK_CTRL_PKT);
                break;

            default:
                return RTK_PHYLIB_ERR_INPUT;
        }
        for (an = 0; an < 4; an++)
        {
            match.macsec_TCI_AN = an | tci_an;
            match.flow_index = flow_base + an;
            RTK_PHYLIB_ERR_CHK(__rtk_phylib_macsec_hw_flow_match_rule_set(phydev, dir, (flow_base + an), &match));
        }
        MACSEC_SC_MATCH(macsec_db, dir, sc_id) = pSc->rx.flow_match;

        /* flow ctrl */
        flow.flow_type = RTK_MACSEC_FLOW_INGRESS;
        flow.dest_port = RTK_MACSEC_PORT_CONTROLLED;

        flow.params.ingress.replay_protect = pSc->rx.replay_protect;
        flow.params.ingress.sa_in_use = 0;
        flow.params.ingress.validate_frames = pSc->rx.validate_frames;
        flow.params.ingress.confidentiality_offset = 0;

        for (an = 0; an < 4; an++)
        {
            flow.sa_index = PHY_MACSEC_HW_SA_ID(sc_id, an);
            RTK_PHYLIB_ERR_CHK(__rtk_phylib_macsec_hw_flow_action_rule_set(phydev, dir, (flow_base + an), &flow));
        }

        /* TC */
        hwsa.direction = dir;
        hwsa.replay_window = pSc->rx.replay_window;
        rtk_phylib_memcpy(hwsa.sci, pSc->rx.sci, 8 * sizeof(uint8));
        for (an = 0; an < 4; an++)
        {
            RTK_PHYLIB_ERR_CHK(__rtk_phylib_macsec_hw_sa_set(phydev, dir, PHY_MACSEC_HW_SA_ID(sc_id, an), &hwsa));
        }
    }

    RTK_PHYLIB_BYTE_ARRAY_TO_VAL(macsec_db->sci[dir][sc_id], hwsa.sci, 0, 8);

    MACSEC_SC_CS(macsec_db, dir, sc_id) = cs;
    MACSEC_SC_SET_USED(macsec_db, dir, sc_id);
    PR_DBG("[%s]%s SC:%u \n", __FUNCTION__, (RTK_MACSEC_DIR_EGRESS == dir) ? "TX" : "RX", sc_id);

    if (dir == RTK_MACSEC_DIR_EGRESS)
    {
        RTK_PHYLIB_ERR_CHK(__rtk_phylib_macsec_hw_flow_enable_set(phydev, dir, flow_base, active));
    }
    else /* RTK_MACSEC_DIR_INGRESS */
    {
        for (an = 0; an < 4; an++)
        {
            sa_id = PHY_MACSEC_HW_SA_ID(sc_id, an);
            if(MACSEC_SA_IS_USED(macsec_db, dir, sa_id))
            {
                RTK_PHYLIB_ERR_CHK(__rtk_phylib_macsec_hw_flow_enable_set(phydev, dir, flow_base + an, active));
            }
        }
    }

    *pSc_id = sc_id;
    return ret;
}

int32 rtk_phylib_macsec_sc_update(rtk_phydev *phydev, rtk_macsec_dir_t dir,
    rtk_macsec_sc_t *pSc, uint32 *pSc_id, uint8 active)
{
    struct rtk_phy_priv *priv = phydev->priv;
    rtk_macsec_port_info_t *macsec_db = priv->macsec;

    int32  ret = 0;
    uint8 an = 0, tci_an = 0;
    uint32 sc_id = 0xFFFFFFFF;
    uint32 flow_base = 0;
    rtk_macsec_cipher_t cs = RTK_MACSEC_CIPHER_GCM_ASE_128;
    phy_macsec_flow_action_t flow[4];
    phy_macsec_flow_match_t match[4];
    phy_macsec_sa_params_t hwsa[4];
    uint64 sci_val = 0;
    rtk_macsec_sc_status_t sc_status;
    uint32 hwsa_flags = 0, hwsa_key_bytes = 0;

    rtk_phylib_memset(&flow, 0, sizeof(flow));
    rtk_phylib_memset(&match, 0, sizeof(match));
    rtk_phylib_memset(&hwsa, 0, sizeof(hwsa));

    if (pSc == NULL || pSc_id == NULL)
        return RTK_PHYLIB_ERR_INPUT;

    if (RTK_MACSEC_DIR_INGRESS == dir)
        RTK_PHYLIB_BYTE_ARRAY_TO_VAL(sci_val, pSc->rx.sci, 0, 8);
    else
        RTK_PHYLIB_BYTE_ARRAY_TO_VAL(sci_val, pSc->tx.sci, 0, 8);
    RTK_PHYLIB_ERR_CHK(rtk_phylib_macsec_sci_to_scid(phydev, dir, sci_val, &sc_id));
    PR_DBG("[%s]update %s SC %u\n", __FUNCTION__, (RTK_MACSEC_DIR_EGRESS == dir) ? "TX" : "RX", sc_id);
    flow_base = PHY_MACSEC_HW_FLOW_ID(sc_id);

    RTK_PHYLIB_ERR_CHK(rtk_phylib_macsec_sc_status_get(phydev, dir, sc_id, &sc_status));
    if (dir == RTK_MACSEC_DIR_EGRESS)
    {
        if (sc_status.tx.hw_sc_flow_status == 1) /* Disable active flow before update */
        {
            RTK_PHYLIB_ERR_CHK(__rtk_phylib_macsec_hw_flow_enable_set(phydev, dir, flow_base, DISABLED));
        }

        RTK_PHYLIB_ERR_CHK(__rtk_phylib_macsec_hw_flow_action_rule_get(phydev, dir, flow_base, &flow[0]));
        RTK_PHYLIB_ERR_CHK(__rtk_phylib_macsec_hw_flow_match_rule_get(phydev, dir, flow_base, &match[0]));
        for (an = 0; an < 4; an++)
        {
            RTK_PHYLIB_ERR_CHK(__rtk_phylib_macsec_hw_sa_get(phydev, dir, PHY_MACSEC_HW_SA_ID(sc_id, an), &hwsa[an]));
        }
    }
    else /* RTK_MACSEC_DIR_INGRESS */
    {
        for (an = 0; an < 4; an++)
        {
            if (sc_status.rx.hw_sc_flow_status[an] == 1) /* Disable active flow before update */
            {
                RTK_PHYLIB_ERR_CHK(__rtk_phylib_macsec_hw_flow_enable_set(phydev, dir, flow_base + an, DISABLED));
            }

            RTK_PHYLIB_ERR_CHK(__rtk_phylib_macsec_hw_flow_action_rule_get(phydev, dir, flow_base + an, &flow[0]));
            RTK_PHYLIB_ERR_CHK(__rtk_phylib_macsec_hw_flow_match_rule_get(phydev, dir, flow_base + an, &match[0]));
            RTK_PHYLIB_ERR_CHK(__rtk_phylib_macsec_hw_sa_get(phydev, dir, PHY_MACSEC_HW_SA_ID(sc_id, an), &hwsa[an]));
        }
    }

    cs = (dir == RTK_MACSEC_DIR_EGRESS) ? (pSc->tx.cipher_suite) : (pSc->rx.cipher_suite);
    switch (cs)
    {
        case RTK_MACSEC_CIPHER_GCM_ASE_128:
            hwsa_flags = 0;
            hwsa_key_bytes = 16;
            if(dir == RTK_MACSEC_DIR_INGRESS && pSc->rx.replay_window > 0xFFFFFFFF)
            {
                PR_ERR("PN replay_window %u out of range 0~%u", pSc->rx.replay_window, (0xFFFFFFFF));
                return RTK_PHYLIB_ERR_INPUT;
            }
            break;
        case RTK_MACSEC_CIPHER_GCM_ASE_256:
            hwsa_flags = 0;
            hwsa_key_bytes = 32;
            if(dir == RTK_MACSEC_DIR_INGRESS && pSc->rx.replay_window > 0xFFFFFFFF)
            {
                PR_ERR("PN replay_window %u out of range 0~%u", pSc->rx.replay_window, (0xFFFFFFFF));
                return RTK_PHYLIB_ERR_INPUT;
            }
            break;
        case RTK_MACSEC_CIPHER_GCM_ASE_XPN_128:
            hwsa_flags = RTK_PHY_MACSEC_SA_FLAG_XPN;
            hwsa_key_bytes = 16;
            if(dir == RTK_MACSEC_DIR_INGRESS && pSc->rx.replay_window > 0x40000000)
            {
                PR_ERR("XPN replay_window %u out of range 0~%u", pSc->rx.replay_window, (0x40000000));
                return RTK_PHYLIB_ERR_INPUT;
            }
            break;
        case RTK_MACSEC_CIPHER_GCM_ASE_XPN_256:
            hwsa_flags = RTK_PHY_MACSEC_SA_FLAG_XPN;
            hwsa_key_bytes = 32;
            if(dir == RTK_MACSEC_DIR_INGRESS && pSc->rx.replay_window > 0x40000000)
            {
                PR_ERR("XPN replay_window %u out of range 0~%u", pSc->rx.replay_window, (0x40000000));
                return RTK_PHYLIB_ERR_INPUT;
            }
            break;
        default:
            return RTK_PHYLIB_ERR_INPUT;
    }

    if (dir == RTK_MACSEC_DIR_EGRESS)
    {
        PR_DBG("[%s]SCI = 0x%02X%02X%02X%02X%02X%02X%02X%02X\n", __FUNCTION__,
                 pSc->tx.sci[0], pSc->tx.sci[1], pSc->tx.sci[2], pSc->tx.sci[3],
                 pSc->tx.sci[4], pSc->tx.sci[5], pSc->tx.sci[6], pSc->tx.sci[7]);
        PR_DBG("[%s]pf %u/is %u/es %u/scb %u/cp %u\n", __FUNCTION__,
                 pSc->tx.protect_frame, pSc->tx.include_sci, pSc->tx.use_es,  pSc->tx.use_scb, pSc->tx.conf_protect);

        /* match rule */
        switch (pSc->tx.flow_match)
        {
            case RTK_MACSEC_MATCH_NON_CTRL:
                match[0].matchMask = MACSEC_SA_MATCH_MASK_CTRL_PKT;
                break;

            case RTK_MACSEC_MATCH_MAC_DA:
                rtk_phylib_memcpy(match[0].mac_da, pSc->tx.mac_da.octet, 6 * sizeof(uint8));
                match[0].matchMask = (MACSEC_SA_MATCH_MASK_MAC_DA_FULL
                    | MACSEC_SA_MATCH_MASK_CTRL_PKT);
                break;

            default:
                return RTK_PHYLIB_ERR_INPUT;
        }
        match[0].flow_index = flow_base;
        RTK_PHYLIB_ERR_CHK(__rtk_phylib_macsec_hw_flow_match_rule_set(phydev, dir, flow_base, &match[0]));
        MACSEC_SC_MATCH(macsec_db, dir, sc_id) = pSc->tx.flow_match;

        /* flow ctrl */
        flow[0].params.egress.protect_frame = pSc->tx.protect_frame;
        flow[0].params.egress.include_sci = pSc->tx.include_sci;
        flow[0].params.egress.use_es = pSc->tx.use_es;
        flow[0].params.egress.use_scb = pSc->tx.use_scb;
        flow[0].params.egress.conf_protect = pSc->tx.conf_protect;

        RTK_PHYLIB_ERR_CHK(__rtk_phylib_macsec_hw_flow_action_rule_set(phydev, dir, flow_base, &flow[0]));

        /* TC */
        for (an = 0; an < 4; an++)
        {
            rtk_phylib_memcpy(hwsa[an].sci, pSc->tx.sci, 8 * sizeof(uint8));
            hwsa[an].flags = hwsa_flags;
            hwsa[an].key_bytes = hwsa_key_bytes;
            RTK_PHYLIB_ERR_CHK(__rtk_phylib_macsec_hw_sa_set(phydev, dir, PHY_MACSEC_HW_SA_ID(sc_id, an), &hwsa[an]));

        }
    }
    else /* RTK_MACSEC_DIR_INGRESS */
    {
        PR_DBG("[%s]SCI = 0x%02X%02X%02X%02X%02X%02X%02X%02X\n", __FUNCTION__,
                 pSc->rx.sci[0], pSc->rx.sci[1], pSc->rx.sci[2], pSc->rx.sci[3],
                 pSc->rx.sci[4], pSc->rx.sci[5], pSc->rx.sci[6], pSc->rx.sci[7]);
        PR_DBG("[%s]rp %u/rw %u\n", __FUNCTION__,
                 pSc->rx.replay_protect, pSc->rx.replay_window);
        MACSEC_SC_MATCH(macsec_db, dir, sc_id) = pSc->rx.flow_match;
        for (an = 0; an < 4; an++)
        {
            /* match rule */
            rtk_phylib_memcpy(match[an].sci, pSc->rx.sci, 8 * sizeof(uint8));
            switch (pSc->rx.flow_match)
            {
                case RTK_MACSEC_MATCH_SCI:
                    tci_an = 0x20;
                    match[an].matchMask = (MACSEC_SA_MATCH_MASK_MACSEC_SCI
                        | MACSEC_SA_MATCH_MASK_MACSEC_TCI_AN_SC
                        | MACSEC_SA_MATCH_MASK_CTRL_PKT);
                    break;

                case RTK_MACSEC_MATCH_MAC_SA:
                    tci_an = 0x0;
                    rtk_phylib_memcpy(match[an].mac_sa, pSc->rx.mac_sa.octet, 6 * sizeof(uint8));
                    match[an].matchMask = (MACSEC_SA_MATCH_MASK_MAC_SA_FULL
                        | MACSEC_SA_MATCH_MASK_MACSEC_TCI_AN_SC
                        | MACSEC_SA_MATCH_MASK_CTRL_PKT);
                    break;

                default:
                    return RTK_PHYLIB_ERR_INPUT;
            }
            match[an].macsec_TCI_AN = an | tci_an;
            match[an].flow_index = flow_base + an;
            RTK_PHYLIB_ERR_CHK(__rtk_phylib_macsec_hw_flow_match_rule_set(phydev, dir, (flow_base + an), &match[an]));


            /* flow ctrl */
            flow[an].params.ingress.replay_protect = pSc->rx.replay_protect;
            flow[an].params.ingress.validate_frames = pSc->rx.validate_frames;
            flow[an].sa_index = PHY_MACSEC_HW_SA_ID(sc_id, an);
            RTK_PHYLIB_ERR_CHK(__rtk_phylib_macsec_hw_flow_action_rule_set(phydev, dir, (flow_base + an), &flow[an]));

            /* TC */
            hwsa[an].replay_window = pSc->rx.replay_window;
            rtk_phylib_memcpy(hwsa[an].sci, pSc->rx.sci, 8 * sizeof(uint8));

            hwsa[an].flags = hwsa_flags;
            hwsa[an].key_bytes =hwsa_key_bytes;

            RTK_PHYLIB_ERR_CHK(__rtk_phylib_macsec_hw_sa_set(phydev, dir, PHY_MACSEC_HW_SA_ID(sc_id, an), &hwsa[an]));
        }
    }
    RTK_PHYLIB_BYTE_ARRAY_TO_VAL(macsec_db->sci[dir][sc_id], hwsa[0].sci, 0, 8);

    MACSEC_SC_CS(macsec_db, dir, sc_id) = cs;
    MACSEC_SC_SET_USED(macsec_db, dir, sc_id);
    PR_DBG("[%s]%s SC:%u \n", __FUNCTION__,  (RTK_MACSEC_DIR_EGRESS == dir) ? "TX" : "RX", sc_id);

    /* Recover active flow after update */
    if (dir == RTK_MACSEC_DIR_EGRESS)
    {
        if (sc_status.tx.hw_sc_flow_status == 1)
        {
            RTK_PHYLIB_ERR_CHK(__rtk_phylib_macsec_hw_flow_enable_set(phydev, dir, flow_base, ENABLED));
        }
    }
    else /* RTK_MACSEC_DIR_INGRESS */
    {
        for (an = 0; an < 4; an++)
        {
            if (sc_status.rx.hw_sc_flow_status[an] == 1)
            {
                RTK_PHYLIB_ERR_CHK(__rtk_phylib_macsec_hw_flow_enable_set(phydev, dir, flow_base + an, ENABLED));
            }
        }
    }

    *pSc_id = sc_id;
    return ret;
}

int32 rtk_phylib_macsec_sc_get(rtk_phydev *phydev, rtk_macsec_dir_t dir,
    uint32 sc_id, rtk_macsec_sc_t *pSc)
{
    struct rtk_phy_priv *priv = phydev->priv;
    rtk_macsec_port_info_t *macsec_db = priv->macsec;

    int32 ret = 0;
    uint32 flow_base = 0, sa_base = 0;
    phy_macsec_flow_action_t flow;
    phy_macsec_flow_match_t match;
    phy_macsec_sa_params_t hwsa;

    if (pSc == NULL)
        return RTK_PHYLIB_ERR_INPUT;
    if (sc_id >= MACSEC_SC_MAX(macsec_db))
        return RTK_PHYLIB_ERR_INPUT;

    if (MACSEC_SC_IS_CLEAR(macsec_db, dir, sc_id))
    {
        PR_ERR("%s SC %u is not existed!",(RTK_MACSEC_DIR_EGRESS == dir) ? "TX" : "RX",sc_id);
        return RTK_PHYLIB_ERR_ENTRY_NOTFOUND;
    }
    rtk_phylib_memset(pSc, 0, sizeof(rtk_macsec_sc_t));

    flow_base = PHY_MACSEC_HW_FLOW_ID(sc_id);
    sa_base = PHY_MACSEC_HW_SA_ID(sc_id, 0);

    RTK_PHYLIB_ERR_CHK(__rtk_phylib_macsec_hw_flow_action_rule_get(phydev, dir, flow_base, &flow));
    RTK_PHYLIB_ERR_CHK(__rtk_phylib_macsec_hw_sa_get(phydev, dir, sa_base, &hwsa));

    if (dir == RTK_MACSEC_DIR_EGRESS)
    {
        pSc->tx.cipher_suite = MACSEC_SC_CS(macsec_db, dir, sc_id);
        pSc->tx.flow_match = MACSEC_SC_MATCH(macsec_db, dir, sc_id);
        if (RTK_MACSEC_MATCH_MAC_DA == pSc->tx.flow_match)
        {
            RTK_PHYLIB_ERR_CHK(__rtk_phylib_macsec_hw_flow_match_rule_get(phydev, dir, flow_base, &match));
            rtk_phylib_memcpy(pSc->tx.mac_da.octet, match.mac_da, 6 * sizeof(uint8));
        }

        pSc->tx.protect_frame = flow.params.egress.protect_frame;
        pSc->tx.include_sci = flow.params.egress.include_sci;
        pSc->tx.use_es = flow.params.egress.use_es;
        pSc->tx.use_scb = flow.params.egress.use_scb;
        pSc->tx.conf_protect = flow.params.egress.conf_protect;

        rtk_phylib_memcpy(pSc->tx.sci, hwsa.sci, 8 * sizeof(uint8));
    }
    else /* RTK_MACSEC_DIR_INGRESS */
    {
        pSc->rx.cipher_suite = MACSEC_SC_CS(macsec_db, dir, sc_id);
        pSc->rx.flow_match = MACSEC_SC_MATCH(macsec_db, dir, sc_id);
        RTK_PHYLIB_ERR_CHK(__rtk_phylib_macsec_hw_flow_match_rule_get(phydev, dir, flow_base, &match));
        if (RTK_MACSEC_MATCH_MAC_SA == pSc->rx.flow_match)
        {
            rtk_phylib_memcpy(pSc->rx.mac_sa.octet, match.mac_sa, 6 * sizeof(uint8));
        }
        rtk_phylib_memcpy(pSc->rx.sci, match.sci, 8 * sizeof(uint8));

        pSc->rx.replay_protect = flow.params.ingress.replay_protect;
        pSc->rx.validate_frames = flow.params.ingress.validate_frames;

        pSc->rx.replay_window = hwsa.replay_window;
    }
    return ret;
}

int32 rtk_phylib_macsec_sc_del(rtk_phydev *phydev, rtk_macsec_dir_t dir, uint32 sc_id)
{
    struct rtk_phy_priv *priv = phydev->priv;
    rtk_macsec_port_info_t *macsec_db = priv->macsec;

    int32 ret = 0;
    uint8 an = 0;
    uint32 flow_base = 0, flow_id = 0;

    PR_DBG("[%s]%s SC:%u\n", __FUNCTION__, (RTK_MACSEC_DIR_EGRESS == dir) ? "TX" : "RX", sc_id);

    if (sc_id >= MACSEC_SC_MAX(macsec_db))
        return RTK_PHYLIB_ERR_INPUT;

    if (MACSEC_SC_IS_CLEAR(macsec_db, dir, sc_id))
        return 0;

    flow_base = PHY_MACSEC_HW_FLOW_ID(sc_id);

    if (dir == RTK_MACSEC_DIR_EGRESS)
    {
        flow_id = flow_base;

        RTK_PHYLIB_ERR_CHK(__rtk_phylib_macsec_hw_flow_enable_set(phydev, dir, flow_id, 0));
        RTK_PHYLIB_ERR_CHK(__rtk_phylib_macsec_hw_flow_match_rule_del(phydev, dir, flow_id));
        RTK_PHYLIB_ERR_CHK(__rtk_phylib_macsec_hw_flow_action_rule_del(phydev, dir, flow_id));

        for (an = 0; an < 4; an++)
        {
            RTK_PHYLIB_ERR_CHK(__rtk_phylib_macsec_hw_sa_del(phydev, dir, PHY_MACSEC_HW_SA_ID(sc_id, an)));
            MACSEC_SA_UNSET_USED(macsec_db, dir, PHY_MACSEC_HW_SA_ID(sc_id, an));
        }
    }
    else /* RTK_MACSEC_DIR_INGRESS */
    {
        for (an = 0; an < 4; an++)
        {
            flow_id = flow_base + an;
            RTK_PHYLIB_ERR_CHK(__rtk_phylib_macsec_hw_flow_enable_set(phydev, dir, flow_id, 0));
            RTK_PHYLIB_ERR_CHK(__rtk_phylib_macsec_hw_flow_match_rule_del(phydev, dir, flow_id));
            RTK_PHYLIB_ERR_CHK(__rtk_phylib_macsec_hw_flow_action_rule_del(phydev, dir, flow_id));
            RTK_PHYLIB_ERR_CHK(__rtk_phylib_macsec_hw_sa_del(phydev, dir, PHY_MACSEC_HW_SA_ID(sc_id, an)));
            MACSEC_SA_UNSET_USED(macsec_db, dir, PHY_MACSEC_HW_SA_ID(sc_id, an));
        }
    }

    macsec_db->sci[dir][sc_id] = 0;
    MACSEC_SC_UNSET_USED(macsec_db, dir, sc_id);
    return ret;
}

int32 rtk_phylib_macsec_sc_status_get(rtk_phydev *phydev, rtk_macsec_dir_t dir,
    uint32 sc_id, rtk_macsec_sc_status_t *pSc_status)
{
    struct rtk_phy_priv *priv = phydev->priv;
    rtk_macsec_port_info_t *macsec_db = priv->macsec;

    int32 ret = 0;
    phy_macsec_flow_action_t flow;
    uint32 flow_base = 0, flow_id = 0;
    uint32 flow_reg = 0, flow_data = 0;
    rtk_enable_t ena = DISABLED;
    rtk_macsec_an_t an;

    if (pSc_status == NULL)
        return RTK_PHYLIB_ERR_INPUT;
    if (sc_id >= MACSEC_SC_MAX(macsec_db))
        return RTK_PHYLIB_ERR_INPUT;
    if (MACSEC_SC_IS_CLEAR(macsec_db, dir, sc_id))
    {
        PR_ERR("%s SC %u is not existed!", (RTK_MACSEC_DIR_EGRESS == dir) ? "TX" : "RX",sc_id);
        return RTK_PHYLIB_ERR_ENTRY_NOTFOUND;
    }
    rtk_phylib_memset(pSc_status, 0, sizeof(rtk_macsec_sc_status_t));

    flow_base = PHY_MACSEC_HW_FLOW_ID(sc_id);
    RTK_PHYLIB_ERR_CHK(__rtk_phylib_macsec_hw_flow_action_rule_get(phydev, dir, flow_base, &flow));

    if (dir == RTK_MACSEC_DIR_EGRESS)
    {
        RTK_PHYLIB_ERR_CHK(__rtk_phylib_macsec_hw_flow_action_rule_get(phydev, dir, flow_base, &flow));
        RTK_PHYLIB_ERR_CHK(__rtk_phylib_macsec_hw_flow_enable_get(phydev, dir, flow_base, &ena));
        pSc_status->tx.hw_flow_index = flow_base;
        pSc_status->tx.hw_sa_index = flow.sa_index;
        pSc_status->tx.sa_inUse = flow.params.egress.sa_in_use;
        pSc_status->tx.hw_sc_flow_status = (ena == ENABLED) ? 1 : 0;
        pSc_status->tx.running_an = PHY_MACSEC_HW_SA_TO_AN(flow.sa_index);

        flow_reg = MACSEC_REG_SAM_FLOW_CTRL(flow_base);
        MACSEC_REG_GET(phydev, dir, flow_reg, &flow_data);
        pSc_status->tx.hw_flow_data = flow_data;
    }
    else /* RTK_MACSEC_DIR_INGRESS */
    {
        pSc_status->rx.hw_flow_base = flow_base;
        for (an = RTK_MACSEC_AN0; an < RTK_MACSEC_AN_MAX; an++)
        {
            flow_id = flow_base + an;
            RTK_PHYLIB_ERR_CHK(__rtk_phylib_macsec_hw_flow_action_rule_get(phydev, dir, flow_id, &flow));
            RTK_PHYLIB_ERR_CHK(__rtk_phylib_macsec_hw_flow_enable_get(phydev, dir, flow_id, &ena));
            pSc_status->rx.hw_sa_index[an] = flow.sa_index;
            pSc_status->rx.sa_inUse[an] = flow.params.ingress.sa_in_use;
            pSc_status->rx.hw_sc_flow_status[an] = (ena == ENABLED) ? 1 : 0;

            flow_reg = MACSEC_REG_SAM_FLOW_CTRL(flow_id);
            MACSEC_REG_GET(phydev, dir, flow_reg, &flow_data);
            pSc_status->rx.hw_flow_data[an] = flow_data;
        }
    }

    return ret;
}

int32 rtk_phylib_macsec_sa_activate(rtk_phydev *phydev, rtk_macsec_dir_t dir,
    uint32 sc_id, rtk_macsec_an_t an)
{
    struct rtk_phy_priv *priv = phydev->priv;
    rtk_macsec_port_info_t *macsec_db = priv->macsec;

    int32 ret = 0;
    uint32 sa_id = PHY_MACSEC_HW_SA_ID(sc_id, an);
    uint32 flow_id = 0;
    phy_macsec_flow_action_t flow;

    if (sc_id >= MACSEC_SC_MAX(macsec_db))
        return RTK_PHYLIB_ERR_INPUT;
    if (sa_id >= MACSEC_SA_MAX(macsec_db))
        return RTK_PHYLIB_ERR_INPUT;
    if (MACSEC_SC_IS_CLEAR(macsec_db, dir, sc_id))
    {
        PR_ERR("[%s]%s SC %u is not existed!", __FUNCTION__,
                (RTK_MACSEC_DIR_EGRESS == dir) ? "TX" : "RX",sc_id);
        return RTK_PHYLIB_ERR_ENTRY_NOTFOUND;
    }
    if (MACSEC_SA_IS_CLEAR(macsec_db, dir, sa_id))
    {
        PR_ERR("[%s]%s SA(SC %u, AN %u) is not existed!", __FUNCTION__,
                (RTK_MACSEC_DIR_EGRESS == dir) ? "TX" : "RX", sc_id, an);
        return RTK_PHYLIB_ERR_ENTRY_NOTFOUND;
    }

    PR_DBG("[%s]%s SC:%u AN:%u\n", __FUNCTION__, (RTK_MACSEC_DIR_EGRESS == dir) ? "TX" : "RX", sc_id, an);
    if (dir == RTK_MACSEC_DIR_EGRESS)
    {
        flow_id = PHY_MACSEC_HW_FLOW_ID(sc_id);
        RTK_PHYLIB_ERR_CHK(__rtk_phylib_macsec_hw_flow_action_rule_get(phydev, dir, flow_id, &flow));
        flow.sa_index = sa_id;
        flow.params.egress.sa_in_use = 1;
    }
    else /* RTK_MACSEC_DIR_INGRESS */
    {
        flow_id = PHY_MACSEC_HW_FLOW_ID(sc_id) + an;
        RTK_PHYLIB_ERR_CHK(__rtk_phylib_macsec_hw_flow_action_rule_get(phydev, dir, flow_id, &flow));
        flow.sa_index = sa_id;
        flow.params.ingress.sa_in_use = 1;
    }

    RTK_PHYLIB_ERR_CHK(__rtk_phylib_macsec_hw_flow_action_rule_set(phydev, dir, flow_id, &flow));
    RTK_PHYLIB_ERR_CHK(__rtk_phylib_macsec_hw_flow_enable_set(phydev, dir, flow_id, ENABLED));
    return ret;
}

int32 rtk_phylib_macsec_rxsa_disable(rtk_phydev *phydev, uint32 rxsc_id,
    rtk_macsec_an_t an)
{
    struct rtk_phy_priv *priv = phydev->priv;
    rtk_macsec_port_info_t *macsec_db = priv->macsec;

    int32 ret = 0;
    rtk_macsec_dir_t dir = RTK_MACSEC_DIR_INGRESS;
    uint32 sa_id = PHY_MACSEC_HW_SA_ID(rxsc_id, an);
    uint32 flow_id = PHY_MACSEC_HW_FLOW_ID(rxsc_id) + an;
    phy_macsec_flow_action_t flow;

    PR_DBG("[%s]SC %u AN %u\n", __FUNCTION__, rxsc_id, an);

    if (rxsc_id >= MACSEC_SC_MAX(macsec_db))
        return RTK_PHYLIB_ERR_INPUT;
    if (sa_id >= MACSEC_SA_MAX(macsec_db))
        return RTK_PHYLIB_ERR_INPUT;
    if (MACSEC_SC_IS_CLEAR(macsec_db, dir, rxsc_id))
    {
        PR_ERR("%s SC %u is not existed!",
                (RTK_MACSEC_DIR_EGRESS == dir) ? "TX" : "RX" ,rxsc_id);
        return RTK_PHYLIB_ERR_ENTRY_NOTFOUND;
    }
    if (MACSEC_SA_IS_CLEAR(macsec_db, dir, sa_id))
    {
        PR_ERR("[%s]%s SA(SC %u, AN %u) is not existed!", __FUNCTION__,
                (RTK_MACSEC_DIR_EGRESS == dir) ? "TX" : "RX", rxsc_id, an);
        return RTK_PHYLIB_ERR_ENTRY_NOTFOUND;
    }

    RTK_PHYLIB_ERR_CHK(__rtk_phylib_macsec_hw_flow_action_rule_get(phydev, dir, flow_id, &flow));
    flow.params.ingress.sa_in_use = 0;
    RTK_PHYLIB_ERR_CHK(__rtk_phylib_macsec_hw_flow_action_rule_set(phydev, dir, flow_id, &flow));

    return ret;
}

int32
rtk_phylib_macsec_txsa_disable(rtk_phydev *phydev, uint32 txsc_id)
{
    struct rtk_phy_priv *priv = phydev->priv;
    rtk_macsec_port_info_t *macsec_db = priv->macsec;

    int32 ret = 0;
    rtk_macsec_dir_t dir = RTK_MACSEC_DIR_EGRESS;
    uint32 flow_id = PHY_MACSEC_HW_FLOW_ID(txsc_id);
    phy_macsec_flow_action_t flow;

    PR_DBG("[%s]SC %u\n", __FUNCTION__, txsc_id);

    if (txsc_id >= MACSEC_SC_MAX(macsec_db))
        return RTK_PHYLIB_ERR_INPUT;

    if (MACSEC_SC_IS_CLEAR(macsec_db, dir, txsc_id))
    {
        PR_ERR("%s SC %u is not existed!",
                (RTK_MACSEC_DIR_EGRESS == dir) ? "TX" : "RX",txsc_id);
        return RTK_PHYLIB_ERR_ENTRY_NOTFOUND;
    }

    RTK_PHYLIB_ERR_CHK(__rtk_phylib_macsec_hw_flow_action_rule_get(phydev, dir, flow_id, &flow));
    flow.params.egress.sa_in_use = 0;
    RTK_PHYLIB_ERR_CHK(__rtk_phylib_macsec_hw_flow_action_rule_set(phydev, dir, flow_id, &flow));

    return ret;
}

int32 rtk_phylib_macsec_sa_create(rtk_phydev *phydev, rtk_macsec_dir_t dir,
    uint32 sc_id, rtk_macsec_an_t an, rtk_macsec_sa_t *pSa)
{
    struct rtk_phy_priv *priv = phydev->priv;
    rtk_macsec_port_info_t *macsec_db = priv->macsec;

    int32 ret = 0;
    phy_macsec_sa_params_t hwsa;
    rtk_macsec_cipher_t cs;
    rtk_macsec_sc_status_t sc_status;
    uint32 sa_id = PHY_MACSEC_HW_SA_ID(sc_id, an);
    uint32 post_sa_id = PHY_MACSEC_HW_SA_ID(sc_id, ((an + 3) % 4));
    uint8 flow_state_recover = 0;
    uint32 flow_id = PHY_MACSEC_HW_FLOW_ID(sc_id) + an;

    PR_DBG("[%s]%s SC:%u AN:%u\n", __FUNCTION__, (RTK_MACSEC_DIR_EGRESS == dir) ? "TX" : "RX", sc_id, an);

    if (pSa == NULL)
        return RTK_PHYLIB_ERR_INPUT;
    if (sc_id >= MACSEC_SC_MAX(macsec_db))
        return RTK_PHYLIB_ERR_INPUT;
    if (sa_id >= MACSEC_SA_MAX(macsec_db))
        return RTK_PHYLIB_ERR_INPUT;

    if (MACSEC_SC_IS_CLEAR(macsec_db, dir, sc_id))
    {
        PR_ERR("%s SC %u is not existed!",
                (RTK_MACSEC_DIR_EGRESS == dir) ? "TX" : "RX", sc_id);
        return RTK_PHYLIB_ERR_ENTRY_NOTFOUND;
    }

    cs = MACSEC_SC_CS(macsec_db, dir, sc_id);
    PR_DBG("[%s]CS: %u, key_bytes: %u\n", __FUNCTION__, cs, pSa->key_bytes);
    switch (cs)
    {
        case RTK_MACSEC_CIPHER_GCM_ASE_128:
        case RTK_MACSEC_CIPHER_GCM_ASE_XPN_128:
            if (pSa->key_bytes != 16)
            {
                PR_ERR("Bad key_bytes:%u for AES-128.", pSa->key_bytes);
                return RTK_PHYLIB_ERR_INPUT;
            }
            break;

        case RTK_MACSEC_CIPHER_GCM_ASE_XPN_256:
        case RTK_MACSEC_CIPHER_GCM_ASE_256:
            if (pSa->key_bytes != 32)
            {
                PR_ERR("Bad key_bytes:%u for AES-256.", pSa->key_bytes);
                return RTK_PHYLIB_ERR_INPUT;
            }
            break;
        default:
            return RTK_PHYLIB_ERR_FAILED;
    }

    RTK_PHYLIB_ERR_CHK(rtk_phylib_macsec_sc_status_get(phydev, dir, sc_id, &sc_status));
    if (dir == RTK_MACSEC_DIR_EGRESS)
    {
        /* disable flow for running AN */
        if ((sc_status.tx.hw_sc_flow_status == 1) && (sc_status.tx.running_an == an))
        {
            RTK_PHYLIB_ERR_CHK(__rtk_phylib_macsec_hw_flow_enable_set(phydev, dir, flow_id, DISABLED));
            flow_state_recover = 1;
        }

    }
    else /* RTK_MACSEC_DIR_INGRESS */
    {
        if (sc_status.rx.hw_sc_flow_status[an] == 1)
        {
            RTK_PHYLIB_ERR_CHK(rtk_phylib_macsec_rxsa_disable(phydev, sc_id, an));
            flow_state_recover = 1;
        }
    }

    RTK_PHYLIB_ERR_CHK(__rtk_phylib_macsec_hw_sa_get(phydev, dir, sa_id, &hwsa));
    switch (cs)
    {
        case RTK_MACSEC_CIPHER_GCM_ASE_128:
        case RTK_MACSEC_CIPHER_GCM_ASE_256:
            hwsa.key_bytes = pSa->key_bytes;
            hwsa.seq = pSa->pn;
            hwsa.seq_h = 0;
            rtk_phylib_memset(hwsa.salt, 0x0, sizeof(uint8) * 12);
            rtk_phylib_memset(hwsa.ssci, 0x0, sizeof(uint8) * 4);
            break;

        case RTK_MACSEC_CIPHER_GCM_ASE_XPN_128:
        case RTK_MACSEC_CIPHER_GCM_ASE_XPN_256:
            hwsa.flags = RTK_PHY_MACSEC_SA_FLAG_XPN;
            hwsa.key_bytes = pSa->key_bytes;
            hwsa.seq = pSa->pn;
            hwsa.seq_h = pSa->pn_h;
            rtk_phylib_memcpy(hwsa.salt, pSa->salt, sizeof(uint8) * 12);
            rtk_phylib_memcpy(hwsa.ssci, pSa->ssci, sizeof(uint8) * 4);
            break;
        default:
            return RTK_PHYLIB_ERR_FAILED;
    }

    rtk_phylib_memcpy(hwsa.key, pSa->key, sizeof(uint8) * hwsa.key_bytes);
    RTK_PHYLIB_ERR_CHK(__rtk_phylib_macsec_hw_sa_set(phydev, dir, sa_id, &hwsa));

    if ((dir == RTK_MACSEC_DIR_EGRESS) && MACSEC_SA_IS_USED(macsec_db, dir, post_sa_id))
    {
        RTK_PHYLIB_ERR_CHK(__rtk_phylib_macsec_hw_sa_get(phydev, dir, post_sa_id, &hwsa));
        hwsa.next_sa_valid = 1;
        RTK_PHYLIB_ERR_CHK(__rtk_phylib_macsec_hw_sa_set(phydev, dir, post_sa_id, &hwsa));
    }

    if (flow_state_recover == 1)
    {
        RTK_PHYLIB_ERR_CHK(__rtk_phylib_macsec_hw_flow_enable_set(phydev, dir, flow_id, ENABLED));
    }

    MACSEC_SA_SET_USED(macsec_db, dir, sa_id);
    return ret;
}

int32 rtk_phylib_macsec_sa_get(rtk_phydev *phydev, rtk_macsec_dir_t dir,
    uint32 sc_id, rtk_macsec_an_t an, rtk_macsec_sa_t *pSa)
{
    struct rtk_phy_priv *priv = phydev->priv;
    rtk_macsec_port_info_t *macsec_db = priv->macsec;

    int32 ret = 0;
    uint32 sa_id = PHY_MACSEC_HW_SA_ID(sc_id, an);
    phy_macsec_sa_params_t hwsa;

    if (pSa == NULL)
        return RTK_PHYLIB_ERR_INPUT;
    if (sc_id >= MACSEC_SC_MAX(macsec_db))
        return RTK_PHYLIB_ERR_INPUT;
    if (sa_id >= MACSEC_SA_MAX(macsec_db))
        return RTK_PHYLIB_ERR_INPUT;
    if (MACSEC_SC_IS_CLEAR(macsec_db, dir, sc_id))
    {
        PR_ERR("%s SC %u is not existed!", (RTK_MACSEC_DIR_EGRESS == dir) ? "TX" : "RX",sc_id);
        return RTK_PHYLIB_ERR_ENTRY_NOTFOUND;
    }
    if (MACSEC_SC_IS_CLEAR(macsec_db, dir, sc_id))
    {
        PR_ERR("[%s]%s SA(SC %u, AN %u) is not existed!", __FUNCTION__,
                (RTK_MACSEC_DIR_EGRESS == dir) ? "TX" : "RX", sc_id, an);
        return RTK_PHYLIB_ERR_ENTRY_NOTFOUND;
    }

    rtk_phylib_memset(pSa, 0x0, sizeof(rtk_macsec_sa_t));
    RTK_PHYLIB_ERR_CHK(__rtk_phylib_macsec_hw_sa_get(phydev, dir, sa_id, &hwsa));

    pSa->key_bytes = hwsa.key_bytes;
    rtk_phylib_memcpy(pSa->key, hwsa.key, sizeof(uint8) * hwsa.key_bytes);
    if (hwsa.flags & RTK_PHY_MACSEC_SA_FLAG_XPN)
    {
        pSa->pn = hwsa.seq;
        pSa->pn_h = hwsa.seq_h;
        rtk_phylib_memcpy(pSa->salt, hwsa.salt, sizeof(uint8) * 12);
        rtk_phylib_memcpy(pSa->ssci, hwsa.ssci, sizeof(uint8) * 4);
    }
    else /* PN */
    {
        pSa->pn = hwsa.seq;
    }

    return ret;
}

int32 rtk_phylib_macsec_sa_del(rtk_phydev *phydev, rtk_macsec_dir_t dir,
    uint32 sc_id, rtk_macsec_an_t an)
{
    struct rtk_phy_priv *priv = phydev->priv;
    rtk_macsec_port_info_t *macsec_db = priv->macsec;

    int32 ret = 0;
    uint32 sa_id = PHY_MACSEC_HW_SA_ID(sc_id, an);
    phy_macsec_sa_params_t hwsa;
    rtk_macsec_sc_status_t sc_status;

    PR_DBG("[%s]%s SC:%u AN:%u\n", __FUNCTION__, (RTK_MACSEC_DIR_EGRESS == dir) ? "TX" : "RX", sc_id, an);

    if (sc_id >= MACSEC_SC_MAX(macsec_db))
        return RTK_PHYLIB_ERR_INPUT;
    if (sa_id >= MACSEC_SA_MAX(macsec_db))
        return RTK_PHYLIB_ERR_INPUT;
    if (MACSEC_SC_IS_CLEAR(macsec_db, dir, sc_id))
    {
        PR_ERR("%s SC %u is not existed!",
                (RTK_MACSEC_DIR_EGRESS == dir) ? "TX" : "RX",sc_id);
        return RTK_PHYLIB_ERR_ENTRY_NOTFOUND;
    }

    RTK_PHYLIB_ERR_CHK(rtk_phylib_macsec_sc_status_get(phydev, dir, sc_id, &sc_status));
    if (dir == RTK_MACSEC_DIR_EGRESS)
    {
        /* disable flow for running AN */
        if ((sc_status.tx.hw_sc_flow_status == 1) && (sc_status.tx.running_an == an))
        {
            RTK_PHYLIB_ERR_CHK(rtk_phylib_macsec_txsa_disable(phydev, sc_id));
        }
    }
    else /* RTK_MACSEC_DIR_INGRESS */
    {
        if (sc_status.rx.hw_sc_flow_status[an] == 1)
        {
            RTK_PHYLIB_ERR_CHK(rtk_phylib_macsec_rxsa_disable(phydev, sc_id, an));
        }
    }

    RTK_PHYLIB_ERR_CHK(__rtk_phylib_macsec_hw_sa_get(phydev, dir, sa_id, &hwsa));
    rtk_phylib_memset(hwsa.key, 0, sizeof(uint8) * RTK_MACSEC_MAX_KEY_LEN);
    rtk_phylib_memset(hwsa.salt, 0x0, sizeof(uint8) * 12);
    rtk_phylib_memset(hwsa.ssci, 0x0, sizeof(uint8) * 4);
    hwsa.seq = 0;
    hwsa.seq_h = 0;
    RTK_PHYLIB_ERR_CHK(__rtk_phylib_macsec_hw_sa_set(phydev, dir, sa_id, &hwsa));

    MACSEC_SA_UNSET_USED(macsec_db, dir, sa_id);
    return ret;
}

int32 rtk_phylib_macsec_stat_port_get(rtk_phydev *phydev, rtk_macsec_stat_t stat,
    uint64 *pCnt)
{
    struct rtk_phy_priv *priv = phydev->priv;
    rtk_macsec_port_info_t *macsec_db = priv->macsec;
    int32 ret = 0;
    uint64 cnt_h = 0, cnt_l = 0;
    uint32 data= 0;

    if (pCnt == NULL)
        return RTK_PHYLIB_ERR_INPUT;

    switch (stat)
    {
        case RTK_MACSEC_STAT_InPktsUntagged:
            MACSEC_REG_GET(phydev, RTK_MACSEC_DIR_INGRESS, 0xC418, &data);
            cnt_l = (uint64)data;
            MACSEC_REG_GET(phydev, RTK_MACSEC_DIR_INGRESS, 0xC41C, &data);
            cnt_h = (uint64)data;
            macsec_db->port_stats.InPktsUntagged += ((cnt_h << 32) | cnt_l);
            *pCnt = macsec_db->port_stats.InPktsUntagged;
            break;
        case RTK_MACSEC_STAT_InPktsNoTag:
            MACSEC_REG_GET(phydev, RTK_MACSEC_DIR_INGRESS, 0xC410, &data);
            cnt_l = (uint64)data;
            MACSEC_REG_GET(phydev, RTK_MACSEC_DIR_INGRESS, 0xC414, &data);
            cnt_h = (uint64)data;
            macsec_db->port_stats.InPktsNoTag += ((cnt_h << 32) | cnt_l);
            *pCnt = macsec_db->port_stats.InPktsNoTag;
            break;
        case RTK_MACSEC_STAT_InPktsBadTag:
            MACSEC_REG_GET(phydev, RTK_MACSEC_DIR_INGRESS, 0xC428, &data);
            cnt_l = (uint64)data;
            MACSEC_REG_GET(phydev, RTK_MACSEC_DIR_INGRESS, 0xC42C, &data);
            cnt_h = (uint64)data;
            macsec_db->port_stats.InPktsBadTag += ((cnt_h << 32) | cnt_l);
            *pCnt = macsec_db->port_stats.InPktsBadTag;
            break;
        case RTK_MACSEC_STAT_InPktsUnknownSCI:
            MACSEC_REG_GET(phydev, RTK_MACSEC_DIR_INGRESS, 0xC440, &data);
            cnt_l = (uint64)data;
            MACSEC_REG_GET(phydev, RTK_MACSEC_DIR_INGRESS, 0xC444, &data);
            cnt_h = (uint64)data;
            macsec_db->port_stats.InPktsUnknownSCI += ((cnt_h << 32) | cnt_l);
            *pCnt = macsec_db->port_stats.InPktsUnknownSCI;
            break;
        case RTK_MACSEC_STAT_InPktsNoSCI:
            MACSEC_REG_GET(phydev, RTK_MACSEC_DIR_INGRESS, 0xC438, &data);
            cnt_l = (uint64)data;
            MACSEC_REG_GET(phydev, RTK_MACSEC_DIR_INGRESS, 0xC43C, &data);
            cnt_h = (uint64)data;
            macsec_db->port_stats.InPktsNoSCI += ((cnt_h << 32) | cnt_l);
            *pCnt = macsec_db->port_stats.InPktsNoSCI;
            break;
        case RTK_MACSEC_STAT_OutPktsUntagged:
            MACSEC_REG_GET(phydev, RTK_MACSEC_DIR_EGRESS, 0xC418, &data);
            cnt_l = (uint64)data;
            MACSEC_REG_GET(phydev, RTK_MACSEC_DIR_EGRESS, 0xC41C, &data);
            cnt_h = (uint64)data;
            macsec_db->port_stats.OutPktsUntagged += ((cnt_h << 32) | cnt_l);
            *pCnt = macsec_db->port_stats.OutPktsUntagged;
            break;
        default:
            return RTK_PHYLIB_ERR_INPUT;
    }
    return ret;
}

int32 rtk_phylib_macsec_stat_txsa_get(rtk_phydev *phydev, uint32 sc_id,
    rtk_macsec_an_t an, rtk_macsec_txsa_stat_t stat, uint64 *pCnt)
{
    struct rtk_phy_priv *priv = phydev->priv;
    rtk_macsec_port_info_t *macsec_db = priv->macsec;

    int32 ret = 0;
    uint64 cnt_h = 0, cnt_l = 0;
    uint32 data= 0, base = 0;
    uint32 sa_id = PHY_MACSEC_HW_SA_ID(sc_id, an);
    rtk_macsec_dir_t dir = RTK_MACSEC_DIR_EGRESS;

    if (pCnt == NULL)
        return RTK_PHYLIB_ERR_INPUT;
    if (sc_id >= MACSEC_SC_MAX(macsec_db))
        return RTK_PHYLIB_ERR_INPUT;
    if (sa_id >= MACSEC_SA_MAX(macsec_db))
        return RTK_PHYLIB_ERR_INPUT;
    if (MACSEC_SC_IS_CLEAR(macsec_db, dir, sc_id))
    {
        return RTK_PHYLIB_ERR_ENTRY_NOTFOUND;
    }
    if (MACSEC_SA_IS_CLEAR(macsec_db, dir, sa_id))
    {
        return RTK_PHYLIB_ERR_ENTRY_NOTFOUND;
    }

    base = (sa_id * 0x80);
    switch (stat)
    {
        case RTK_MACSEC_TXSA_STAT_OutPktsTooLong:
            MACSEC_REG_GET(phydev, dir, (base + 0x8018), &data);
            cnt_l = (uint64)data;
            MACSEC_REG_GET(phydev, dir, (base + 0x801C), &data);
            cnt_h = (uint64)data;
            macsec_db->txsa_stats[sa_id]->OutPktsTooLong += ((cnt_h << 32) | cnt_l);
            *pCnt = macsec_db->txsa_stats[sa_id]->OutPktsTooLong;
            break;
        case RTK_MACSEC_TXSA_STAT_OutOctetsProtectedEncrypted:
            MACSEC_REG_GET(phydev, dir, (base + 0x8000), &data);
            cnt_l = (uint64)data;
            MACSEC_REG_GET(phydev, dir, (base + 0x8004), &data);
            cnt_h = (uint64)data;
            macsec_db->txsa_stats[sa_id]->OutOctetsProtectedEncrypted += ((cnt_h << 32) | cnt_l);
            *pCnt = macsec_db->txsa_stats[sa_id]->OutOctetsProtectedEncrypted;
            break;
        case RTK_MACSEC_TXSA_STAT_OutPktsProtectedEncrypted:
            MACSEC_REG_GET(phydev, dir, (base + 0x8010), &data);
            cnt_l = (uint64)data;
            MACSEC_REG_GET(phydev, dir, (base + 0x8014), &data);
            cnt_h = (uint64)data;
            macsec_db->txsa_stats[sa_id]->OutPktsProtectedEncrypted += ((cnt_h << 32) | cnt_l);
            *pCnt = macsec_db->txsa_stats[sa_id]->OutPktsProtectedEncrypted;
            break;

        default:
            return RTK_PHYLIB_ERR_INPUT;
    }
    return ret;
}

int32 rtk_phylib_macsec_stat_rxsa_get(rtk_phydev *phydev, uint32 sc_id,
    rtk_macsec_an_t an, rtk_macsec_rxsa_stat_t stat, uint64 *pCnt)
{
    struct rtk_phy_priv *priv = phydev->priv;
    rtk_macsec_port_info_t *macsec_db = priv->macsec;

    int32 ret = 0;
    uint64 cnt_h = 0, cnt_l = 0;
    uint32 data= 0, base = 0;
    uint32 sa_id = PHY_MACSEC_HW_SA_ID(sc_id, an);
    rtk_macsec_dir_t dir = RTK_MACSEC_DIR_INGRESS;

    if (pCnt == NULL)
        return RTK_PHYLIB_ERR_INPUT;
    if (sc_id >= MACSEC_SC_MAX(macsec_db))
        return RTK_PHYLIB_ERR_INPUT;
    if (sa_id >= MACSEC_SA_MAX(macsec_db))
        return RTK_PHYLIB_ERR_INPUT;
    if (MACSEC_SC_IS_CLEAR(macsec_db, dir, sc_id))
    {
        return RTK_PHYLIB_ERR_ENTRY_NOTFOUND;
    }
    if (MACSEC_SA_IS_CLEAR(macsec_db, dir, sa_id))
    {
        return RTK_PHYLIB_ERR_ENTRY_NOTFOUND;
    }

    base = (sa_id * 0x80);
    switch (stat)
    {
        case RTK_MACSEC_RXSA_STAT_InPktsUnusedSA:
            MACSEC_REG_GET(phydev, dir, (base + 0x8048), &data);
            cnt_l = (uint64)data;
            MACSEC_REG_GET(phydev, dir, (base + 0x804C), &data);
            cnt_h = (uint64)data;
            macsec_db->rxsa_stats[sa_id]->InPktsUnusedSA += ((cnt_h << 32) | cnt_l);
            *pCnt = macsec_db->rxsa_stats[sa_id]->InPktsUnusedSA;
            break;
        case RTK_MACSEC_RXSA_STAT_InPktsNotUsingSA:
            MACSEC_REG_GET(phydev, dir, (base + 0x8040), &data);
            cnt_l = (uint64)data;
            MACSEC_REG_GET(phydev, dir, (base + 0x8044), &data);
            cnt_h = (uint64)data;
            macsec_db->rxsa_stats[sa_id]->InPktsNotUsingSA += ((cnt_h << 32) | cnt_l);
            *pCnt = macsec_db->rxsa_stats[sa_id]->InPktsNotUsingSA;
            break;
        case RTK_MACSEC_RXSA_STAT_InPktsUnchecked:
            MACSEC_REG_GET(phydev, dir, (base + 0x8010), &data);
            cnt_l = (uint64)data;
            MACSEC_REG_GET(phydev, dir, (base + 0x8014), &data);
            cnt_h = (uint64)data;
            macsec_db->rxsa_stats[sa_id]->InPktsUnchecked += ((cnt_h << 32) | cnt_l);
            *pCnt = macsec_db->rxsa_stats[sa_id]->InPktsUnchecked;
            break;
        case RTK_MACSEC_RXSA_STAT_InPktsDelayed:
            MACSEC_REG_GET(phydev, dir, (base + 0x8018), &data);
            cnt_l = (uint64)data;
            MACSEC_REG_GET(phydev, dir, (base + 0x801C), &data);
            cnt_h = (uint64)data;
            macsec_db->rxsa_stats[sa_id]->InPktsDelayed += ((cnt_h << 32) | cnt_l);
            *pCnt = macsec_db->rxsa_stats[sa_id]->InPktsDelayed;
            break;
        case RTK_MACSEC_RXSA_STAT_InPktsLate:
            MACSEC_REG_GET(phydev, dir, (base + 0x8020), &data);
            cnt_l = (uint64)data;
            MACSEC_REG_GET(phydev, dir, (base + 0x8024), &data);
            cnt_h = (uint64)data;
            macsec_db->rxsa_stats[sa_id]->InPktsLate += ((cnt_h << 32) | cnt_l);
            *pCnt = macsec_db->rxsa_stats[sa_id]->InPktsLate;
            break;
        case RTK_MACSEC_RXSA_STAT_InPktsOK:
            MACSEC_REG_GET(phydev, dir, (base + 0x8028), &data);
            cnt_l = (uint64)data;
            MACSEC_REG_GET(phydev, dir, (base + 0x802C), &data);
            cnt_h = (uint64)data;
            macsec_db->rxsa_stats[sa_id]->InPktsOK += ((cnt_h << 32) | cnt_l);
            *pCnt = macsec_db->rxsa_stats[sa_id]->InPktsOK;
            break;
        case RTK_MACSEC_RXSA_STAT_InPktsInvalid:
            MACSEC_REG_GET(phydev, dir, (base + 0x8030), &data);
            cnt_l = (uint64)data;
            MACSEC_REG_GET(phydev, dir, (base + 0x8034), &data);
            cnt_h = (uint64)data;
            macsec_db->rxsa_stats[sa_id]->InPktsInvalid += ((cnt_h << 32) | cnt_l);
            *pCnt = macsec_db->rxsa_stats[sa_id]->InPktsInvalid;
            break;
        case RTK_MACSEC_RXSA_STAT_InPktsNotValid:
            MACSEC_REG_GET(phydev, dir, (base + 0x8038), &data);
            cnt_l = (uint64)data;
            MACSEC_REG_GET(phydev, dir, (base + 0x803C), &data);
            cnt_h = (uint64)data;
            macsec_db->rxsa_stats[sa_id]->InPktsNotValid += ((cnt_h << 32) | cnt_l);
            *pCnt = macsec_db->rxsa_stats[sa_id]->InPktsNotValid;
            break;
        case RTK_MACSEC_RXSA_STAT_InOctetsDecryptedValidated:
            MACSEC_REG_GET(phydev, dir, (base + 0x8000), &data);
            cnt_l = (uint64)data;
            MACSEC_REG_GET(phydev, dir, (base + 0x8004), &data);
            cnt_h = (uint64)data;
            macsec_db->rxsa_stats[sa_id]->InOctetsDecryptedValidated += ((cnt_h << 32) | cnt_l);
            *pCnt = macsec_db->rxsa_stats[sa_id]->InOctetsDecryptedValidated;
            break;
        default:
            return RTK_PHYLIB_ERR_INPUT;
    }
    return ret;
}

/* ------------------------------------------------------------------------------------------------------------------------------- */
/* id mapping */
int32 rtk_phylib_macsec_sci_to_scid(rtk_phydev *phydev, rtk_macsec_dir_t dir, uint64 sci, uint32 *sc_id)
{
    struct rtk_phy_priv *priv = phydev->priv;
    rtk_macsec_port_info_t *macsec_db = priv->macsec;

    int32 ret = RTK_PHYLIB_ERR_ENTRY_NOTFOUND;
    uint32 i = 0;
    uint64 sci_val = 0;

    PR_DBG("[%s]sci: 0x%016llX\n", __FUNCTION__, sci);
    for (i = 0; i < MACSEC_SC_MAX(macsec_db); i++)
    {
        if(MACSEC_SC_IS_USED(macsec_db, dir, i))
        {
            sci_val = macsec_db->sci[dir][i];
            PR_DBG("[%s]find sci: 0x%016llX, sc id: %u, 0x%016llX\n", __FUNCTION__, sci, i, sci_val);
            if (sci_val == sci)
            {
                *sc_id = i;
                return 0;
            }
        }
    }
    return ret;
}

