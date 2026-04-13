#ifndef _BP_LINUX_INTERRUPT_H
#define _BP_LINUX_INTERRUPT_H
#include <linux/version.h>
#include_next <linux/interrupt.h>

#if LINUX_VERSION_IS_LESS(5,9,0)

static inline void
tasklet_setup(struct tasklet_struct *t,
	      void (*callback)(struct tasklet_struct *))
{
	void (*cb)(unsigned long data) = (void *)callback;

	tasklet_init(t, cb, (unsigned long)t);
}

#define from_tasklet(var, callback_tasklet, tasklet_fieldname) \
	container_of(callback_tasklet, typeof(*var), tasklet_fieldname)

#endif

#if LINUX_VERSION_IS_LESS(5,13,0)

#define tasklet_unlock_spin_wait LINUX_BACKPORT(tasklet_unlock_spin_wait)
#if defined(CONFIG_SMP) || defined(CONFIG_PREEMPT_RT)
void tasklet_unlock_spin_wait(struct tasklet_struct *t);

#else
static inline void tasklet_unlock_spin_wait(struct tasklet_struct *t) { }
#endif

/*
 * Do not use in new code. Disabling tasklets from atomic contexts is
 * error prone and should be avoided.
 */
#define tasklet_disable_in_atomic LINUX_BACKPORT(tasklet_disable_in_atomic)
static inline void tasklet_disable_in_atomic(struct tasklet_struct *t)
{
	tasklet_disable_nosync(t);
	tasklet_unlock_spin_wait(t);
	smp_mb();
}
#endif

#endif /* _BP_LINUX_INTERRUPT_H */
