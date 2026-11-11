/* SPDX-License-Identifier:	GPL-2.0+ */
/*
 * Copyright (C) 2021 MediaTek Incorporation. All Rights Reserved.
 *
 * Author: Alvin Kuo <Alvin.Kuo@mediatek.com>
 */

#ifndef __COMMON_H__
#define __COMMON_H__

#include <sys/types.h>
#include <time.h>

#define LOG_FMT(FMT) "[TOPS_TOOL] [%s]: " FMT, __func__

#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))

int time_to_str(time_t *time_sec, char *time_str, unsigned int time_str_size);
int mkdir_p(const char *path, mode_t mode);

#endif /* __COMMON_H__ */
