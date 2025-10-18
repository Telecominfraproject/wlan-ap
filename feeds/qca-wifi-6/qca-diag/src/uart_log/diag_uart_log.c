// Copyright (c) 2012, 2014 by Qualcomm Technologies, Inc.  All Rights Reserved.
//Qualcomm Technologies Proprietary and Confidential.

/*====*====*====*====*====*====*====*====*====*====*====*====*====*====*====*

			  Test Application for Diag Interface

GENERAL DESCRIPTION
  Contains main implementation of Diagnostic Services Application for UART.

EXTERNALIZED FUNCTIONS
  None

INITIALIZATION AND SEQUENCING REQUIREMENTS

Copyright (c) 2012, 2014 Qualcomm Technologies, Inc.
All Rights Reserved.
Qualcomm Technologies Confidential and Proprietary

*====*====*====*====*====*====*====*====*====*====*====*====*====*====*====*/

/*===========================================================================

			EDIT HISTORY FOR MODULE

This section contains comments describing changes made to the module.
Notice that changes are listed in reverse chronological order.

$Header:

when       who    what, where, why
--------   ---     ----------------------------------------------------------
01/06/12   SJ     Created
===========================================================================*/

#include <stdio.h>
#include <stdlib.h>
#include "string.h"
#include "malloc.h"
#include <unistd.h>
#include <fcntl.h>
#include <getopt.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "errno.h"
#include <unistd.h>
#include <signal.h>

#ifndef FEATURE_LE_DIAG
#include <termios.h>
#else
#include <linux/termios.h>
#endif
#include <unistd.h>
#include "msg.h"
#include "diag_lsm.h"
#include "stdio.h"
#include "diagpkt.h"
#include "diag_lsmi.h"
#include "diag_shared_i.h"

/*strlcpy is from OpenBSD and not supported by Meego.
 * GNU has an equivalent g_strlcpy implementation into glib.
 * Featurized with compile time USE_GLIB flag for Meego builds.
 */
#ifdef USE_GLIB
#define strlcpy g_strlcpy
#define strlcat g_strlcat
#endif

#define DIAG_UART_WAKELOCK_NAME		"diag_uart_wakelock"

extern int fd_uart;
unsigned char read_buf[4096], buf[4100], send_buf[4100];
boolean bInit_Success = FALSE;
int i, unused_len = 0, start, end, num_read;
static speed_t input_baudrate = B115200, output_baudrate = B115200;
char dev_name[FILE_NAME_LEN] = "/dev/ttyHSL0";
char pid_file[32] = "/data/diag_uart_pid";
char valid_procs[][5] = {"MSM", "MDM", "MDM2", "MDM3", "MDM4", "QSC", ""};
char proc[5];
int logging_proc = MSM;
static int enable_wakelock = 0;

void usage (char *progname)
{
	printf("\n Usage for %s:\n", progname);
	printf("\n-d, --device:\t UART device node (/dev/ttyHSL0)\n");
	printf("\n-o, --obaud:\t Output baud rate (115200)\n");
	printf("\n-i, --ibaud:\t Input baud rate (115200)\n");
	printf("\n-p, --proc:\t Logging proc type MSM, MDM\n");
	printf("\n-e  --enablelock:\t Run using wake lock to keep APPS processor on\n");
	printf("\n-h, --help:\t usage help\n");
	printf("\ne.g. diag_uartlog -d <device node> -b <Out baud rate>"
			" -i <In baud rate>\n");
}

int checkbaud(long baud, char *type)
{
	switch (baud) {
	case 50:
		return B50;
	case 75:
		return B75;
	case 110:
		return B110;
	case 134:
		return B134;
	case 150:
		return B150;
	case 200:
		return B200;
	case 300:
		return B300;
	case 600:
		return B600;
	case 1200:
		return B1200;
	case 1800:
		return B1800;
	case 2400:
		return B2400;
	case 4800:
		return B4800;
	case 9600:
		return B9600;
	case 19200:
		return B19200;
	case 38400:
		return B38400;
	case 57600:
		return B57600;
	case 115200:
		return B115200;
	case 230400:
		return B230400;
	default:
		printf("Invalid baudrate for %s using default"
			" 115200\n", type);
		return B115200;
	}
}

