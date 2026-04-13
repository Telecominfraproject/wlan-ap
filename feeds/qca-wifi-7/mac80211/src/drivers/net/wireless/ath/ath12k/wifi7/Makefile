# SPDX-License-Identifier: BSD-3-Clause-Clear
obj-$(CPTCFG_ATH12K) += ath12k_wifi7.o
ath12k_wifi7-y += core.o \
	          pci.o \
	          wmi.o \
	          mhi.o \
	          ce.o \
	          hw.o \
		  hal_tx.o \
		  hal_rx.o \
		  dp_rx.o \
		  dp_tx.o \
		  dp.o \
		  hal.o \
		  hal_qcn9274.o \
		  hal_wcn7850.o \
		  hal_mon.o \
		  hal_mon_qcn9274.o \
		  hal_mon_wcn7850.o \
		  dp_mon1.o \
		  dp_mon2.o \
		  dp_mon1_filter.o

ath12k_wifi7-$(CPTCFG_QCN_EXTN) += qcn_extns/wifi7_hw_extn.o

ath12k_wifi7-$(CPTCFG_ATH12K_AHB) += ahb.o

ifeq ($(CONFIG_KERNEL_ATH_GCOV_PROFILE),)
GCOV_PROFILE:=y
endif
