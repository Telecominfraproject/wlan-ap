/*====*====*====*====*====*====*====*====*====*====*====*====*====*====*====*

			  Test Application for Diag Interface

GENERAL DESCRIPTION
  Contains main implementation of Diagnostic Services Application for sockets.

EXTERNALIZED FUNCTIONS
  None

INITIALIZATION AND SEQUENCING REQUIREMENTS

Copyright (c) 2012-2014 Qualcomm Technologies, Inc.
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
06/21/12   DP     Created
===========================================================================*/

#include <stdio.h>
#include <stdlib.h>
#include "string.h"
#include "malloc.h"
#include <fcntl.h>
#include <getopt.h>
#include <unistd.h>
#include "msg.h"
#include "diag_lsm.h"
#include "diag_lsmi.h"
#include "diag_shared_i.h"
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include "errno.h"
#include <signal.h>
#include <sys/stat.h>
#include <sys/types.h>

/* strlcpy is from OpenBSD and not supported by Linux Embedded.
 * GNU has an equivalent g_strlcpy implementation into glib.
 * Featurized with compile time USE_GLIB flag for Linux Embedded builds.
 */
#ifdef USE_GLIB
#define strlcpy g_strlcpy
#endif

#define DIAG_SOCKET_WAKELOCK_NAME		"diag_socket_wakelock"

#define PID_FILE "/root/diag_logs/diag_socket_pid"
#define PID_DIR "/root/diag_logs"
#define MAX_CHAN	3

static unsigned char read_buf[4096];
static char ip_addr_name[FILE_NAME_LEN] = "192.168.0.10";
static char port_number_string[FILE_NAME_LEN] = "2500";
static int port_number = 2500;
static int num_connect_retries = 10000;
static int ip_addr_entered;
static int keep_logging = 1;
static int keep_reading = 1;
static int num_sockets = 0;
static int num_connected = 0;
static fd_set read_fs;
sigset_t pselect_set;
static int enable_wakelock = 0;
static int kill_socket_log = 0;
extern unsigned int device_num;

struct diag_socket {
	int fd;
	int id;
	int connected;
};

static struct diag_socket *sockets;

void sock_log_handler(int signal)
{
	/* Continue logging */
	(void)signal;
	keep_logging = 1;
}

void sock_kill_handler(int signal)
{
	/* Logging should be stoped and the main thread exited */
	(void)signal;
	keep_logging = 0;
}

/*
 * Call back function that is called if data from a remote device is
 * discovered and there is no socket for it
 */
int diag_socket_callback(void *data_ptr, int socket_id)
{
	int i;
	int success = 0;
	int max_sockets = diag_get_max_channels();
	(void)data_ptr;

	for (i = 0; i < max_sockets; i++) {
		if (sockets[i].fd == -1) {
			sockets[i].id = socket_id;
			num_sockets++;
			keep_reading = 0;
			success = 1;
			break;
		}
	}

	if (success) {
		DIAG_LOGE("In diag_socket_callback, success: %d, socket_id: %d, max_sockets: %d, num_sockets: %d\n",
			success, socket_id, max_sockets, num_sockets);
	} else {
		DIAG_LOGE("In diag_socket_callback, Not able to setup for new socket connection for id: %d. num_sockets: %d, max_sockets: %d\n",
			socket_id, num_sockets, max_sockets);
	}

	return success;
}

void usage (char *progname)
{
	printf("\n Usage for %s:\n", progname);
	printf("\n-a, --address:\t IP address\n");
	printf("\n-d, --device:\t Device number(0: IPQ, 1: MDM1, 2: MDM2)\n");
	printf("\n-p, --port:\t Port number\n");
	printf("\n-r, --retry:\t Number of retries for connect\n");
	printf("\n-e  --enable wakelock:\t Run using wake lock to keep APPS processor on\n");
	printf("\n-k, --kill:\t kill existing instance of diag_socket_log\n");
	printf("\n-h, --help:\t usage help\n");
	printf("\ne.g. diag_socket_log -a <ip address> -p <port number> -r <num retries>\n");
}

