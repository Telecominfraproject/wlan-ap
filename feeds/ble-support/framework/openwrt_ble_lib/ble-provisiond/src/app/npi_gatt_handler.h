/* SPDX-License-Identifier: BSD-3-Clause */
/**
 * @file npi_gatt_handler.h
 * @brief NPI GATT Server bridge for the TI CC2652R1 (multi_role firmware).
 *
 * On the TI NPI transport the GATT server runs on the MCU. This module receives
 * NPI GATT events (phone writes/reads) forwarded from the MCU, routes them into
 * the SHARED, transport-agnostic v2 logic (gatt_v2*.c — the very same code the
 * BlueZ path uses), and sends NPI commands back for read responses and
 * notifications.
 *
 * Only the uCentral Provisioning v2 service is supported. The legacy 16-bit
 * FE00/FE10 services have been retired on both transports.
 *
 * ── Characteristic addressing ──
 * The bridge carries the COMPLETE 16-byte characteristic UUID in every
 * CHAR_WRITE / CHAR_READ / NOTIFY / READ_RSP frame — not a 16-bit alias and not
 * an ATT handle index. This costs 16 bytes per frame but keeps the protocol
 * UUID-explicit and free of any fixed-table assumption in the firmware.
 *
 * The 16 bytes are in BLE wire order, i.e. LITTLE-ENDIAN (reversed relative to
 * the printed UUID string), which is how the TI attribute table stores them. So
 * `e063c3a2-0748-40be-935e-01ebdeafa814` travels as
 * `14 a8 af de eb 01 5e 93 be 40 48 07 a2 c3 63 e0`.
 *
 * NOTE: this widened layout requires the matching multi_role firmware update
 * (128-bit attribute table + 16-byte UUID reporting). Frames in the old 16-bit
 * format are rejected with an explicit log line so the mismatch is obvious.
 */
#ifndef NPI_GATT_HANDLER_H
#define NPI_GATT_HANDLER_H

#include <stdint.h>
#include <stdbool.h>

/* NPI GATT Event types from MCU */
#define NPI_GATT_EVT_CHAR_WRITE     0x01  /* Client wrote to a characteristic */
#define NPI_GATT_EVT_CHAR_READ      0x02  /* Client requesting read value */
#define NPI_GATT_EVT_CONNECTED      0x03  /* BLE client connected */
#define NPI_GATT_EVT_DISCONNECTED   0x04  /* BLE client disconnected */
#define NPI_GATT_EVT_MTU_EXCHANGE   0x05  /* MTU negotiated */
#define NPI_GATT_EVT_SCAN_RESULT    0x06  /* reserved: Observer scan report */
#define NPI_GATT_EVT_READY          0x07  /* firmware booted & advertising ready */
#define NPI_GATT_EVT_BDADDR         0x08  /* firmware's actual BD address (6B LE) */
#define NPI_GATT_EVT_NOTIFY_STATUS  0x09  /* outcome of a NOTIFY: uuid(16) status(1) cccd(1) */
#define NPI_GATT_EVT_RX_ERROR       0x0A  /* firmware RX errors: badfcs(2) oversize(2) */
#define NPI_GATT_EVT_CMD_STATUS     0x0B  /* outcome of a command: cmd1(1) status(1) */

/* NPI GATT commands TO MCU */
#define NPI_GATT_CMD_NOTIFY         0x10  /* Send GATT notification */
#define NPI_GATT_CMD_READ_RSP       0x11  /* Read response value */
#define NPI_GATT_CMD_SET_NAME       0x20  /* Set GAP device name (payload: name) */
#define NPI_GATT_CMD_SET_BDADDR     0x21  /* Set BD address (payload: 6B LE) */
#define NPI_GATT_CMD_ADV_START      0x22  /* Start connectable advertising */
#define NPI_GATT_CMD_ADV_STOP       0x23  /* Stop advertising */
#define NPI_GATT_CMD_QUERY_READY    0x28  /* Ask firmware to (re)emit READY */
#define NPI_GATT_CMD_DISCONNECT     0x29  /* Terminate any active GATT link */
#define NPI_GATT_CMD_REQ_MTU        0x2A  /* Ask the peer to raise the ATT MTU:
                                           * conn(2) mtu(2); mtu 0 = chip max */
#define NPI_GATT_CMD_SET_ADV_MFR    0x2B  /* Advertisement manufacturer data
                                           * (§19.6): company(2, LE) data(N) */

/*
 * Pseudo-command the firmware reports its feature level under, inside an
 * EVT_CMD_STATUS frame (cmd=0xFD, status=level). Not a command we send.
 *
 * It exists to answer one question from the log alone: is the chip running the
 * firmware that was just built? Inferring that from side effects cost several
 * bring-up rounds. Keep NPI_GATT_FW_LEVEL_MIN equal to the lowest firmware level
 * this daemon behaves correctly with, and bump both together.
 */
#define NPI_GATT_FW_LEVEL           0xFD
/*
 * 2 = A1/A4 reads deferred to the host (§19.9.2). Level 1 answers them from the
 * chip's value cache, which returns the previous fragment.
 */
#define NPI_GATT_FW_LEVEL_MIN       2

