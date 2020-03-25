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

###############################################################################
#
# OS application layer library
#
###############################################################################
UNIT_NAME := osa

# Template type:
UNIT_TYPE := LIB

UNIT_SRC := src/os_backtrace.c
UNIT_SRC += src/os_socket.c
UNIT_SRC += src/os_nif_linux.c
UNIT_SRC += src/os_regex.c
UNIT_SRC += src/os_proc.c
UNIT_SRC += src/os_file_ops.c
UNIT_SRC += src/os_file.c
UNIT_SRC += src/os_random.c

UNIT_CFLAGS := -I$(UNIT_PATH)/inc
ifeq ($(BUILD_WITH_LIBGCC_BACKTRACE),y)
UNIT_CFLAGS += -fasynchronous-unwind-tables
endif

UNIT_LDFLAGS := -lpthread
ifeq ($(BUILD_WITH_LIBGCC_BACKTRACE),y)
UNIT_LDFLAGS += -lgcc_s
endif

UNIT_EXPORT_CFLAGS := $(UNIT_CFLAGS)
UNIT_EXPORT_LDFLAGS := $(UNIT_LDFLAGS)

# don't include this in EXPORT_CFLAGS
UNIT_CFLAGS += -Isrc/wm/lm
UNIT_CFLAGS += -DWITH_LIBGCC_BACKTRACE

UNIT_DEPS := src/lib/common
UNIT_DEPS += src/lib/ds
UNIT_DEPS += src/lib/target
UNIT_DEPS += src/lib/log