void parse_args(int argc, char **argv)
{
	int command;
	struct option longopts[] =
	{
		{ "address",    1,      NULL,   'a'},
		{ "port",       1,      NULL,   'p'},
		{ "retry",      1,      NULL,   'r'},
		{ "kill",	0,	NULL,	'k'},
		{ "help",       0,      NULL,   'h'},
	};

	while ((command = getopt_long(argc, argv, "a:d:p:r:khe", longopts,
					NULL)) != -1) {
		switch (command) {
		case 'a':
			strlcpy(ip_addr_name, optarg, FILE_NAME_LEN);
			ip_addr_entered = 1;
			break;
		case 'p':
			strlcpy(port_number_string, optarg, FILE_NAME_LEN);
			port_number = atol(optarg);
			break;
		case 'r':
			num_connect_retries = atol(optarg);
			break;
		case 'k':
			kill_socket_log = 1;
			break;
		case 'e':
			enable_wakelock = 1;
			break;
		case 'd':
			device_num = to_integer(optarg);
			if (device_num >= MAX_CHAN)
				device_num = 0;
			break;
		case 'h':
		default:
			usage(argv[0]);
			exit(0);
		};
	}
}

/* stop_socket_log is called when another instance of diag_socket_log is to be killed off */
static void stop_socket_log(char *pid_file)
{
	int fd;
	int ret;
	pid_t pid;
	char pid_buff[10];

	/* Determine the process id of the instance of diag_socket_log */
	fd = open(pid_file, O_RDONLY);
	if (fd < 0) {
		DIAG_LOGE("\n diag_socket_log: Unable to open pid file, errno: %d\n", errno);
		return;
	}

	ret = read(fd, pid_buff, 10);
	if (ret < 0) {
		DIAG_LOGE("\n diag_socket_log: Unable to read pid file, errno: %d\n", errno);
		close(fd);
		return;
	}

	close(fd);

	/* Make sure the buffer is properly terminated */
	if (ret == sizeof(pid_buff))
		ret--;
	pid_buff[ret] = '\0';

	pid = atoi(pid_buff);

	if (pid == 0 || (kill(pid, SIGTERM)) < 0) {
		DIAG_LOGE("\ndiag_socket_log: Unable to kill diag_socket_log instance pid: %d, errno: %d\n", pid, errno);
	} else {
		DIAG_LOGE("\ndiag_socket_log: stopping diag_socket_log instance pid: %d\n", pid);
	}

	return;
}

void close_connection(struct diag_socket *socket)
{
	int status = 0;

	if (keep_logging)
		diag_switch_logging(USB_MODE, NULL);

	if (socket->fd != -1) {
		FD_CLR(socket->fd, &read_fs);

		status = close(socket->fd);
		if (status == -1) {
			DIAG_LOGE("diag_socket_log: Error closing socket: %s, socket: %d, errno: %d\n",
				strerror(errno), socket->fd, errno);
		}
		socket->fd = -1;
		num_connected -= 1;
	}
}

