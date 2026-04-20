/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2024 Qualcomm Innovation Center, Inc. All rights reserved.
 */
#ifndef TMELCOM_MESSAGE_UIDS_H
#define TMELCOM_MESSAGE_UIDS_H

/*----------------------------------------------------------------------------
 * Documentation
 * -------------------------------------------------------------------------*/

/* TMEL Messages Unique Identifiers bit layout
    _____________________________________
   |           |            |           |
   | 31------16| 15-------8 | 7-------0 |
   | Reserved  |messageType | actionID  |
   |___________|____________|___________|
               \___________  ___________/
                           \/
                      TMEL_MSG_UID
*/

/*   TMEL Messages Unique Identifiers Parameter ID bit layout
_________________________________________________________________________________________
|     |     |     |     |     |     |     |     |     |     |     |    |    |    |       |
|31-30|29-28|27-26|25-24|23-22|21-20|19-18|17-16|15-14|13-12|11-10|9--8|7--6|5--4|3-----0|
| p14 | p13 | p12 | p11 | p10 | p9  | p8  | p7  | p6  | p5  | p4  | p3 | p2 | p1 | nargs |
|type |type |type |type |type |type |type |type |type |type |type |type|type|type|       |
|_____|_____|_____|_____|_____|_____|_____|_____|_____|_____|_____|____|____|____|_______|

*/

/**
   Macro used to define unique TMEL Message Identifier based on
   message type and action identifier.
*/
#define TMEL_MSG_UID_CREATE(m, a) ((u32)(((m & 0xff) << 8) | (a & 0xff)))

/** Helper macro to extract the messageType from TMEL_MSG_UID. */
#define TMEL_MSG_UID_MSG_TYPE(v)      ((v & GENMASK(15,8)) >> 8)

/** Helper macro to extract the actionID from TMEL_MSG_UID. */
#define TMEL_MSG_UID_ACTION_ID(v)     (v & GENMASK(7,0))

/****************************************************************************
 *
 * All definitions of supported messageType's.
 *
 * 0x00 -> 0xF0 messageType used for production use cases.
 * 0xF1 -> 0xFF messageType reserved(can be used for test puprposes).
 *
 * <Template> : TMEL_MSG_<MSGTYPE_NAME>
 * **************************************************************************/
#define TMEL_MSG_SECBOOT                 0x00
#define TMEL_MSG_CRYPTO                  0x01
#define TMEL_MSG_MC3                     0x02
#define TMEL_MSG_FUSE                    0x03
#define TMEL_MSG_ACCESS_CONTROL          0x04
#define TMEL_MSG_ATTESTATION             0x05
#define TMEL_MSG_ONBOARDING              0x06
#define TMEL_MSG_KM                      0x07 /* Key management services */
#define TMEL_MSG_FWUP                    0x08
#define TMEL_MSG_LOG                     0x09
#define TMEL_MSG_FEATURE_LICENSE         0x0A
#define TMEL_MSG_HCS                     0x0B /* Host crypto services */
#define TMEL_MSG_QWES                    0x0C /* QWES services */
#define TMEL_MSG_CDUMP                   0x0D /* TME-L Crashdump */
#define TMEL_MSG_SCP                     0x0E /* SCP (SCP11a, SCP03) related services */
#define TMEL_MSG_UWB_KD                  0x0F /* UWB Key Derivation (CCC, FiRa) related services */
#define TMEL_MSG_TIMETEST                0x10 /* Performance Framework*/


/* Test use-cases */
#define TMEL_MSG_LOOPBACK_TEST           0xFF

/****************************************************************************
 *
 * All definitions of action ID's per messageType.
 *
 * 0x00 -> 0xBF actionID used for production use cases.
 * 0xC0 -> 0xFF messageType must be reserved for test use cases.
 *
 * NOTE: Test ID's shouldn't appear in this file.
 *
 * <Template> : TMEL_ACTION_<MSGTYPE_NAME>_<ACTIONID_NAME>
 * **************************************************************************/

/*----------------------------------------------------------------------------
  Action ID's for TMEL_MSG_SECBOOT
 *-------------------------------------------------------------------------*/
#define TMEL_ACTION_SECBOOT_REQ                          0x01   /* Deprecated. */
#define TMEL_ACTION_SECBOOT_BT_PATCH_AUTH                0x02   /* Deprecated. */
#define TMEL_ACTION_SECBOOT_OEM_MRC_STATE_UPDATE         0x03
#define TMEL_ACTION_SECBOOT_SEC_AUTH                     0x04
#define TMEL_ACTION_SECBOOT_ELF_IMAGE_SIGN_VERIFY        0x05   /* Deprecated. */
#define TMEL_ACTION_SECBOOT_ELF_IMAGE_SEGMENTS_AUTH      0x06   /* Deprecated. */
#define TMEL_ACTION_SECBOOT_NOTIFY_BOOT_MILESTONE        0x07
#define TMEL_ACTION_SECBOOT_UPDATE_ARB_VERSION           0x08
#define TMEL_ACTION_SECBOOT_GET_ARB_VERSION              0x09
#define TMEL_ACTION_SECBOOT_SS_TEAR_DOWN                 0x0A
#define TMEL_ACTION_SECBOOT_XIP_AUTH                     0x0B   /* Deprecated. */
#define TMEL_ACTION_SECBOOT_GET_STATE                    0x0C
#define TMEL_ACTION_SECBOOT_UPDATE_ARB_VER_SWID_LIST     0x0D

/*----------------------------------------------------------------------------
  Action ID's for TMEL_MSG_CRYPTO
 *-------------------------------------------------------------------------*/
#define TMEL_ACTION_CRYPTO_INVALID                       0x00
#define TMEL_ACTION_CRYPTO_GET_RANDOM                    0x01
#define TMEL_ACTION_CRYPTO_GET_RANDOM_ECC_VALUE          0x02
#define TMEL_ACTION_CRYPTO_ECC_PUB_EXTRACT               0x03
#define TMEL_ACTION_CRYPTO_MULTIPLY_ECC_VALUE            0x04    /* Deprecated */
#define TMEL_ACTION_CRYPTO_DIGEST                        0x05
#define TMEL_ACTION_CRYPTO_DIGEST_EX                     0x06
#define TMEL_ACTION_CRYPTO_GENERATE_KEYPAIR              0x07
#define TMEL_ACTION_CRYPTO_KEY_IMPORT                    0x08
#define TMEL_ACTION_CRYPTO_KEY_CLEAR                     0x09
#define TMEL_ACTION_CRYPTO_HMAC                          0x0A
#define TMEL_ACTION_CRYPTO_AES_ENCRYPT_EX                0x0B
#define TMEL_ACTION_CRYPTO_AES_DECRYPT_EX                0x0C
#define TMEL_ACTION_CRYPTO_ECDSA_SIGN_BUFFER             0x0D
#define TMEL_ACTION_CRYPTO_ECDSA_VERIFY_BUFFER           0x0E
#define TMEL_ACTION_CRYPTO_ECDSA_SIGN_DIGEST             0x0F
#define TMEL_ACTION_CRYPTO_ECDSA_VERIFY_DIGEST           0x10
#define TMEL_ACTION_CRYPTO_ECC_GET_PUBLIC_KEY            0x11
#define TMEL_ACTION_CRYPTO_GENERATE_KEY                  0x12
#define TMEL_ACTION_CRYPTO_DERIVE_SHARED_SECRET          0x13
#define TMEL_ACTION_CRYPTO_ECC_MUL_POINT_BY_GEN          0x14    /* Deprecated */
#define TMEL_ACTION_CRYPTO_DERIVE_KEY                    0x15
#define TMEL_ACTION_CRYPTO_WRAP_KEY                      0x16
#define TMEL_ACTION_CRYPTO_UNWRAP_KEY                    0x17

