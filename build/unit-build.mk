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
# Build component index
#
TOP_MAKE := $(abspath $(firstword $(MAKEFILE_LIST)))
TOP_DIR  := $(dir $(TOP_MAKE))

# Canonical path relative to TOP_DIR - absolute path is returned if outside $(TOP_DIR)
CANNED_PATH = $(patsubst $(abspath $(TOP_DIR))/%,%,$(abspath $1))

# Collapses a full path to a single-level path (replace slashes with __)
FLATTEN_PATH = $(subst /,.,$(patsubst %/,%,$1))

UNIT_MARK_FILE = $(OBJDIR)/$(call FLATTEN_PATH,$(1))/.target
UNIT_MARK_FILES = $(foreach U,$(1),$(call UNIT_MARK_FILE,$(U)))

# Create directory to store unit test binaries
TESTBINDIR = $(BINDIR)/utest
WORKDIRS += $(TESTBINDIR)

##########################################################
# Overriding CFLAGS from the command line doesn't really
# work at the moment (make CFLAGS=...). As a workaround,
# implement LOCAL_CFLAGS, which can be used to add 
# developer C flags to the target's CFLAGS.
#
# This is useful for specifying options like
# "-fmax-errors=5" to limit the number of errors displayed
# during edit-compile-error-repeat cycles.
##########################################################
ifneq ($(LOCAL_CFLAGS),)
CFLAGS += $(LOCAL_CFLAGS)
endif

##########################################################
# Definition of a "BIN" type unit
##########################################################
define UNIT_BUILD_BIN
# Add the unit to the global list of units
UNIT_ALL += $(UNIT_PATH)
UNIT_ALL_INSTALL += $(UNIT_PATH)/install
UNIT_ALL_CLEAN += $(UNIT_PATH)/clean
UNIT_ALL_BIN_UNITS += $(UNIT_PATH)
UNIT_ALL_BIN_FILES += $(BINDIR)/$(UNIT_BIN)

$(call UNIT_MAKE_RULES)

$(UNIT_BUILD)/.target: $(BINDIR)/$(UNIT_BIN)

# Use the .target file to actually see if any of the libraries changed
# Use -Wl,--start-group to force GNU LD to ignore the link order; this might be slower for larger projects
$(BINDIR)/$(UNIT_BIN): $(call UNIT_MARK_FILES,$(UNIT_DEPS)) $(UNIT_OBJ)
	$$(NQ) " $(call color_link,link)    [$$(call COLOR_BOLD,$(UNIT_BIN))] $$@"
	$$(Q)$$(CC) -L$(LIBDIR) -Wl,--start-group $$(UNIT_BIN_LDFLAGS) $$(foreach DEP,$$(sort $$(DEPS_$(UNIT_PATH))),$$(LDFLAGS_$$(DEP))) $(UNIT_OBJ) -Wl,--end-group $$(LDFLAGS) $(UNIT_LDFLAGS) -o $$@

$(UNIT_PATH)/install: $(UNIT_BUILD)/.target
ifneq ($$(UNIT_INSTALL),n)
	$$(call app_install,$(BINDIR)/$(UNIT_BIN),$(UNIT_DIR))
endif

$(UNIT_PATH)/uninstall:
	$$(call app_uninstall,$(BINDIR)/$(UNIT_BIN),$(UNIT_DIR))

$(call UNIT_MAKE_DIRS)
$(call UNIT_MAKE_INFO)
$(call UNIT_MAKE_CLEAN,$(BINDIR)/$(UNIT_BIN))
$(call UNIT_C_RULES)
endef

##########################################################
# Definition of a "TEST_BIN" type unit
##########################################################
define UNIT_BUILD_TEST_BIN
# Add the unit to the global list of units
UNIT_ALL += $(UNIT_PATH)
UNIT_ALL_INSTALL += $(UNIT_PATH)/install
UNIT_ALL_CLEAN += $(UNIT_PATH)/clean
UNIT_ALL_TEST_BIN_UNITS += $(UNIT_PATH)
UNIT_ALL_TEST_BIN_FILES += $(TESTBINDIR)/$(UNIT_BIN)/unit

$(call UNIT_MAKE_RULES)

$(UNIT_BUILD)/.target: $(TESTBINDIR)/$(UNIT_BIN) $(TESTBINDIR)/$(UNIT_BIN)/unit $(TESTBINDIR)/$(UNIT_BIN)/data

