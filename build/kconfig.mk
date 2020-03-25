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

#
# Each target platform must define the location of the Kconfig configuration
# file by setting the KCONFIG_TARGET variable. If the variable is empty, a
# default config file will be used (see KCONFIG_DEFAULT below).
#
# The most prominent variables used throughout this makefile:
#
# KCONFIG_TARGET - Path to the current Kconfig configuration file. This is a
#                  distilled version of the file (diff), meaning that it
#                  doesn't contain options that are not different from the
#                  defaults
# KCONFIG_DEFAULT- Path to the defualt kconfig file, if none is set
#
# KCONFIG        - This either points to KCONFIG_TARGET or KCONFIG_DEFAULT
#
# KCONFIG_WORK   - Full Kconfig file generate from KCONFIG; this is what is
#                  currently being used as the "real" kconfig file
#
# KWORKDIR       - Working directory, this is where KCONFIG_WORK will be stored

KWORKDIR := work/$(TARGET)/kconfig
KCONFIG_DEFAULT:=kconfig/targets/config_default

# For locally installed kconfiglib using PIP
export PATH:=${PATH}:${HOME}/.local/bin

ifeq ($(shell tput colors),256)
export MENUCONFIG_STYLE?=
MENUCONFIG_STYLE+=list=fg:\#3b396f,bg:\#efefef
MENUCONFIG_STYLE+=body=fg:\#3b396f,bg:\#ffffff
MENUCONFIG_STYLE+=selection=fg:\#ffffff,bg:\#b4ad99
MENUCONFIG_STYLE+=path=fg:\#000000,bg:\#c6c6c6
MENUCONFIG_STYLE+=frame=fg:\#ffffff,bg:\#6168fd
MENUCONFIG_STYLE+=inv-list=fg:\#17e3ad,bg:\#efefef
MENUCONFIG_STYLE+=inv-selection=fg:\#17e3ad,bg:\#948d79
MENUCONFIG_STYLE+=text=list
MENUCONFIG_STYLE+=help=path
MENUCONFIG_STYLE+=separator=frame
MENUCONFIG_STYLE+=edit=path
MENUCONFIG_STYLE+=jump-edit=path
else
export MENUCONFIG_STYLE?=
MENUCONFIG_STYLE+=list=fg:black,bg:white,bold
MENUCONFIG_STYLE+=body=fg:magenta,bg:white,bold
MENUCONFIG_STYLE+=selection=fg:white,bg:black,bold
MENUCONFIG_STYLE+=path=fg:black,bg:white,bold
MENUCONFIG_STYLE+=edit=fg:white,bg:magenta,bold
MENUCONFIG_STYLE+=frame=fg:white,bg:magenta,bold
MENUCONFIG_STYLE+=inv-list=fg:cyan,bg:white
MENUCONFIG_STYLE+=inv-selection=fg:cyan,bg:black,bold
MENUCONFIG_STYLE+=text=list
MENUCONFIG_STYLE+=help=path
MENUCONFIG_STYLE+=separator=frame
MENUCONFIG_STYLE+=jump-edit=path
endif

ifeq ($(wildcard $(KCONFIG_TARGET)),)
$(warning $(call COLOR_YELLOW,No kconfig for target $(TARGET).) Using default: $(KCONFIG_DEFAULT))
KCONFIG:=$(KCONFIG_DEFAULT)
else
$(info Using configuration file: $(KCONFIG_TARGET))
KCONFIG:=$(KCONFIG_TARGET)
endif

# KCONFIG_WORK is the full (it includes defualt options) configuration file that will be generated from KCONFIG.
KCONFIG_WORK:=$(KWORKDIR)/$(notdir $(KCONFIG))

include $(KCONFIG_WORK)

$(KCONFIG_WORK): $(KCONFIG)
	@echo "$(call COLOR_GREEN,Generating kconfig file and headers: $(KCONFIG_WORK))"
	$(Q)mkdir -p "$(KWORKDIR)"
	$(Q)kconfig/diffconfig --unminimize  --config "$(KCONFIG)" --out "$(KCONFIG_WORK)" kconfig/Kconfig
	$(Q)KCONFIG_CONFIG="$(KCONFIG_WORK)" genconfig kconfig/Kconfig --header-path "$(KCONFIG_WORK).h"

.PHONY: _kconfig_target_check
_kconfig_target_check:
	$(Q)[ -n "$(KCONFIG_TARGET)" ] || { echo "$(call COLOR_RED,ERROR:) KCONFIG_TARGET not set. Please define KCONFIG_TARGET for your target before calling menuconfig."; exit 1; }

.PHONY: _kconfig_update
_kconfig_update:
# Write out minimized config
	@echo "$(call COLOR_GREEN,Generating minimized configuration: $(KCONFIG))"
	$(Q)kconfig/diffconfig --config "$(KCONFIG_WORK)" --out "$(KCONFIG)" kconfig/Kconfig

.PHONY: _menuconfig
_menuconfig: $(KCONFIG_WORK)
	$(Q)KCONFIG_CONFIG="$(KCONFIG_WORK)" menuconfig kconfig/Kconfig

.PHONY: menuconfig
menuconfig: _kconfig_target_check _menuconfig _kconfig_update

kconfig_install:
	pip3 install --user kconfiglib==10.40.0
