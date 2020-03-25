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

COL_CFG_GRAY=[30;1m
COL_CFG_RED=[31;1m
COL_CFG_GREEN=[32;1m
COL_CFG_YELLOW=[33;1m
COL_CFG_BLUE=[34;1m
COL_CFG_MAGENTA=[35;1m
COL_CFG_CYAN=[36;1m
COL_CFG_WHITE=[37;1m
COL_CFG_BOLD=[1;1m
COL_CFG_NONE=[0;0m

#
# The variables below are suitable for calling with $(call ...):
#     echo $(call COLOR_RED,Error: Build failed)
#
COLOR_GRAY=$(COL_CFG_GRAY)$1$(COL_CFG_NONE)
COLOR_RED=$(COL_CFG_RED)$1$(COL_CFG_NONE)
COLOR_GREEN=$(COL_CFG_GREEN)$1$(COL_CFG_NONE)
COLOR_YELLOW=$(COL_CFG_YELLOW)$1$(COL_CFG_NONE)
COLOR_BLUE=$(COL_CFG_BLUE)$1$(COL_CFG_NONE)
COLOR_MAGENTA=$(COL_CFG_MAGENTA)$1$(COL_CFG_NONE)
COLOR_CYAN=$(COL_CFG_CYAN)$1$(COL_CFG_NONE)
COLOR_WHITE=$(COL_CFG_WHITE)$1$(COL_CFG_NONE)
COLOR_BOLD=$(COL_CFG_BOLD)$1$(COL_CFG_NONE)

IS_TTY ?= $(shell tty -s && echo 1 || echo 0)

ifneq ($(IS_TTY),1)
COL_CFG_GRAY=
COL_CFG_RED=
COL_CFG_GREEN=
COL_CFG_YELLOW=
COL_CFG_BLUE=
COL_CFG_MAGENTA=
COL_CFG_CYAN=
COL_CFG_WHITE=
COL_CFG_BOLD=
COL_CFG_NONE=
endif

define color_external
$(COLOR_BLUE)
endef
define color_gendep
$(COLOR_GRAY)
endef
define color_strip
$(COLOR_GRAY)
endef
define color_install
$(COLOR_GREEN)
endef
define color_compile
$(COLOR_YELLOW)
endef
define color_compgen
$(COLOR_BLUE)
endef
define color_generate
$(COLOR_BLUE)
endef
define color_copy
$(COLOR_YELLOW)
endef
define color_clean
$(COLOR_MAGENTA)
endef
define color_link
$(COLOR_CYAN)
endef
define color_target
$(COLOR_BOLD)
endef
define color_callmakefile
$(COLOR_BLUE)
endef
define color_profile
$(COL_CFG_RED)$(1)$(COL_CFG_NONE)
endef


