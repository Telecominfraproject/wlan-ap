/*
 * LZMA compressed kernel loader for Realtek 819X
 *
 * Copyright (C) 2011 Gabor Juhos <juhosg@openwrt.org>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 */

#include <stddef.h>

#define BSP_UART0_BASE      0xB8147000
#define UART_THR        	(BSP_UART0_BASE + 0x024)
#define UART_LSR       		(BSP_UART0_BASE + 0x014)
#define REG8(reg)   (*(volatile unsigned char   *)((unsigned int)reg))


void serial_outc(char c)
{
        int i=0;

        while (1)
        {
                i++;
                if (i >=0x6000)
                        break;
                if (REG8(UART_LSR) & 0x20)
                        break;
        }
        REG8(UART_THR) = (c);
}


void board_putc(int ch)
{
	serial_outc(ch);
}

void board_init(void)
{
	//tlwr1043nd_init();
}