/*----------------------------------------------------------------------------
  Action ID's for TMEL_MSG_HCS
 *-------------------------------------------------------------------------*/
#define TMEL_ACTION_HCS_INVALID                       0x00
#define TMEL_ACTION_HCS_SHA_DIGEST                    0x01
#define TMEL_ACTION_HCS_HMAC_DIGEST                   0x02
#define TMEL_ACTION_HCS_ECC_SIGN_DIGEST               0x03
#define TMEL_ACTION_HCS_ECC_SIGN_MSG                  0x04
#define TMEL_ACTION_HCS_ECC_VERIFY_DIGEST             0x05
#define TMEL_ACTION_HCS_ECC_VERIFY_MSG                0x06
#define TMEL_ACTION_HCS_ECC_GET_PUBKEY                0x07
#define TMEL_ACTION_HCS_ECDH_GET_PUBKEY               0x08
#define TMEL_ACTION_HCS_ECDH_SHARED_SECRET            0x09
#define TMEL_ACTION_HCS_AES_ENCRYPT                   0x0A
#define TMEL_ACTION_HCS_AES_DECRYPT                   0x0B
#define TMEL_ACTION_HCS_PRNG_GET                      0x0C

/*----------------------------------------------------------------------------
  Action ID's for TMEL_MSG_KM
 *-------------------------------------------------------------------------*/
#define TMEL_ACTION_KM_INVALID                         0x00
#define TMEL_ACTION_KM_CLEAR                           0x01
#define TMEL_ACTION_KM_IMPORT                          0x02
#define TMEL_ACTION_KM_GENERATE                        0x03
#define TMEL_ACTION_KM_DERIVE                          0x04
#define TMEL_ACTION_KM_WRAP                            0x05
#define TMEL_ACTION_KM_UNWRAP                          0x06
#define TMEL_ACTION_KM_DISTRIBUTE                      0x07
#define TMEL_ACTION_KM_EXPORT_ECDH_IP_KEY              0x08

/*----------------------------------------------------------------------------
  Action ID's for TMEL_MSG_MC3
 *-------------------------------------------------------------------------*/
#define TMEL_ACTION_MC3_INVALID                          0x00
#define TMEL_ACTION_MC3_AUTHENTICATION_REQ               0x01
#define TMEL_ACTION_MC3_ATTESTATION_REQ                  0x02
#define TMEL_ACTION_MC3_ONBOARDING_REQ                   0x03
#define TMEL_ACTION_MC3_ONBOARDING_CONFIRMATION          0x04
#define TMEL_ACTION_MC3_DECRYPT_MESSAGE_REQ              0x05
#define TMEL_ACTION_MC3_DECRYPT_MESSAGE_CONFIRMATION     0x06
#define TMEL_ACTION_MC3_AUTHENTICATE_MSG_REQ             0x07
#define TMEL_ACTION_MC3_AUTHENTICATE_MSG_CONFIRMATION    0x08

/*----------------------------------------------------------------------------
  Action ID's for TMEL_MSG_FUSE
 *-------------------------------------------------------------------------*/
#define TMEL_ACTION_FUSE_READ_SINGLE                     0x00    /* Deprecated */
#define TMEL_ACTION_FUSE_READ_MULTIPLE                   0x01    /* Deprecated */
#define TMEL_ACTION_FUSE_WRITE_SINGLE                    0x02    /* Deprecated */
#define TMEL_ACTION_FUSE_WRITE_MULTIPLE                  0x03    /* Deprecated */
#define TMEL_ACTION_FUSE_WRITE_SECURE                    0x04    /* Deprecated */
#define TMEL_ACTION_FUSE_READ_SINGLE_ROW                 0x05
#define TMEL_ACTION_FUSE_READ_MULTIPLE_ROW               0x06
#define TMEL_ACTION_FUSE_WRITE_SINGLE_ROW                0x07
#define TMEL_ACTION_FUSE_WRITE_MULTIPLE_ROW              0x08
#define TMEL_ACTION_FUSE_ROM_PATCH_REQ                   0x09

/*----------------------------------------------------------------------------
  Action ID's for TMEL_MSG_ACCESS_CONTROL
 *-------------------------------------------------------------------------*/
#define TMEL_ACTION_ACCESS_CONTROL_INVALID                0x00
#define TMEL_ACTION_ACCESS_CONTROL_NVM_REGISTER           0x01   /* Deprecated */

#define TMEL_ACTION_ACCESS_CONTROL_LOCK_RG_FOR_QAD        0x02
#define TMEL_ACTION_ACCESS_CONTROL_DISABLE_LOCK_RG_IPC    0x03
#define TMEL_ACTION_ACCESS_CONTROL_PROTECT_TMEL_RESOURCES 0x04
#define TMEL_ACTION_ACCESS_CONTROL_SET_XPU_DBGAR          0x05
#define TMEL_ACTION_ACCESS_CONTROL_ENABLE_SILENT_LOGGING  0x06
#define TMEL_ACTION_ACCESS_CONTROL_SECURE_IO_READ         0x07
#define TMEL_ACTION_ACCESS_CONTROL_SECURE_IO_WRITE        0x08

/*----------------------------------------------------------------------------
  Action ID's for TMEL_MSG_FWUP
 *-------------------------------------------------------------------------*/
#define TMEL_ACTION_FWUP_VERIFY_UPDATE                   0x01
#define TMEL_ACTION_FWUP_APPLY_UPDATE                    0x02

/*----------------------------------------------------------------------------
  Action ID's for TMEL_MSG_LOG
 *-------------------------------------------------------------------------*/
#define TMEL_ACTION_LOG_SET_CONFIG                       0x00
#define TMEL_ACTION_LOG_GET_CONFIG                       0x01
#define TMEL_ACTION_LOG_GET                              0x02
#define TMEL_ACTION_Q6LOG_GET                            0x03

