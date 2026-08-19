/**
 * @file scan_file.c
 * @brief Scan results file writer — JSON output to /var/ble_scan/.
 *
 * Supports max_records limit with rotate or stop behavior.
 */

#define _GNU_SOURCE

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <syslog.h>

#include "scan_file.h"

/* ── Helpers ── */

static int ensure_dir(void)
{
    struct stat st;
    if (stat(SCAN_FILE_DIR, &st) != 0) {
        if (mkdir(SCAN_FILE_DIR, 0755) != 0) {
            syslog(LOG_ERR, "scan_file: cannot create %s: %s",
                   SCAN_FILE_DIR, strerror(errno));
            return -1;
        }
    }
    return 0;
}

static FILE *open_new_file(const char *filter_name)
{
    if (ensure_dir() != 0) return NULL;

    time_t now = time(NULL);
    struct tm *tm = localtime(&now);
    char filename[256];
    snprintf(filename, sizeof(filename),
             "%s/%s_%04d%02d%02d_%02d%02d%02d.json",
             SCAN_FILE_DIR,
             filter_name ? filter_name : "all",
             tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday,
             tm->tm_hour, tm->tm_min, tm->tm_sec);

    FILE *fp = fopen(filename, "w");
    if (!fp) {
        syslog(LOG_ERR, "scan_file: cannot open %s: %s",
               filename, strerror(errno));
        return NULL;
    }

    fprintf(fp, "[\n");
    syslog(LOG_INFO, "scan_file: writing to %s", filename);
    return fp;
}

static void close_file(FILE *fp)
{
    if (!fp) return;
    fprintf(fp, "\n]\n");
    fclose(fp);
}

/* ── Public API ── */

scan_limit_action_t scan_file_parse_limit_action(const char *str)
{
    if (str && strcmp(str, "stop") == 0)
        return SCAN_LIMIT_STOP;
    return SCAN_LIMIT_ROTATE;  /* default */
}

int scan_file_open(scan_file_ctx_t *ctx, const char *filter_name,
                   uint32_t max_records, scan_limit_action_t on_limit)
{
    if (!ctx) return -1;
    memset(ctx, 0, sizeof(*ctx));

    strncpy(ctx->filter_name, filter_name ? filter_name : "all",
            sizeof(ctx->filter_name) - 1);
    ctx->max_records = max_records;
    ctx->on_limit = on_limit;
    ctx->record_count = 0;
    ctx->stopped = false;

    ctx->fp = open_new_file(ctx->filter_name);
    if (!ctx->fp) return -1;

    syslog(LOG_INFO, "scan_file: opened (filter=%s max=%u on_limit=%s)",
           ctx->filter_name, max_records,
           on_limit == SCAN_LIMIT_ROTATE ? "rotate" : "stop");
    return 0;
}

