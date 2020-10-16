/* SPDX-License-Identifier BSD-3-Clause */
#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include <unistd.h>
#include <ev.h>
#include <interAPcomm.h>

struct my_data {
	int x;
	char y;
	int z;
};


int recv_process(void *data) {
	struct my_data *dat = (struct my_data*) data;

	printf("Recv process: %d, %c, %d\n", dat->x, dat->y, dat->z);
	return 0;
}

int main (int argc, char *argv[ ])
{
	unsigned int send = atoi(argv[1]);   /* First arg: broadcast port */
	unsigned short port = 50000;
//	char *dst_ip = "255.255.255.255";
	char *dst_ip = "192.168.9.255";
//	char *data = "InterAP Hello";
	struct my_data data;
	data.x = 1001;
	data.y = 'H';
	data.z = 3003;

//	callback cb = recv_process;


	printf("arg1 = %d\n", send);

	if (send) {
		printf("Send");
		interap_send(port, dst_ip, &data, sizeof(data));
	}
	else {
		printf("Recieve");
//		interap_recv(port, cb, sizeof(struct my_data));
	}

	while (1)
	{
		sleep(3);
		printf("In while loop\n");
	}
	return 1;
}