/*--------------------------------------------------------------------------
  Action ID's for TMEL_MSG_FEATURE_LICENSE
 *-------------------------------------------------------------------------*/
#define TMEL_ACTION_FEATURE_LICENSE_BTSS                 0x00

/*--------------------------------------------------------------------------
  Action ID's for TMEL_MSG_QWES
 *-------------------------------------------------------------------------*/
#define TMEL_ACTION_QWES_INIT_ATTESTATION                0x00
#define TMEL_ACTION_QWES_DEVICE_ATTESTATION              0x01
#define TMEL_ACTION_QWES_DEVICE_PROVISIONING             0x02
#define TMEL_ACTION_QWES_LICENSING_INSTALL               0x03
#define TMEL_ACTION_QWES_LICENSING_CHECK                 0x04
#define TMEL_ACTION_QWES_LICENSING_ENFORCEHWFEATURES     0x05
#define TMEL_ACTION_QWES_TTIME_CLOUD_REQUEST             0x06
#define TMEL_ACTION_QWES_TTIME_SET                       0x07
#define TMEL_ACTION_QWES_LICENSING_TBDLICENSES           0x09

/*--------------------------------------------------------------------------
  Action ID's for TMEL_MSG_CDUMP
 *-------------------------------------------------------------------------*/
#define TMEL_ACTION_CDUMP_SAVE_DUMP_ADDR                 0x00

/*--------------------------------------------------------------------------
  Action ID's for TMEL_MSG_SCP
 *-------------------------------------------------------------------------*/
#define TMEL_ACTION_SCP_SCP03_RDSCAPDU                   0x00
#define TMEL_ACTION_SCP_SCP03_RDSRAPDU                   0x01
#define TMEL_ACTION_SCP_SCP11A_INITIATE                  0x02
#define TMEL_ACTION_SCP_SCP11A_ESTABLISH_SESSION         0x03

/*--------------------------------------------------------------------------
  Action ID's for TMEL_MSG_UWB_KD
 *-------------------------------------------------------------------------*/
#define TMEL_ACTION_UWB_CCC_TMESALTEDHASH                  0x00
#define TMEL_ACTION_UWB_CCC_TMEUAD                         0x01
#define TMEL_ACTION_UWB_CCC_TMEMUPSK1                      0x02
#define TMEL_ACTION_UWB_CCC_TMEDURSKDUDSK                  0x03
#define TMEL_ACTION_UWB_FIRA_TMEImportSecSessionKey        0x04
#define TMEL_ACTION_UWB_FIRA_TMEDERIVEDCONFIGDIGEST        0x05
#define TMEL_ACTION_UWB_FIRA_TMESECDATAPRIVACYKEY          0x06
#define TMEL_ACTION_UWB_FIRA_TMESECDATAPROTECTIONKEY       0x07
#define TMEL_ACTION_UWB_FIRA_TMEPHYSTSINDEX                0x08
#define TMEL_ACTION_UWB_FIRA_TMESECDERIVEDAUTHENTICATION   0x09
#define TMEL_ACTION_UWB_FIRA_TMESECMASKINGKEY              0x0A

/*--------------------------------------------------------------------------
  Action ID's for TMEL_MSG_TIMETEST
 *-------------------------------------------------------------------------*/
#define TMEL_ACTION_TIMETEST_CONFIG_TTDATA                 0x00

/****************************************************************************
 *
 * All definitions of TMEL Message UID's (messageType | actionID).
 *
 * <Template> : TMEL_MSG_UID_<MSGTYPE_NAME>_<ACTIONID_NAME>
 * *************************************************************************/

/*----------------------------------------------------------------------------
  UID's for TMEL_MSG_SECBOOT
 *-------------------------------------------------------------------------*/
/*
 * BT patch Authentication Request.
 *
 * Note: Deprecated.
 */
#define TMEL_MSG_UID_SECBOOT_BT_PATCH_AUTH  TMEL_MSG_UID_CREATE(TMEL_MSG_SECBOOT,\
                                               TMEL_ACTION_SECBOOT_BT_PATCH_AUTH)

/*
 * OEM MRC Update Request
 */
#define TMEL_MSG_UID_SECBOOT_OEM_MRC_STATE_UPDATE    TMEL_MSG_UID_CREATE(TMEL_MSG_SECBOOT,\
                                                        TMEL_ACTION_SECBOOT_OEM_MRC_STATE_UPDATE)

/*
 * Elf Image Signature & Segment Hash Verification Request.
 */
#define TMEL_MSG_UID_SECBOOT_SEC_AUTH    TMEL_MSG_UID_CREATE(TMEL_MSG_SECBOOT,\
                                            TMEL_ACTION_SECBOOT_SEC_AUTH)

/*
 * Elf Image Signature Verification Request.
 *
 * Note: Deprecated.
 */
#define TMEL_MSG_UID_SECBOOT_ELF_IMAGE_SIGN_VERIFY  TMEL_MSG_UID_CREATE(TMEL_MSG_SECBOOT,\
                                                       TMEL_ACTION_SECBOOT_ELF_IMAGE_SIGN_VERIFY)

/*
 * Elf Image Segment Hash Verification Request.
 *
 * Note: Deprecated.
 */
#define TMEL_MSG_UID_SECBOOT_ELF_IMAGE_SEGMENTS_AUTH  TMEL_MSG_UID_CREATE(TMEL_MSG_SECBOOT,\
                                                         TMEL_ACTION_SECBOOT_ELF_IMAGE_SEGMENTS_AUTH)

/*
 * Secure Boot Milestone Notification.
 */
#define TMEL_MSG_UID_SECBOOT_NOTIFY_BOOT_MILESTONE  TMEL_MSG_UID_CREATE(TMEL_MSG_SECBOOT,\
                                                       TMEL_ACTION_SECBOOT_NOTIFY_BOOT_MILESTONE)

/*
 * Secure Boot Anti Rollback Version Update Request.
 */
#define TMEL_MSG_UID_SECBOOT_UPDATE_ARB_VERSION  TMEL_MSG_UID_CREATE(TMEL_MSG_SECBOOT,\
                                                    TMEL_ACTION_SECBOOT_UPDATE_ARB_VERSION)

/*
 * Secure Boot Get Anti Rollback Version Request.
 */
#define TMEL_MSG_UID_SECBOOT_GET_ARB_VERSION  TMEL_MSG_UID_CREATE(TMEL_MSG_SECBOOT,\
                                                 TMEL_ACTION_SECBOOT_GET_ARB_VERSION)

/*
 * Secure Boot Sub-System Tear Down Request.
 */
