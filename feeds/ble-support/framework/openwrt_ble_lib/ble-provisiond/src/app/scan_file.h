/**
 * @file scan_file.h
 * @brief Scan results file writer.
 *
 * Writes scan results to /var/ble_scan/ directory as JSON files.
 * Filename format: <filter>_<timestamp>.json
 *
 * Supports:
 *   - max_records: 0=unlimited, N=stop/rotate after N records
 *   - on_limit: "rotate" (close old file, open new) or "stop" (stop writing)
 */
#ifndef SCAN_FILE_H
#define SCAN_FILE_H

#include <stdio.h>
#include "scan_filter.h"

#define SCAN_FILE_DIR   "/var/ble_scan"

typedef enum {
    SCAN_LIMIT_ROTATE = 0,  /* Close old file, open new one */
    SCAN_LIMIT_STOP,        /* Stop writing (file stays open but no more entries) */
} scan_limit_action_t;

typedef struct {
    FILE *fp;
    char filter_name[32];
    uint32_t max_records;       /* 0 = unlimited */
    uint32_t record_count;
    scan_limit_action_t on_limit;
    bool stopped;               /* true if on_limit=stop and limit reached */
} scan_file_ctx_t;

/**
 * Open a scan result file for writing.
 * Creates /var/ble_scan/ directory if it doesn't exist.
 *
 * @param ctx          Output context (caller owns).
 * @param filter_name  Filter type string (e.g. "all", "ibeacon", "beacon")
 * @param max_records  Max records per file (0 = unlimited).
 * @param on_limit     Action when limit reached.
 * @return 0 on success, negative on error.
 */
int scan_file_open(scan_file_ctx_t *ctx, const char *filter_name,
                   uint32_t max_records, scan_limit_action_t on_limit);

/**
 * Write a single scan result to the file as JSON.
 * Respects max_records — may rotate or stop.
 * @return 0 on success, 1 if limit reached (stop mode), negative on error.
 */
int scan_file_write_result(scan_file_ctx_t *ctx, const scan_result_ext_t *result);

/**
 * Close the scan file (writes JSON array end bracket).
 */
void scan_file_close(scan_file_ctx_t *ctx);

/**
 * Parse on_limit string ("rotate" or "stop") to enum.
 */
scan_limit_action_t scan_file_parse_limit_action(const char *str);

#endif /* SCAN_FILE_H */
