# SPDX-License-Identifier: ISC
#obj-$(CONFIG_ATH5K)		+= ath5k/
#obj-$(CONFIG_ATH9K_HW)		+= ath9k/
#obj-$(CONFIG_CARL9170)		+= carl9170/
#obj-$(CONFIG_ATH6KL)		+= ath6kl/
#obj-$(CONFIG_AR5523)		+= ar5523/
obj-$(CPTCFG_WIL6210)		+= wil6210/
#obj-$(CONFIG_ATH10K)		+= ath10k/
#obj-$(CONFIG_WCN36XX)		+= wcn36xx/
obj-$(CPTCFG_ATH11K)		+= ath11k/
obj-$(CPTCFG_ATH12K)		+= ath12k/

obj-$(CPTCFG_ATH_COMMON)	+= ath.o

ath-objs :=	main.o \
		regd.o \
		hw.o \
		key.o \
		dfs_pattern_detector.o \
		dfs_pri_detector.o \
		ath_fse.o \
		ath_accel_cfg.o

ath-$(CPTCFG_ATH_DEBUG) += debug.o
ath-$(CPTCFG_ATH_TRACEPOINTS) += trace.o

CFLAGS_trace.o := -I$(src)

ifeq ($(CONFIG_KERNEL_ATH_GCOV_PROFILE),)
GCOV_PROFILE:=y
endif
