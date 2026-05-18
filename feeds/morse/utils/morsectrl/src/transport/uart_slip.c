/*
 * Copyright 2024 Morse Micro
 */

/*
 * Transport layer for communication over UART interface to an embedded device with SLIP framing.
 *
 * Transport frame format:
 *
 *          +-----------------------------------------+-----------+-----------+
 *          |       Command/Response Payload          |   Seq #   |   CRC16   |
 *          +-----------------------------------------+-----------+-----------+
 *
 * * Seq # is used to match command to response. The content is arbitrary and the response will
 *   echoed the value provided in the command.
 * * The command and response payload are opaque to this layer.
 * * CRC16 is a CRC16 calculated over the sequence # and payload. See below for implementation
 *   details.
 *
 * The above frame is then slip encoded before transmission over the UART. On the receive
 * side it is SLIP decoded before the CRC16 is validated and sequence # checked.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "transport.h"
#include "transport_private.h"
#include "../utilities.h"

#include "slip.h"
#include "uart.h"

#define DEFAULT_BAUDRATE            (115200)

#define SEQNUM_LEN                  (4)
#define CRC_LEN                     (2)

static const struct morsectrl_transport_ops uart_slip_ops;

/** @brief Data structure used to represent an instance of this trasport. */
struct morsectrl_uart_slip_transport
{
    struct morsectrl_transport common;
    struct uart_config uart_config;
    struct uart_ctx *uart_ctx;
};

/**
 * @brief Given a pointer to a @ref morsectrl_transport instance, return a reference to the
 *        uart_config field.
 */
static struct uart_config *uart_slip_cfg(struct morsectrl_transport *transport)
{
    struct morsectrl_uart_slip_transport *uart_slip_transport =
        (struct morsectrl_uart_slip_transport *)transport;
    return &uart_slip_transport->uart_config;
}

/**
 * @brief Given a pointer to a @ref morsectrl_transport instance, set the uart_ctx field.
 */
static void uart_slip_ctx_set(struct morsectrl_transport *transport, struct uart_ctx *ctx)
{
    struct morsectrl_uart_slip_transport *uart_slip_transport =
        (struct morsectrl_uart_slip_transport *)transport;
    uart_slip_transport->uart_ctx = ctx;
}

/**
 * @brief Given a pointer to a @ref morsectrl_transport instance, return a reference to the
 *        uart_ctx field.
 */
static struct uart_ctx *uart_slip_ctx(struct morsectrl_transport *transport)
{
    struct morsectrl_uart_slip_transport *uart_slip_transport =
        (struct morsectrl_uart_slip_transport *)transport;
    return uart_slip_transport->uart_ctx;
}

/**
 * @brief Prints an error message if possible.
 *
 * @param transport     Transport to print the error message from.
 * @param error_code    Error code.
 * @param error_msg     Error message.
 */
static void uart_slip_error(int error_code, char *error_msg)
{
    morsectrl_transport_err("UART_SLIP", error_code, error_msg);
}

/**
 * @brief Parse the configuration for the SLIP over UART interface.
 *
 * @param transport     The transport structure.
 * @param debug         Indicates whether debug print statements are enabled.
 * @param iface_opts    String containing the interface to use. May be NULL.
 * @param cfg_opts      Comma separated string with SLIP over UART configuration options.
 * @return              0 on success otherwise relevant error.
 */
static int uart_slip_parse(struct morsectrl_transport **transport,
                           bool debug,
                           const char *iface_opts,
                           const char *cfg_opts)
{
    struct uart_config *config;

    struct morsectrl_uart_slip_transport *uart_slip_transport =
        calloc(1, sizeof(*uart_slip_transport));
    if (!uart_slip_transport)
    {
        mctrl_err("Transport memory allocation failure\n");
        return -ETRANSNOMEM;
    }

    uart_slip_transport->common.debug = debug;
    uart_slip_transport->common.tops = &uart_slip_ops;
    *transport = &uart_slip_transport->common;
    config = uart_slip_cfg(*transport);

    if (cfg_opts == NULL || strlen(cfg_opts) == 0)
    {
        mctrl_err("Must specify the path to the UART file. For example: -c /dev/ttyACM0\n");
        return -ETRANSNOMEM;
    }

    strncpy(config->dev_name, cfg_opts, sizeof(config->dev_name) - 1);
    config->baudrate = DEFAULT_BAUDRATE;

    return 0;
}


