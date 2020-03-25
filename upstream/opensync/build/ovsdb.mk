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

OVSDB_DB_NAME           ?= conf.db.bck
OVSDB_DB_DIR            ?= $(INSTALL_PREFIX)/etc
OVSDB_SCHEMA_DIR        ?= $(INSTALL_PREFIX)/etc

ROOTFS_OVSDB_DIR        = $(BUILD_ROOTFS_DIR)/$(OVSDB_DB_DIR)
ROOTFS_OVSDB_FILENAME   = $(BUILD_ROOTFS_DIR)/$(OVSDB_DB_DIR)/$(OVSDB_DB_NAME)
ROOTFS_OVSDB_SCHEMA_DIR = $(BUILD_ROOTFS_DIR)/$(OVSDB_SCHEMA_DIR)

OVSDB_HOOKS_DIRS ?= ovsdb $(VENDOR_OVSDB_HOOKS)

OVSDB_HOOK_ENV += CONTROLLER_ADDR=$(CONTROLLER_ADDR)
OVSDB_HOOK_ENV += BACKHAUL_SSID=$(BACKHAUL_SSID)
OVSDB_HOOK_ENV += BACKHAUL_PASS=$(BACKHAUL_PASS)
OVSDB_HOOK_ENV += MULTI_BACKHAUL_CREDS=$(MULTI_BACKHAUL_CREDS)
OVSDB_HOOK_ENV += ONBOARD_SSID=$(ONBOARD_SSID)
OVSDB_HOOK_ENV += ONBOARD_PASS=$(ONBOARD_PASS)
OVSDB_HOOK_ENV += BUILD_BHAUL_WDS=$(BUILD_BHAUL_WDS)
OVSDB_HOOK_ENV += BUILD_NSS_THERMAL=$(BUILD_NSS_THERMAL)
OVSDB_HOOK_ENV += $(VERSION_ENV)

define ovsdb_clean
	$(NQ) " $(call color_install,clean) ovsdb in $(ROOTFS_OVSDB_DIR)"
	$(Q)$(RM) -f "$(ROOTFS_OVSDB_FILENAME)"
	$(call ovsdb_rm_lock)
endef

define ovsdb_init_db
	$(NQ) " $(call color_install,init) ovsdb in $(ROOTFS_OVSDB_FILENAME)"
	$(Q)mkdir -p "$(ROOTFS_OVSDB_DIR)"
	$(Q)ovsdb-tool create "$(ROOTFS_OVSDB_FILENAME)" "$(SCHEMA)"
endef

define ovsdb_copy_schema
	$(Q)mkdir -p "$(ROOTFS_OVSDB_SCHEMA_DIR)"
	$(Q)cp "$(SCHEMA)" "$(ROOTFS_OVSDB_SCHEMA_DIR)"
endef

define ovsdb_run_hooks_in_dir
	$(NQ) " $(call color_install,hooks) ovsdb in $(call color_profile,$1)"
	$(Q)for JSON in $$(find $1 -maxdepth 1 -name '*.json' $(Q_STDERR)); do \
		echo "  ovsdb transact: $$JSON"; \
		ovsdb-tool transact "$(ROOTFS_OVSDB_FILENAME)" "$$(cat $$JSON)" $(Q_STDOUT) ; \
    done
	$(Q)for SH in $$(find $1 -maxdepth 1 -name '*.json.sh' $(Q_STDERR)); do \
		echo "  ovsdb transact: $$SH"; \
		OUT=$$( $(OVSDB_HOOK_ENV) sh $$SH \
			| xargs -0 ovsdb-tool transact "$(ROOTFS_OVSDB_FILENAME)" 2>&1); \
		RET=$$?; \
		echo "  $$RET '$$OUT'" $(Q_STDOUT); \
		if [ $$RET != 0 ]; then exit 1; fi; \
		if echo "$$OUT" | grep -q error; then exit 1; fi; \
	done

endef

define ovsdb_run_hooks
	$(foreach DIR,$(OVSDB_HOOKS_DIRS),$(call ovsdb_run_hooks_in_dir,$(DIR)))
endef

define ovsdb_rm_lock
	$(Q)$(RM) -f "$(ROOTFS_OVSDB_DIR)"/.*lock*
endef

define ovsdb_create
	$(NQ) "$(call color_install,create) ovsdb"
	$(call ovsdb_clean)
	$(call ovsdb_init_db)
	$(call ovsdb_copy_schema)
	$(call ovsdb_run_hooks)
	$(call ovsdb_rm_lock)
endef


.PHONY: ovsdb-create

ovsdb-create:
	$(call ovsdb_create)

