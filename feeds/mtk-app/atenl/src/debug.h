/* Copyright (C) 2021-2022 Mediatek Inc. */
#ifndef __ATENL_DEBUG_H
#define __ATENL_DEBUG_H

/* #define CONFIG_ATENL_DEBUG     1 */
/* #define CONFIG_ATENL_DEBUG_VERBOSE     1 */

#define atenl_info(fmt, ...)	(void)fprintf(stdout, fmt, ##__VA_ARGS__)
#define atenl_err(fmt, ...)	(void)fprintf(stderr, fmt, ##__VA_ARGS__)
#ifdef CONFIG_ATENL_DEBUG
#define atenl_dbg(fmt, ...)	atenl_info(fmt, ##__VA_ARGS__)
#else
#define atenl_dbg(fmt, ...)
#endif

static inline void
atenl_dbg_print_data(const void *data, const char *func_name, u32 len)
{
#ifdef CONFIG_ATENL_DEBUG_VERBOSE
	u32 *tmp = (u32 *)data;
	int i;

	for (i = 0; i < DIV_ROUND_UP(len, 4); i++)
		atenl_dbg("%s: [%d] = 0x%08x\n", func_name, i, tmp[i]);
#endif
}

/* #define debug_print(fmt, ...) \ */
/* 	do { if (DEBUG) fprintf(stderr, fmt, __VA_ARGS__); } while (0) */

#endif