void parse_args(int argc, char **argv)
{
	int command, i = 0;
	struct option longopts[] =
	{
		{ "device",	1,	NULL,	'd'},
		{ "obaud",	1,	NULL,	'o'},
		{ "ibaud",	1,	NULL,	'i'},
		{ "proc",	1,	NULL,	'p'},
		{ "enablelock", 0,      NULL,   'e'},
		{ "help",	0,	NULL,	'h'},
	};

	while ((command = getopt_long(argc, argv, "d:o:i:p:eh", longopts,
					NULL)) != -1) {
		switch (command) {
		case 'd':
			strlcpy(dev_name, optarg, FILE_NAME_LEN);
			break;
		case 'o':
			output_baudrate = checkbaud(atol(optarg),
							"output");
			break;
		case 'i':
			input_baudrate = checkbaud(atol(optarg),
							"input");
			break;
		case 'p':
			strlcpy(proc, optarg, 5);
			proc[4] = '\0';
			logging_proc  = -1;
			while (strnlen(valid_procs[i], 4)) {
				if (!strncmp(valid_procs[i], proc, 4)) {
					logging_proc = i;
					break;
				}
				i++;
			}
			if (logging_proc < 0) {
				printf("\ndiag: Invalid Proc name\n");
				exit(0);
			}
			break;
		case 'e':
			enable_wakelock = 1;
			break;
		case 'h':
		default:
			usage(argv[0]);
			exit(0);
		};
	}
}

void create_pid_file(int logging_proc)
{
	int pid_fd;
	struct diag_uart_tbl_t last_app, last_app1;

	unlink(pid_file);
	if ((pid_fd = open(pid_file, O_RDWR | O_CREAT |
					O_EXCL | O_SYNC, 0770)) < 0) {
		perror("diag: pid_file:");
		printf("\ndiag: Unable to create pid file");
		exit(0);
	}
	last_app.pid = getpid();
	last_app.proc_type = logging_proc;
	write(pid_fd, &last_app, sizeof(last_app));
	close(pid_fd);
#if 0
	/* Test */
	pid_fd = open(pid_file, O_RDONLY);
	read(pid_fd, &last_app1.pid, sizeof(int));
	read(pid_fd, &last_app1.proc_type, sizeof(int));
	close(pid_fd);
	printf("\n --- %d %d --\n", last_app1.proc_type, last_app1.pid);
#endif
}

void cleanup_pid_file(int signal)
{
	(void)signal;

	printf("\n .... cleaning the pid file killing app \n");
	unlink(pid_file);
	if (diag_is_wakelock_init()) {
		diag_wakelock_release();
		diag_wakelock_destroy();
	}
	exit(0);
}

