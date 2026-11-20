/* SPDX-License-Identifier: Zlib */
/* Modified for OpenWiFi integration; original vendor notice follows. */
/***************************************************************************//**
 * @file
 * @brief Application
 *******************************************************************************
 * # License
 * <b>Copyright 2020 Silicon Laboratories Inc. www.silabs.com</b>
 *******************************************************************************
 *
 * SPDX-License-Identifier: Zlib
 *
 * The licensor of this software is Silicon Laboratories Inc.
 *
 * This software is provided 'as-is', without any express or implied
 * warranty. In no event will the authors be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software. If you use this software
 *    in a product, an acknowledgment in the product documentation would be
 *    appreciated but is not required.
 * 2. Altered source versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
 *
 ******************************************************************************/

/**
 * This an example application that demonstrates Bluetooth Device Firmware Update (DFU)
 * over UART interface.
 *
 * To use this application you must have a WSTK configured into NCP mode connected to your
 * PC.
 *
 */

#define POSIX
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <time.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include "sl_bt_api.h"
#include "sl_bt_ncp_host.h"
#include "uart.h"
//#include "app_log.h"
#include "app_assert.h"

/* open */
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
/* mmap/munmap */
#include <sys/mman.h>

#ifndef POSIX
#include <windows.h>
#endif

static void on_message_send(uint32_t msg_len, uint8_t* msg_data);
static int32_t uart_rx_wrapper(uint32_t len, uint8_t* data);
static int32_t uart_peek_wrapper(void);

/**
 * Configurable parameters that can be modified to match the test setup.
 */

/** The serial port to use for BGAPI communication. */
static char* uart_port = NULL;
/** The baud rate to use. */
static uint32_t baud_rate = 0;

/** Usage string */
#define USAGE "Usage: %s [serial port] [baud rate] [dfu file] \n\n"

/** dfu file to upload*/
FILE *dfu_file = NULL;

#define MAX_DFU_PACKET 48
uint8_t dfu_data[MAX_DFU_PACKET];
size_t dfu_toload = 0;
size_t dfu_total = 0;
size_t current_pos = 0;

#ifdef POSIX
int32_t handle = -1;
void *handle_ptr = &handle;
#else
HANDLE serial_handle;
void *handle_ptr = &serial_handle;
#endif

/*
 * ==========================
 * = Memory Map Definitions =
 * ==========================
 */
#define MAP_SIZE 4096UL
#define MAP_MASK (MAP_SIZE - 1)
/* */

#define FATAL do { fprintf(stderr, "Error at line %d, file %s (%d) [%s]\n", \
__LINE__, __FILE__, errno, strerror(errno)); exit(1); } while(0)

void mw(unsigned int addr, unsigned int value){
	int fd, i;

	void *reg_addr, *preg_addr;

	if((fd = open("/dev/mem", O_RDWR | O_SYNC)) == -1)
		FATAL;

//	printf("/dev/mem opened.\n");
	fflush(stdout);

	preg_addr = mmap(NULL, MAP_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED,\
		    fd,addr & ~MAP_MASK);

	if(preg_addr == (void *) -1) {
		printf("map_base:%x \n",preg_addr);
		FATAL;
	}
//	    printf("Memory mapped at address %p.\n", preg_addr);

	fflush(stdout);

	reg_addr = preg_addr + (addr & MAP_MASK);

	*(volatile uint32_t *)(reg_addr + 0) = value;
	close(fd);
}

static int dfu_read_size()
{
	if (fseek(dfu_file, 0L, SEEK_END)) {
		return 1;
	}
	dfu_total = dfu_toload = ftell(dfu_file);
	if (fseek(dfu_file, 0L, SEEK_SET)) {
		return 1;
	}
	printf("Bytes to send:%lu\n", dfu_toload);
	return 0;
}

/**
 * Function called when a message needs to be written to the serial port.
 * @param msg_len Length of the message.
 * @param msg_data Message data, including the header.
 * @param data_len Optional variable data length.
 * @param data Optional variable data.
 */
static void on_message_send(uint32_t msg_len, uint8_t* msg_data)
{
	/** Variable for storing function return values. */
	int ret;

#if DEBUG
	printf("on_message_send()\n");
#endif /* DEBUG */

	ret = uartTx(handle_ptr, msg_len, msg_data);
	if (ret < 0) {
		printf("on_message_send() - failed to write to serial port %s, ret: %d, errno: %d\n", uart_port, ret, errno);
		exit(EXIT_FAILURE);
	}
}

