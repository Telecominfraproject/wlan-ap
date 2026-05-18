/*
 * sys_switch_ioctl.h: switch(ioctl) set API
 */

#ifndef SYS_SWITCH_IOCTL_H
#define SYS_SWITCH_IOCTL_H
#include <stdint.h>

#define ETH_DEVNAME "eth0"

#define RAETH_MII_READ                  0x89F3
#define RAETH_MII_WRITE                 0x89F4
#define RAETH_ESW_PHY_DUMP              0x89F7
#define RAETH_MII_READ_CL45             0x89FC
#define RAETH_MII_WRITE_CL45            0x89FD

struct esw_reg {
	uint32_t off;
	uint32_t val;
};

struct ra_mii_ioctl_data {
	uint16_t phy_id;
	uint16_t reg_num;
	uint32_t val_in;
	uint32_t val_out;
};

int sys_cl22_mdio_read(uint8_t phyaddr, uint16_t reg, uint16_t *value);
int sys_cl22_mdio_write(uint8_t phyaddr, uint16_t reg, uint16_t value);
int sys_cl45_mdio_read(uint8_t phyaddr, uint8_t mmd,
			uint16_t reg, uint16_t *value);
int sys_cl45_mdio_write(uint8_t phyaddr, uint8_t mmd,
			uint16_t reg, uint16_t value);

int switch_ioctl_init(void);

#endif /* SYS_SWITCH_IOCTL_H */
