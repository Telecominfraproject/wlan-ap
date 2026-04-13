#ifndef __BACKPORT_MM_H
#define __BACKPORT_MM_H
#include_next <linux/mm.h>
#include <linux/overflow.h>

#if LINUX_VERSION_IS_LESS(4,18,0)
#define kvcalloc LINUX_BACKPORT(kvcalloc)
static inline void *kvcalloc(size_t n, size_t size, gfp_t flags)
{
	return kvmalloc_array(n, size, flags | __GFP_ZERO);
}
#endif /* < 4.18 */

#endif /* __BACKPORT_MM_H */
