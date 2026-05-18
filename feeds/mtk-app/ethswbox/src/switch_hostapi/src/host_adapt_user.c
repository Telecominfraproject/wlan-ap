/******************************************************************************

   Copyright 2023-2024 MaxLinear, Inc.

   For licensing information, see the file 'LICENSE' in the root folder of
   this software module.

******************************************************************************/

#include <unistd.h>
#include "host_adapt.h"
#include "sys_switch_ioctl.h"

#define SMDIO_ADDR 0x10

static void __usleep(unsigned long usec)
{
	/* TO be replaced with OS dependent implementation */
	usleep(usec);
}

static pthread_mutex_t lock;

static void __lock(void *lock_data)
{
	pthread_mutex_lock(lock_data);
}

static void __unlock(void *lock_data)
{
	pthread_mutex_unlock(lock_data);
}

static int mdiobus_read(void *mdiobus_data, uint8_t phyaddr, uint8_t mmd,
			uint16_t reg)
{
	uint16_t val=0;
	int ret;

	if (phyaddr > 31 || reg > GSW_MMD_REG_DATA_LAST)
		return -EINVAL;

	if (mmd == GSW_MMD_DEV)
		ret = sys_cl45_mdio_read(phyaddr, mmd, reg, &val);
	else if (mmd == GSW_MMD_SMDIO_DEV)
		ret = sys_cl22_mdio_read(phyaddr, reg, &val);
	else
		return -EINVAL;

	return ret < 0 ? ret : (int)val;
}

static int mdiobus_write(void *mdiobus_data, uint8_t phyaddr, uint8_t mmd,
			 uint16_t reg, uint16_t val)
{
	if (phyaddr > 31 || reg > GSW_MMD_REG_DATA_LAST)
		return -EINVAL;

	if (mmd == GSW_MMD_DEV)
		return sys_cl45_mdio_write(phyaddr, mmd, reg, val);
	else if (mmd == GSW_MMD_SMDIO_DEV)
		return sys_cl22_mdio_write(phyaddr, reg, val);
	else
		return -EINVAL;
}

static GSW_Device_t gsw_dev = {0};


/* TO be adapted  with target dependent implementation */
int gsw_adapt_init(void)
{
	gsw_dev.usleep = __usleep;

	gsw_dev.lock = __lock;
	gsw_dev.unlock = __unlock;
	gsw_dev.lock_data = &lock;

	gsw_dev.mdiobus_read = mdiobus_read;
	gsw_dev.mdiobus_write = mdiobus_write;
	gsw_dev.mdiobus_data = NULL;

	gsw_dev.phy_addr = SMDIO_ADDR;
	gsw_dev.smdio_phy_addr = SMDIO_ADDR;

	return 0;
}

int32_t api_gsw_get_links(char* lib)
{
	gsw_adapt_init();
	switch_ioctl_init();
	return 0;
}

GSW_Device_t* gsw_get_struc(uint8_t lif_id,uint8_t phy_id)
{
   return &gsw_dev;
}

int gsw_read(const GSW_Device_t *dev, uint32_t regaddr)
{
	return dev->mdiobus_read(dev->mdiobus_data, dev->phy_addr, GSW_MMD_DEV,
				 regaddr);
}

int gsw_write(const GSW_Device_t *dev, uint32_t regaddr, uint16_t data)
{
	return dev->mdiobus_write(dev->mdiobus_data, dev->phy_addr, GSW_MMD_DEV,
				  regaddr, data);
}