# Use the .target file to actually see if any of the libraries changed
# Use -Wl,--start-group to force GNU LD to ignore the link order; this might be slower for larger projects
# Special naming convetion for unit testing $(UNIT_BIN)/unit instead of $(UNIT_BIN)
$(TESTBINDIR)/$(UNIT_BIN):
	$$(Q)$(MKDIR) $(TESTBINDIR)/$(UNIT_BIN)

# In case data folder exist in UNIT_PATH, copy directory and its content to bin folder
.PHONY: $(TESTBINDIR)/$(UNIT_BIN)/data

$(TESTBINDIR)/$(UNIT_BIN)/data:
	$$(Q)if [ -d $(UNIT_PATH)/data ]; then \
			echo " $(call color_copy,copy)    [$(call COLOR_BOLD,$(UNIT_NAME)/data)] -> $(TESTBINDIR)/$(UNIT_BIN)/data"; \
			$(MKDIR) $(TESTBINDIR)/$(UNIT_BIN)/data; \
			$(CP) -r $(UNIT_PATH)/data/. $(TESTBINDIR)/$(UNIT_BIN)/data; \
		else \
			true; \
		fi

$(TESTBINDIR)/$(UNIT_BIN)/unit: $(call UNIT_MARK_FILES,$(UNIT_DEPS)) $(UNIT_OBJ)
	$$(NQ) " $(call color_link,link)    [$$(call COLOR_BOLD,$(UNIT_BIN))] $$@"
	$$(Q)$$(CC) -L$(LIBDIR) -Wl,--start-group $$(UNIT_BIN_LDFLAGS) $$(foreach DEP,$$(sort $$(DEPS_$(UNIT_PATH))),$$(LDFLAGS_$$(DEP))) $(UNIT_OBJ) -Wl,--end-group $$(LDFLAGS) $(UNIT_LDFLAGS) -o $$@

$(UNIT_PATH)/install: $(UNIT_BUILD)/.target
	$$(Q)true

$(call UNIT_MAKE_DIRS)
$(call UNIT_MAKE_INFO)
$(call UNIT_MAKE_CLEAN,$(TESTBINDIR)/$(UNIT_BIN))
$(call UNIT_C_RULES)
endef

##########################################################
# Definition of a "LIB" type unit
##########################################################
# Helper for LIB and STATIC_LIB
define __UNIT_BUILD_ARCHIVE
# Add the unit to the global list of units
UNIT_ALL += $(UNIT_PATH)
UNIT_ALL_CLEAN += $(UNIT_PATH)/clean

$(call UNIT_MAKE_RULES)

$(UNIT_BUILD)/.target: $(UNIT_BUILD)/lib$(UNIT_NAME).a

$(UNIT_BUILD)/lib$(UNIT_NAME).a: $(UNIT_DEPS) $(UNIT_OBJ)
	$$(NQ) " $(call color_link,link)    [$(call COLOR_BOLD,$(UNIT_NAME))] $$@"
	$$(Q)$$(AR) rcs $$@ $(UNIT_OBJ)
	@# error on duplicate symbols to prevent clashing with stubs
	@# ignore symbols with $ . in name or starting with __
	$$(Q)DUP=`nm --defined-only $$@ | $(GREP) ' [[:upper:]] ' | egrep -v '\\$$$$|\\.| __' | cut -d' ' -f3 | sort | uniq -d`; \
		if [ -n "$$$$DUP" ]; then \
		echo ERROR: Duplicate symbols in $$@: ; \
		nm $$@ | $(GREP) -e : -e "$$$$DUP"; \
		rm $$@; exit 1; fi

$(call UNIT_MAKE_DIRS)
$(call UNIT_MAKE_INFO)
$(call UNIT_MAKE_CLEAN,$(UNIT_BUILD)/lib$(UNIT_NAME).a)
$(call UNIT_C_RULES,$(1))
endef

# STATIC_LIB: statically linked to binary
define UNIT_BUILD_STATIC_LIB
UNIT_EXPORT_LDFLAGS += $(UNIT_BUILD)/lib$(UNIT_NAME).a
$(call __UNIT_BUILD_ARCHIVE)
endef