#define TMEL_MSG_UID_SECBOOT_SS_TEAR_DOWN  TMEL_MSG_UID_CREATE(TMEL_MSG_SECBOOT,\
                                                 TMEL_ACTION_SECBOOT_SS_TEAR_DOWN)

/*
 * XIP Auth Request
 *
 * Note: Deprecated. Used in Slate and Helios.
 */
#define TMEL_MSG_UID_SECBOOT_XIP_AUTH                TMEL_MSG_UID_CREATE(TMEL_MSG_SECBOOT,\
                                                        TMEL_ACTION_SECBOOT_XIP_AUTH)

/*
 * Secure Boot Sub-System get tmel state request.
 */
#define TMEL_MSG_UID_SECBOOT_GET_STATE TMEL_MSG_UID_CREATE(TMEL_MSG_SECBOOT,\
                                                 TMEL_ACTION_SECBOOT_GET_STATE)

/*
 * Secure Boot Anti Rollback Version Update(based on the swId List) Request.
 */
#define TMEL_MSG_UID_SECBOOT_UPDATE_ARB_VER_SWID_LIST TMEL_MSG_UID_CREATE(TMEL_MSG_SECBOOT,\
                                          TMEL_ACTION_SECBOOT_UPDATE_ARB_VER_SWID_LIST)

/*----------------------------------------------------------------------------
  UID's for TMEL_MSG_CRYPTO
 *-------------------------------------------------------------------------*/

/*
 * Generate Random Data.
 */
#define TMEL_MSG_UID_CRYPTO_GET_RANDOM    TMEL_MSG_UID_CREATE(TMEL_MSG_CRYPTO,\
                                             TMEL_ACTION_CRYPTO_GET_RANDOM)

/*
 * Generate Random ECC.
 */
#define TMEL_MSG_UID_CRYPTO_GET_RANDOM_ECC_VALUE      TMEL_MSG_UID_CREATE(TMEL_MSG_CRYPTO,\
                                                         TMEL_ACTION_CRYPTO_GET_RANDOM_ECC_VALUE)

/*
 * Generate ECC Pub.
 */
#define TMEL_MSG_UID_CRYPTO_ECC_PUB_EXTRACT           TMEL_MSG_UID_CREATE(TMEL_MSG_CRYPTO,\
                                                         TMEL_ACTION_CRYPTO_ECC_PUB_EXTRACT)

/*
 * Deprecated
 * Multiply ECC value by point message
 */
#define TMEL_MSG_UID_CRYPTO_MULTIPLY_ECC_VALUE        TMEL_MSG_UID_CREATE(TMEL_MSG_CRYPTO,\
                                                         TMEL_ACTION_CRYPTO_MULTIPLY_ECC_VALUE)

/*
 * Digest
 */
#define TMEL_MSG_UID_CRYPTO_DIGEST                    TMEL_MSG_UID_CREATE(TMEL_MSG_CRYPTO,\
                                                         TMEL_ACTION_CRYPTO_DIGEST)

/*
 * Digest_Ex
 */
#define TMEL_MSG_UID_CRYPTO_DIGEST_EX                 TMEL_MSG_UID_CREATE(TMEL_MSG_CRYPTO,\
                                                         TMEL_ACTION_CRYPTO_DIGEST_EX)

/*
 * Generate KeyPair
 */
#define TMEL_MSG_UID_CRYPTO_GENERATE_KEYPAIR          TMEL_MSG_UID_CREATE(TMEL_MSG_CRYPTO,\
                                                         TMEL_ACTION_CRYPTO_GENERATE_KEYPAIR)
/*
 * Import Key
 */
#define TMEL_MSG_UID_CRYPTO_KEY_IMPORT                TMEL_MSG_UID_CREATE(TMEL_MSG_CRYPTO,\
                                                         TMEL_ACTION_CRYPTO_KEY_IMPORT)

/*
 * Clear Key
 */
#define TMEL_MSG_UID_CRYPTO_KEY_CLEAR                 TMEL_MSG_UID_CREATE(TMEL_MSG_CRYPTO,\
                                                         TMEL_ACTION_CRYPTO_KEY_CLEAR)

/*
 * HMAC
 */
#define TMEL_MSG_UID_CRYPTO_HMAC                      TMEL_MSG_UID_CREATE(TMEL_MSG_CRYPTO,\
                                                         TMEL_ACTION_CRYPTO_HMAC)

/*
 * AES Encryption Ex
 */
#define TMEL_MSG_UID_CRYPTO_AES_ENCRYPT_EX            TMEL_MSG_UID_CREATE(TMEL_MSG_CRYPTO,\
                                                         TMEL_ACTION_CRYPTO_AES_ENCRYPT_EX)

/*
 * AES Decryption Ex
 */
#define TMEL_MSG_UID_CRYPTO_AES_DECRYPT_EX            TMEL_MSG_UID_CREATE(TMEL_MSG_CRYPTO,\
                                                         TMEL_ACTION_CRYPTO_AES_DECRYPT_EX)

/*
 * ECDSA Signature
 */
#define TMEL_MSG_UID_CRYPTO_ECDSA_SIGN_BUFFER             TMEL_MSG_UID_CREATE(TMEL_MSG_CRYPTO,\
                                                             TMEL_ACTION_CRYPTO_ECDSA_SIGN_BUFFER)

/*
 * ECDSA Verify
 */
#define TMEL_MSG_UID_CRYPTO_ECDSA_VERIFY_BUFFER       TMEL_MSG_UID_CREATE(TMEL_MSG_CRYPTO,\
                                                         TMEL_ACTION_CRYPTO_ECDSA_VERIFY_BUFFER)

/*
 * ECDSA Signature Digest
 */
#define TMEL_MSG_UID_CRYPTO_ECDSA_SIGN_DIGEST         TMEL_MSG_UID_CREATE(TMEL_MSG_CRYPTO,\
                                                         TMEL_ACTION_CRYPTO_ECDSA_SIGN_DIGEST)

/*
 * ECDSA Verify Digest
 */
#define TMEL_MSG_UID_CRYPTO_ECDSA_VERIFY_DIGEST       TMEL_MSG_UID_CREATE(TMEL_MSG_CRYPTO,\
                                                         TMEL_ACTION_CRYPTO_ECDSA_VERIFY_DIGEST)

/*
 * ECC Get Public Key
 */
#define TMEL_MSG_UID_CRYPTO_ECC_GET_PUBLIC_KEY        TMEL_MSG_UID_CREATE(TMEL_MSG_CRYPTO,\
                                                         TMEL_ACTION_CRYPTO_ECC_GET_PUBLIC_KEY)

/*
 * Generate Key
 */
#define TMEL_MSG_UID_CRYPTO_GENERATE_KEY              TMEL_MSG_UID_CREATE(TMEL_MSG_CRYPTO,\
                                                         TMEL_ACTION_CRYPTO_GENERATE_KEY)

