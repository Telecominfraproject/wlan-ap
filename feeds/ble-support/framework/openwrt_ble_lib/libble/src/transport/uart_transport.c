/**
 * @file uart_transport.c
 * @brief UART HCI transport plugin for libble.
 *
 * Direct UART communication with BLE controllers using termios.
 * Supports raw HCI packet framing and vendor-specific commands.
 */

#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <termios.h>

#include "transport_plugin.h"
#include "../log.h"

#define HCI_PKT_COMMAND     0x01
#define HCI_PKT_EVENT       0x04
#define HCI_EVT_CMD_COMPLETE 0x0E
#define HCI_EVT_LE_META     0x3E
#define UART_RX_BUF_SIZE    1024
#define UART_DEFAULT_BAUD   B115200

static struct {
    int fd;
    uint8_t rx_buf[UART_RX_BUF_SIZE];
    int rx_len;
    transport_event_cb_t event_cb;
    void *event_cb_ctx;
    bool initialized;
    char device_path[128];
} uart_state;

static int uart_configure_port(int fd, speed_t baud)
{
    struct termios tty;
    if (tcgetattr(fd, &tty) != 0) return -errno;

    cfsetospeed(&tty, baud);
    cfsetispeed(&tty, baud);
    tty.c_cflag &= ~(PARENB | CSTOPB | CSIZE | CRTSCTS);
    tty.c_cflag |= CS8 | CREAD | CLOCAL;
    tty.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
    tty.c_iflag &= ~(IXON | IXOFF | IXANY | IGNBRK | BRKINT |
                      PARMRK | ISTRIP | INLCR | IGNCR | ICRNL);
    tty.c_oflag &= ~OPOST;
    tty.c_cc[VMIN] = 0;
    tty.c_cc[VTIME] = 1;

    if (tcsetattr(fd, TCSANOW, &tty) != 0) return -errno;
    return 0;
}

static void uart_parse_rx(void)
{
    int offset = 0;
    while (offset < uart_state.rx_len) {
        if (uart_state.rx_buf[offset] != HCI_PKT_EVENT) {
            offset++;
            continue;
        }
        if (uart_state.rx_len - offset < 3) break;

        uint8_t param_len = uart_state.rx_buf[offset + 2];
        int pkt_total = 1 + 1 + 1 + param_len;
        if (uart_state.rx_len - offset < pkt_total) break;

        uint8_t event_code = uart_state.rx_buf[offset + 1];
        uint8_t *params = &uart_state.rx_buf[offset + 3];

        /* Parse LE Meta for scan results */
        if (event_code == HCI_EVT_LE_META && param_len >= 2 &&
            params[0] == 0x02) {
            ble_event_t event;
            memset(&event, 0, sizeof(event));
            event.type = BLE_EVT_SCAN_RESULT;
            if (param_len >= 10) {
                snprintf(event.scan_result.address, BLE_ADDR_STR_LEN,
                    "%02X:%02X:%02X:%02X:%02X:%02X",
                    params[8], params[7], params[6],
                    params[5], params[4], params[3]);
                event.scan_result.rssi = (int8_t)params[param_len - 1];
            }
            if (uart_state.event_cb)
                uart_state.event_cb(&event, uart_state.event_cb_ctx);
        }
        offset += pkt_total;
    }
    if (offset > 0) {
        int remaining = uart_state.rx_len - offset;
        if (remaining > 0)
            memmove(uart_state.rx_buf, &uart_state.rx_buf[offset], remaining);
        uart_state.rx_len = remaining;
    }
}

static int uart_init(const char *config)
{
    if (uart_state.initialized) return -EALREADY;
    memset(&uart_state, 0, sizeof(uart_state));
    uart_state.fd = -1;

    const char *device = config ? config : "/dev/ttyUSB0";
    strncpy(uart_state.device_path, device, sizeof(uart_state.device_path) - 1);

    uart_state.fd = open(device, O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (uart_state.fd < 0) {
        BLE_LOG_ERR("Failed to open '%s': %s", device, strerror(errno));
        return -errno;
    }

    int ret = uart_configure_port(uart_state.fd, UART_DEFAULT_BAUD);
    if (ret < 0) { close(uart_state.fd); uart_state.fd = -1; return ret; }

    tcflush(uart_state.fd, TCIOFLUSH);
    uart_state.initialized = true;
    BLE_LOG_INFO("UART transport initialized (device=%s)", device);
    return 0;
}

static int uart_deinit(void)
{
    if (!uart_state.initialized) return -EINVAL;
    if (uart_state.fd >= 0) { close(uart_state.fd); uart_state.fd = -1; }
    uart_state.initialized = false;
    BLE_LOG_INFO("UART transport deinitialized");
    return 0;
}

static int uart_send_hci_cmd(const ble_hci_cmd_t *cmd)
{
    if (!uart_state.initialized || !cmd) return -EINVAL;
    uint8_t pkt[4 + 255];
    int pkt_len = 4 + cmd->param_len;
    pkt[0] = HCI_PKT_COMMAND;
    pkt[1] = cmd->opcode & 0xFF;
    pkt[2] = (cmd->opcode >> 8) & 0xFF;
    pkt[3] = cmd->param_len;
    if (cmd->param_len > 0) memcpy(&pkt[4], cmd->params, cmd->param_len);

    ssize_t written = write(uart_state.fd, pkt, pkt_len);
    if (written < 0) return -errno;
    if (written != pkt_len) return -EIO;
    return 0;
}

static int uart_send_vendor_cmd(uint8_t ogf, uint16_t ocf,
                                const uint8_t *params, uint8_t param_len)
{
    ble_hci_cmd_t cmd;
    memset(&cmd, 0, sizeof(cmd));
    cmd.opcode = ((uint16_t)(ogf & 0x3F) << 10) | (ocf & 0x03FF);
    cmd.param_len = param_len;
    if (params && param_len > 0) memcpy(cmd.params, params, param_len);
    return uart_send_hci_cmd(&cmd);
}

static int uart_register_event_callback(transport_event_cb_t cb, void *ctx)
{
    uart_state.event_cb = cb;
    uart_state.event_cb_ctx = ctx;
    return 0;
}

static int uart_get_fd(void) { return uart_state.fd; }

static int uart_process_events(void)
{
    if (!uart_state.initialized) return -EINVAL;
    int space = UART_RX_BUF_SIZE - uart_state.rx_len;
    if (space <= 0) { uart_state.rx_len = 0; space = UART_RX_BUF_SIZE; }
    ssize_t n = read(uart_state.fd, &uart_state.rx_buf[uart_state.rx_len], space);
    if (n > 0) { uart_state.rx_len += n; uart_parse_rx(); }
    return 0;
}

transport_plugin_t uart_transport_plugin = {
    .name                    = "uart_hci",
    .init                    = uart_init,
    .deinit                  = uart_deinit,
    .send_hci_cmd            = uart_send_hci_cmd,
    .send_vendor_cmd         = uart_send_vendor_cmd,
    .register_event_callback = uart_register_event_callback,
    .get_fd                  = uart_get_fd,
    .process_events          = uart_process_events,
    .active                  = false
};
