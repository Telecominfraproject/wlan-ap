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

# git related rules

GIT_ORIGIN ?= origin
BRANCH ?= master

MASTER_REPO = ..

git-clean:
	$(Q) git -C "$(MASTER_REPO)" clean -dXf
	$(Q) git -C "$(MASTER_REPO)" submodule --quiet foreach git clean -dXf

define GIT_UPDATE
	echo -n --- Updating "$1: "; \
	BRANCH="$$(git rev-parse --abbrev-ref HEAD)"; \
	[ "$$BRANCH" = "HEAD" ] && { echo "$(call COLOR_RED,no branch)"; exit 0; }; \
	git rev-parse @{u} > /dev/null 2>&1 || { echo "$(call COLOR_YELLOW,local branch $$BRANCH)"; exit 0 ; }; \
	echo "$(call COLOR_GREEN,$$BRANCH)"; \
	git pull --ff-only
endef

git-update:
	$(Q) (cd "$(MASTER_REPO)"; $(call GIT_UPDATE,Master Repository))
	$(Q) git -C "$(MASTER_REPO)" submodule --quiet foreach '$(call GIT_UPDATE,$$path)'

define GIT_SWITCH
	if git rev-parse -q --verify $(GIT_ORIGIN)/$(BRANCH) > /dev/null; \
	then \
		echo "--- $1: Switching to branch $(call COLOR_GREEN,$(BRANCH))"; \
		git checkout $(BRANCH); \
	elif git rev-parse -q --verify $(BRANCH) > /dev/null; \
	then \
		echo "--- $1: Switching to local branch $(call COLOR_YELLOW,$(BRANCH))"; \
		git checkout $(BRANCH); \
	else \
		echo "--- $1: No branch $(call COLOR_RED,$(BRANCH))"; \
	fi
endef

git-switch:
	$(Q) (cd "$(MASTER_REPO)"; $(call GIT_SWITCH,Master))
	$(Q) git -C "$(MASTER_REPO)" submodule --quiet foreach --recursive '$(call GIT_SWITCH,$$path)'

define GIT_STATUS
	CMOD=" " ;\
	[ "$$(git status --porcelain | wc -l)" -gt 0 ] && CMOD='!' ;\
	BRANCH="$$(git rev-parse --abbrev-ref HEAD)" ;\
	if [ "$$BRANCH" = "HEAD" ] ;\
	then \
		STATUS="$(call COLOR_YELLOW,NO BRANCH )" ;\
	elif ! git rev-parse @{u} > /dev/null 2>&1 ;\
	then \
		STATUS="$(call COLOR_GRAY,LOCAL     )" ;\
	else \
		L=$$(git rev-parse HEAD) ;\
		R=$$(git rev-parse @{u}) ;\
		M=$$(git merge-base "$$L" "$$R") ;\
		if [ "$$L" = "$$R" ] ;\
		then \
			STATUS="$(call COLOR_GREEN,OK        )";\
		elif [ "$$L" = "$$M" ] ;\
		then \
			STATUS="$(call COLOR_YELLOW,PULL      )";\
		elif [ "$$R" = "$$M" ] ;\
		then \
			STATUS="$(call COLOR_RED,PUSH      )";\
		else \
			STATUS="$(call COLOR_MAGENTA,MERGE     )";\
		fi ;\
	fi ;\
	echo "$$CMOD $$STATUS $1"
endef

git-status:
	$(Q) echo -n "$(call COLOR_GREEN,OK) - branch up to date; "
	$(Q) echo -n "$(call COLOR_YELLOW,NO BRANCH) - no branch; "
	$(Q) echo -n "$(call COLOR_YELLOW,PULL) - pull needed; "
	$(Q) echo "$(call COLOR_GRAY,LOCAL) - on local branch"
	$(Q) echo -n "$(call COLOR_RED,PUSH) - push needed; "
	$(Q) echo -n "$(call COLOR_MAGENTA,MERGE) - branch diverged; "
	$(Q) echo  "! - there are uncommited files"
	$(Q) echo
	$(Q) (cd .. ; $(call GIT_STATUS,Master))
	$(Q) git -C "$(MASTER_REPO)" submodule --quiet foreach --recursive '$(call GIT_STATUS,$$path)'
	$(Q) echo

