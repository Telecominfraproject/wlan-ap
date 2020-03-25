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

# Handle vendor and platform repositories not part of core:

ifeq ($(ALL_VENDORS),)
SHOW_VENDOR_INFO = 1
endif
ifeq ($(ALL_PLATFORMS),)
SHOW_VENDOR_INFO = 1
endif

ALL_VENDORS := $(shell [ -d vendor ] && ls vendor)
ALL_PLATFORMS := $(shell [ -d platform ] && ls platform)

export ALL_VENDORS
export ALL_PLATFORMS

ifeq ($(SHOW_VENDOR_INFO),1)
ifneq ($(ALL_VENDORS),)
$(info All vendors: $(ALL_VENDORS))
endif

ifneq ($(ALL_PLATFORMS),)
$(info All platforms: $(ALL_PLATFORMS))
endif
endif

# Include platform-specific defaults
-include platform/*/build/default.mk

# Include vendor-specific defaults
-include vendor/*/build/default.mk