/**
 * @brief Initalise an SLIP over UART interface.
 *
 * @note This should be done after parsing the configuration.
 *
 * @param transport Transport structure.
 * @return          0 on success otherwise relevant error.
 */
static int uart_slip_init(struct morsectrl_transport *transport)
{
    struct uart_ctx *ctx;
    struct uart_config *config = uart_slip_cfg(transport);


    srand(time(NULL));

    ctx = uart_init(config);
    if (ctx == NULL)
        return ETRANSERR;
    uart_slip_ctx_set(transport, ctx);

    return ETRANSSUCC;
}

/**
 * @brief De-initalise an FTDI Transport.
 *
 * @param transport The transport structure.
 * @return          0 on success otherwise relevant error.
 */
static int uart_slip_deinit(struct morsectrl_transport *transport)
{
    struct uart_ctx *ctx = uart_slip_ctx(transport);

    uart_slip_ctx_set(transport, NULL);

    return uart_deinit(ctx);
}


/**
 * @brief Allocate @ref morsectrl_transport_buff.
 *
 * @param transport Transport structure.
 * @param size      Size of command and morse headers or raw data.
 * @return          Allocated @ref morsectrl_transport_buff or NULL on failure.
 */
static struct morsectrl_transport_buff *uart_slip_alloc(struct morsectrl_transport *transport,
                                                        size_t size)
{
    struct morsectrl_transport_buff *buff;

    if (!transport)
        return NULL;

    if (size <= 0)
        return NULL;

    buff = calloc(1, sizeof(*buff));
    if (!buff)
        return NULL;

    buff->capacity = size + SEQNUM_LEN + CRC_LEN;
    buff->memblock = calloc(1, buff->capacity);
    if (!buff->memblock)
    {
        free(buff);
        return NULL;
    }
    buff->data = buff->memblock;
    buff->data_len = size;

    return buff;
}

static int uart_slip_tx_char(uint8_t c, void *arg)
{
    int ret;
    struct morsectrl_transport *transport = arg;
    struct uart_ctx *ctx = uart_slip_ctx(transport);


    ret = uart_write(ctx, &c, 1);
    if (ret == 1)
    {
        return 0;
    }
    else
    {
        return -1;
    }
}

/**
 * Static table used for the table_driven implementation.
 */
