/**
 * @file log.h
 * @brief Internal logging macros for libble.
 *
 * Uses syslog on OpenWrt, stderr for debug builds.
 * NOT installed — internal use only.
 *
 * Uses BLE_LOG_ prefix to avoid name conflicts with syslog.h
 * constants (LOG_ERR, LOG_WARNING, LOG_INFO, LOG_DEBUG).
 */
#ifndef LIBBLE_LOG_H
#define LIBBLE_LOG_H

#include <syslog.h>
#include <stdio.h>

#ifdef BLE_DEBUG
  #define BLE_LOG_ERR(fmt, ...)   fprintf(stderr, "[ERR]  libble: " fmt "\n", ##__VA_ARGS__)
  #define BLE_LOG_WARN(fmt, ...)  fprintf(stderr, "[WARN] libble: " fmt "\n", ##__VA_ARGS__)
  #define BLE_LOG_INFO(fmt, ...)  fprintf(stderr, "[INFO] libble: " fmt "\n", ##__VA_ARGS__)
  #define BLE_LOG_DBG(fmt, ...)   fprintf(stderr, "[DBG]  libble: " fmt "\n", ##__VA_ARGS__)
#else
  #define BLE_LOG_ERR(fmt, ...)   syslog(LOG_ERR,     "libble: " fmt, ##__VA_ARGS__)
  #define BLE_LOG_WARN(fmt, ...)  syslog(LOG_WARNING, "libble: " fmt, ##__VA_ARGS__)
  #define BLE_LOG_INFO(fmt, ...)  syslog(LOG_INFO,    "libble: " fmt, ##__VA_ARGS__)
  #define BLE_LOG_DBG(fmt, ...)   syslog(LOG_DEBUG,   "libble: " fmt, ##__VA_ARGS__)
#endif

#endif /* LIBBLE_LOG_H */
