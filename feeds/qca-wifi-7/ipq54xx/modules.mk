define KernelPackage/bootconfig
  SUBMENU:=Other modules
  TITLE:=Bootconfig partition for failsafe
  KCONFIG:=CONFIG_BOOTCONFIG_PARTITION
  FILES:=$(LINUX_DIR)/drivers/platform/ipq/bootconfig.ko@ge6.1
  AUTOLOAD:=$(call AutoLoad,56,bootconfig,1)
endef

define KernelPackage/bootconfig/description
  Bootconfig partition for failsafe
endef

$(eval $(call KernelPackage,bootconfig))

