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

#include <regex.h>

#include "log.h"
#include "os_regex.h"

#define MODULE_ID  LOG_MODULE_ID_OSA

/*
 * Go through the @p reg_list list of regular expressions,
 * if str_to_match matches, call the associated callback.
 */
int os_reg_list_match(
        os_reg_list_t*  relist,
        char*           str,
        regmatch_t*     pmatch,
        size_t          nmatch)
{
    os_reg_list_t *re;

    for (re = relist; re->re_str != NULL; re++)
    {
        /* Compile the regular expression */
        if (!(re->__re_flags & OS_REG_FLAG_INIT))
        {
            re->__re_flags |= OS_REG_FLAG_INIT;

            if (regcomp(&re->__re_ex, re->re_str, REG_EXTENDED) != 0)
            {
                re->__re_flags |= OS_REG_FLAG_INVALID;
                LOG(ERR, "Error compiling regular expression::regex=%s", re->re_str);
                continue;
            }
        }

        /* Skip invalid entries */
        if (re->__re_flags & OS_REG_FLAG_INVALID)
        {
            continue;
        }

        if (regexec(&re->__re_ex, str, nmatch, pmatch, 0) == 0)
        {
            break;
        }
    }

    /*
     * The last entry (re->re_str == NULL) defines what value
     * is returned in case there is no match
     */
    return re->re_id;
}

/*
 * Copy a substring from @p src to @p dest; the offset and size of
 * the source substring are defined by the regex regmatch_t
 * structure @p srm
 */
void os_reg_match_cpy(
        char*       dest,
        size_t      destsz,
        const char* src,
        regmatch_t  srm)
{
    size_t len = 0;

    if (srm.rm_eo < 0 || srm.rm_so < 0)
    {
        *dest = '\0';
    }

    len = srm.rm_eo - srm.rm_so;
    if (len >= destsz)
    {
        len = destsz - 1;
    }

    memcpy(dest, src + srm.rm_so, len);
    dest[len] = '\0';
}
