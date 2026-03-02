/*
 * Copyright (c) 2020 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <string.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <unistd.h>
#include <netdb.h>
#include <ifaddrs.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>

#define PORT 5002
#define DIAG_PORT 5003

#define BIT(n) (1 << n)

#define PING BIT(0)
#define PONG BIT(1)

char filename[200];
char client_message[2000];
char diag_buffer[1024];
socklen_t addrlen;
int rbytes = 0;
int diag_rbytes = 0;
int new_diag_socket, new_qld_socket;
int udpfd;
int bytes = 1024;

struct sockaddr_in qld_serverAddr;
struct sockaddr_in qld_udpAddr;
struct sockaddr_in qld_servAddr;
struct sockaddr qld_ackAddr;
socklen_t acklen;
struct sockaddr_in diag_serverAddr;
struct sockaddr_in udpaddr;
int stop_data_thread = 0;
int start_data_streaming = 0;

void * control_thread(void *arg)
{
	int start_req_flag = 0;
	int start_accept_flag = 0;
	int stop_req_flag = 0;
	int stop_accept_flag = 0;
	char start_command[] = {0x7E, 0x01, 0x05, 0x00, 0x4b,
				0x5a, 0x03, 0x01, 0x01, 0x7E};
	char stop_command[] = {0x7E, 0x01, 0x05, 0x00, 0x4b,
				0x5a, 0x03, 0x01, 0x00, 0x7E};
	int i;

	/* the diag start pattern is
	 * 0x7E 0x01 0x05 0x00 0x4b 0x5a 0x03 0x01 0x01 0x7E */
	/* the diag stop pattern is
	 * 0x7E 0x01 0x05 0x00 0x4b 0x5a 0x03 0x01 0x00 0x7E */
	do {
		start_req_flag = 0;
		rbytes = recv(new_qld_socket, client_message,
			sizeof(client_message), 0);

		if (!(memcmp(start_command, client_message, sizeof(start_command)))) {
			printf("Received  a start command\n");
			start_req_flag = 1;
		}
		if (!(memcmp(stop_command, client_message, sizeof(stop_command)))) {
			printf("Received  a stop command\n");
			stop_req_flag = 1;
		}

		send(new_diag_socket, client_message, rbytes, 0);

		diag_rbytes = recv(new_diag_socket, diag_buffer,
				sizeof(diag_buffer), 0);
		if ((start_req_flag) && (diag_buffer[0] == 0x7E) &&
				(diag_buffer[4] != 0x13)) {
			printf("Received response for start command\n");
			start_accept_flag = 1;
		}
		send(new_qld_socket, diag_buffer, diag_rbytes, 0);
		if (start_req_flag && start_accept_flag) {
			start_data_streaming = 1;
			start_req_flag = start_accept_flag = 0;
		}
		if (stop_req_flag && stop_accept_flag) {
			start_data_streaming = 0;
			stop_req_flag = stop_accept_flag = 0;
			break;
		}
	} while (1);
	stop_data_thread = 1;
	pthread_exit(NULL);
}

int main(int argc, char *argv[])
{
	int qld_socket, diag_socket;
	int qld_port_num;
	int diag_port_num;
	int stream_port_num;
	int yes = 1;
	pthread_t control_t;
	pthread_t data_t;
	int sendbytes = bytes;
	int flags;

	if (argc != 4) {
		fprintf(stderr, "Usage %s <qld_port> <diag_port> stream_port\n", argv[0]);
		exit(1);
	} else {
		qld_port_num = atoi(argv[1]);
		diag_port_num = atoi(argv[2]);
		stream_port_num = atoi(argv[3]);
	}

	//Create the socket.
	qld_socket = socket(PF_INET, SOCK_STREAM, 0);
	// Configure settings of the server address struct
	// Address family = Internet
	qld_serverAddr.sin_family = AF_INET;
	//Set port number, using htons function to use proper byte order
	qld_serverAddr.sin_port = htons(qld_port_num);
	//Set IP address to localhost
	qld_serverAddr.sin_addr.s_addr = htonl(INADDR_ANY);
	//Set all bits of the padding field to 0
	memset(qld_serverAddr.sin_zero, '\0', sizeof(qld_serverAddr.sin_zero));
	//Bind the address struct to the socket
	if (setsockopt(qld_socket, SOL_SOCKET, SO_REUSEADDR, &yes,
				sizeof(int)) == -1) {
		perror("setsockopt");
		exit(1);
	}

	//Create the socket.
	diag_socket = socket(PF_INET, SOCK_STREAM, 0);
	// Configure settings of the server address struct
	// Address family = Internet
	diag_serverAddr.sin_family = AF_INET;
	//Set port number, using htons function to use proper byte order
	diag_serverAddr.sin_port = htons(diag_port_num);
	//Set IP address to localhost
	diag_serverAddr.sin_addr.s_addr = htonl(INADDR_ANY);
	//Set all bits of the padding field to 0
	memset(diag_serverAddr.sin_zero, '\0', sizeof(diag_serverAddr.sin_zero));
	//Bind the address struct to the socket
	if (setsockopt(diag_socket, SOL_SOCKET, SO_REUSEADDR, &yes,
				sizeof(int)) == -1) {
		perror("setsockopt");
		exit(1);
	}

	/* TODO FIXME - all error handling */
	if (bind(qld_socket, (struct sockaddr *) &qld_serverAddr,
				sizeof(qld_serverAddr))) {
		printf("Could not bind to qld_socket port num %d\n",  qld_port_num);
		exit(-1);
	}

	if (bind(diag_socket, (struct sockaddr *) &diag_serverAddr,
				sizeof(diag_serverAddr))) {
		printf("Could not bind to diag_socket port num %d\n",  diag_port_num);
		exit(-1);
	}

	/* First let us establish connection with diagsocketapp */
	if(listen(diag_socket,1)==0) {
		printf("Listening for diagsocketapp\n");
	} else {
		printf("Could not listen to diagsocketapp\n");
		exit(-1);
	}
	addrlen = sizeof(diag_serverAddr);
	new_diag_socket = accept(diag_socket, (struct sockaddr *)
			&diag_serverAddr, &addrlen);

	printf("diag connected , socket fd is %d , ip is : %s , port : %d\n",
			new_diag_socket,
			inet_ntoa(diag_serverAddr.sin_addr),
			ntohs(diag_serverAddr.sin_port));

	//Listen on the socket, with 40 max connection requests queued
	if(listen(qld_socket, 1)==0) {
		printf("Listening for QLD connection\n");
	} else {
		printf("could not listen on qldsocket\n");
		exit(-1);
	}
	addrlen = sizeof(qld_serverAddr);
	new_qld_socket = accept(qld_socket, (struct sockaddr *)
			&qld_serverAddr, &addrlen);

	printf("QLD connected , socket fd is %d , ip is : %s , port : %d\n",
			new_qld_socket,
			inet_ntoa(qld_serverAddr.sin_addr),
			ntohs(qld_serverAddr.sin_port));

	if (pthread_create(&control_t, NULL, control_thread, NULL) != 0) {
		printf("Failed to create control thread\n");
		exit(1);
	}

	pthread_join(control_t, NULL);

	return 0;
}