# LIB: static lib, linked either against binary or into main shared lib
define UNIT_BUILD_LIB
ifeq ($(BUILD_SHARED_LIB),y)
$(call __UNIT_BUILD_ARCHIVE,-fPIC)
else
UNIT_EXPORT_LDFLAGS += $(UNIT_BUILD)/lib$(UNIT_NAME).a
$(call __UNIT_BUILD_ARCHIVE)
endif
UNIT_ALL_LIB_UNITS += $(UNIT_PATH)
UNIT_ALL_LIB_FILES += $(UNIT_BUILD)/lib$(UNIT_NAME).a
UNIT_FILES_$(UNIT_PATH) += $(UNIT_BUILD)/lib$(UNIT_NAME).a
endef

##########################################################
# Definition of a "MAKEFILE" type unit
##########################################################
define UNIT_BUILD_MAKEFILE
# Add the unit to the global list of units
UNIT_ALL += $(UNIT_PATH)
UNIT_ALL_INSTALL += $(UNIT_PATH)/install
UNIT_ALL_CLEAN += $(UNIT_PATH)/clean

$(call UNIT_MAKE_RULES)

$(UNIT_BUILD)/.target: $(BINDIR)/$(UNIT_BIN)

# $(BINDIR)/$(UNIT_BIN) is never created. This is to call a makefile on every build
$(BINDIR)/$(UNIT_BIN): $(UNIT_DEPS) $(UNIT_PRE) $(call UNIT_MARK_FILES,$(UNIT_DEPS)) $(UNIT_OBJ)
	$$(NQ) " $(call color_callmakefile,compMak) [$(call COLOR_BOLD,$(UNIT_NAME))] $$@"
	$$(Q)make $$(if $(V),,-s) -C $(UNIT_PATH) -f unit.Makefile $(UNIT_MAKEFILE_FLAGS) all

$(UNIT_PATH)/install: $(UNIT_BUILD)/.target
	$$(NQ) " $(call color_install,install) [$(call COLOR_BOLD,$(UNIT_NAME))] $$@"
	$$(Q)make $$(if $(V),,-s) -C $(UNIT_PATH) -f unit.Makefile $(UNIT_MAKEFILE_FLAGS) install

$(call UNIT_MAKE_DIRS)
$(call UNIT_MAKE_INFO)
$(call UNIT_MAKE_CLEAN,$(UNIT_BUILD)/$(UNIT_BIN))
$(call UNIT_C_RULES)
endef

##########################################################
# Definition of a "SHLIB" type unit
##########################################################
define UNIT_BUILD_SHLIB
# Add the unit to the global list of units
UNIT_ALL += $(UNIT_PATH)
UNIT_ALL_INSTALL += $(UNIT_PATH)/install
UNIT_ALL_CLEAN += $(UNIT_PATH)/clean
UNIT_EXPORT_LDFLAGS += -l$(UNIT_NAME)

$(call UNIT_MAKE_RULES)

$(UNIT_BUILD)/.target: $(LIBDIR)/lib$(UNIT_NAME).so

$(LIBDIR)/lib$(UNIT_NAME).so: $(UNIT_DEPS) $(UNIT_PRE) $(call UNIT_MARK_FILES,$(UNIT_DEPS)) $(UNIT_OBJ)
	$$(NQ) " $(call color_link,link)    [$(call COLOR_BOLD,$(UNIT_NAME))] $$@"
	$$(Q)$$(CC) $(UNIT_OBJ) -L$(LIBDIR) -shared -Wl,-soname=lib$(UNIT_NAME).so $$(LDFLAGS) $$(foreach DEP,$$(sort $$(DEPS_$(UNIT_PATH))),$$(LDFLAGS_$$(DEP))) $(UNIT_LDFLAGS) -o $$@

$(UNIT_PATH)/install: $(UNIT_BUILD)/.target
	$$(call app_install,$(LIBDIR)/lib$(UNIT_NAME).so,$(UNIT_DIR))

$(UNIT_PATH)/uninstall:
	$$(call app_uninstall,$(LIBDIR)/lib$(UNIT_NAME).so,$(UNIT_DIR))

$(call UNIT_MAKE_DIRS)
$(call UNIT_MAKE_INFO)
$(call UNIT_MAKE_CLEAN,$(LIBDIR)/lib$(UNIT_NAME).so)
$(call UNIT_C_RULES,-fPIC)
endef