static const uint16_t crc16_lookup_table[256] = {
    0x0000, 0x1021, 0x2042, 0x3063, 0x4084, 0x50a5, 0x60c6, 0x70e7,
    0x8108, 0x9129, 0xa14a, 0xb16b, 0xc18c, 0xd1ad, 0xe1ce, 0xf1ef,
    0x1231, 0x0210, 0x3273, 0x2252, 0x52b5, 0x4294, 0x72f7, 0x62d6,
    0x9339, 0x8318, 0xb37b, 0xa35a, 0xd3bd, 0xc39c, 0xf3ff, 0xe3de,
    0x2462, 0x3443, 0x0420, 0x1401, 0x64e6, 0x74c7, 0x44a4, 0x5485,
    0xa56a, 0xb54b, 0x8528, 0x9509, 0xe5ee, 0xf5cf, 0xc5ac, 0xd58d,
    0x3653, 0x2672, 0x1611, 0x0630, 0x76d7, 0x66f6, 0x5695, 0x46b4,
    0xb75b, 0xa77a, 0x9719, 0x8738, 0xf7df, 0xe7fe, 0xd79d, 0xc7bc,
    0x48c4, 0x58e5, 0x6886, 0x78a7, 0x0840, 0x1861, 0x2802, 0x3823,
    0xc9cc, 0xd9ed, 0xe98e, 0xf9af, 0x8948, 0x9969, 0xa90a, 0xb92b,
    0x5af5, 0x4ad4, 0x7ab7, 0x6a96, 0x1a71, 0x0a50, 0x3a33, 0x2a12,
    0xdbfd, 0xcbdc, 0xfbbf, 0xeb9e, 0x9b79, 0x8b58, 0xbb3b, 0xab1a,
    0x6ca6, 0x7c87, 0x4ce4, 0x5cc5, 0x2c22, 0x3c03, 0x0c60, 0x1c41,
    0xedae, 0xfd8f, 0xcdec, 0xddcd, 0xad2a, 0xbd0b, 0x8d68, 0x9d49,
    0x7e97, 0x6eb6, 0x5ed5, 0x4ef4, 0x3e13, 0x2e32, 0x1e51, 0x0e70,
    0xff9f, 0xefbe, 0xdfdd, 0xcffc, 0xbf1b, 0xaf3a, 0x9f59, 0x8f78,
    0x9188, 0x81a9, 0xb1ca, 0xa1eb, 0xd10c, 0xc12d, 0xf14e, 0xe16f,
    0x1080, 0x00a1, 0x30c2, 0x20e3, 0x5004, 0x4025, 0x7046, 0x6067,
    0x83b9, 0x9398, 0xa3fb, 0xb3da, 0xc33d, 0xd31c, 0xe37f, 0xf35e,
    0x02b1, 0x1290, 0x22f3, 0x32d2, 0x4235, 0x5214, 0x6277, 0x7256,
    0xb5ea, 0xa5cb, 0x95a8, 0x8589, 0xf56e, 0xe54f, 0xd52c, 0xc50d,
    0x34e2, 0x24c3, 0x14a0, 0x0481, 0x7466, 0x6447, 0x5424, 0x4405,
    0xa7db, 0xb7fa, 0x8799, 0x97b8, 0xe75f, 0xf77e, 0xc71d, 0xd73c,
    0x26d3, 0x36f2, 0x0691, 0x16b0, 0x6657, 0x7676, 0x4615, 0x5634,
    0xd94c, 0xc96d, 0xf90e, 0xe92f, 0x99c8, 0x89e9, 0xb98a, 0xa9ab,
    0x5844, 0x4865, 0x7806, 0x6827, 0x18c0, 0x08e1, 0x3882, 0x28a3,
    0xcb7d, 0xdb5c, 0xeb3f, 0xfb1e, 0x8bf9, 0x9bd8, 0xabbb, 0xbb9a,
    0x4a75, 0x5a54, 0x6a37, 0x7a16, 0x0af1, 0x1ad0, 0x2ab3, 0x3a92,
    0xfd2e, 0xed0f, 0xdd6c, 0xcd4d, 0xbdaa, 0xad8b, 0x9de8, 0x8dc9,
    0x7c26, 0x6c07, 0x5c64, 0x4c45, 0x3ca2, 0x2c83, 0x1ce0, 0x0cc1,
    0xef1f, 0xff3e, 0xcf5d, 0xdf7c, 0xaf9b, 0xbfba, 0x8fd9, 0x9ff8,
    0x6e17, 0x7e36, 0x4e55, 0x5e74, 0x2e93, 0x3eb2, 0x0ed1, 0x1ef0
};

/**
 * @brief Compute the CRC-16 for the data buffer using the XMODEM model.
 *
 * @param crc       Seed for CRC calc, zero in most cases this is zero (0).
 * @param data      Pointer to the start of the data to calculate the crc over.
 * @param data_len  Length of the data array in bytes.
 *
 * @return Returns the CRC value.
 *
 * @note This implementation(with a few modifications) and corresponding table was generated using
 *       pycrc v0.9.2 (MIT) using the XMODEM model. https://pycrc.org/. The code generated by pycrc
 *       is not considered a substantial portion of the software, therefore the licence does not
 *       cover the generated code, and the author of pycrc will not claim any copyright on the
 *       generated code (https://pypi.org/project/pycrc/0.9.2/).
 */
static uint16_t morse_crc16(uint16_t crc, const void *data, size_t data_len)
{
    const uint8_t *d = (const uint8_t *)data;

    while (data_len--)
    {
        crc = (crc16_lookup_table[((crc >> 8) ^ *d++)] ^ (crc << 8));
    }
    return crc;
}

static int uart_slip_send(struct morsectrl_transport *transport,
                         struct morsectrl_transport_buff *cmd,
                         struct morsectrl_transport_buff *resp)
{
    struct uart_ctx *ctx;
    int ret = -ETRANSERR;
    int i;
    uint8_t *cmd_seq_num_field;
    uint8_t *rsp_seq_num_field;
    uint8_t *crc_field;
    uint16_t crc;
    size_t original_cmd_data_len;
    struct slip_rx_state slip_rx_state = SLIP_RX_STATE_INIT(resp->data, resp->capacity);
    enum slip_rx_status slip_rx_status = SLIP_RX_IN_PROGRESS;

    if (!transport || !transport->tops || !cmd || !resp)
    {
        return -ETRANSERR;
    }

