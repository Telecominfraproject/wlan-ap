#ifndef __BACKPORT_LINUX_THERMAL_H
#define __BACKPORT_LINUX_THERMAL_H
#include_next <linux/thermal.h>
#include <linux/version.h>

#ifdef CONFIG_THERMAL
#if LINUX_VERSION_IS_LESS(5,9,0)
static inline int thermal_zone_device_enable(struct thermal_zone_device *tz)
{ return 0; }
#endif /* < 5.9.0 */
#else /* CONFIG_THERMAL */
#if LINUX_VERSION_IS_LESS(5,9,0)
static inline int thermal_zone_device_enable(struct thermal_zone_device *tz)
{ return -ENODEV; }
#endif /* < 5.9.0 */
#endif /* CONFIG_THERMAL */

#if LINUX_VERSION_IS_LESS(5,9,0)
#define thermal_zone_device_enable LINUX_BACKPORT(thermal_zone_device_enable)
static inline int thermal_zone_device_enable(struct thermal_zone_device *tz)
{ return 0; }

#define thermal_zone_device_disable LINUX_BACKPORT(thermal_zone_device_disable)
static inline int thermal_zone_device_disable(struct thermal_zone_device *tz)
{ return 0; }
#endif /* < 5.9 */

#endif /* __BACKPORT_LINUX_THERMAL_H */