##########################################################
# Definition of a "STUB" type unit
##########################################################
define UNIT_BUILD_STUB
# Add the unit to the global list of units
UNIT_ALL += $(UNIT_PATH)
UNIT_ALL_INSTALL += $(UNIT_PATH)/install
UNIT_ALL_CLEAN += $(UNIT_PATH)/clean

$(call UNIT_MAKE_RULES)

$(UNIT_PATH)/install: $(UNIT_BUILD)/.target

$(call UNIT_MAKE_DIRS)
$(call UNIT_MAKE_INFO)
$(call UNIT_MAKE_CLEAN)
$(call UNIT_C_RULES)
endef

##########################################################
# Generic Make rules, applicable to all targets
##########################################################
define UNIT_MAKE_RULES
.PHONY: $(UNIT_NAME) $(UNIT_PATH)/ $(UNIT_PATH)/compile $(UNIT_PATH)/uninstall
$(UNIT_PATH) $(UNIT_PATH)/: $(UNIT_PATH)/compile

$(UNIT_PATH)/compile: $(UNIT_BUILD)/.target

# Build up .target dependency tree
$(UNIT_BUILD)/.target: workdirs $(UNIT_DIRS) \
								$(UNIT_PRE) \
								$(call UNIT_MARK_FILES,$(UNIT_DEPS))
	$$(Q)touch "$$@"
endef

define UNIT_MAKE_DISABLED
$(call UNIT_MAKE_INFO)
.PHONY: $(UNIT_NAME) $(UNIT_PATH)/ $(UNIT_PATH)/compile
$(UNIT_PATH) $(UNIT_PATH)/: $(UNIT_PATH)/compile
$(UNIT_PATH)/compile:
	$(NQ) "ERROR: DISABLED UNIT: $(UNIT_NAME) PATH: $(UNIT_PATH) BUILD: $(UNIT_BUILD)"
	@exit 1
endef

##########################################################
# Common C makefile rules
##########################################################
GEN_C_FLAGS = $$(CFLAGS) $(UNIT_CFLAGS) $$(foreach DEP,$$(sort $$(DEPS_$(UNIT_PATH)) $$(DEPS_CFLAGS_$(UNIT_PATH))),$$(CFLAGS_$$(DEP)))

define UNIT_C_RULES
# Single step dependency + compilation, generate the .d and .o file
# at the same time; but include the .d file only if it exists.
$(UNIT_BUILD)/%.o: %.c $(UNIT_PRE)
	$$(NQ) " $(call color_compile,compile) [$(call COLOR_BOLD,$(UNIT_NAME))] $$<"
	$$(Q)$$(CC) $(call GEN_C_FLAGS) $(1) $$< -MMD -c -o $$@

# Expand external rules
ifdef UNIT_EXTERNAL_RULES
$$(eval $$(UNIT_EXTERNAL_RULES))
endif

# Auto-generated files -- position after external rules to avoid quirks in GNU Make 3.81
$(UNIT_BUILD)/%.o: $(UNIT_BUILD)/%.c
	$$(NQ) " $(call color_compgen,compgen) [$(call COLOR_BOLD,$(UNIT_NAME))] $$<"
	$$(Q)$$(CC) $(call GEN_C_FLAGS)  $(1) $$< -MMD -c -o $$@

# Generate dependencies from source files
-include $(patsubst %.o,%.d,$(UNIT_OBJ))
endef

###########################################################
# Common rules for creating directories
###########################################################
define UNIT_MAKE_DIRS
$(UNIT_DIRS):
	$$(Q)$(MKDIR) $(UNIT_DIRS)
endef

