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

# this has to be included after target-arch and ARCH_MK
# so that CC and TARGET are known

CFLAGS += $(OPTIMIZE) $(DEBUGFLAGS) $(DEFINES) $(INCLUDES)
CFLAGS += $(CFG_DEFINES)
CFLAGS += $(PML_TARGET_CFLAGS)

TARGET_DEF := TARGET_$(shell echo -n "$(TARGET)" | tr -sc '[A-Za-z0-9]' _ | tr '[a-z]' '[A-Z]')
CFLAGS += -D$(TARGET_DEF) -DTARGET_NAME="\"$(TARGET)\""

# gcc version specific flags
ifndef GCCVERFLAGS
ifneq ($(CC),)
GCCVER := $(shell $(CC) -dumpversion 2>/dev/null | cut -f1 -d.)
endif
ifneq ($(GCCVER),)
ifeq ($(shell [ $(GCCVER) -ge 7 ] && echo y),y)
#GCCVERFLAGS += -Wno-format-truncation
GCCVERFLAGS += -Wno-error=format-truncation
endif
ifeq ($(shell [ $(GCCVER) -ge 8 ] && echo y),y)
#GCCVERFLAGS += -Wno-stringop-truncation
GCCVERFLAGS += -Wno-error=stringop-truncation
endif
endif
endif
CFLAGS += $(GCCVERFLAGS)