/*
 * Derive Shared Secret
 */
#define TMEL_MSG_UID_CRYPTO_DERIVE_SHARED_SECRET      TMEL_MSG_UID_CREATE(TMEL_MSG_CRYPTO,\
                                                         TMEL_ACTION_CRYPTO_DERIVE_SHARED_SECRET)

/*
 * Deprecated
 * ECC Extended Public Key
 */
#define TMEL_MSG_UID_CRYPTO_ECC_MUL_POINT_BY_GEN      TMEL_MSG_UID_CREATE(TMEL_MSG_CRYPTO,\
                                                         TMEL_ACTION_CRYPTO_ECC_MUL_POINT_BY_GEN)

/*
 * Derive Key
 */
#define TMEL_MSG_UID_CRYPTO_DERIVE_KEY                TMEL_MSG_UID_CREATE(TMEL_MSG_CRYPTO,\
                                                         TMEL_ACTION_CRYPTO_DERIVE_KEY)
/*
 * Wrap Key
 */
#define TMEL_MSG_UID_CRYPTO_WRAP_KEY                TMEL_MSG_UID_CREATE(TMEL_MSG_CRYPTO,\
                                                         TMEL_ACTION_CRYPTO_WRAP_KEY)

/*
 * UnWrap Key
 */
#define TMEL_MSG_UID_CRYPTO_UNWRAP_KEY              TMEL_MSG_UID_CREATE(TMEL_MSG_CRYPTO,\
                                                         TMEL_ACTION_CRYPTO_UNWRAP_KEY)

/*----------------------------------------------------------------------------
  UID's for TMEL_MSG_HCS
 *-------------------------------------------------------------------------*/
/*
 * Hash (HMAC-SHA or SHA)
 * ref: TMESHAMessage_t
 */
#define TMEL_MSG_UID_HCS_SHA_DIGEST          TMEL_MSG_UID_CREATE(TMEL_MSG_HCS,\
                                                         TMEL_ACTION_HCS_SHA_DIGEST)
#define TMEL_MSG_UID_HCS_HMAC_DIGEST         TMEL_MSG_UID_CREATE(TMEL_MSG_HCS,\
                                                         TMEL_ACTION_HCS_HMAC_DIGEST)

/*
 * ECDSA sign (digest or msg)
 * ref: TMEECCSignMessage_t
 */
#define TMEL_MSG_UID_HCS_ECC_SIGN_DIGEST     TMEL_MSG_UID_CREATE(TMEL_MSG_HCS,\
                                                   TMEL_ACTION_HCS_ECC_SIGN_DIGEST)

#define TMEL_MSG_UID_HCS_ECC_SIGN_MSG        TMEL_MSG_UID_CREATE(TMEL_MSG_HCS,\
                                                   TMEL_ACTION_HCS_ECC_SIGN_MSG)


/*
 * ECDSA verify (digest or msg)
 * ref: TMEECCVerifyMessage_t
 */
#define TMEL_MSG_UID_HCS_ECC_VERIFY_DIGEST     TMEL_MSG_UID_CREATE(TMEL_MSG_HCS,\
                                                   TMEL_ACTION_HCS_ECC_VERIFY_DIGEST)

#define TMEL_MSG_UID_HCS_ECC_VERIFY_MSG        TMEL_MSG_UID_CREATE(TMEL_MSG_HCS,\
                                                   TMEL_ACTION_HCS_ECC_VERIFY_MSG)


/*
 * ECC get public key
 * ref: TMEECCGetPubKeyMessage_t
 */
#define TMEL_MSG_UID_HCS_ECC_GET_PUBKEY            TMEL_MSG_UID_CREATE(TMEL_MSG_HCS,\
                                                       TMEL_ACTION_HCS_ECC_GET_PUBKEY)

#define TMEL_MSG_UID_HCS_ECDH_GET_PUBKEY           TMEL_MSG_UID_CREATE(TMEL_MSG_HCS,\
                                                       TMEL_ACTION_HCS_ECDH_GET_PUBKEY)

/*
 * ECDH shared secret
 * ref: TMEECDHSharedSecretMessage_t
 */
#define TMEL_MSG_UID_HCS_ECDH_SHARED_SECRET            TMEL_MSG_UID_CREATE(TMEL_MSG_HCS,\
                                                           TMEL_ACTION_HCS_ECDH_SHARED_SECRET)

/*
 * AES encrypt
 * ref: TMEAESEncryptMessage_t
 */
#define TMEL_MSG_UID_HCS_AES_ENCRYPT            TMEL_MSG_UID_CREATE(TMEL_MSG_HCS,\
                                                       TMEL_ACTION_HCS_AES_ENCRYPT)

/*
 * AES decrypt
 * ref: TMEAESDecryptMessage_t
 */
#define TMEL_MSG_UID_HCS_AES_DECRYPT            TMEL_MSG_UID_CREATE(TMEL_MSG_HCS,\
                                                       TMEL_ACTION_HCS_AES_DECRYPT)

/*
 * Get PRNG number
 * ref: TMEPRNGGetMessage_t
 */
#define TMEL_MSG_UID_HCS_PRNG_GET            TMEL_MSG_UID_CREATE(TMEL_MSG_HCS,\
                                                       TMEL_ACTION_HCS_PRNG_GET)

/*----------------------------------------------------------------------------
  UID's for TMEL_MSG_KM
 *-------------------------------------------------------------------------*/
/*
 * Generate Key (TMEKMGenerateKeyMessage_t)
 */
#define TMEL_MSG_UID_KM_GENERATE             TMEL_MSG_UID_CREATE(TMEL_MSG_KM,\
                                                         TMEL_ACTION_KM_GENERATE)

/*
 * Import Key (TMEKMImportKeyMessage_t)
 */
#define TMEL_MSG_UID_KM_IMPORT                TMEL_MSG_UID_CREATE(TMEL_MSG_KM,\
                                                         TMEL_ACTION_KM_IMPORT)

/*
 * Clear Key (TMEKMClearKeyMessage_t)
 */
#define TMEL_MSG_UID_KM_CLEAR                 TMEL_MSG_UID_CREATE(TMEL_MSG_KM,\
                                                         TMEL_ACTION_KM_CLEAR)

/*
 * Derive Key (TMEKMDeriveKeyMessage_t)
 */
#define TMEL_MSG_UID_KM_DERIVE            TMEL_MSG_UID_CREATE(TMEL_MSG_KM,\
                                                     TMEL_ACTION_KM_DERIVE)

/*
 * Wrap Key (TMEKMWrapKeyMessage_t)
 */
#define TMEL_MSG_UID_KM_WRAP              TMEL_MSG_UID_CREATE(TMEL_MSG_KM,\
                                                        TMEL_ACTION_KM_WRAP)