##########################################################
# Common rules for the info target
##########################################################
define UNIT_MAKE_INFO
.PHONY: $(UNIT_PATH)/info
$(UNIT_PATH)/info:
	$(NQ) "UNIT_NAME:           " $(UNIT_NAME)
	$(NQ) "UNIT_PATH:           " $(UNIT_PATH)
	$(NQ) "UNIT_DISABLE:        " $(UNIT_DISABLE)
	$(NQ) "UNIT_TYPE:           " $(UNIT_TYPE)
	$(NQ) "UNIT_DEPS:           " $(UNIT_DEPS)
	$(NQ) "EXPAND DEPS:         " '$$(sort $$(DEPS_$(UNIT_PATH)))'
	$(NQ) "UNIT_DEPS_CFLAGS:    " $(UNIT_DEPS_CFLAGS)
	$(NQ) "EXPAND DEPS_CFLAGS:  " '$$(sort $$(DEPS_CFLAGS_$(UNIT_PATH)))'
	$(NQ) "UNIT_SRC:            " $(UNIT_SRC)
	$(NQ) "UNIT_SRC_TOP:        " $(UNIT_SRC_TOP)
	$(NQ) "SOURCES:             " $(UNIT_SOURCES)
	$(NQ) "UNIT_CFLAGS:         " $(UNIT_CFLAGS)
	$(NQ) "UNIT_LDFLAGS:        " $(UNIT_LDFLAGS)
	$(NQ) "UNIT_BUILD:          " $(UNIT_BUILD)
	$(NQ) "UNIT_DIRS:           " $(UNIT_DIRS)
	$(NQ) "UNIT_OBJ:            " $(UNIT_OBJ)
	$(NQ) "UNIT_EXPORT_CFLAGS:  " $(UNIT_EXPORT_CFLAGS)
	$(NQ) "UNIT_EXPORT_LDFLAGS: " $(UNIT_EXPORT_LDFLAGS)
	$(NQ) "UNIT_CLEAN:          " $(UNIT_CLEAN)
endef

##########################################################
# Common rules for the clean target
##########################################################
define UNIT_MAKE_CLEAN
.PHONY: $(UNIT_PATH)/clean
$(UNIT_PATH)/clean: $(UNIT_PATH)/uninstall
	$$(NQ) " $(call color_clean,clean)   [$(call COLOR_BOLD,$(UNIT_NAME))] $(UNIT_PATH)"
	$$(Q)$(RM) -r $(UNIT_BUILD)/.target $(UNIT_CLEAN) $(1)
	$$(Q)echo $(UNIT_DIRS) | xargs -n 1 | sort -r | while read DIR; do [ ! -d "$$$$DIR" ] || find "$$$$DIR" -type d -delete|| true; done

.PHONY: $(UNIT_PATH)/rclean
$(UNIT_PATH)/rclean:
	$$(Q)$(MAKE) $(UNIT_PATH)/clean $$(foreach DEP,$$(sort $$(DEPS_$(UNIT_PATH))),$$(DEP)/clean)
endef

# Include override.mk to override upper layer unit rules, unless disabled
define INCLUDE_OVERRIDE
OVERRIDE_DIR := $(1)/$(2)
LAYER_DIR := $(1)
_OVERRIDE_DISABLE  := $$(UNIT_DISABLE_$$(OVERRIDE_DIR))
ifeq ($$(_OVERRIDE_DISABLE),)
_OVERRIDE_DISABLE  := $$(UNIT_DISABLE_LAYER_$(1))
endif
ifneq ($$(_OVERRIDE_DISABLE),y)
-include $$(OVERRIDE_DIR)/override.mk
endif

endef

##########################################################
# Create a UNIT
##########################################################
define UNIT_MAKE
# Clear all variables that might be used by the unit
UNIT_NAME:=
UNIT_BIN:=
UNIT_DEPS:=
# UNIT_DEPS_CFLAGS: use this to pull cflags from a dependant unit without adding a link and makefile dependency
UNIT_DEPS_CFLAGS:=
UNIT_TYPE:=
UNIT_SRC:=
UNIT_SRC_TOP:=
UNIT_SRC_EXT:=
UNIT_OBJ:=
UNIT_CLEAN:=
UNIT_EXPORT_CFLAGS:=
UNIT_EXPORT_LDFLAGS:=
UNIT_CFLAGS:=
UNIT_LDFLAGS:=
UNIT_DIRS:=
UNIT_DISABLE:=
UNIT_PRE:=
UNIT_DIR:=
UNIT_POST_MACRO:=
UNIT_INSTALL:=

UNIT_MK         := $(1)
UNIT_PATH       := $(call CANNED_PATH,$(dir $(UNIT_MK)))
UNIT_BUILD      := $(OBJDIR)/$$(call FLATTEN_PATH,$$(UNIT_PATH))
# The build directories must be created before individual unit.mk files set
# other rules (like auto-generating files)

include $(UNIT_MK)

# Include per-unit override.mk in all sub-layers, if it exists
# UNIT_BASE_PATH is UNIT_PATH without LAYER
UNIT_BASE_PATH  := $$(patsubst $$(LAYER)%,%,$$(UNIT_PATH))
$$(eval $$(foreach SUBLAYER,$$(SUB_LAYERS),$$(call INCLUDE_OVERRIDE,$$(SUBLAYER),$$(UNIT_BASE_PATH))))
LAYER_DIR       := $(LAYER)

