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
 * DESCRIPTION
 * ===========
 * Any application that wishes to log data from a processor/ASIC to a file can
 * use these APIs to save diag data to a file. This module creates a file and
 * dumps the data onto the file for each processor individually. It maintains
 * two internal buffers and uses them interchangeably to store data passed
 * by the caller. It creates a thread during initialization that takes care of
 * writing the buffers to the file system. On exit, it flushes the internal
 * buffers to the file and frees the internal buffers. It is highly recommended
 * that the exit function is called after stopping the read thread where the
 * data is coming from.
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

#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include "comdef.h"
#include "errno.h"
#include "stdio.h"
#include "stringl.h"
#include "diag_logger.h"
#include "diag_lsmi.h"
#include "./../include/diag_lsm.h"

static struct diag_logger_proc_t logger_proc[NUM_PROC];

static int diag_logger_buf_init(int proc, int buf_size)
{
	int i;
	struct diag_logger_proc_t *logger = NULL;

	if (buf_size <= 0)
		return -EINVAL;

	if (proc < 0 || proc >= NUM_PROC) {
		DIAG_LOGE("diag: In %s, invalid proc %d\n", __func__, proc);
		return -EINVAL;
	}

	logger = &logger_proc[proc];
	logger->output_buf = malloc(buf_size * NUM_BUFFERS);
	if (!logger->output_buf)
		return -ENOMEM;

	for (i = 0; i < NUM_BUFFERS; i++) {
		logger->buffer[i].free = 1;
		logger->buffer[i].data_ready = 0;
		logger->buffer[i].bytes_in_buff = 0;
		logger->buffer[i].buf_capacity = buf_size;
		pthread_mutex_init(&(logger->buffer[i].write_mutex), NULL);
		pthread_cond_init(&(logger->buffer[i].write_cond), NULL);
		pthread_mutex_init(&(logger->buffer[i].read_mutex), NULL);
		pthread_cond_init(&(logger->buffer[i].read_cond), NULL);
		logger->buffer[i].data = &(logger->output_buf[i * buf_size]);
	}

	logger->curr_read = 0;
	logger->curr_write = 0;
	logger->buffer_0 = &logger->output_buf[0];
	logger->buffer_1 = &logger->output_buf[buf_size];
	return 0;
}

static void diag_logger_buf_exit(int proc)
{
	int i;
	struct diag_logger_proc_t *logger = NULL;

	if (proc < 0 || proc >= NUM_PROC) {
		DIAG_LOGE("diag: In %s, invalid proc %d\n", __func__, proc);
		return;
	}

	logger = &logger_proc[proc];
	for (i = 0; i < NUM_BUFFERS; i++) {
		pthread_mutex_destroy(&(logger->buffer[i].write_mutex));
		pthread_cond_destroy(&(logger->buffer[i].write_cond));
		pthread_mutex_destroy(&(logger->buffer[i].read_mutex));
		pthread_cond_destroy(&(logger->buffer[i].read_cond));
	}

	if (logger->output_buf)
		free(logger->output_buf);
}

void *process_incoming_data(void *data)
{
	unsigned int i;
	unsigned int j;
	unsigned int num_chunks;
	unsigned int last_chunk;
	unsigned int proc = (uint32_t)(uintptr_t)data;
	char *write_buffer = NULL;
	struct diag_logger_proc_t *logger = NULL;
	struct diag_logger_buf_t *buf_ptr = NULL;

	if (proc >= NUM_PROC) {
		DIAG_LOGE("diag: In %s, invalid proc %d\n", __func__, proc);
		return NULL;
	}

	logger = &logger_proc[proc];
	while (1) {
		buf_ptr = &logger->buffer[logger->curr_write];
		pthread_mutex_lock(&buf_ptr->write_mutex);
		while (!buf_ptr->data_ready) {
			pthread_cond_wait(&buf_ptr->write_cond,
					  &buf_ptr->write_mutex);
		}

		write_buffer = (char *)buf_ptr->data;
		if (!write_buffer) {
			DIAG_LOGE(" Invalid Write Buffer in %s\n", __func__);
			break;
		}

		num_chunks = buf_ptr->bytes_in_buff / WRITE_QUANTUM;
		last_chunk = buf_ptr->bytes_in_buff % WRITE_QUANTUM;

		for(i = 0; i < num_chunks; i++) {
			write(logger->op_file, write_buffer, WRITE_QUANTUM);
			write_buffer += WRITE_QUANTUM;
		}

		if (last_chunk > 0)
			write(logger->op_file, write_buffer, last_chunk);

		/* File pool structure */
		buf_ptr->free = 1;
		buf_ptr->data_ready = 0;
		buf_ptr->bytes_in_buff = 0;
		if (logger->curr_write)
			buf_ptr->data = logger->buffer_1;
		else
			buf_ptr->data = logger->buffer_0;

		/* Free Read thread if waiting on same buffer */
		pthread_mutex_lock(&buf_ptr->read_mutex);
		pthread_cond_signal(&buf_ptr->read_cond);
		pthread_mutex_unlock(&buf_ptr->read_mutex);
		pthread_mutex_unlock(&buf_ptr->write_mutex);
		logger->curr_write = !logger->curr_write;
	}

	return NULL;
}

