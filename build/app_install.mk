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

ifdef STRIP
define app_strip
	$(NQ) " $(call color_strip,strip)   [$(call COLOR_BOLD,$(notdir $1))] $(1)"
	$(Q)$(STRIP) $(1)
endef
else # ! STRIP
define app_strip
endef
endif

define app_install_rootfs_path
$(APP_ROOTFS)$(INSTALL_PREFIX)/$(if $(2),$(2),bin)/$(notdir $1)
endef

define app_install_rootfs
	$(Q)$(INSTALL) -m 755 $(1) $(call app_install_rootfs_path,$(1),$(2))
	$(Q)$(call app_strip,$(call app_install_rootfs_path,$(1),$(2)))
endef

ifndef unit_test_install
define unit_test_install
	$(NQ) "  $(call color_install,unit test install skipped)"
endef
endif

ifndef app_install
define app_install
	$(NQ) " $(call color_install,install) [$(call COLOR_BOLD,$(notdir $1))] $(1)"
	$(call app_install_rootfs,$(1),$(2))
endef
endif

define app_uninstall
	$(Q)$(RM) $(call app_install_rootfs_path,$(1),$(2))
endef
