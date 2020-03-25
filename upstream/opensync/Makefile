# Copyright (c) 2015, Plume Design Inc. All rights reserved.
# 
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#    1. Redistributions of source code must retain the above copyright
#       notice, this list of conditions and the following disclaimer.
#    2. Redistributions in binary form must reproduce the above copyright
#       notice, this list of conditions and the following disclaimer in the
#       documentation and/or other materials provided with the distribution.
#    3. Neither the name of the Plume Design Inc. nor the
#       names of its contributors may be used to endorse or promote products
#       derived from this software without specific prior written permission.
# 
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL Plume Design Inc. BE LIABLE FOR ANY
# DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
# (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
# LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
# ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
# SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

default: all

include build/color.mk

# Default configuration
include build/default.mk
include build/flags.mk

# Modify default configuration for external vendors
include build/vendor.mk

# Include target-specific configuration
include build/target-arch.mk

# Include vendor-specific configuration
include build/vendor-arch.mk

# KConfig configuration
include build/kconfig.mk

# Create CFG_DEFINES based on configuration options
include build/cfg-defines.mk

# Vendor defines
include build/vendor-defines.mk

ifneq ($(_OVERRIDE_MAIN_MAKEFILE),1)

.PHONY: all build_all clean distclean FORCE

all: build_all

world: build_all
	$(MAKE) openwrt_all

# Include architecture specific makefile
include $(ARCH_MK)

include build/flags2.mk
include build/dirs.mk
include build/verbose.mk
include build/version.mk
include build/git.mk
include build/unit-build.mk
include build/tags.mk
include build/app_install.mk
include build/ovsdb.mk
include build/rootfs.mk
include build/schema.mk
include build/devshell.mk
include build/help.mk
include build/doc.mk

build_all: workdirs schema-check unit-install

clean: unit-clean
	$(NQ) " $(call color_clean,clean)   [$(call COLOR_BOLD,workdir)] $(WORKDIR)"
	$(Q)$(RM) -r $(WORKDIR)

DISTCLEAN_TARGETS := clean

distclean: $(DISTCLEAN_TARGETS)
	$(NQ) " cleanup all artifacts"
	$(Q)$(RM) -r $(WORKDIRS) tags cscope.out files.idx .files.idx.dep

ifneq ($(filter-out $(OS_TARGETS),$(TARGET)),)
$(error Unsupported TARGET ($(TARGET)). Supported targets are: \
	$(COL_CFG_GREEN)$(OS_TARGETS)$(COL_CFG_NONE))
endif

# Include makefile for target-specific rules, if it exists
TARGET_MAKEFILE ?= $(VENDOR_DIR)/Makefile
-include $(TARGET_MAKEFILE)

# backward compatibility
SDK_DIR    ?= $(OWRT_ROOT)
SDK_ROOTFS ?= $(OWRT_ROOTFS)
INSTALL_ROOTFS_DIR ?= $(SDK_ROOTFS)
ifeq ($(INSTALL_ROOTFS_DIR),)
INSTALL_ROOTFS_DIR = $(WORKDIR)/rootfs-install
endif

endif # _OVERRIDE_MAIN_MAKEFILE

