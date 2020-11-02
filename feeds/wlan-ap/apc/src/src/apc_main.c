/* SPDX-License-Identifier: BSD-3-Clause */
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <pthread.h>
#include <sys/errno.h>
#include <getopt.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <net/if.h>
#include <ev.h>
#include <interap/interAPcomm.h>

#include <nest/apcn.h>
#include <apc.h>
#include <protocol.h>

static ev_io iac_io;
static ev_timer  check_timer;
static unsigned int CheckIp;
static int CheckCount;


list iface_list;
struct apc_iface * apc_ifa = NULL;
struct apc_proto * ApcProto;
unsigned int MyIpAddr = 0;
unsigned int WaitingToReelect = 0;
unsigned int ApcGrpId = 0;
unsigned char MyBasicMac[6];
unsigned int FloatingIpSaved = 0;
unsigned int RestartProxy = 0;
struct apc_spec ApcSpecSaved;

int recv_sock_fd = -1;

void * mb_allocz(unsigned size)
{
	printf("mb_allocz %u\n", size);
	return(calloc(1, size));
}

void mb_free(void * m)
{
	printf("mb_free %p\n", m);
	free(m);
}

timer * tm_new(void)
{
	timer *tt = malloc(sizeof(timer));

	printf("tm_new %p\n", tt);
	return(tt);
}

#define MAX_TIM_NUM     100
timer *Timers[MAX_TIM_NUM];

void tm_start(timer *tm, unsigned after)
{
	int i;
	
	printf("tm_start %p %u (%p)\n", tm, after, tm->hook);
	/* First, try to find if this timer was already started */
	for(i=0; i<MAX_TIM_NUM; i++)
	{
		if (Timers[i] == tm)
		{
			tm->expires = after;
			return;
		}
	}
	/* Now, try to find empty slot */
	for(i=0; i<MAX_TIM_NUM; i++)
	{
		if (Timers[i] == NULL)
		{
			tm->expires = after;
			Timers[i] = tm;
			return;
		}
	}
}

void tm_stop(timer * tm)
{
	int i;
	
	printf("tm_stop %p\n", tm);
	for(i=0; i<MAX_TIM_NUM; i++)
	{
		if (Timers[i] == tm)
		{
			Timers[i] = NULL;
			return;
		}
	}
}

static void timers_go(void)
{
	int i;
	timer * tm;
	
	for(i=0; i<MAX_TIM_NUM; i++)
	{
		if (Timers[i])
		{
			tm = Timers[i];
			tm->expires -= 1;
			if (tm->expires == 0)
			{
				printf("=== calling hook %p\n", tm->hook);
				tm->hook(tm);
				if (tm->recurrent)
					tm->expires = tm->recurrent;
				else
					Timers[i] = NULL;
			}
		}
	}
}

u32 u32_mkmask(unsigned n)
{
	printf("u32_mkmask %u\n", n);
	return(0xFFFFFF00);
}

int u32_masklen(u32 x)
{
	printf("u32_mkmask %x\n", x);
	return(24);
}

int receive_from_socket(void *data, ssize_t n)
{
	unsigned char *cdata = (unsigned char *)data;

	struct apc_neighbor * neigh;
	unsigned int ipaddr;

	if (!WaitingToReelect)
	{
		ipaddr = ntohl(*((unsigned int *)&(cdata[12])));
		neigh = find_neigh_by_ip(apc_ifa, ipaddr);
		
		printf("received from IAP (%d) (%x) %x %x %x %x %x %x %x %x %x %x\n",
			n, ipaddr, cdata[0], cdata[1], cdata[2], cdata[3],
			cdata[4], cdata[5], cdata[6], cdata[7], cdata[8],
			cdata[9]);

		apc_receive_hello(&(cdata[0]), apc_ifa, neigh, ipaddr, n);
	}
	return(0);
}

int get_mac_addr(const char *iface, unsigned char *mac)
{

	struct ifreq ifr;
	int ret = 0;
	int fd = socket(PF_INET, SOCK_DGRAM, IPPROTO_IP);

	strncpy(ifr.ifr_name, iface, IFNAMSIZ-1);
	ret = ioctl(fd, SIOCGIFHWADDR, &ifr);

	if (ret == 0)
	{
		memset(mac, 0, 6);
		memcpy(mac, ifr.ifr_hwaddr.sa_data, 6);
	}
	
	close(fd);
	
	return ret;
}


