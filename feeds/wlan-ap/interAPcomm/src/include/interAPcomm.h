/* SPDX-License-Identifier BSD-3-Clause */
#include <ev.h>
int interap_send(unsigned short port, char *dst_ip,
		 void *data, unsigned int len);
int interap_recv(unsigned short port, int (*recv_cb)(void *, ssize_t),
		 unsigned int len, struct ev_loop *loop,
		 ev_io *io);
void interap_rcv_close(void);

typedef int (*callback)(void *, ssize_t);
typedef struct recv_arg {
	callback cb;
	unsigned int len;
}recv_arg;

#define IAC_IFACE "br-wan"
