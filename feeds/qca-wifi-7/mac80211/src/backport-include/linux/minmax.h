#include <linux/version.h>

#if LINUX_VERSION_IS_GEQ(5,10,0)
#include_next <linux/minmax.h>
#else
#include <linux/kernel.h>
#endif
