#ifndef _BACKPORT_LINUX_NOSPEC_H
#define _BACKPORT_LINUX_NOSPEC_H

#if LINUX_VERSION_IS_GEQ(4,15,2) || \
    LINUX_VERSION_IN_RANGE(4,14,18, 4,15,0)
#include_next <linux/nospec.h>
#else
#define array_index_nospec(index, size)	(index)
#endif

#endif /* _BACKPORT_LINUX_NOSPEC_H */
