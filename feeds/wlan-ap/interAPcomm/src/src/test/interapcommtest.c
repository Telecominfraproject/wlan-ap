/* SPDX-License-Identifier BSD-3-Clause */
#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include <unistd.h>
#include <ev.h>
#include <interAPcomm.h>

#include <libubus.h>

struct my_data {
	int x;
	char y;
	int z;
};


int recv_process(void *data, ssize_t n)
{
	struct my_data *dat = (struct my_data*) data;

	printf("Recv process: %d, %c, %d\n", dat->x, dat->y, dat->z);
	return 0;
}

int main (int argc, char *argv[ ])
{
	unsigned int send = atoi(argv[1]);
	unsigned short port = 50020;
//	char *dst_ip = "255.255.255.255";
	char *dst_ip = "10.42.0.255";
//	char *data = "InterAP Hello";
	struct my_data data;
	data.x = 1001;
	data.y = 'H';
	data.z = 3003;

	printf("send = %d\n", send);

	if (send) {
		printf("Send");
		while (1)
		{
			sleep(3);
			printf("Sending...\n");
			interap_send(port, dst_ip, &data, sizeof(data));
		}
	}
	else {
		printf("Recieve");
		uloop_init();
		callback cb = recv_process;

		interap_recv(port, cb, sizeof(struct my_data), NULL, NULL);
		uloop_run();
		uloop_done();

	}

	return 1;
}
