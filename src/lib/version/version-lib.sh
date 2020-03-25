
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
# version-lib.sh - provides version data for image creation
# (meant to be sourced)
#
#
# Interface of version.lib.sh
# ---------------------------
#
# input:
#  CURDIR   current directory
#  VERSION_TARGET
#  TARGET
#  VENDOR_DIR
#  VERSION_FILE
#  BUILD_NUMBER
#  IMAGE_DEPLOYMENT_PROFILE
#  LAYER_LIST
#  VER_DATE
#
# output:
#  SHA1           (sha1 of git repository used to build)
#  DIRTY_STRING   (local modifications string)
#  VERSION        (marketing version)
#  USERNAME       (user used to build the image)
#  HOSTNAME       (hostname used to build the image)
#  VER_DATE       (date of image build)
#  APP_VERSION    (application version string)
#  BUILD_NUMBER   (consecutive build number)
#  OSYNC_VERSION  (OpenSync version - core/.version)
#
# Format: VERSION-BUILD_NUMBER-gSHA1-DIRTY_STRING-IMAGE_DEPLOYMENT_PROFILE
#

if [ -z "$CURDIR" ]; then
    CURDIR=`dirname $0`
fi

cd ${CURDIR}/../../../

if [ -e .git -o -e ../.git ]; then
    SHA1='g'`[ -e ../.git ] && cd ..; git log --pretty=oneline --abbrev-commit -1 | awk '{ print $1 }' | cut -b1-7`
    DIRTY=`[ -e ../.git ] && cd ..; git status --porcelain | grep -v -e '^??' | wc -l`
else
    echo "WARNING: version not in git" 1>&2
    SHA1="notgit"
    DIRTY=0
fi

# per vendor/product versioning:
if [ -z "$VERSION_FILE" ]; then
    VERSION_FILE="$VENDOR_DIR/.version.$VERSION_TARGET"
    if [ ! -f "$VERSION_FILE" ]; then
        VERSION_FILE="$VENDOR_DIR/.version.$TARGET"
    fi
    if [ ! -f "$VERSION_FILE" ]; then
        VERSION_FILE="$VENDOR_DIR/.version"
    fi
    if [ ! -f "$VERSION_FILE" ]; then
        VERSION_FILE=".version"
    fi
fi

VERSION=`cat $VERSION_FILE`

OSYNC_VERSION=`cat .version`

# echo "VERSION_FILE=$VERSION_FILE : $VERSION" >&2

DIRTY_STRING=""
if [ ${DIRTY} -ne 0 ]; then
    DIRTY_STRING="-mods"
fi

USERNAME=`id -n -u`
HOSTNAME=`hostname`

if [ -z "$VER_DATE" ]; then
    VER_DATE=`date`
fi

# First see if BUILD_NUMBER is defined in environment by Jenkins,
# then try to find it in file, and if not found use 0
if [ -z "${BUILD_NUMBER}" ]; then
    if [ -f "${CURDIR}/../../../.buildnum" ]; then
        BUILD_NUMBER=`cat ${CURDIR}/../../../.buildnum`
    fi
fi
if [ -z "${BUILD_NUMBER}" ]; then
    BUILD_NUMBER="0"
fi

APP_VERSION="${VERSION}"

if [ "${VERSION_NO_BUILDNUM}" != "1" ]; then
    # append build number
    APP_VERSION="${APP_VERSION}-${BUILD_NUMBER}"
fi

if [ "${VERSION_NO_SHA1}" != "1" ]; then
    # append SHA1
    APP_VERSION="${APP_VERSION}-${SHA1}"
fi

if [ "${VERSION_NO_MODS}" != "1" ]; then
    # append dirty string
    APP_VERSION="${APP_VERSION}${DIRTY_STRING}"
fi

# append profile
if [ "${VERSION_NO_PROFILE}" != "1" ]; then
    if [ -z "${IMAGE_DEPLOYMENT_PROFILE}" ]; then
        IMAGE_DEPLOYMENT_PROFILE="development"
    fi
    if [ -n "${IMAGE_DEPLOYMENT_PROFILE}" -a "${IMAGE_DEPLOYMENT_PROFILE}" != "none" ]; then
        APP_VERSION="${APP_VERSION}-${IMAGE_DEPLOYMENT_PROFILE}"
    fi
fi

cd - >/dev/null
