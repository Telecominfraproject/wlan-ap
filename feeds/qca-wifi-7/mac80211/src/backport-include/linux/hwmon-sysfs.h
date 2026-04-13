#ifndef __BACKPORT_LINUX_HWMON_SYSFS_H
#define __BACKPORT_LINUX_HWMON_SYSFS_H
#include_next <linux/hwmon-sysfs.h>

#ifndef SENSOR_DEVICE_ATTR_RO
#define SENSOR_DEVICE_ATTR_RO(_name, _func, _index)		\
	SENSOR_DEVICE_ATTR(_name, 0444, _func##_show, NULL, _index)
#endif

#ifndef SENSOR_DEVICE_ATTR_RW
#define SENSOR_DEVICE_ATTR_RW(_name, _func, _index)		\
	SENSOR_DEVICE_ATTR(_name, 0644, _func##_show, _func##_store, _index)
#endif

#endif /* __BACKPORT_LINUX_HWMON_SYSFS_H */