/*
 * Pseudo-commands carrying the connection interval currently in force, in units
 * of 1.25 ms (so 24 = 30 ms), high byte first then low byte.
 *
 * This is the only window we have onto that number: on this transport the CC2652
 * is the controller, so there is no HCI on the AP and no btmon equivalent. And it
 * is the number that decides how fast provisioning feels — every ATT round trip
 * costs at least one interval, and a deferred A1/A4 read costs two.
 *
 * Two bytes because the values that matter overflow one: the chip's stock
 * connection-update defaults ask for 400-800 units (500-1000 ms).
 */
#define NPI_GATT_CONN_INTERVAL_HI   0xFB
#define NPI_GATT_CONN_INTERVAL      0xFC

/**
 * Interval above which provisioning is visibly degraded, in 1.25 ms units.
 *
 * 40 units = 50 ms. A 7-fragment A4 document costs two intervals per fragment
 * plus the app's own turnaround, so at 50 ms it is well under a second and at
 * 400 units (500 ms) it does not finish at all.
 */
#define NPI_GATT_CONN_INTERVAL_SLOW 40

/** Length of the UUID field carried in bridge frames. */
#define NPI_GATT_UUID_LEN           16

/**
 * Largest value the firmware will cache for one characteristic
 * (gatt_provision.h PROV_VAL_MAX). Values are clamped here rather than at the
 * ATT MTU: the firmware knows the real per-connection MTU and the TI stack
 * already limits a notification to MTU-3 when it reads the value out, whereas
 * this side would be guessing. Clamping on a guess used to corrupt the
 * firmware's readable cache, because it caches whatever a notify carried.
 */
#define NPI_GATT_VALUE_MAX          256

/**
 * @brief NPI GATT event received from the MCU.
 */
typedef struct {
	uint8_t  event_type;
	uint16_t conn_handle;
	uint8_t  uuid[NPI_GATT_UUID_LEN];  /**< characteristic UUID, BLE (LE) order */
	uint16_t mtu;
	uint8_t  data[512];
	uint16_t data_len;
} npi_gatt_event_t;

/**
 * Initialize the NPI GATT bridge (called once at startup).
 *
 * Registers the libble bridge callback and installs this transport's notify
 * hook into the shared v2 core, so A2/A7 updates are pushed to the phone as NPI
 * notifications.
 */
int npi_gatt_handler_init(void);

/** Deinitialize. */
void npi_gatt_handler_deinit(void);

/**
 * Process a decoded GATT event from the MCU.
 *
 * @param evt  Parsed GATT event.
 * @return 0 on success, negative on error.
 */
int npi_gatt_handle_event(const npi_gatt_event_t *evt);

/**
 * Send a GATT notification to the phone via the MCU.
 *
 * @param conn_handle  Connection handle.
 * @param uuid         16-byte characteristic UUID (BLE/LE order).
 * @param data         Notification payload.
 * @param data_len     Payload length.
 * @return 0 on success, negative on error.
 */
int npi_gatt_send_notify(uint16_t conn_handle,
						 const uint8_t uuid[NPI_GATT_UUID_LEN],
						 const uint8_t *data, uint16_t data_len);

/**
 * Answer a pending characteristic read.
 *
 * @param conn_handle  Connection handle.
 * @param uuid         16-byte characteristic UUID (BLE/LE order).
 * @param data         Response payload.
 * @param data_len     Payload length.
 * @return 0 on success, negative on error.
 */
int npi_gatt_send_read_rsp(uint16_t conn_handle,
						   const uint8_t uuid[NPI_GATT_UUID_LEN],
						   const uint8_t *data, uint16_t data_len);

/**
 * Push the provisioning setup to the multi_role firmware: set the GAP device
 * name (from UCI) and the BD address, then start connectable advertising.
 *
 * @param name   NUL-terminated device name (e.g. "OpenWrt-BLE").
 * @param bdaddr 6-byte BD address, little-endian (as the chip expects).
 * @return 0 on success.
 */
int npi_gatt_send_provision_setup(const char *name, const uint8_t bdaddr[6]);

/**
 * Store the provisioning params (device name from UCI, BD address from eth0).
 * They are pushed to the firmware when it reports NPI_GATT_EVT_READY, which
 * guarantees the name update lands after the firmware's advertising is up.
 *
 * @param name    NUL-terminated device name.
 * @param bdaddr  6-byte BD address little-endian, or NULL if unknown.
 */
void npi_gatt_set_provision_params(const char *name, const uint8_t *bdaddr);

/**
 * Mirror the UCI ble.gatt_server.enabled flag into the handler. When disabled,
 * the provision setup sends ADV_STOP instead of ADV_START so the TI firmware
 * stops advertising the provisioning service — matching the BlueZ path, where
 * services are only advertised when enabled. Call before npi_gatt_query_ready()
 * / the first READY so the correct ADV state is applied.
 *
 * @param enabled true if provisioning GATT should advertise.
 */
void npi_gatt_set_gatt_enabled(bool enabled);

/**
 * Ask the firmware to (re)emit NPI_GATT_EVT_READY by sending NPI_CMD_QUERY_READY.
 * Used at daemon startup (including after a restart): the firmware only emits
 * READY on its own until first provisioned, so a restarted daemon would never
 * see it. This actively re-triggers the READY→provision handshake so the device
 * name is re-pushed. Also arms the handler to send the setup on the next READY.
 *
 * @return 0 on success.
 */
int npi_gatt_query_ready(void);

#endif /* NPI_GATT_HANDLER_H */
