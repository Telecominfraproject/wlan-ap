/*
 * sys_switch_ioctl.c: switch(ioctl) set API
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <linux/if.h>
#include "sys_switch_ioctl.h"

#define MTK_PHYIAC_PHY_ACS_ST 0x8000
#define MTK_PHYIAC_MDIO_PHY_ADDR_SHFT 5

static int esw_fd;

int switch_ioctl_init(void)
{
	esw_fd = socket(AF_INET, SOCK_DGRAM, 0);
	if (esw_fd < 0) {
		perror("socket");
		return -EINVAL;
	}

	return 0;
}

void switch_ioctl_fin(void)
{
	close(esw_fd);
}

int sys_cl22_mdio_read(uint8_t phyaddr, uint16_t reg, uint16_t *value)
{
	struct ra_mii_ioctl_data mii;
	struct ifreq ifr;

	strncpy(ifr.ifr_name, ETH_DEVNAME, 5);
	ifr.ifr_data = &mii;

	mii.phy_id = phyaddr;
	mii.reg_num = reg;

	if (-1 == ioctl(esw_fd, RAETH_MII_READ, &ifr)) {
		perror("ioctl");
		close(esw_fd);
		exit(0);
	}
	*value = mii.val_out;

	return 0;
}

int sys_cl22_mdio_write(uint8_t phyaddr, uint16_t reg, uint16_t value)
{
	struct ra_mii_ioctl_data mii;
	struct ifreq ifr;

	strncpy(ifr.ifr_name, ETH_DEVNAME, 5);
	ifr.ifr_data = &mii;

	mii.phy_id = phyaddr;
	mii.reg_num = reg;
	mii.val_in = value;

	if (-1 == ioctl(esw_fd, RAETH_MII_WRITE, &ifr)) {
		perror("ioctl");
		close(esw_fd);
		exit(0);
	}
	return 0;
}


int sys_cl45_mdio_read(uint8_t phyaddr, uint8_t mmd,
			uint16_t reg, uint16_t *value)
{
	struct ra_mii_ioctl_data mii;
	uint16_t reg_value;
	struct ifreq ifr;

	strncpy(ifr.ifr_name, ETH_DEVNAME, 5);
	ifr.ifr_data = &mii;

	reg_value = MTK_PHYIAC_PHY_ACS_ST |
		(phyaddr << MTK_PHYIAC_MDIO_PHY_ADDR_SHFT) | mmd;

	mii.phy_id = reg_value;
	mii.reg_num = reg;

	if (-1 == ioctl(esw_fd, RAETH_MII_READ_CL45, &ifr)) {
		perror("ioctl");
		close(esw_fd);
		exit(0);
	}
	*value = mii.val_out;
	return 0;
}

int sys_cl45_mdio_write(uint8_t phyaddr, uint8_t mmd,
			uint16_t reg, uint16_t value)
{
	struct ra_mii_ioctl_data mii;
	uint16_t reg_value;
	struct ifreq ifr;

	strncpy(ifr.ifr_name, ETH_DEVNAME, 5);
	ifr.ifr_data = &mii;

	reg_value = MTK_PHYIAC_PHY_ACS_ST |
		(phyaddr << MTK_PHYIAC_MDIO_PHY_ADDR_SHFT) | mmd;

	mii.phy_id = reg_value;
	mii.reg_num = reg;
	mii.val_in = value;

	if (-1 == ioctl(esw_fd, RAETH_MII_WRITE_CL45, &ifr)) {
		perror("ioctl");
		close(esw_fd);
		exit(0);
	}
	return 0;
}