int open_connection(struct diag_socket *sock)
{
	int connect_success = 0;
	struct addrinfo hints;
	struct addrinfo *res = NULL;
	char ip_info_name[INET6_ADDRSTRLEN];
	char *display_name = ip_addr_name;
	int turn_on = 1;
	struct linger sock_linger;
	int status;
	int retries_count;

	/* Open and Configure socket */
	sock->fd = socket(AF_INET, SOCK_STREAM, 0);
	if (sock->fd == -1) {
		DIAG_LOGE("diag_socket_log: Error calling socket: %s, errno: %d\n",
				strerror(errno), errno);
		exit(0);
	}

	/* Set socket options */
	status = setsockopt(sock->fd, SOL_SOCKET, SO_REUSEADDR, &turn_on,
				(socklen_t)sizeof(turn_on));
	if (status == -1) {
		DIAG_LOGE("diag_socket_log:, getsockopt SO_REUSEADDR error, %s, errno: %d\n",
				strerror(errno), errno);
	}

	sock_linger.l_onoff = 1;
	sock_linger.l_linger = 0;
	status = setsockopt(sock->fd, SOL_SOCKET, SO_LINGER, &sock_linger,
				(socklen_t)sizeof(struct linger));
	if (status == -1) {
		DIAG_LOGE("diag_socket_log:, getsockopt SO_LINGER error, %s, errno: %d\n",
				strerror(errno), errno);
	}

	/*
	 * With the possibility the target has a remote device and
	 * having more than one socket to read/write, we need to
	 * set the socket to be non-blocking
	 */
	status = fcntl(sock->fd, F_SETFL, O_NONBLOCK);
	if (status == -1) {
		DIAG_LOGE("diag_socket_log:, fcntl O_NONBLOCK error, %s, errno: %d\n",
			strerror(errno), errno);
	}

	memset(&hints, 0, sizeof(hints));

	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;

	DIAG_LOGE("diag_socket_log: Translating address: %s\n", ip_addr_name);

	status = getaddrinfo(ip_addr_name, port_number_string, &hints, &res);

	if (status != 0 || !res) {
		if (status == EAI_SYSTEM) {
			DIAG_LOGE("diag_socket_log: getaddrinfo error: %d, %s, errno: %d\n",
				status, gai_strerror(status), errno);
		} else {
			DIAG_LOGE("diag_socket_log: getaddrinfo error: %d, %s\n",
				status, gai_strerror(status));
		}
		if (!res)
			DIAG_LOGE("diag_socket_log: getaddrinfo did not return list of addrinfo structures.\n");
		DIAG_LOGE("diag_socket_log: Exiting ...\n");
		exit(0);
	} else {
		void *ip_info_addr;

		/* If the family is IPV4 */
		if (res->ai_family == AF_INET) {
			struct sockaddr_in *ipv4 = (struct sockaddr_in *)res->ai_addr;
			ip_info_addr = &(ipv4->sin_addr);
		} else {
			struct sockaddr_in6 *ipv6 = (struct sockaddr_in6 *)res->ai_addr;
			ip_info_addr = &(ipv6->sin6_addr);
		}
		inet_ntop(res->ai_family, ip_info_addr, ip_info_name, sizeof(ip_info_name));
		display_name = ip_info_name;
	}

	DIAG_LOGE("diag_socket_log: Trying to connect to address: %s, port: %d\n",
			display_name, port_number);

	retries_count = 0;
	while (retries_count < num_connect_retries) {
		/* If we are not to keep logging, stop trying to connect */
		if (!keep_logging) {
			DIAG_LOGE("diag_socket_log: Discontinuing trying to connect\n");
			break;
		}

		status = connect(sock->fd, res[0].ai_addr, res[0].ai_addrlen);
		if (status == -1) {
			if (errno == EISCONN) {
				DIAG_LOGE("diag_socket_log: Socket %d already connected, errno: %d\n",
					sock->fd, errno);
				connect_success = 1;
				break;
			} else if (errno == EINPROGRESS) {
				if (retries_count == 1) {
					DIAG_LOGE("diag_socket_log: %s, errno: %d\n",
							strerror(errno), errno);
				}
			} else {
				DIAG_LOGE("diag_socket_log: Error calling connect: %s, errno: %d\n",
					strerror(errno), errno);

				if (errno == ENETUNREACH) {
					DIAG_LOGE("diag_socket_log: Network is unreachable, "
							"errno: %d Exiting ...\n", errno);
					exit(0);
				} else if (errno == EINVAL) {
					DIAG_LOGE("diag_socket_log: Invalid argument.  Exiting... \n");
					exit(0);
				} else if (errno == ECONNREFUSED) {
					DIAG_LOGE("diag_socket_log: Error is ECONNREFUSED - "
						"No one is listening on the remote ip address\n");
				}
				retries_count++;
			}
		} else {
			connect_success = 1;
			break;
		}
		sleep(2);
	}

	freeaddrinfo(res);

	if (connect_success) {
		DIAG_LOGE("diag_socket_log: Successful connect to address: %s, port number: %d\n",
				display_name, port_number);
	}

	return connect_success;
}

int read_socket(struct diag_socket *socket)
{
	int num_read;

	/* Read from the socket */
	memset(read_buf, 0, 4096);
	num_read = recv(socket->fd, (unsigned char *)read_buf, 4096, 0);
	if (num_read > 0) {
		/* Send the data read off of the socket to the kernel via the library */
		diag_send_socket_data(socket->id, read_buf, num_read);
	} else if (num_read == -1) {
		DIAG_LOGE("diag_socket_log: Read socket error: %s, errno: %d\n",
				strerror(errno), errno);
		switch (errno) {
		case EINTR:
		case EWOULDBLOCK:
			/* Don't close connection. We can continue from these errors. */
			break;
		default:
			close_connection(socket);
			socket->connected = 0;
			break;
		}
	} else {
		/*
		 * When read returns 0, the peer closed the connection.
		 * Try to re-establish the connection
		 */
		DIAG_LOGE("diag_socket_log: Peer closed connection. Trying to recover connection\n");
		close_connection(socket);
		socket->connected = 0;
	}

	return num_read;
}