/*
 * Unwrap Key (TMEKMUnwrapKeyMessage_t)
 */
#define TMEL_MSG_UID_KM_UNWRAP            TMEL_MSG_UID_CREATE(TMEL_MSG_KM,\
                                                        TMEL_ACTION_KM_UNWRAP)

/*
 * Distribute Key (TMEKMDistributeKeyMessage_t)
 */
#define TMEL_MSG_UID_KM_DISTRIBUTE            TMEL_MSG_UID_CREATE(TMEL_MSG_KM,\
                                                        TMEL_ACTION_KM_DISTRIBUTE)

/*
 * Derive and Export ECDH IP Protection Public Key (TMEKMExportEcdhIpKeyMessage_t)
 */
#define TMEL_MSG_UID_KM_EXPORT_ECDH_IP            TMEL_MSG_UID_CREATE(TMEL_MSG_KM,\
                                                     TMEL_ACTION_KM_EXPORT_ECDH_IP_KEY)

/*----------------------------------------------------------------------------
  UID's for TMEL_MSG_MC3
 *-------------------------------------------------------------------------*/
/*
 * MC3 Authentication
 */
#define TMEL_MSG_UID_MC3_AUTHENTICATION_REQ          TMEL_MSG_UID_CREATE(TMEL_MSG_MC3,\
                                                        TMEL_ACTION_MC3_AUTHENTICATION_REQ)

/*
 * MC3 Attestation Token construction
 */
#define TMEL_MSG_UID_MC3_ATTESTATION_REQ           TMEL_MSG_UID_CREATE(TMEL_MSG_MC3,\
                                                      TMEL_ACTION_MC3_ATTESTATION_REQ)


/*
 * MC3 Onboarding request
 */
#define TMEL_MSG_UID_MC3_ONBOARDING_REQ              TMEL_MSG_UID_CREATE(TMEL_MSG_MC3,\
                                                        TMEL_ACTION_MC3_ONBOARDING_REQ)


/*
 * MC3 Onboarding Confirmation
 */
#define TMEL_MSG_UID_MC3_ONBOARDING_CONFIRMATION    TMEL_MSG_UID_CREATE(TMEL_MSG_MC3,\
                                                       TMEL_ACTION_MC3_ONBOARDING_CONFIRMATION)

/*
 * MC3 Decryption request
 */
#define TMEL_MSG_UID_MC3_DECRYPT_MESSAGE_REQ        TMEL_MSG_UID_CREATE(TMEL_MSG_MC3,\
                                                       TMEL_ACTION_MC3_DECRYPT_MESSAGE_REQ)

/*
 * MC3 Decryption confirmation request
 */
#define TMEL_MSG_UID_MC3_DECRYPT_MESSAGE_CONFIRMATION  TMEL_MSG_UID_CREATE(TMEL_MSG_MC3,\
                                                          TMEL_ACTION_MC3_DECRYPT_MESSAGE_CONFIRMATION)

/*
 * MC3 Authenticate request
 */
#define TMEL_MSG_UID_MC3_AUTHENTICATE_MSG_REQ           TMEL_MSG_UID_CREATE(TMEL_MSG_MC3,\
                                                           TMEL_ACTION_MC3_AUTHENTICATE_MSG_REQ)

/*
 * MC3 Authentication msg confirmation request
 */
#define TMEL_MSG_UID_MC3_AUTHENTICATE_MSG_CONFIRMATION            TMEL_MSG_UID_CREATE(TMEL_MSG_MC3,\
                                                                     TMEL_ACTION_MC3_AUTHENTICATE_MSG_CONFIRMATION)

/*----------------------------------------------------------------------------
  UID's for TMEL_MSG_FUSE
 *-------------------------------------------------------------------------*/

/*
 * Read single fuse
 */
#define TMEL_MSG_UID_FUSE_READ_SINGLE_ROW           TMEL_MSG_UID_CREATE(TMEL_MSG_FUSE,\
                                                       TMEL_ACTION_FUSE_READ_SINGLE_ROW)
/*
 * Read multiple fuses
 */
#define TMEL_MSG_UID_FUSE_READ_MULTIPLE_ROW          TMEL_MSG_UID_CREATE(TMEL_MSG_FUSE,\
                                                       TMEL_ACTION_FUSE_READ_MULTIPLE_ROW)

/*
 * Write single fuse
 */
#define TMEL_MSG_UID_FUSE_WRITE_SINGLE_ROW           TMEL_MSG_UID_CREATE(TMEL_MSG_FUSE,\
                                                       TMEL_ACTION_FUSE_WRITE_SINGLE_ROW)

/*
 * Write multiple fuses
 */
#define TMEL_MSG_UID_FUSE_WRITE_MULTIPLE_ROW          TMEL_MSG_UID_CREATE(TMEL_MSG_FUSE,\
                                                       TMEL_ACTION_FUSE_WRITE_MULTIPLE_ROW)

/* Deprecated */
/*
 * Write secure fuse
 */
#define TMEL_MSG_UID_FUSE_WRITE_SECURE               TMEL_MSG_UID_CREATE(TMEL_MSG_FUSE,\
                                                        TMEL_ACTION_FUSE_WRITE_SECURE)

/*
 * Write rom patch fuse
 */
#define TMEL_MSG_UID_FUSE_ROM_PATCH_REQ              TMEL_MSG_UID_CREATE(TMEL_MSG_FUSE,\
                                                        TMEL_ACTION_FUSE_ROM_PATCH_REQ)

/*----------------------------------------------------------------------------
  UID's for TMEL_MSG_ACCESS_CONTROL
 *-------------------------------------------------------------------------*/
/* Deprecated */
/*
 * Provide or release NVM register access
 */
#define TMEL_MSG_UID_ACCESS_CONTROL_NVM_REGISTER            TMEL_MSG_UID_CREATE(TMEL_MSG_ACCESS_CONTROL,\
                                                              TMEL_ACTION_ACCESS_CONTROL_NVM_REGISTER)

/*
 * Lock a RG on behalf of given QAD.
 */
#define TMEL_MSG_UID_ACCESS_CONTROL_LOCK_RG_FOR_QAD            TMEL_MSG_UID_CREATE(TMEL_MSG_ACCESS_CONTROL,\
                                                                 TMEL_ACTION_ACCESS_CONTROL_LOCK_RG_FOR_QAD)

/*
 * Disable IPC service to lock RG on behalf of another QAD.
 */
#define TMEL_MSG_UID_ACCESS_CONTROL_DISABLE_LOCK_RG_IPC           TMEL_MSG_UID_CREATE(TMEL_MSG_ACCESS_CONTROL,\
                                                                    TMEL_ACTION_ACCESS_CONTROL_DISABLE_LOCK_RG_IPC)

