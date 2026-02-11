#ifndef __BACKPORT_LINUX_SYSFS_H
#define __BACKPORT_LINUX_SYSFS_H
#include_next <linux/sysfs.h>
#include <linux/version.h>

#ifndef __ATTR_RW
#define __ATTR_RW(_name) __ATTR(_name, (S_IWUSR | S_IRUGO),		\
			 _name##_show, _name##_store)
#endif

#if LINUX_VERSION_IS_LESS(5,10,0)
#define sysfs_emit LINUX_BACKPORT(sysfs_emit)
#define sysfs_emit_at LINUX_BACKPORT(sysfs_emit_at)
#ifdef CONFIG_SYSFS
__printf(2, 3)
int sysfs_emit(char *buf, const char *fmt, ...);
__printf(3, 4)
int sysfs_emit_at(char *buf, int at, const char *fmt, ...);
#else /* CONFIG_SYSFS */
__printf(2, 3)
static inline int sysfs_emit(char *buf, const char *fmt, ...)
{
	return 0;
}

__printf(3, 4)
static inline int sysfs_emit_at(char *buf, int at, const char *fmt, ...)
{
	retur 0;
}
#endif /* CONFIG_SYSFS */
#endif /* < 5.10 */

#ifndef __ATTR_RW_MODE
#define __ATTR_RW_MODE(_name, _mode) {					\
	.attr	= { .name = __stringify(_name),				\
		    .mode = VERIFY_OCTAL_PERMISSIONS(_mode) },		\
	.show	= _name##_show,						\
	.store	= _name##_store,					\
}
#endif

#endif /* __BACKPORT_LINUX_SYSFS_H */
