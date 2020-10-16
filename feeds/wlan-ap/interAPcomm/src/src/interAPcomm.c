/* SPDX-License-Identifier BSD-3-Clause */
#include <stdio.h>
#include <sys/socket.h>
#include <errno.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <interAPcomm.h>

/*Receiver socket*/
int recv_sock = -1;

recv_arg ra;

static void receive_data(struct ev_loop *ev, ev_io *io, int event)
{
	void *recv_data;
	int recv_data_len;

	recv_data = malloc(ra.len);
	memset(recv_data, 0, ra.len);
	if ((recv_data_len = recvfrom(recv_sock, recv_data, ra.len,
				      0, NULL, 0)) < 0)
		printf("recvfrom() failed");

	ra.cb(recv_data);

}

int interap_recv(unsigned short port, int (*recv_cb)(void *), unsigned int len,
		 struct ev_loop *loop, ev_io *io)
{
	struct sockaddr_in addr;
	int bcast_perm;

	ra.cb = recv_cb;
	ra.len = len;

	if ( port == 0 || recv_cb == NULL || len == 0) {
		printf("Error: port=%d, recv_cb =%p, len = %d", port, recv_cb,
			len);
		return -1;
	}

	if ((recv_sock = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
		printf("socket failed");
		return -1;
	}

	/* Set socket to allow broadcast */
	bcast_perm = 1;
	if (setsockopt(recv_sock, SOL_SOCKET, SO_BROADCAST, (void *) &bcast_perm,
			sizeof(bcast_perm)) < 0)
		printf("setsockopt failed");

	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(INADDR_ANY);  /* Any incoming interface */
	addr.sin_port = htons(port);

	if (bind(recv_sock, (struct sockaddr *) &addr, sizeof(addr)) < 0){
		printf("bind failed");
		return -1;
	}

	printf("Interap recving: p:%d\n", port);
	ev_io_init(io, receive_data, recv_sock, EV_READ);
	ev_io_start(loop, io);

	return 0;
}

int interap_send(unsigned short port, char *dst_ip, void *data,
		 unsigned int len)
{
	int sock;
	struct sockaddr_in addr;
	int bcast_perm;
	unsigned int slen;

	if (port == 0 || dst_ip == NULL || data == NULL || len == 0)
		return -1;

	if ((sock = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
		printf("socket failed");
		return -1;
	}

	/* Set socket to allow broadcast */
	bcast_perm = 1;
	if (setsockopt(sock, SOL_SOCKET, SO_BROADCAST, (void *) &bcast_perm,
			sizeof(bcast_perm)) < 0)
		printf("setsockopt() failed");

	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = inet_addr(dst_ip);
	addr.sin_port = htons(port);

	slen = sendto(sock, data, len, 0, (struct sockaddr *)
		      &addr, sizeof(addr));

	if (slen != len)
		printf("Err(%s:%d):sendto: sent %d bytes than expected %d\n",
			strerror(errno), errno, slen, len);

	close(sock);


	return 0;
}
