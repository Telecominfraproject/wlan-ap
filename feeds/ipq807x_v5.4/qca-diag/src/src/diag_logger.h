#ifndef DIAG_LOGGER_H
#define DIAG_LOGGER_H

/*
 * Copyright (c) 2014 by Qualcomm Technologies, Inc. All Rights Reserved.
 * Qualcomm Technologies Proprietary and Confidential.
 *
 *
 * DIAG LOGGER
 * ===========
 * This module saves diag data to a specified file. It handles proper opening
 * and closing of the output file(s). This supports diag data coming from
 * multiple ASICs. This file is internal to diag and is used by diag
 * applications only.
 *
 *
 * EXTERNAL FUNCTIONS
 * ==================
 * diag_logger_init
 * diag_logger_write
 * diag_logger_exit
 * diag_logger_flush
 *
 */

#include "comdef.h"
#include "./../include/diag_lsm.h"

#define WRITE_QUANTUM	(64 * 1024)
#define NUM_BUFFERS	2

struct diag_logger_buf_t {
	int free;
	int data_ready;
	unsigned int bytes_in_buff;
	unsigned int buf_capacity;
	unsigned char *data;
	pthread_mutex_t write_mutex;
	pthread_cond_t write_cond;
	pthread_mutex_t read_mutex;
	pthread_cond_t read_cond;
};

struct diag_logger_proc_t {
	int op_file;
	pthread_t buf_process_hdl;
	uint8 curr_read;
	uint8 curr_write;
	unsigned char *output_buf;
	unsigned int output_buf_size;
	unsigned char *buffer_0;
	unsigned char *buffer_1;
	struct diag_logger_buf_t buffer[NUM_BUFFERS];
};

int diag_logger_init(int proc, char *file_name, int buf_size);
int diag_logger_write(int proc, void *buf, uint32_t len);
void diag_logger_exit(int proc);
void diag_logger_flush(int proc);

#endif