    /* We need to restore the data_len field before the function returns, so we stash the
     * value here. */
    original_cmd_data_len = cmd->data_len;

    /* Append random sequence number */
    cmd_seq_num_field = cmd->data + cmd->data_len;
    cmd->data_len += SEQNUM_LEN;
    MCTRL_ASSERT(cmd->data_len <= cmd->capacity, "Tx buffer insufficient (%u < %u)",
                 cmd->capacity, cmd->data_len);
    for (i = 0; i < SEQNUM_LEN; i++)
    {
        /* NOLINTNEXTLINE(runtime/threadsafe_fn)*/
        cmd_seq_num_field[i] = rand();
    }

    /* Append CRC16 */
    crc = morse_crc16(0, cmd->data, cmd->data_len);
    crc_field = cmd->data + cmd->data_len;
    cmd->data_len += CRC_LEN;
    MCTRL_ASSERT(cmd->data_len <= cmd->capacity, "Tx buffer insufficient (%u < %u)",
                 cmd->capacity, cmd->data_len);
    crc_field[0] = crc & 0x0ff;
    crc_field[1] = (crc >> 8) & 0x0ff;

    /* Slip encode and transmit the packet */
    ret = slip_tx(uart_slip_tx_char, transport, cmd->data, cmd->data_len);
    cmd->data_len = original_cmd_data_len;

    if (ret != 0)
    {
        uart_slip_error(ret, "Failed to send command");
        goto fail;
    }

    resp->data_len = 0;

    ctx = uart_slip_ctx(transport);

    while (true)
    {
        slip_rx_state_reset(&slip_rx_state);
        do
        {
            uint8_t rx_char;
            ret = uart_read(ctx, &rx_char, 1);
            if (ret < 0)
            {
                uart_slip_error(ret, "Failed to rx command");
                goto fail;
            }
            else if (ret == 0)
            {
                continue;
            }

            slip_rx_status = slip_rx(&slip_rx_state, rx_char);
        } while (slip_rx_status == SLIP_RX_IN_PROGRESS);

        if (slip_rx_status != SLIP_RX_COMPLETE)
        {
            if (slip_rx_status == SLIP_RX_BUFFER_LIMIT)
            {
                uart_slip_error(-ETRANSERR, "Response exceeded allocated buffer");
            }
            uart_slip_error(-ETRANSERR, "Slip RX transfer incomplete");
            ret = -ETRANSERR;
            goto fail;
        }

        resp->data_len = slip_rx_state.length;
        if (resp->data_len < SEQNUM_LEN + CRC_LEN)
        {
            if (resp->data_len > 0)
            {
                uart_slip_error(-ETRANSERR, "Received frame too short. Ignoring it...");
            }
            continue;
        }

        /* Remove and validate CRC */
        resp->data_len -= CRC_LEN;
        crc = morse_crc16(0, resp->data, resp->data_len);
        crc_field = resp->data + resp->data_len;
        if ((crc_field[0] != (crc & 0xff)) || crc_field[1] != ((crc >> 8) & 0xff))
        {
            uart_slip_error(-ETRANSERR, "CRC error for received frame. Ignoring it...");
            continue;
        }

        /* Remove and validate sequence number */
        resp->data_len -= SEQNUM_LEN;
        rsp_seq_num_field = resp->data + resp->data_len;

        if (memcmp(cmd_seq_num_field, rsp_seq_num_field, SEQNUM_LEN) != 0)
        {
            uart_slip_error(-ETRANSERR, "Seq # incorrect for received frame. Ignoring it...");
            continue;
        }

        return ETRANSSUCC;
    }
fail:
    return ret;
}

static const struct morsectrl_transport_ops uart_slip_ops = {
    .name = "uart_slip",
    .description = "Tunnel commands over a UART interface using SLIP framing",
    .has_reset = false,
    .has_driver = false,
    .parse = uart_slip_parse,
    .init = uart_slip_init,
    .deinit = uart_slip_deinit,
    .write_alloc = uart_slip_alloc,
    .read_alloc = uart_slip_alloc,
    .send = uart_slip_send,
    .reg_read = NULL,
    .reg_write = NULL,
    .mem_read = NULL,
    .mem_write = NULL,
    .raw_read = NULL,
    .raw_write = NULL,
    .raw_read_write = NULL,
    .reset_device = NULL,
    .get_ifname = NULL,
};

REGISTER_TRANSPORT(uart_slip_ops);