int main(int argc, char *argv[])
{
	struct termios options, options_save;
	int token_offset = 4, ret, pid_fd;
	struct diag_uart_tbl_t last_app;
	struct sigaction sact;

	/* Basic setup for pid file */
	if (mkdir("/data", 0770)) {
		if (errno != EEXIST)
			printf("\ndiag: Unable to create directory");
	}

	*(int *)send_buf = USER_SPACE_DATA_TYPE;

	parse_args(argc, argv);
	/* Open and Configure UART */
	fd_uart = open(dev_name, O_RDWR | O_NOCTTY | O_EXCL);
	if (fd_uart < 0) {
		printf("\n Unable to open device %s\n", dev_name);
		exit(0);
	}
	tcflush(fd_uart, TCIOFLUSH);
	fcntl(fd_uart, F_SETFL, 0);
	ioctl(fd_uart, TCGETS, &options_save);
	options = options_save;
	options.c_cc[VTIME]    = 0;   /* inter-character timer unused */
	options.c_cc[VMIN]     = 4;   /* blocking read until 4 chars received */
	options.c_cflag &= ~PARENB;
	options.c_cflag &= ~CSTOPB;
	options.c_cflag &= ~CSIZE;
	options.c_cflag |= (CS8 | CLOCAL | CREAD);
	options.c_iflag = 0;
	options.c_oflag = 0;
	options.c_lflag = 0;
	cfsetospeed(&options, output_baudrate);
	cfsetispeed(&options, input_baudrate);
	ioctl(fd_uart, TCSETS, &options);

	if (!bInit_Success)
		bInit_Success = Diag_LSM_Init(NULL);

	sigemptyset(&sact.sa_mask);
	sact.sa_flags = 0;
	sact.sa_handler = cleanup_pid_file;
	sigaction(SIGTERM, &sact, NULL);
	sigaction(SIGHUP, &sact, NULL);
	sigaction(SIGINT, &sact, NULL);

	/* Open the pid_file to get information about any diag_uart_log
	 * already running.
	 */
	pid_fd = open(pid_file, O_RDONLY);
	if (pid_fd < 0) {
		if (errno != ENOENT) {
			perror("diag: pid_file:");
			printf("\ndiag: exiting... error:%d", errno);
			return 0;
		} else {
			/* pid_file not present diag_uart_log is not running
			 * create pid file and start app.
			 */
			create_pid_file(logging_proc);
		}
	} else {

		/* pid_file present diag_uart_log is already running */
		/* Kill the previous APP */

		ret = read(pid_fd, &last_app.proc_type, sizeof(int));
		if (ret < 0) {
			printf("\ndiag: Unable to read pid file");
			perror("diag:");
			close(pid_fd);
			return 0;
		}

		ret = read(pid_fd, &last_app.pid, sizeof(int));
		if (ret < 0) {
			printf("\ndiag: Unable to read pid file");
			perror("diag:");
			close(pid_fd);
			return 0;
		}
		close(pid_fd);

		if ((logging_proc == last_app.proc_type) &&
				(kill(last_app.pid, 0) == 0)) {
			printf("\ndiag: diag_uart_log already running with"
					" same proc logging\n");
			return 0;
		}

		/* Not checking for the error, we might have a stale file */
		if (last_app.pid != 0) {
			kill(last_app.pid, SIGUSR1);
			/* Adding sleep to allow switching of mode */
			sleep(5);
		}

		create_pid_file(logging_proc);
	}

	if (enable_wakelock) {
		diag_wakelock_init(DIAG_UART_WAKELOCK_NAME);
		diag_wakelock_acquire();
	}
	printf("\ndiag: Switching logging to UART mode PROC: %d\n",
					logging_proc);
	/* Switch the logging mode */
	diag_switch_logging(UART_MODE, (char *)&logging_proc);

	while (1) {
		if (fd_uart > 0) {
			memset(read_buf, 0, 4096);
			/* read from UART */
			num_read = read(fd_uart, (unsigned char *)read_buf,
					4096);
			if (num_read > 0) {
				memcpy(buf+unused_len, read_buf, num_read);
				start = 0;
				end = 0;
				for (i = 0; i < unused_len+num_read; i++) {
					if (buf[i] == CONTROL_CHAR) {
						if (logging_proc != MSM) {
							token_offset = 4;
							*(int *)(send_buf + 4) = -logging_proc;
							memcpy(send_buf + 8, buf + start, end-start + 1);
						} else
							memcpy(send_buf + 4, buf + start, end-start + 1);

						diag_send_data(send_buf, end - start + 5 + token_offset);

						token_offset = 0;
						start = i + 1;
						end = i + 1;
						continue;
					}

					end = end + 1;
				}

				unused_len = end - start;
				memcpy(buf, buf+start, unused_len);
			}
		}
	}

	unlink(pid_file);
	if (enable_wakelock) {
		diag_wakelock_release();
		diag_wakelock_destroy();
	}
	ioctl(fd_uart, TCSETS, &options_save);
	close(fd_uart);
	Diag_LSM_DeInit();
	return 0;
}
