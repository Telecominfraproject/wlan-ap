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

DOC_SHORTVER    := $(shell cat .version | cut -d. -f-2)
PROJECT_NAME    := "OpenSync $(DOC_SHORTVER) Southbound API"
DOC_OUTPUT_NAME := "OpenSync_$(DOC_SHORTVER)_Southbound_API.pdf"

.PHONY: doc
doc:
	$(NQ) "PROJECT_NAME=\"$(PROJECT_NAME)\""
	$(NQ) " $(call color_generate,doc) $(call color_target,[HTML])"
	$(Q)cd doc; \
		(cat doxygen.conf; \
		echo "PROJECT_NAME=\"$(PROJECT_NAME)\"") \
		| doxygen - > doxygen.log 2>&1
	$(Q)echo -n "  "; ls -l doc/html/index.html
	$(NQ) " $(call color_generate,doc) $(call color_target,[PDF])"
	$(Q)-cd doc/latex; make > ../latex.log 2>&1
	$(Q)echo -n "  "; ls -l doc/latex/*.pdf \
		|| ( set -x; cat doc/latex.log; cat doc/latex/*.log | grep '^!' )
	$(NQ) " $(call color_generate,doc) $(call color_target,[copy]) $(DOC_OUTPUT_NAME)"
	$(Q)cp -v doc/latex/refman.pdf doc/$(DOC_OUTPUT_NAME)

.PHONY: doc-clean
doc-clean:
	$(NQ) " $(call color_clean,doc-clean) $(call color_target,[doc]) doc/html"
	$(Q)rm -rf doc/html
	$(NQ) " $(call color_clean,doc-clean) $(call color_target,[doc]) doc/doxygen.log"
	$(Q)rm -f doc/doxygen.log
	$(NQ) " $(call color_clean,doc-clean) $(call color_target,[doc]) doc/latex"
	$(Q)rm -rf doc/latex
	$(NQ) " $(call color_clean,doc-clean) $(call color_target,[doc]) doc/latex.log"
	$(Q)rm -f doc/latex.log
