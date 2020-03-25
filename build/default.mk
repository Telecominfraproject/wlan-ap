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

# Default definitions
TOP_SDK                     := ../sdk
DEFAULT_IMAGE_TYPE          := squashfs
DEFAULT_TARGET              := native
INSTALL_PREFIX              ?= /usr/plume
ROOTFS_COMPONENTS           ?= common target/$(TARGET) $(ROOTFS_PROFILE_COMPONENTS)

# Build features
CFG_DEFINES :=

# specifies whether bhaul links should use WDS or GRE
BUILD_BHAUL_WDS             ?= n
# specifies whether Band Steering is built or not
BUILD_BAND_STEERING         ?= y
# specifies whether Speed test is built or not
BUILD_SPEED_TEST            ?= n
# specifies whether logs should have PLUME prefix or not
BUILD_LOG_PREFIX_PLUME      ?= n
# specifies whether logs should include hostname
BUILD_LOG_HOSTNAME          ?= n
# specifies whether queue manager is built or not
BUILD_QM                    ?= y
# merge all static libraries to a single shared library and link that
BUILD_SHARED_LIB            ?= y
# specifies whether queue stats are built or not
BUILD_CAPACITY_QUEUE_STATS  ?= n
# specifies whether client nickname support is built or not
BUILD_CLIENT_NICKNAME       ?= n
# specifies whether client freeze support is built or not
BUILD_CLIENT_FREEZE         ?= n
# remote mqtt logging
BUILD_REMOTE_LOG            ?= y
# backtrace based on libgcc_s
BUILD_WITH_LIBGCC_BACKTRACE ?= y


# Default shell commands and flags

MAKEFLAGS := --no-print-directory

CP       = cp
MKDIR    = mkdir -p
DIRNAME  = dirname
CAT      = cat
SED      = sed
MV       = mv -f
RM       = rm -f
CHMOD    = chmod
INSTALL  = install -D
TAR      = tar
GREP     = grep

SRCEXT   = cpp
SRCEXT.c = c
DEPEXT   = d
OBJEXT   = o