/*
 * Protect TMEL owned resources.
 */
#define TMEL_MSG_UID_ACCESS_CONTROL_PROTECT_TMEL_RESOURCES     TMEL_MSG_UID_CREATE(TMEL_MSG_ACCESS_CONTROL,\
                                                                 TMEL_ACTION_ACCESS_CONTROL_PROTECT_TMEL_RESOURCES)

/*
 * Set XPU DBGAR registers.
 */
#define TMEL_MSG_UID_ACCESS_CONTROL_SET_XPU_DBGAR            TMEL_MSG_UID_CREATE(TMEL_MSG_ACCESS_CONTROL,\
                                                                 TMEL_ACTION_ACCESS_CONTROL_SET_XPU_DBGAR)

/*
 * Enable Silent Logging feature.
 */
#define TMEL_MSG_UID_ACCESS_CONTROL_ENABLE_SILENT_LOGGING            TMEL_MSG_UID_CREATE(TMEL_MSG_ACCESS_CONTROL,\
                                                                         TMEL_ACTION_ACCESS_CONTROL_ENABLE_SILENT_LOGGING)

/*
 * Enable TME-L Secure IO Read IPC
 */
#define TMEL_MSG_UID_ACCESS_CONTROL_SECURE_IO_READ                   TMEL_MSG_UID_CREATE(TMEL_MSG_ACCESS_CONTROL, \
									TMEL_ACTION_ACCESS_CONTROL_SECURE_IO_READ)

/*
 * Enable TME-L Secure IO Write IPC
 */
#define TMEL_MSG_UID_ACCESS_CONTROL_SECURE_IO_WRITE                   TMEL_MSG_UID_CREATE(TMEL_MSG_ACCESS_CONTROL, \
									TMEL_ACTION_ACCESS_CONTROL_SECURE_IO_WRITE)

/*----------------------------------------------------------------------------
  UID's for TMEL_MSG_FWUP
 *-------------------------------------------------------------------------*/
/*
 * Verify udpate manifest
 */
#define TMEL_MSG_UID_FWUP_VERIFY_UPDATE           TMEL_MSG_UID_CREATE(TMEL_MSG_FWUP,\
                                                              TMEL_ACTION_FWUP_VERIFY_UPDATE)
/*
 * Apply update
 */
#define TMEL_MSG_UID_FWUP_APPLY_UPDATE            TMEL_MSG_UID_CREATE(TMEL_MSG_FWUP,\
                                                        TMEL_ACTION_FWUP_APPLY_UPDATE)

/*----------------------------------------------------------------------------
  UID's for TMEL_MSG_LOG
 *-------------------------------------------------------------------------*/
/*
 * Set TMEL Log Config
 */
#define TMEL_MSG_UID_LOG_SET_CONFIG    TMEL_MSG_UID_CREATE(TMEL_MSG_LOG,\
                                             TMEL_ACTION_LOG_SET_CONFIG)

/*
 * Get TMEL Log Config
 */
#define TMEL_MSG_UID_LOG_GET_CONFIG    TMEL_MSG_UID_CREATE(TMEL_MSG_LOG,\
                                             TMEL_ACTION_LOG_GET_CONFIG)

/*
 * Get TMEL Log
 */
#define TMEL_MSG_UID_LOG_GET           TMEL_MSG_UID_CREATE(TMEL_MSG_LOG,\
                                             TMEL_ACTION_LOG_GET)

/*
 * Get Q6 err Log
 */
#define TMEL_MSG_UID_Q6LOG_GET           TMEL_MSG_UID_CREATE(TMEL_MSG_LOG,\
                                             TMEL_ACTION_Q6LOG_GET)

/*----------------------------------------------------------------------------
  UID's for TMEL_MSG_FEATURE_LICENSE
 *-------------------------------------------------------------------------*/
/*
 * Get Feature License Status
 */
#define TMEL_MSG_UID_FEATURE_LICENSE_STATUS_BTSS             TMEL_MSG_UID_CREATE(TMEL_MSG_FEATURE_LICENSE,\
                                                                TMEL_ACTION_FEATURE_LICENSE_BTSS)

/*----------------------------------------------------------------------------
  UID's for TMEL_MSG_QWES
 *-------------------------------------------------------------------------*/
/*
 * Initialize Device Attestation
 */
#define TMEL_MSG_UID_QWES_INIT_ATTESTATION             TMEL_MSG_UID_CREATE(TMEL_MSG_QWES,\
                                                                TMEL_ACTION_QWES_INIT_ATTESTATION)

/*
 * Perform Device Attestation
 */
#define TMEL_MSG_UID_QWES_DEVICE_ATTESTATION              TMEL_MSG_UID_CREATE(TMEL_MSG_QWES,\
                                                                TMEL_ACTION_QWES_DEVICE_ATTESTATION)

/*
 * Perform Secure Device Provisioning using QWES
 */
#define TMEL_MSG_UID_QWES_DEVICE_PROVISIONING              TMEL_MSG_UID_CREATE(TMEL_MSG_QWES,\
                                                                TMEL_ACTION_QWES_DEVICE_PROVISIONING)

/*
 * Perform Install License
 */
#define TMEL_MSG_UID_QWES_LICENSING_INSTALL              TMEL_MSG_UID_CREATE(TMEL_MSG_QWES,\
                                                                TMEL_ACTION_QWES_LICENSING_INSTALL)

/*
 * Perform Check
 */
#define TMEL_MSG_UID_QWES_LICENSING_CHECK              TMEL_MSG_UID_CREATE(TMEL_MSG_QWES,\
                                                                TMEL_ACTION_QWES_LICENSING_CHECK)

/*
 * Perform Enforce HW Features
 */
#define TMEL_MSG_UID_QWES_LICENSING_ENFORCEHWFEATURES             TMEL_MSG_UID_CREATE(TMEL_MSG_QWES,\
                                                                TMEL_ACTION_QWES_LICENSING_ENFORCEHWFEATURES)

/*
 * Perform Get TTime Request for cloud
 */
#define TMEL_MSG_UID_QWES_TTIME_CLOUD_REQUEST              TMEL_MSG_UID_CREATE(TMEL_MSG_QWES,\
                                                                TMEL_ACTION_QWES_TTIME_CLOUD_REQUEST)

/*
 * Perform Set TTime from cloud response packet
 */
#define TMEL_MSG_UID_QWES_TTIME_SET            TMEL_MSG_UID_CREATE(TMEL_MSG_QWES,\
                                                         TMEL_ACTION_QWES_TTIME_SET)

/*
 * Get the list of to be deleted licenses
 */
#define TMEL_MSG_UID_QWES_LICENSING_TBDLICENSES		TMEL_MSG_UID_CREATE(TMEL_MSG_QWES,\
							TMEL_ACTION_QWES_LICENSING_TBDLICENSES)

