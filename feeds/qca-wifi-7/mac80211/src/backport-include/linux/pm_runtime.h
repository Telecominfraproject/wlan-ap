#ifndef __BACKPORT_PM_RUNTIME_H
#define __BACKPORT_PM_RUNTIME_H
#include_next <linux/pm_runtime.h>
#include <linux/version.h>

#if LINUX_VERSION_IS_LESS(5,10,0)
#define pm_runtime_resume_and_get LINUX_BACKPORT(pm_runtime_resume_and_get)

/**
 * pm_runtime_resume_and_get - Bump up usage counter of a device and resume it.
 * @dev: Target device.
 *
 * Resume @dev synchronously and if that is successful, increment its runtime
 * PM usage counter. Return 0 if the runtime PM usage counter of @dev has been
 * incremented or a negative error code otherwise.
 */
static inline int pm_runtime_resume_and_get(struct device *dev)
{
	int ret;

	ret = __pm_runtime_resume(dev, RPM_GET_PUT);
	if (ret < 0) {
		pm_runtime_put_noidle(dev);
		return ret;
	}

	return 0;
}
#endif /* < 5.10 */

#endif /* __BACKPORT_PM_RUNTIME_H */