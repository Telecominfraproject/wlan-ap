/**
 * @file scan_filter.c
 * @brief BLE scan filter and beacon identification.
 */

#define _GNU_SOURCE

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <syslog.h>

#include "scan_filter.h"

/* ── Beacon Identification ── */

static bool is_ibeacon(const uint8_t *mfr_data, uint8_t len,
                       ibeacon_data_t *out)
{
    /* iBeacon: company=0x004C, data starts with 02 15, then 16B UUID +
     * 2B major + 2B minor + 1B TX power = 23 bytes total */
    if (len < 23) return false;
    if (mfr_data[0] != 0x02 || mfr_data[1] != 0x15) return false;

    if (out) {
        memcpy(out->uuid, &mfr_data[2], 16);
        snprintf(out->uuid_str, sizeof(out->uuid_str),
                 "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
                 mfr_data[2], mfr_data[3], mfr_data[4], mfr_data[5],
                 mfr_data[6], mfr_data[7], mfr_data[8], mfr_data[9],
                 mfr_data[10], mfr_data[11], mfr_data[12], mfr_data[13],
                 mfr_data[14], mfr_data[15], mfr_data[16], mfr_data[17]);
        out->major = (mfr_data[18] << 8) | mfr_data[19];
        out->minor = (mfr_data[20] << 8) | mfr_data[21];
        out->tx_power = (int8_t)mfr_data[22];
    }
    return true;
}

static bool is_altbeacon(const uint8_t *mfr_data, uint8_t len,
                         altbeacon_data_t *out)
{
    /* AltBeacon: data starts with BE AC, then 20B beacon_id +
     * 1B ref_rssi + 1B mfr_reserved = 24 bytes */
    if (len < 24) return false;
    if (mfr_data[0] != 0xBE || mfr_data[1] != 0xAC) return false;

    if (out) {
        memcpy(out->beacon_id, &mfr_data[2], 20);
        out->ref_rssi = (int8_t)mfr_data[22];
        out->mfr_reserved = mfr_data[23];
    }
    return true;
}

/* Note: Eddystone uses ServiceData (UUID 0xFEAA), not ManufacturerData.
 * BlueZ exposes this as "ServiceData" property in PropertiesChanged.
 * For now, we detect Eddystone from the adv_data if available. */
static bool is_eddystone(const uint8_t *svc_data, uint8_t len,
                         eddystone_data_t *out)
{
    /* Eddystone: ServiceData UUID = 0xFEAA, frame starts after UUID */
    if (len < 3) return false;

    if (out) {
        out->frame_type = svc_data[0];
        out->tx_power = (len > 1) ? (int8_t)svc_data[1] : 0;

        if (out->frame_type == 0x00 && len >= 18) {
            /* UID frame: 10B namespace + 6B instance */
            memcpy(out->namespace_id, &svc_data[2], 10);
            memcpy(out->instance_id, &svc_data[12], 6);
        } else if (out->frame_type == 0x10 && len > 2) {
            /* URL frame: scheme + encoded URL */
            const char *schemes[] = {"http://www.", "https://www.", "http://", "https://"};
            uint8_t scheme = svc_data[2];
            int pos = 0;
            if (scheme < 4)
                pos = snprintf(out->url, sizeof(out->url), "%s", schemes[scheme]);
            for (int i = 3; i < len && pos < (int)sizeof(out->url) - 1; i++)
                out->url[pos++] = (char)svc_data[i];
            out->url[pos] = '\0';
        }
    }
    return true;
}

/* ── Public API ── */

