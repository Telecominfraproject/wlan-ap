#include <sys/ioctl.h>
#include <sys/socket.h>

#include "atenl.h"

int atenl_eth_init(struct atenl *an)
{
	struct sockaddr_ll addr = {};
	struct ifreq ifr = {};
	int ret;
 
	if (!an->bridge_name) {
		perror("Bridge name not specified");
		ret = -EINVAL;
		goto out;
	}

	memcpy(ifr.ifr_name, an->bridge_name, strlen(an->bridge_name));
	ret = socket(PF_PACKET, SOCK_RAW, htons(ETH_P_RACFG));
	if (ret < 0) {
		perror("socket");
		goto out;
	}
	an->sock_eth = ret;

	addr.sll_family = AF_PACKET;
	addr.sll_ifindex = if_nametoindex(an->bridge_name);

	ret = bind(an->sock_eth, (struct sockaddr *)&addr, sizeof(addr));
	if (ret < 0) {
		perror("bind");
		goto out;
	}

	ret = ioctl(an->sock_eth, SIOCGIFHWADDR, &ifr);
	if (ret < 0) {
		perror("ioctl(SIOCGIFHWADDR)");
		goto out;
	}

	memcpy(an->mac_addr, ifr.ifr_hwaddr.sa_data, ETH_ALEN);
	atenl_info("Open Ethernet socket success on %s, mac addr = " MACSTR "\n",
		   an->bridge_name, MAC2STR(an->mac_addr));

	ret = 0;
out:
	return ret;
}

int atenl_eth_recv(struct atenl *an, struct atenl_data *data)
{
	struct ethhdr *hdr;
	int len;

	len = recvfrom(an->sock_eth, data->buf, sizeof(data->buf), 0, NULL, NULL);

	if (len < ETH_HLEN + RACFG_HLEN) {
		atenl_err("packet len is too short: %d\n", len);
		return -EINVAL;
	}

	hdr = (struct ethhdr *)data->buf;
	if (hdr->h_proto != htons(ETH_P_RACFG)) {
		atenl_err("invalid protocol type\n");
		return -EINVAL;
	}

	if (!ether_addr_equal(an->mac_addr, hdr->h_dest) &&
	    !is_broadcast_ether_addr(hdr->h_dest)) {
		atenl_err("invalid dest MAC\n");
		return -EINVAL;
	}

	data->len = len;

	return 0;
}

int atenl_eth_send(struct atenl *an, struct atenl_data *data)
{
	struct ethhdr *ehdr = (struct ethhdr *)data->buf;
	struct sockaddr_ll addr = {};
	int ret, len = data->len;

	if (an->unicast)
		ether_addr_copy(ehdr->h_dest, ehdr->h_source);
	else
		eth_broadcast_addr(ehdr->h_dest);

	ether_addr_copy(ehdr->h_source, an->mac_addr);
	ehdr->h_proto = htons(ETH_P_RACFG);

	if (len < 60)
		len = 60;
	else if (len > 1514) {
		atenl_err("response ethernet length is too long\n");
		return -1;
	}

	atenl_dbg_print_data(data->buf, __func__, len);

	addr.sll_family = PF_PACKET;
	addr.sll_protocol = htons(ETH_P_RACFG);
	addr.sll_ifindex = if_nametoindex(an->bridge_name);
	addr.sll_pkttype = PACKET_BROADCAST;
	addr.sll_hatype = ARPHRD_ETHER;
	addr.sll_halen = ETH_ALEN;
	memset(addr.sll_addr, 0, 8);
	eth_broadcast_addr(addr.sll_addr);

	ret = sendto(an->sock_eth, data->buf, len, 0,
		     (struct sockaddr *)&addr, sizeof(addr));
	if (ret < 0) {
		perror("sendto");
		return ret;
	}
	
	return 0;
}
