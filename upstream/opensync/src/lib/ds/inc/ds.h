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

#ifndef DS_DS_H_INCLUDED
#define DS_DS_H_INCLUDED

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define DS_ITER_ERROR ((void *)0x1)

/**
 * Similar to the container_of() macro used in the Linux kernel.
 *
 * Given the member address, this function returns the pointer to the base structure.
 *
 * For example:
 *
 *  struct foo
 *  {
 *      int a;
 *      int x;
 *      int b;
 *  };
 *
 * If a function is passed the pointer to variable @p x, it can return the pointer
 * to the containing @p foo structure by using CONTAINER_OF(x_ptr, struct foo, x);
 */
#define TYPE_CHECK(a, type, member) \
    (true ? a : &((type *)NULL)->member)

#define CONTAINER_OF(ptr, type, member) \
    ((type *)((uintptr_t)TYPE_CHECK(ptr, type, member) - offsetof(type, member)))

/** Calculate container address from node */
#define NODE_TO_CONT(node, offset)    ( (node) == NULL ? NULL : (void *)((char *)(node) - (offset)) )
/** Calculate node address from container */
#define CONT_TO_NODE(cont, offset)    ( (void *)((char *)(cont) + (offset)) )

/**
 * Key compare function; this function should return a negative number
 * if a < b; 0 if a == b or a positive number if a > b
 */
typedef int ds_key_cmp_t(void *a, void *b);

/** Integer comparator */
extern ds_key_cmp_t ds_int_cmp;
/** String comparator */
extern ds_key_cmp_t ds_str_cmp;
/** Pointer comparison (the key value is stored directly) */
extern ds_key_cmp_t ds_void_cmp;

#endif /* DS_DS_H_INCLUDED */
