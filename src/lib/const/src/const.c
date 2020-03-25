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

/*
 * Constant Helpers
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "const.h"

c_item_t *
_c_get_item_by_key(c_item_t *list, int list_sz, int key)
{
    c_item_t    *item;
    int         i;

    for (item = list,i = 0;i < list_sz; item++, i++) {
        if ((int)(item->key) == key) {
            return item;
        }
    }

    return NULL;
}

c_item_t *
_c_get_item_by_strkey(c_item_t *list, int list_sz, const char *key)
{
    c_item_t    *item;
    int         i;

    for (item = list,i = 0;i < list_sz; item++, i++) {
        if (strcmp((char *)(item->key), key) == 0) {
            return item;
        }
    }

    return NULL;
}

c_item_t *
_c_get_item_by_str(c_item_t *list, int list_sz, const char *str)
{
    c_item_t    *item;
    int         i;

    for (item = list,i = 0;i < list_sz; item++, i++) {
        if (strcmp((char *)(item->data), str) == 0) {
            return item;
        }
    }

    return NULL;
}

intptr_t
_c_get_data_by_key(c_item_t *list, int list_sz, int key)
{
    c_item_t    *item = _c_get_item_by_key(list, list_sz, key);

    if (!item) {
        return -1;
    }

    return item->data;
}

char *
_c_get_str_by_key(c_item_t *list, int list_sz, int key)
{
    c_item_t    *item = _c_get_item_by_key(list, list_sz, key);

    if (!item) {
        return "";
    }

    return (char *)(item->data);
}

char *
_c_get_str_by_strkey(c_item_t *list, int list_sz, const char *key)
{
    c_item_t    *item = _c_get_item_by_strkey(list, list_sz, key);

    if (!item) {
        return "";
    }

    return (char *)(item->data);
}

char *
_c_get_strkey_by_str(c_item_t *list, int list_sz, const char *str)
{
    c_item_t    *item = _c_get_item_by_str(list, list_sz, str);

    if (!item) {
        return "";
    }

    return (char *)(item->key);
}

bool
_c_get_value_by_key(c_item_t *list, int list_sz, int key, uint32_t *dest)
{
    c_item_t    *item = _c_get_item_by_key(list, list_sz, key);

    if (!item) {
        return false;
    }

    *dest = item->value;
    return true;
}

bool
_c_get_param_by_key(c_item_t *list, int list_sz, int key, uint32_t *dest)
{
    c_item_t    *item = _c_get_item_by_key(list, list_sz, key);

    if (!item) {
        return false;
    }

    *dest = item->param;
    return true;
}