int check_for_remote(uint16 *remote_ids)
{
	int status;
	int socket_count = 1;	/* Set to 1 for the MSM socket */

	status = diag_has_remote_device(remote_ids);
	if (status != 1) {
		DIAG_LOGE("diag_socket_log: Error determining if there is a remote device\n");
	} else {
		if (*remote_ids != 0) {
			DIAG_LOGE("diag_socket_log: Remote device detected, remote_ids: 0x%x\n",
				(unsigned int)*remote_ids);
		} else {
			DIAG_LOGE("diag_socket_log: This device does not have any remote devices\n");
		}
	}

	if (remote_ids != 0) {
		uint16 ids = *remote_ids;
		while (ids)
		{
			if (ids & 0x1) {
				socket_count++;
			}
			ids = ids >> 1;
		}
	}

	return socket_count;
}

int main(int argc, char *argv[])
{
	boolean bInit_Success = FALSE;
	boolean bDeInit_Success = FALSE;

	int pid, pid_fd;
	char pid_buff[10];
	char pid_file[32] = PID_FILE;

	int connected = 0;
	int num_read;
	int status;
	int i;
	struct sigaction sock_log_action, sock_kill_action;
	uid_t uid;
	uint16 remote_ids;

	int select_fd = -1;
	int max_channels;
	sigemptyset(&pselect_set);
	sigaddset(&pselect_set, SIGCONT);

	ip_addr_entered = 0;

	sigemptyset(&sock_log_action.sa_mask);
	sock_log_action.sa_handler = sock_log_handler;
	sock_log_action.sa_flags = 0;
	sigaction(SIGCONT, &sock_log_action, NULL);

	sigemptyset(&sock_kill_action.sa_mask);
	sock_kill_action.sa_handler = sock_kill_handler;
	sock_kill_action.sa_flags = 0;
	sigaction(SIGTERM, &sock_kill_action, NULL);
	sigaction(SIGHUP, &sock_kill_action, NULL);
	sigaction(SIGINT, &sock_kill_action, NULL);

	parse_args(argc, argv);

	uid = getuid();
	if (uid != 0) {
		DIAG_LOGE("diag_socket_log: You must be root to run diag_socket_log."
			"Exiting ...\n");
		exit(0);
	}

	/* If another instance of diag_socket_log is to be killed off */
	if (kill_socket_log) {
		stop_socket_log(pid_file);
		exit(0);
	}

	/* Make sure the default directory exists so the diag_socket.pid file
	 * can be placed there
	 */
	if (mkdir(PID_DIR, 0660))
		if (errno != EEXIST)
			DIAG_LOGE("diag_socket_log: Fail creating directory for diag_socket.pid file");

	/*
	 * Determine our process id and write it to the file so that
	 * when we want to stop diag_socket_log we know its process id
	 */
	unlink(pid_file);
	if ((pid_fd = open(pid_file, O_RDWR | O_CREAT | O_EXCL | O_SYNC,
								0660)) < 0) {
		DIAG_LOGE("\n diag_socket_log: Unable to open Pid File,"
			"errno: %d\n ODL feature will loose data\n", errno);
	} else {
		pid = getpid();
		memset(pid_buff, 0, 10);
		snprintf(pid_buff, 10, "%d", pid);
		write(pid_fd, pid_buff, 10);
		close(pid_fd);
	}

	if (!ip_addr_entered) {
		DIAG_LOGE("diag_socket_log: No ip address entered. You must enter an "
				"ip address.  Exiting...\n");
		usage(argv[0]);
		exit(0);
	}

	/* Initialize the Diag LSM userspace library */
	bInit_Success = Diag_LSM_Init(NULL);
	if (!bInit_Success) {
		DIAG_LOGE("diag_socket_log: Diag_LSM_Init() failed. Exiting...\n");
		return -1;
	}
	DIAG_LOGE("diag_socket_log: Diag_LSM_Init succeeded.\n");

/* Acquire wakelock if the client is requesting for wakelock*/
	if(enable_wakelock == 1) {
		diag_wakelock_init(DIAG_SOCKET_WAKELOCK_NAME);
		diag_wakelock_acquire();
	}
	max_channels = diag_get_max_channels();
	sockets =  malloc(max_channels * sizeof(struct diag_socket));
	if (sockets == NULL) {
		DIAG_LOGE("diag_socket_log: Unable to allocate memory for socket structures. Exiting ...\n");

		/* Need to cleanup the diag client items already established */
		if(enable_wakelock == 1) {
			diag_wakelock_release();
			diag_wakelock_destroy();
		}
		Diag_LSM_DeInit();
		return -1;
	}

	for (i = 0; i < max_channels; i++)
	{
		sockets[i].fd = -1;
		sockets[i].id = -1;
		sockets[i].connected = 0;
	}


	remote_ids = 0;
	num_sockets = check_for_remote(&remote_ids);
	DIAG_LOGE("diag_socket_log: socket_count is: %d\n", num_sockets);


	/* Set up for connecting to the MSM channel */
	num_connected = 0;
	num_sockets = 1;
	if (remote_ids && device_num && (device_num & remote_ids)) {
		sockets[0].id = device_num;
	} else {
		device_num = 0;
		sockets[0].id = MSM;
	}

	keep_reading = 1;

	/*
	 * Register callback function to be called if
	 * remote device data is discovered
	 */
	status = diag_register_socket_cb(diag_socket_callback, sockets);

	while (keep_logging) {
		/* If we need to open a new socket connection */
		if (num_connected < num_sockets) {
			for (i  = 0; i < num_sockets; i++)
			{
				if (sockets[i].fd == -1) {
					connected = open_connection(&sockets[i]);
					if (connected) {
						num_connected++;
						sockets[i].connected = 1;
						diag_set_socket_fd(sockets[i].id, sockets[i].fd);
						if (sockets[i].fd >= select_fd)
							select_fd = sockets[i].fd + 1;
					} else {
						DIAG_LOGE("diag_socket_log: Not able to successfully "
							"connect after %d retries.  Exiting ...\n",
							num_connect_retries);
						break;
					}
				}
			}
		}
		if (num_connected == num_sockets) {
			diag_switch_logging(SOCKET_MODE, NULL);
			keep_reading = 1;
		} else if (!keep_logging) {
			DIAG_LOGE("diag_socket_log: Diag logging mode changed "
					"away from sockets. Exiting ...\n");
		} else {
			break;
		}

		while ((num_connected > 0) && keep_reading) {
			/* Enter sockets that need to be monitored for reading */
			FD_ZERO(&read_fs);
			for (i  = 0; i < num_sockets; i++)
			{
				if ((sockets[i].fd != -1) && sockets[i].connected) {
					FD_SET(sockets[i].fd, &read_fs);
				}
			}

			/*
			 * Determine is any of the sockets have data that needs to be read.
			 * Note that select() will put the process/thread to sleep until
			 * there is data to be read on at least one of the sockets.
			 */
			status = pselect(select_fd, &read_fs, NULL, NULL, NULL,&pselect_set);
			if (status < 0) {
				DIAG_LOGE("diag_socket_log: Error calling select, %s, errno: %d\n",
					strerror(errno), errno);
				if (!keep_logging) {
					/* Thread is being shutdown.  Fall out of the loop
					   to allow this function to end. */
					break;
				}
			}

			for (i = 0; i < num_sockets; i++) {
				if (sockets[i].fd != -1) {
					/* If the socket has data to be read */
					if (FD_ISSET(sockets[i].fd, &read_fs)) {
						num_read = read_socket(&sockets[i]);
						/* If an error occurred, determine if we need to reconnect */
						if ((num_read <= 0) && (num_connected < num_sockets)) {
							keep_reading = 0;

							if (!keep_logging) {
								/* Thread is being shutdown.  Fall out of the loop
								   to allow this function to end. */
								break;
							}
						}
					}
				}
			}

			if (!keep_logging) {
				DIAG_LOGE("diag_socket_log: Diag logging mode changed away from sockets.\n");
				DIAG_LOGE("diag_socket_log: Closing connection. Exiting ...\n");
				keep_reading = 0;
				break;
			}
		}
	}

	DIAG_LOGE("diag_socket_log: keep_logging: %d, fell out of loop. preparing to exit\n", keep_logging);

	bDeInit_Success = Diag_LSM_DeInit();
	DIAG_LOGE("diag_socket_log: Diag_LSM_DeInit() status: %d, exiting ...\n", bDeInit_Success);

	for (i = 0; i < max_channels; i++)
	{
		if (sockets[i].fd != -1)
			close(sockets[i].fd);
		sockets[i].fd = -1;
		sockets[i].connected = 0;
	}

	free(sockets);

	if(enable_wakelock == 1) {
		diag_wakelock_release();
		diag_wakelock_destroy();
	}
	return 0;
}