static int hw_init(int argc, char* argv[])
{
	if (argc < 4) {
		printf(USAGE, argv[0]);
		exit(EXIT_FAILURE);
	}
	/**
   * Handle the command-line arguments.
   */

	//filename
	dfu_file = fopen(argv[3], "rb");
	if (dfu_file == NULL) {
		printf("cannot open file %s\n", argv[3]);
		exit(-1);
	}
	baud_rate = atoi(argv[2]);
	uart_port = argv[1];

	if (!uart_port || !baud_rate || !dfu_file) {
		printf(USAGE, argv[0]);
		exit(EXIT_FAILURE);
	}
	/**
   * Initialise the serial port.
   */
	return uartOpen(handle_ptr, (int8_t*)uart_port, baud_rate, 0, 100);
}

static void sync_dfu_boot()
{
	sl_bt_msg_t evt = { 0 };
	uint8_t ret;

	printf("Syncing");

	do {
		printf(".");

		ret = sl_bt_pop_event(&evt);

		if (0 == ret) {
			switch (SL_BT_MSG_ID(evt.header)) {
				case sl_bt_evt_dfu_boot_id:
					printf("DFU OK\nBootloader version: %u (0x%x)\n", evt.data.evt_dfu_boot.version, evt.data.evt_dfu_boot.version);
					return;
				default:
					printf("ID:%08x\n", SL_BT_MSG_ID(evt.header));
					break;
			}
		} else {
//      sl_bt_dfu_reset(1);
			mw(0x1002000,0x201); //set gpio2 to low
			mw(0x1002004,0x0);

			mw(0x102D000,0x201); //ble reset, set gpio45
			mw(0x102D004,0x0);
			usleep(500000);
			mw(0x102D004,0x3);
			sleep(1);
		}
	} while (1);
}

void upload_dfu_file()
{
	uint16_t result;
	size_t dfu_size;
	current_pos = 0;

	/** get dfu file size*/
	if (dfu_read_size()) {
		printf("Error, DFU file read failed\n");
		exit(EXIT_FAILURE);
	}

	/** move target to dfu mode*/
	sync_dfu_boot();

	/** update firmware*/
	sl_bt_dfu_flash_set_address(0);

	printf("DFU packet size:%lu\n", dfu_toload);
	while (dfu_toload > 0) {
		dfu_size = dfu_toload > sizeof(dfu_data) ? sizeof(dfu_data) : dfu_toload;
		if (fread(dfu_data, 1, dfu_size, dfu_file) != dfu_size) {
			printf("%lu:%lu:%lu\n", current_pos, dfu_size, dfu_total);
			printf("File read failure\n");
			exit(EXIT_FAILURE);
		}

		printf("\r%d%%", (int)(100 * current_pos / dfu_total));

		result = sl_bt_dfu_flash_upload(dfu_size, dfu_data);
		if (result) {//error
			printf("\nError in upload 0x%.4x\n", result);
			exit(EXIT_FAILURE);
		}
		current_pos += dfu_size;
		dfu_toload -= dfu_size;
	}
	result = sl_bt_dfu_flash_upload_finish();
	if (result) {
		printf("\nError in dfu 0x%.4x", result);
	} else {
		printf("\nfinish\n");
	}

	fclose(dfu_file);
//  sl_bt_dfu_reset(0);
	mw(0x1002000,0x201); //set gpio2 to high
	mw(0x1002004,0x3);

	mw(0x102D000,0x201); //ble reset, set gpio45
	mw(0x102D004,0x0);
	usleep(500000);
	mw(0x102D004,0x3);
}
/***************************************************************************//**
 * Application initialisation.
 ******************************************************************************/
void app_init(int argc, char* argv[])
{
	sl_status_t sc = sl_bt_api_initialize_nonblock(on_message_send, uart_rx_wrapper,
																								 uart_peek_wrapper);
	app_assert(sc == SL_STATUS_OK,
						 "[E: 0x%04x] Failed to init Bluetooth NCP\n",
						 (int)sc);

	//////////////////////////////////////////
	// Add your application init code here! //
	//////////////////////////////////////////

	if (hw_init(argc, argv) < 0) {
		printf("HW init failure\n");
		exit(EXIT_FAILURE);
	}
}
/***************************************************************************//**
 * Application process actions.
 ******************************************************************************/
void app_process_action(void)
{
	//////////////////////////////////////////
	// Add your application tick code here! //
	//////////////////////////////////////////
}

static inline int32_t uart_rx_wrapper(uint32_t len, uint8_t* data)
{
	return uartRx(handle_ptr, len, data);
}

static inline int32_t uart_peek_wrapper(void)
{
	int32_t sc;
	static bool log_first = false;

	sc = uartRxPeek(handle_ptr);
	if (sc < 0) {
		sc = 0;
		if (!log_first) {
			log_first = true;
			printf("\nPeek is not supported in your environment, the program will hang.\n");
			printf("Please try other OS, or environment (preferred: MingW, Cygwin)\n\n");
		}
	}
	return sc;
}
