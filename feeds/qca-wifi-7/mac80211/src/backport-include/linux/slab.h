#ifndef __BACKPORT_SLAB_H
#define __BACKPORT_SLAB_H
#include_next <linux/slab.h>
#include <linux/version.h>

#ifdef CPTCFG_MAC80211_ATHMEMDEBUG
#include <linux/athdebug_slab.h>
#endif

#ifdef CPTCFG_MAC80211_ATHMEMDEBUG
#include <linux/athdebug_slab.h>
#endif

#if LINUX_VERSION_IS_LESS(5,9,0)
#define kfree_sensitive(x)	kzfree(x)
#endif

#endif /* __BACKPORT_SLAB_H */