int diag_logger_init(int proc, char *file_name, int buf_size)
{
	int err = 0;
	struct diag_logger_proc_t *logger = NULL;

	if (proc < 0 || proc >= NUM_PROC) {
		DIAG_LOGE("diag: In %s, invalid proc %d\n", __func__, proc);
		return -EINVAL;
	}

	logger = &logger_proc[proc];
	logger->op_file = open(file_name, O_CREAT|O_RDWR|O_SYNC|O_TRUNC, 644);
	if (logger->op_file < 0) {
		DIAG_LOGE("diag: In %s, unable to open output file %s\n, er: %d\n",
			  __func__, file_name, errno);
		return -EIO;
	}

	err = diag_logger_buf_init(proc, buf_size);
	if (err) {
		DIAG_LOGE("diag: In %s, failed to initialize buffers, err: %d\n", __func__, err);
		return err;
	}

	pthread_create(&logger_proc[proc].buf_process_hdl, NULL, process_incoming_data, (void *)(uintptr_t)proc);
	if (logger_proc[proc].buf_process_hdl == 0) {
		DIAG_LOGE("diag: In %s, Failed to create thread to process incoming data\n", __func__);
		diag_logger_buf_exit(proc);
		return -EIO;
	}

	return 0;
}

int diag_logger_write(int proc, void *buf, uint32_t len)
{
	unsigned int copy_bytes = 0;
	unsigned int count_received_bytes = len;
	unsigned int bytes_in_buff = 0;
	unsigned char *ptr = buf;
	struct diag_logger_proc_t *logger = NULL;
	struct diag_logger_buf_t *buf_ptr = NULL;

	if (!buf || len == 0)
		return -EINVAL;

	if (proc < 0 || proc >= NUM_PROC) {
		DIAG_LOGE("diag: In %s, invalid proc %d\n", __func__, proc);
		return -EINVAL;
	}

	logger = &logger_proc[proc];
	buf_ptr = &logger->buffer[logger->curr_read];

	bytes_in_buff = buf_ptr->bytes_in_buff;
	while (len >= (buf_ptr->buf_capacity - bytes_in_buff)) {
		copy_bytes = buf_ptr->buf_capacity - bytes_in_buff;
		memcpy(buf_ptr->data, ptr, copy_bytes);
		count_received_bytes -= copy_bytes;
		buf_ptr->data_ready = 1;
		buf_ptr->free = 0;
		ptr += copy_bytes;
		buf_ptr->data += copy_bytes;
		buf_ptr->bytes_in_buff += copy_bytes;

		pthread_cond_signal(&buf_ptr->write_cond);
		pthread_mutex_unlock(&(buf_ptr->write_mutex));
		logger->curr_read = !logger->curr_read;
		buf_ptr = &logger->buffer[logger->curr_read];
		pthread_mutex_lock(&(buf_ptr->read_mutex));

		if (!buf_ptr->free) {
			pthread_mutex_unlock(&(buf_ptr->write_mutex));
			pthread_cond_wait(&(buf_ptr->read_cond),
					  &(buf_ptr->read_mutex));
			pthread_mutex_lock(&(buf_ptr->write_mutex));
		}

		pthread_mutex_unlock(&(buf_ptr->read_mutex));
		bytes_in_buff = buf_ptr->bytes_in_buff;
	}

	if (len > 0) {
		memcpy(buf_ptr->data, ptr, count_received_bytes);
		buf_ptr->data += count_received_bytes;
		buf_ptr->bytes_in_buff += count_received_bytes;
	}

	return 0;
}

void diag_logger_exit(int proc)
{
	struct diag_logger_proc_t *logger = NULL;

	if (proc < 0 || proc >= NUM_PROC) {
		DIAG_LOGE("diag: In %s, invalid proc %d\n", __func__, proc);
		return;
	}

	logger = &logger_proc[proc];
	diag_logger_flush(proc);
	if (logger->op_file)
		close(logger->op_file);
	diag_logger_buf_exit(proc);
}

void diag_logger_flush(int proc)
{
	struct diag_logger_proc_t *logger = NULL;

	if (proc < 0 || proc >= NUM_PROC) {
		DIAG_LOGE("diag: In %s, invalid proc %d\n", __func__, proc);
		return;
	}

	logger = &logger_proc[proc];
	logger->buffer[logger->curr_read].data_ready = 1;
	pthread_cond_signal(&logger->buffer[logger->curr_read].write_cond);
	pthread_mutex_unlock(&(logger->buffer[logger->curr_read].write_mutex));
}

