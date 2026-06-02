/*
 * Copyright 2024 Morse Micro
 */

/*
 * UART platform abstration API.
 */

#pragma once

#include <stddef.h>
#include <stdint.h>

struct uart_ctx;

#define UART_MAX_DEVICE_NAME_LEN         (256)

struct uart_config
{
    char dev_name[UART_MAX_DEVICE_NAME_LEN];

    int baudrate;
};

struct uart_ctx *uart_init(const struct uart_config *cfg);

int uart_deinit(struct uart_ctx *ctx);

int uart_read(struct uart_ctx *ctx, uint8_t *buf, size_t len);

int uart_write(struct uart_ctx *ctx, const uint8_t *buf, size_t len);