int scan_file_write_result(scan_file_ctx_t *ctx, const scan_result_ext_t *result)
{
    if (!ctx || !result) return -1;

    /* Check if writing is stopped */
    if (ctx->stopped) return 1;

    /* Check limit */
    if (ctx->max_records > 0 && ctx->record_count >= ctx->max_records) {
        if (ctx->on_limit == SCAN_LIMIT_STOP) {
            if (!ctx->stopped) {
                syslog(LOG_INFO, "scan_file: max_records (%u) reached, stopping",
                       ctx->max_records);
                ctx->stopped = true;
            }
            return 1;
        } else {
            /* Rotate: close old file, open new one */
            syslog(LOG_INFO, "scan_file: max_records (%u) reached, rotating",
                   ctx->max_records);
            close_file(ctx->fp);
            ctx->fp = open_new_file(ctx->filter_name);
            ctx->record_count = 0;
            if (!ctx->fp) {
                ctx->stopped = true;
                return -1;
            }
        }
    }

    if (!ctx->fp) return -1;

    /* Write comma separator (except first entry) */
    if (ctx->record_count > 0)
        fprintf(ctx->fp, ",\n");

    fprintf(ctx->fp, "  {\n");
    fprintf(ctx->fp, "    \"timestamp\": \"%s\",\n", result->timestamp);
    fprintf(ctx->fp, "    \"address\": \"%s\",\n", result->address);
    fprintf(ctx->fp, "    \"address_type\": \"%s\",\n", result->address_type);
    fprintf(ctx->fp, "    \"rssi\": %d,\n", result->rssi);
    fprintf(ctx->fp, "    \"name\": \"%s\",\n", result->name);
    fprintf(ctx->fp, "    \"connectable\": %s,\n",
            result->connectable ? "true" : "false");

    /* Raw adv data as hex string */
    fprintf(ctx->fp, "    \"adv_data\": \"");
    for (int i = 0; i < result->adv_data_len; i++)
        fprintf(ctx->fp, "%02x", result->adv_data[i]);
    fprintf(ctx->fp, "\",\n");
    fprintf(ctx->fp, "    \"adv_data_len\": %d,\n", result->adv_data_len);

    /* Beacon type */
    const char *type_str = "unknown";
    switch (result->beacon_type) {
    case BEACON_TYPE_IBEACON:   type_str = "ibeacon"; break;
    case BEACON_TYPE_EDDYSTONE: type_str = "eddystone"; break;
    case BEACON_TYPE_ALTBEACON: type_str = "altbeacon"; break;
    default: break;
    }
    fprintf(ctx->fp, "    \"beacon_type\": \"%s\"", type_str);

    /* Beacon-specific fields */
    if (result->beacon_type == BEACON_TYPE_IBEACON) {
        fprintf(ctx->fp, ",\n    \"ibeacon\": {\n");
        fprintf(ctx->fp, "      \"uuid\": \"%s\",\n", result->ibeacon.uuid_str);
        fprintf(ctx->fp, "      \"major\": %u,\n", result->ibeacon.major);
        fprintf(ctx->fp, "      \"minor\": %u,\n", result->ibeacon.minor);
        fprintf(ctx->fp, "      \"tx_power\": %d\n", result->ibeacon.tx_power);
        fprintf(ctx->fp, "    }");
    } else if (result->beacon_type == BEACON_TYPE_EDDYSTONE) {
        fprintf(ctx->fp, ",\n    \"eddystone\": {\n");
        fprintf(ctx->fp, "      \"frame_type\": \"0x%02x\",\n",
                result->eddystone.frame_type);
        fprintf(ctx->fp, "      \"tx_power\": %d", result->eddystone.tx_power);
        if (result->eddystone.frame_type == 0x00) {
            fprintf(ctx->fp, ",\n      \"namespace\": \"");
            for (int i = 0; i < 10; i++)
                fprintf(ctx->fp, "%02x", result->eddystone.namespace_id[i]);
            fprintf(ctx->fp, "\",\n      \"instance\": \"");
            for (int i = 0; i < 6; i++)
                fprintf(ctx->fp, "%02x", result->eddystone.instance_id[i]);
            fprintf(ctx->fp, "\"");
        } else if (result->eddystone.frame_type == 0x10 &&
                   result->eddystone.url[0]) {
            fprintf(ctx->fp, ",\n      \"url\": \"%s\"", result->eddystone.url);
        }
        fprintf(ctx->fp, "\n    }");
    } else if (result->beacon_type == BEACON_TYPE_ALTBEACON) {
        fprintf(ctx->fp, ",\n    \"altbeacon\": {\n");
        fprintf(ctx->fp, "      \"beacon_id\": \"");
        for (int i = 0; i < 20; i++)
            fprintf(ctx->fp, "%02x", result->altbeacon.beacon_id[i]);
        fprintf(ctx->fp, "\",\n      \"ref_rssi\": %d\n",
                result->altbeacon.ref_rssi);
        fprintf(ctx->fp, "    }");
    }

    fprintf(ctx->fp, "\n  }");
    fflush(ctx->fp);

    ctx->record_count++;
    return 0;
}

void scan_file_close(scan_file_ctx_t *ctx)
{
    if (!ctx) return;
    if (ctx->fp) {
        close_file(ctx->fp);
        ctx->fp = NULL;
    }
    syslog(LOG_INFO, "scan_file: closed (records=%u)", ctx->record_count);
}
