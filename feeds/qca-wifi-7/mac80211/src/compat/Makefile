ccflags-y += -I$(src) -Wframe-larger-than=1280
ifeq ($(CONFIG_BACKPORT_INTEGRATE),)
obj-m += compat.o
else
obj-y += compat.o
endif
compat-y += main.o

# Kernel backport compatibility code
compat-$(CPTCFG_KERNEL_4_18) += backport-4.18.o
compat-$(CPTCFG_KERNEL_4_19) += backport-4.19.o
compat-$(CPTCFG_KERNEL_5_2) += backport-5.2.o backport-genetlink.o
compat-$(CPTCFG_KERNEL_5_3) += backport-5.3.o
compat-$(CPTCFG_KERNEL_5_5) += backport-5.5.o
compat-$(CPTCFG_KERNEL_5_9) += backport-5.9.o
compat-$(CPTCFG_KERNEL_5_10) += backport-5.10.o
compat-$(CPTCFG_KERNEL_5_11) += backport-5.11.o
compat-$(CPTCFG_KERNEL_5_13) += backport-5.13.o
compat-$(CPTCFG_KERNEL_5_15) += backport-5.15.o
compat-$(CPTCFG_KERNEL_6_4) += backport-6.4.o
