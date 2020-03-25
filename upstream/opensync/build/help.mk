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

.PHONY: help
help:
	$(NQ) "Makefile help"
	$(NQ) ""
	$(NQ) "Makefile commands:"
	$(NQ) "   all                       builds all enabled units"
	$(NQ) "   os                        builds device image based on specified SDK"
	$(NQ) "   tags                      Creates CTAGS for src directories"
	$(NQ) "   cscope                    Creates CSCOPE for the same directories as ctags"
	$(NQ) "   clean                     Removes generated, compiled objects"
	$(NQ) "   distclean                 Invokes clean and also cleans the ctags and cscope files"
	$(NQ) "   devshell                  Run shell with all environment variables set for TARGET"
	$(NQ) ""
	$(NQ) "Build Unit commands:"
	$(NQ) "   unit-all                  Build ALL active units"
	$(NQ) "   unit-install              Build and install ALL active units"
	$(NQ) "   unit-clean                Clean ALL active units"
	$(NQ) "   unit-list                 List ALL active units"
	$(NQ) ""
	$(NQ) "   UNIT_PATH/clean           Clean a single UNIT"
	$(NQ) "   UNIT_PATH/rclean          Clean a single UNIT and its dependencies"
	$(NQ) "   UNIT_PATH/compile         Compile a single UNIT and its dependencies"
	$(NQ) "   UNIT_PATH/install         Install UNIT products to target rootfs"
	$(NQ) ""
	$(NQ) "Control variables:"
	$(NQ) "   V                         make verbose level. (values: 0, 1)"
	$(NQ) "                               default = 0"
	$(NQ) "   TARGET                    Target identifier. See Supported targets."
	$(NQ) "                               default: $(DEFAULT_TARGET)"
	$(NQ) "                               current: $(TARGET)"
	$(NQ) "                             Supported targets:"
	@for x in $(OS_TARGETS); do echo "                               "$$x; done
	$(NQ) "   IMAGE_TYPE                squashfs (FLASH) or initramfs(BOOTP),"
	$(NQ) "                               default: $(DEFAULT_IMAGE_TYPE)"
	$(NQ) "   IMAGE_DEPLOYMENT_PROFILE  Supported deployment profiles:"
	@for x in $(VALID_IMAGE_DEPLOYMENT_PROFILES); do echo "                               "$$x; done
	$(NQ) ""