# Override UNIT_DISABLE with UNIT_DISABLE_LAYER_$(LAYER) if defined
ifneq ($$(UNIT_DISABLE_LAYER_$$(LAYER)),)
UNIT_DISABLE    := $$(UNIT_DISABLE_LAYER_$$(LAYER))
endif
# Override UNIT_DISABLE with UNIT_DISABLE_$(UNIT_PATH) if defined
ifneq ($$(UNIT_DISABLE_$$(UNIT_PATH)),)
UNIT_DISABLE    := $$(UNIT_DISABLE_$$(UNIT_PATH))
endif
# Process !TARGET disable rules
UNIT_DISABLE    := $$(if $$(filter !$$(TARGET),$$(UNIT_DISABLE)),no,$$(UNIT_DISABLE))
# Expand to y if unit is disabled, otherwise to n
UNIT_DISABLE    := $$(if $$(filter !% $$(TARGET) y yes 1 true True TRUE, $$(UNIT_DISABLE)),y,n)

ifeq ($$(UNIT_BIN),)
UNIT_BIN := $$(UNIT_NAME)
endif

$$(eval UNIT_NAME_$$(UNIT_PATH) = $$(UNIT_NAME))

# Expand all variables
UNIT_SOURCES    := $$(foreach SRC,$$(UNIT_SRC),$$(call CANNED_PATH,$$(UNIT_PATH)/$$(SRC)))
UNIT_SOURCES    += $$(foreach SRC,$$(UNIT_SRC_TOP),$$(call CANNED_PATH,$$(SRC)))
UNIT_OBJ        += $$(foreach SRC,$$(UNIT_SOURCES),$$(patsubst %.c,$$(UNIT_BUILD)/%.o,$$(SRC)))

# Expand external variables if defined
ifdef UNIT_EXTERNAL_SRC
$$(eval $$(UNIT_EXTERNAL_SRC))
endif

UNIT_DIRS       += $$(sort $$(UNIT_BUILD) $$(foreach OBJ,$$(UNIT_OBJ),$$(call CANNED_PATH,$$(dir $$(OBJ)))))
# Add object files to clean target
UNIT_CLEAN      += $$(UNIT_OBJ)
# Add dependency files
UNIT_CLEAN      += $$(patsubst %.o,%.d,$$(UNIT_OBJ))

# Expand UNIT_POST_MACRO if defined
ifdef UNIT_POST_MACRO
$$(eval $$(UNIT_POST_MACRO))
endif

# Expand rules
ifeq ($$(UNIT_DISABLE),y)
$$(eval $$(UNIT_MAKE_DISABLED))
else ifndef UNIT_BUILD_$$(UNIT_TYPE)
$$(error $$(UNIT_PATH) Unknown type: $$(UNIT_TYPE))
else
$$(eval $$(UNIT_BUILD_$$(UNIT_TYPE)))
endif

$$(eval UNIT_DEPS_$$(UNIT_PATH) = $$(UNIT_DEPS))
$$(eval UNIT_DEPS_CFLAGS_$$(UNIT_PATH) = $$(UNIT_DEPS_CFLAGS))
$$(eval DEPS_$$(UNIT_PATH) = $$(UNIT_DEPS) $$(foreach DEP,$$(UNIT_DEPS),$$$$(DEPS_$$(DEP))))

# moved handling of DEPS_CFLAGS to after all layers are processed
# $$(eval DEPS_CFLAGS_$$(UNIT_PATH) += $$(UNIT_DEPS_CFLAGS))
# $$(eval DEPS_CFLAGS_$$(UNIT_PATH) += $$(foreach DEP,$$(UNIT_DEPS),$$$$(DEPS_CFLAGS_$$(DEP))))
# $$(eval DEPS_CFLAGS_$$(UNIT_PATH) += $$(foreach DEP,$$(UNIT_DEPS_CFLAGS),$$$$(DEPS_$$(DEP))))

# Handle exported CFLAGS and LDFLAGS
CFLAGS_$$(UNIT_PATH) := $$(UNIT_EXPORT_CFLAGS)
LDFLAGS_$$(UNIT_PATH) := $$(UNIT_EXPORT_LDFLAGS)
UNIT_TYPE_$$(UNIT_PATH) := $$(UNIT_TYPE)
endef