static void GetLocalIpv4Addr(unsigned char * IpAddr, const char *iface)
{
	int fd, rc;
	struct ifreq ifr;
	
	fd = socket(AF_INET, SOCK_DGRAM, 0);
	ifr.ifr_addr.sa_family = AF_INET;
	strncpy(ifr.ifr_name, iface, IFNAMSIZ-1);
	
	rc = ioctl(fd, SIOCGIFADDR, &ifr);
	close(fd);
	
	if (rc == 0)
	{
		/* IP address bytes will be in network order */
		memcpy(IpAddr,
		      &(((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr.s_addr),
			 4);
	}
	else
		memset(IpAddr, 0, 4);

	return;
}

/*************************************/
/* library functions */
/*************************************/
int br_socket_fd = -1;
int set_socket(void)
{
	if (br_socket_fd == -1)
		br_socket_fd = socket(AF_LOCAL, SOCK_STREAM, 0);

	if (br_socket_fd < 0)
		return -1;

	return(0);
}

/*************************************/


static void check_timer_handler(struct ev_loop *loop, ev_timer *timer,
				 int revents) 
{
	timers_go();
	if (WaitingToReelect)
	{
		WaitingToReelect -= 1;
		if (WaitingToReelect == 0);
		{
			apc_ifa->state = APC_IS_DOWN;
			apc_iface_sm(apc_ifa, ISM_UP);
		}
	}

	CheckCount += 1;
	if (CheckCount == 10)
	{
		/* Check IP address periodically - if it's different from what
		 it was on startup, exit and nanny will re-start us.*/
		GetLocalIpv4Addr((unsigned char *)&CheckIp, "br-wan");
		CheckIp = ntohl(CheckIp);
		if (CheckIp && (MyIpAddr != CheckIp))
		{
			printf("IP address changed from %x to %x - restart APC election\n", MyIpAddr, CheckIp);
			exit(0);
		}
		
		CheckCount = 0;
		if (ApcSpecSaved.IsApc == I_AM_APC)
		{
		//Radius stuff
		}
	}
}

int main(int argc, char *const* argv)
{
	struct proto_config c;
	struct proto * apc_proto;
	struct ev_loop *loop = EV_DEFAULT;

	/*Socket*/
	set_socket();

	/*Radius stuff*/

	printf("Basic MAC\n");
	memset(MyBasicMac, 0, 6);
	if (get_mac_addr("br-wan", MyBasicMac) == 0) {
		printf("APC: br-wan mac:%02X:%02X:%02X:%02X:%02X:%02X\n",
								MyBasicMac[0],
								MyBasicMac[1],
								MyBasicMac[2],
								MyBasicMac[3],
								MyBasicMac[4],
								MyBasicMac[5]);
	}
	else {
		printf("Failed to get AP br-wan mac\n");
	}

	/*get local ip of br-wan*/
	MyIpAddr = 0;
	while(1)
	{
		GetLocalIpv4Addr((unsigned char *)&MyIpAddr, "br-wan");
		MyIpAddr = ntohl(MyIpAddr);
		if (MyIpAddr && (MyIpAddr != 0xC0A8010A))
			break;

		sleep(1);
	}

	/*listening interAP*/
	callback cb = receive_from_socket;
	if (interap_recv(IAC_APC_ELECTION_PORT, cb, 1000,
			 loop, &iac_io) < 0)
		printf("Error: Failed InterAP receive");

	memset(Timers, 0, sizeof(Timers));
	
	memset(&c, 0, sizeof(struct proto_config));
	c.protocol = &proto_apc;
	c.name = proto_apc.name;
	c.preference = proto_apc.preference;
	c.debug = 1;
	c.router_id = MyIpAddr;
	apc_proto = proto_apc.init(&c);
	ApcProto = (struct apc_proto *)apc_proto;
	proto_apc.start(apc_proto);

	ev_timer_init(&check_timer, check_timer_handler, 1, 1);

	ev_timer_start(loop, &check_timer);

	ev_run(loop, 0);

	return(1);
}

