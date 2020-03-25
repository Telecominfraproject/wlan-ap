#!/bin/bash

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


pod=$1

# ssh proxy
PROXY=lab
DEFAULT_KNOWN_HOSTS=${HOME}/.ssh/known_hosts
KNOWN_HOSTS=${KNOWN_HOSTS:=${DEFAULT_KNOWN_HOSTS}}

# Clear entry from $HOME/.ssh/known_hosts
clear_known_host() {
    local host=$1
    local known_hosts=$2
    ssh-keygen -f ${known_hosts} -R ${host}
}

# Add an entry to ${HOME}/.ssh/known_hosts
add_known_host() {
    local host=$1
    local known_hosts=$2
    entry=$(/usr/bin/ssh-keyscan -t rsa ${host})
    echo ${entry} >> ${known_hosts}
}

# Build list of pods
declare -a pods
pods[1]=$pod

for i in ${pods[@]}; do
    clear_known_host ${i} ${KNOWN_HOSTS}
    add_known_host ${i} ${KNOWN_HOSTS}
done
