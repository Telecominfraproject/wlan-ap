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
# Schema
#
###############################################################################
UNIT_NAME := schema
UNIT_TYPE := LIB

# Source folder
SCHEMA := interfaces/plume.ovsschema

# Source files
UNIT_SRC := src/schema.c

# Additional object files
UNIT_CLEAN := $(UNIT_BUILD)/schema_gen.h
UNIT_CLEAN += $(UNIT_BUILD)/schema_pre.h

# Flags for building this unit
UNIT_CFLAGS := -I$(UNIT_BUILD) -I$(UNIT_PATH)/inc
UNIT_CFLAGS += -Isrc/lib/common/inc/
UNIT_CFLAGS += -Isrc/lib/log/inc/

# Flags for exporting this unit
UNIT_EXPORT_CFLAGS := $(UNIT_CFLAGS)

# Dependencies
UNIT_DEPS := src/lib/pjs
UNIT_DEPS += src/lib/ds
UNIT_DEPS += src/lib/const

# Additional build prerequisites
UNIT_PRE := $(UNIT_BUILD)/schema_gen.h
UNIT_PRE += $(UNIT_BUILD)/schema_pre.h

# Custom rules
SCHEMA_COMP := $(UNIT_PATH)/schema.py
$(UNIT_BUILD)/schema_gen.h: $(SCHEMA) $(SCHEMA_COMP)
	$(NQ) " $(call color_generate,generate)[$(call COLOR_BOLD,schema)] $@"
	$(Q) $(SCHEMA_COMP) $< > $@

# generate pre-processed schema header, for human readable structures
SCHEMA_H := $(UNIT_PATH)/inc/schema.h
SCHEMA_CFLAGS := -Isrc/lib/pjs/inc $(UNIT_CFLAGS) -I $(UNIT_BUILD)
SCHEMA_PRE_INC := -imacros stddef.h -imacros stdbool.h -imacros string.h -imacros jansson.h
$(UNIT_BUILD)/schema_pre.h: $(UNIT_BUILD)/schema_gen.h
	$(NQ) " $(call color_generate,generate)[$(call COLOR_BOLD,schema)] $@"
	$(Q) $(CC) -E -P $(INCLUDES) $(SDK_INCLUDES) $(SCHEMA_CFLAGS) $(SCHEMA_PRE_INC) $(SCHEMA_H) -o $@
	$(Q) sed -i '/^ *$$/d;s/;/;\n/g;s/{/\n{\n/g' $@
#	$(Q) if which clang-format >/dev/null 2>&1; then clang-format -style=WebKit -i $@; fi

