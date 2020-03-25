/*
Copyright (c) 2015, Plume Design Inc. All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
   1. Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
   2. Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
   3. Neither the name of the Plume Design Inc. nor the
      names of its contributors may be used to endorse or promote products
      derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL Plume Design Inc. BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include <stdio.h>
#include "version_defs.h"

static const char app_build_time[]     = APP_BUILD_TIME;
static const char app_build_version[]  = APP_BUILD_VERSION_LONG;
static const char app_build_author[]   = APP_BUILD_AUTHOR;
static const char app_build_ver_only[] = APP_BUILD_VERSION;
static const char app_build_num_only[] = APP_BUILD_NUMBER;
static const char app_build_number[]   = APP_BUILD_VERSION"-"APP_BUILD_NUMBER;
static const char app_build_commit[]   = APP_BUILD_COMMIT;
static const char app_build_profile[]  = APP_BUILD_PROFILE;

const char *app_build_ver_get()
{
    return app_build_version;
}
const char *app_build_time_get()
{
    return app_build_time;
}
const char *app_build_author_get()
{
    return app_build_author;
}
const char *app_build_commit_get()
{
    return app_build_commit;
}
const char *app_build_num_only_get()
{
    return app_build_num_only;
}
const char *app_build_ver_only_get()
{
    return app_build_ver_only;
}
const char *app_build_number_get()
{
    return app_build_number;
}
const char *app_build_profile_get()
{
    return app_build_profile;
}

void app_build_version_show(const char *service_name)
{
    char ver[256];
    snprintf(ver, sizeof(ver),
             "v%s (%s) [%s]",
             app_build_ver_get(),
             app_build_author_get(),
             app_build_time_get());

    printf("%s %s\n", service_name, ver);
}

