#ifndef __BP_ATOMIC_H
#define __BP_ATOMIC_H
#include_next <linux/atomic.h>

/* these were introduced together, so just a single check is enough */
#ifndef atomic_try_cmpxchg_acquire
#ifndef atomic_try_cmpxchg
#define __atomic_try_cmpxchg(type, _p, _po, _n)				\
({									\
	typeof(_po) __po = (_po);					\
	typeof(*(_po)) __r, __o = *__po;				\
	__r = atomic_cmpxchg##type((_p), __o, (_n));			\
	if (unlikely(__r != __o))					\
		*__po = __r;						\
	likely(__r == __o);						\
})

#define atomic_try_cmpxchg(_p, _po, _n)		__atomic_try_cmpxchg(, _p, _po, _n)
#define atomic_try_cmpxchg_relaxed(_p, _po, _n)	__atomic_try_cmpxchg(_relaxed, _p, _po, _n)
#define atomic_try_cmpxchg_acquire(_p, _po, _n)	__atomic_try_cmpxchg(_acquire, _p, _po, _n)
#define atomic_try_cmpxchg_release(_p, _po, _n)	__atomic_try_cmpxchg(_release, _p, _po, _n)
#else /* atomic_try_cmpxchg */
#define atomic_try_cmpxchg_relaxed	atomic_try_cmpxchg
#define atomic_try_cmpxchg_acquire	atomic_try_cmpxchg
#define atomic_try_cmpxchg_release	atomic_try_cmpxchg
#endif /* atomic_try_cmpxchg */

#endif /* atomic_try_cmpxchg_acquire */

#if LINUX_VERSION_IS_LESS(4,19,0)
#ifndef atomic_fetch_add_unless
static inline int atomic_fetch_add_unless(atomic_t *v, int a, int u)
{
	return __atomic_add_unless(v, a, u);
}
#endif
#endif

#ifndef __atomic_pre_full_fence
#define __atomic_pre_full_fence         smp_mb__before_atomic
#endif

#ifndef __atomic_post_full_fence
#define __atomic_post_full_fence        smp_mb__after_atomic
#endif

#endif /* __BP_ATOMIC_H */