beacon_type_t scan_filter_identify(const ble_event_t *event,
                                   scan_result_ext_t *ext)
{
    if (!event || !ext) return BEACON_TYPE_UNKNOWN;

    /* Copy base fields */
    strncpy(ext->address, event->scan_result.address, 17);
    strncpy(ext->address_type, event->scan_result.address_type, 7);
    ext->rssi = event->scan_result.rssi;
    strncpy(ext->name, event->scan_result.name, 63);
    ext->connectable = event->scan_result.connectable;
    memcpy(ext->adv_data, event->scan_result.adv_data,
           event->scan_result.adv_data_len);
    ext->adv_data_len = event->scan_result.adv_data_len;

    /* Timestamp */
    time_t now = time(NULL);
    struct tm *tm = localtime(&now);
    strftime(ext->timestamp, sizeof(ext->timestamp),
             "%Y-%m-%dT%H:%M:%S", tm);

    /* Try to identify beacon type from adv_data (ManufacturerData) */
    ext->beacon_type = BEACON_TYPE_UNKNOWN;

    if (ext->adv_data_len >= 23) {
        if (is_ibeacon(ext->adv_data, ext->adv_data_len, &ext->ibeacon)) {
            ext->beacon_type = BEACON_TYPE_IBEACON;
            return BEACON_TYPE_IBEACON;
        }
        if (is_altbeacon(ext->adv_data, ext->adv_data_len, &ext->altbeacon)) {
            ext->beacon_type = BEACON_TYPE_ALTBEACON;
            return BEACON_TYPE_ALTBEACON;
        }
    }

    /* Eddystone detection from adv_data (if service data was captured) */
    /* Note: full Eddystone detection requires ServiceData from BlueZ */
    if (ext->adv_data_len >= 3) {
        if (is_eddystone(ext->adv_data, ext->adv_data_len, &ext->eddystone)) {
            ext->beacon_type = BEACON_TYPE_EDDYSTONE;
            return BEACON_TYPE_EDDYSTONE;
        }
    }

    return BEACON_TYPE_UNKNOWN;
}

bool scan_filter_match(const scan_result_ext_t *result, uint32_t filter_mask,
                       uint16_t custom_company_id)
{
    if (!result) return false;

    /* No filter — pass everything */
    if (filter_mask == SCAN_FILTER_ALL) return true;

    /* Advertising only (non-beacon) */
    if (filter_mask == SCAN_FILTER_ADVERTISING)
        return (result->beacon_type == BEACON_TYPE_UNKNOWN);

    /* Beacon all — any recognized beacon */
    if (filter_mask & SCAN_FILTER_BEACON_ALL)
        return (result->beacon_type != BEACON_TYPE_UNKNOWN);

    /* Specific beacon types (can be combined) */
    if ((filter_mask & SCAN_FILTER_IBEACON) &&
        result->beacon_type == BEACON_TYPE_IBEACON)
        return true;

    if ((filter_mask & SCAN_FILTER_EDDYSTONE) &&
        result->beacon_type == BEACON_TYPE_EDDYSTONE)
        return true;

    if ((filter_mask & SCAN_FILTER_ALTBEACON) &&
        result->beacon_type == BEACON_TYPE_ALTBEACON)
        return true;

    if (filter_mask & SCAN_FILTER_CUSTOM) {
        /* Custom: match by company ID in adv_data */
        (void)custom_company_id;
        /* For now, pass if there's any manufacturer data */
        return (result->adv_data_len > 0);
    }

    return false;
}

uint32_t scan_filter_parse(const char *filter_str)
{
    if (!filter_str || filter_str[0] == '\0' || strcmp(filter_str, "all") == 0)
        return SCAN_FILTER_ALL;

    if (strcmp(filter_str, "advertising") == 0)
        return SCAN_FILTER_ADVERTISING;

    if (strcmp(filter_str, "beacon") == 0)
        return SCAN_FILTER_BEACON_ALL;

    uint32_t mask = 0;
    char buf[128];
    strncpy(buf, filter_str, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';

    char *tok = strtok(buf, ",");
    while (tok) {
        while (*tok == ' ') tok++;
        if (strcmp(tok, "ibeacon") == 0)       mask |= SCAN_FILTER_IBEACON;
        else if (strcmp(tok, "eddystone") == 0) mask |= SCAN_FILTER_EDDYSTONE;
        else if (strcmp(tok, "altbeacon") == 0) mask |= SCAN_FILTER_ALTBEACON;
        else if (strcmp(tok, "custom") == 0)    mask |= SCAN_FILTER_CUSTOM;
        else if (strcmp(tok, "beacon") == 0)    mask |= SCAN_FILTER_BEACON_ALL;
        tok = strtok(NULL, ",");
    }

    return mask ? mask : SCAN_FILTER_ALL;
}
