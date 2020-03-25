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


VERSION_TARGET ?= $(TARGET)
VER_GEN := src/lib/version/version-gen
VER_STAMP := src/lib/version/rootfs-version-stamp
VER_DATE := $(shell date)
VERSION_STAMP_DIR ?= $(INSTALL_PREFIX)

VERSION_ENV += VER_DATE="$(VER_DATE)"
VERSION_ENV += TARGET=$(TARGET)
VERSION_ENV += VERSION_TARGET=$(VERSION_TARGET)
VERSION_ENV += VENDOR_DIR=$(VENDOR_DIR)
VERSION_ENV += LAYER_LIST="$(LAYER_LIST)"
VERSION_ENV += BUILD_NUMBER=$(BUILD_NUMBER)
VERSION_ENV += IMAGE_DEPLOYMENT_PROFILE=$(IMAGE_DEPLOYMENT_PROFILE)
VERSION_ENV += SDK_DIR=$(SDK_DIR)
VERSION_ENV += SDK_BASE=$(SDK_BASE)
VERSION_ENV += VERSION_NO_BUILDNUM=$(VERSION_NO_BUILDNUM)
VERSION_ENV += VERSION_NO_SHA1=$(VERSION_NO_SHA1)
VERSION_ENV += VERSION_NO_MODS=$(VERSION_NO_MODS)
VERSION_ENV += VERSION_NO_PROFILE=$(VERSION_NO_PROFILE)
VERSION_ENV += VERSION_STAMP_DIR=$(VERSION_STAMP_DIR)

ifeq ($(BUILD_NUMBER),)
BUILD_NUMBER := $(shell $(VERSION_ENV) $(VER_GEN) build_number)
endif

define version-gen
	$(VERSION_ENV) $(2) $(VER_GEN) $(1)
endef

define rootfs-version-stamp
	$(VERSION_ENV) $(2) $(VER_STAMP) $(1)
endef

# $(info VERSION ENV: $(call ver-env))

version:
	$(Q) $(call version-gen) version

version_long:
	$(Q) $(call version-gen) make

version_matrix:
	$(Q) $(call version-gen) matrix

version_json:
	$(Q) $(call version-gen) json_matrix