/*----------------------------------------------------------------------------
  UID's for TMEL_MSG_CDUMP
 *-------------------------------------------------------------------------*/
/*
 * Save Crashdump buffer address sent by TZ
 */
#define TMEL_MSG_UID_CDUMP_SAVE_ADDR                   TMEL_MSG_UID_CREATE(TMEL_MSG_CDUMP,\
                                                                TMEL_ACTION_CDUMP_SAVE_DUMP_ADDR)

/*----------------------------------------------------------------------------
  UID's for TMEL_MSG_SCP
 *-------------------------------------------------------------------------*/
/*
 * SCP, SCP03 Command APDU Encryption/ MAC
*/
#define TMEL_MSG_UID_SCP_SCP03_TMEGETRDSCAPDU                TMEL_MSG_UID_CREATE(TMEL_MSG_SCP,\
                                                                TMEL_ACTION_SCP_SCP03_RDSCAPDU)

/*
 * SCP, SCP03 Response APDU Decrypt/ Authenticate
*/
#define TMEL_MSG_UID_SCP_SCP03_TMEGETRDSRAPDU                TMEL_MSG_UID_CREATE(TMEL_MSG_SCP,\
                                                                TMEL_ACTION_SCP_SCP03_RDSRAPDU)

/*
 * SCP, SCP11a Initiate Session Request
 */
#define TMEL_MSG_UID_SCP_SCP11A_INITIATE             TMEL_MSG_UID_CREATE(TMEL_MSG_SCP,\
                                                                TMEL_ACTION_SCP_SCP11A_INITIATE)
/*
 * SCP, SCP11a Establish Secure Channel Request
 */
#define TMEL_MSG_UID_SCP_SCP11A_ESTABLISH_SESSION            TMEL_MSG_UID_CREATE(TMEL_MSG_SCP,\
                                                                        TMEL_ACTION_SCP_SCP11A_ESTABLISH_SESSION)

/*-------------------------------------------------------------------------
  UID's for TMEL_MSG_UWB_KD
 *-------------------------------------------------------------------------*/
/*
 * TMESaltedHash; helper function for UWB CCC Key Derivation
*/
#define TMEL_MSG_UID_UWB_CCC_TMESALTEDHASH                TMEL_MSG_UID_CREATE(TMEL_MSG_UWB_KD,\
                                                                TMEL_ACTION_UWB_CCC_TMESALTEDHASH)

/*
 * TMEUAD; helper function for UWB CCC Key Derivation
*/
#define TMEL_MSG_UID_UWB_CCC_TMEUAD                TMEL_MSG_UID_CREATE(TMEL_MSG_UWB_KD,\
                                                                TMEL_ACTION_UWB_CCC_TMEUAD)

/*
 * TMEmUPSK1; helper function for UWB CCC Key Derivation
*/
#define TMEL_MSG_UID_UWB_CCC_TMEMUPSK1                TMEL_MSG_UID_CREATE(TMEL_MSG_UWB_KD,\
                                                                TMEL_ACTION_UWB_CCC_TMEMUPSK1)

/*
 * TMEdURSKdUDSK; helper function for UWB CCC Key Derivation
*/
#define TMEL_MSG_UID_UWB_CCC_TMEDURSKDUDSK                TMEL_MSG_UID_CREATE(TMEL_MSG_UWB_KD,\
                                                                TMEL_ACTION_UWB_CCC_TMEDURSKDUDSK)

/*
 * TMEImportSecSessionKey; helper function for UWB FiRa Key Derivation
*/
#define TMEL_MSG_UID_UWB_FIRA_TMEImportSecSessionKey                       TMEL_MSG_UID_CREATE(TMEL_MSG_UWB_KD,\
                                                                              TMEL_ACTION_UWB_FIRA_TMEImportSecSessionKey)

/*
 * TMEDerivedConfigDigest; helper function for UWB FiRa Key Derivation
*/
#define TMEL_MSG_UID_UWB_FIRA_TMEDERIVEDCONFIGDIGEST                   TMEL_MSG_UID_CREATE(TMEL_MSG_UWB_KD,\
                                                                              TMEL_ACTION_UWB_FIRA_TMEDERIVEDCONFIGDIGEST)

/*
 * TMEsecDataPrivacyKey; helper function for UWB FiRa Key Derivation
*/
#define TMEL_MSG_UID_UWB_FIRA_TMESECDATAPRIVACYKEY                     TMEL_MSG_UID_CREATE(TMEL_MSG_UWB_KD,\
                                                                              TMEL_ACTION_UWB_FIRA_TMESECDATAPRIVACYKEY)

/*
 * TMEsecMaskingKey; helper function for UWB FiRa Key Derivation
*/
#define TMEL_MSG_UID_UWB_FIRA_TMESECMASKINGKEY                         TMEL_MSG_UID_CREATE(TMEL_MSG_UWB_KD,\
                                                                              TMEL_ACTION_UWB_FIRA_TMESECMASKINGKEY)

/*
 * TMEsecDataProtectionKey; helper function for UWB FiRa Key Derivation
*/
#define TMEL_MSG_UID_UWB_FIRA_TMESECDATAPROTECTIONKEY                  TMEL_MSG_UID_CREATE(TMEL_MSG_UWB_KD,\
                                                                              TMEL_ACTION_UWB_FIRA_TMESECDATAPROTECTIONKEY)


/*
 * TMEphyStsIndex; helper function for UWB FiRa Key Derivation
*/
#define TMEL_MSG_UID_UWB_FIRA_TMEPHYSTSINDEX                           TMEL_MSG_UID_CREATE(TMEL_MSG_UWB_KD,\
                                                                              TMEL_ACTION_UWB_FIRA_TMEPHYSTSINDEX)

/*
 * TMEsecDerivedAuthentication; helper function for UWB FiRa Key Derivation
*/
#define TMEL_MSG_UID_UWB_FIRA_TMESECDERIVEDAUTHENTICATION              TMEL_MSG_UID_CREATE(TMEL_MSG_UWB_KD,\
                                                                              TMEL_ACTION_UWB_FIRA_TMESECDERIVEDAUTHENTICATION)

/*-------------------------------------------------------------------------
  UID's for TMEL_MSG_TIMETEST
 *-------------------------------------------------------------------------*/

/*
 * Update Time Test data buffer and configure Watch Points
*/
#define TMEL_MSG_UID_TIMETEST_CONFIG_TTDATA                                        TMEL_MSG_UID_CREATE(TMEL_MSG_TIMETEST,\
                                                                                   TMEL_ACTION_TIMETEST_CONFIG_TTDATA)

#endif /* TMELCOM_MESSAGE_UIDS_H */