# Process all layers (core, PLATFORM, INCLUDE_LAYERS, VENDOR)
# sub-layers can override a parent layer with override.mk
LAYER_LIST := . $(PLATFORM_DIR) $(INCLUDE_LAYERS) $(VENDOR_DIR)
ifeq ($(SHOW_VENDOR_INFO),1)
$(info Layers($(words $(LAYER_LIST))): $(LAYER_LIST))
endif

# Scan src subdirectories for units.
# It is important to sort the finds by folder depths, because GNU make seems to
# process rules in-order instead of using shortest match.
define PROCESS_LAYER
ifneq ($$(LAYER),)
SUB_LAYERS := $$(wordlist 2,$$(words $$(SUB_LAYERS)),$$(SUB_LAYERS))
UNIT_MKLIST := $$(shell find -L "$(TOP_DIR)/$$(LAYER)/src" -name $1 -printf "%08d%p\n" | sort -nr | cut -c9-)
$$(eval $$(foreach UNIT_MK,$$(UNIT_MKLIST),$$(call UNIT_MAKE,$$(UNIT_MK))))
endif
endef

define PROCESS_LAYERS
UNIT_MK_NAME := $(1)
SUB_LAYERS := $$(LAYER_LIST)
$$(foreach LAYER,$$(LAYER_LIST),$$(eval $$(call PROCESS_LAYER,$$(UNIT_MK_NAME))))
endef

# Source-in all units definitions
$(eval $(call PROCESS_LAYERS,'unit.mk'))

# Save a list of all but the last unit so it can be referenced by the following mk rules
UNIT_ALL_NOT_LAST := $(UNIT_ALL)

# Process unit-last.mk last
$(eval $(call PROCESS_LAYERS,'unit-last.mk'))


# Process DEPS_CFLAGS_* after all layers to resolve circular deps
# $1: list of units to expand (result)
# $2: list of already expanded deps to filter out to prevent cyclic dependancy
define EXPAND_DEPS
$1 $(if $1,$(call EXPAND_DEPS,$(sort $(filter-out $2 $1,$(foreach UNIT,$1,$(DEPS_$(UNIT)) $(foreach DEP,$(DEPS_$(UNIT)),$(UNIT_DEPS_CFLAGS_$(UNIT)))))),$2 $1),)
endef
# note: sort also removes duplicate words
$(foreach UNIT_PATH, $(UNIT_ALL),\
$(eval DEPS_CFLAGS_$(UNIT_PATH):=$(filter-out $(UNIT_PATH) $(DEPS_$(UNIT_PATH)),\
$(call sort,$(call EXPAND_DEPS,$(UNIT_PATH) $(UNIT_DEPS_CFLAGS_$(UNIT_PATH)),))))\
)

.PHONY: unit-list unit-all unit-clean
unit-list:
	$(NQ) Currently active units:
	$(NQ)
	@$(foreach UNIT,$(UNIT_ALL),printf "    [%-16s] %s\n" "$(UNIT_NAME_$(UNIT))" "$(UNIT)";)
	$(NQ)

.PHONY: unit-list-deps
unit-list-deps:
	@$(foreach UNIT,$(UNIT_ALL),echo $(UNIT): $(UNIT_DEPS_$(UNIT));)

.PHONY: unit-list-deps-dot
unit-list-deps-dot:
	@echo "digraph {"
	@$(foreach _UNIT,$(UNIT_ALL),echo "$(_UNIT);" |tr / _ ; \
		$(foreach _DEP,$(UNIT_DEPS_$(_UNIT)),echo "$(_UNIT) -> $(_DEP);"| tr / _;) \
		$(foreach _DEP,$(UNIT_DEPS_CFLAGS_$(_UNIT)),echo "$(_UNIT) -> $(_DEP) [color=blue penwidth=2];"| tr / _;) \
		)
	@echo "}"

$(TESTBINDIR)/clean:
	$(NQ) " $(call color_clean,clean)   [$(call COLOR_BOLD,$(notdir $(TESTBINDIR)))] $(TESTBINDIR)"
	$(Q)$(RM) -r $(TESTBINDIR)

unit-all: workdirs $(UNIT_ALL)
unit-install: $(UNIT_ALL_INSTALL)
unit-clean: $(UNIT_ALL_CLEAN) $(TESTBINDIR)/clean
