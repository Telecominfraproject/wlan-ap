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

#ifndef __OVSDB_UTILS_H__
#define __OVSDB_UTILS_H__

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

#include "ds_tree.h"

/**
 * @brief represents an array of strings and its number of elements
 */
struct str_set
{
    char **array;
    size_t nelems;
};


/**
 * @brief represents an array of integers and its number of elements
 */
struct int_set
{
    int *array;
    size_t nelems;
};

/**
 * @brief represent a <string key, string value> pair stashed in a DS tree
 */
struct str_pair
{
    char *key;
    char *value;
    ds_tree_node_t pair_node;
};

/**
 * @brief represent a <string key, int value> pair stashed in a DS tree
 */
struct str_ipair
{
    char *key;
    int value;
    ds_tree_node_t pair_node;
};

/**
 * @brief comparing routine for a string pair tree
 *
 * @param a the first string of the comparison
 * @param b the second string of the comparison
 * @return the string comparision result
 */
int
str_tree_cmp(void *a, void *b);


/**
 * @brief : converts a static array of strings in a dynamically allocated array
 *
 * Allocates an array with the expected number of entries, and duplicates
 * the strings form the given array.
 *
 * @param elem_size provisioned size of strings in the input array
 * @param nelems number of actual elements in the input array
 * @param schema_set the static input array
 * @return a pointer to a <# of elems, strings> container if successful,
 * NULL otherwise
 */
struct str_set *
schema2str_set(size_t elem_size, size_t nelems,
               char schema_set[][elem_size]);


/**
 * @brief frees a strings array container
 *
 * @param set the strings array container
 */
void
free_str_set(struct str_set *set);


/**
 * @brief : convert 2 static arrays of in a dynamically allocated tree
 *
 * Takes a set of 2 arrays representing <key, value> pairs, and creates
 * a DS tree of values keyed by <key>
 *
 * @param elem_size provisioned size of strings in the input arrays
 * @param nelems number of actual elements in the input arrays
 * @param keys the static input array of keys
 * @param values the static input array of keys
 * @return a pointer to a ds_tree if successful, NULL otherwise
 */
ds_tree_t *
schema2tree(size_t key_size, size_t val_size, size_t nelems,
            char keys[][key_size],
            char values[][val_size]);


/**
 * @brief allocates a string pair for inclusion in a string pair tree
 *
 * @param key the key element of the pair
 * @param value the value element of the pair
 * @return a string pair pointer if successfull, NULL otherwise.
 */
struct str_pair *
get_pair(const char *key, const char *value);


/**
 * @brief frees a string pair
 *
 * @param pair the string pair to free
 */
void
free_str_pair(struct str_pair *pair);


/**
 * @brief frees a <string key, string value> tree
 *
 * @param set the <string key, string value> tree
 */
void
free_str_tree(ds_tree_t *tree);


/**
 * @brief : converts a static array of integers in a dynamically allocated array
 *
 * Allocates an array with the expected number of entries, and fills it with the
 * values of the input array.
 *
 * @param nelems number of actual elements in the input array
 * @param schema_set the static input array
 * @return a pointer to a <# of elems, integers> container if successful,
 * NULL otherwise
 */
struct int_set *
schema2int_set(size_t nelems, int schema_set[]);


/**
 * @brief frees a integer array container
 *
 * @param set the integer array container
 */
void
free_int_set(struct int_set *set);

/**
 * @brief : convert 2 static arrays of in a dynamically allocated tree
 *
 * Takes a set of 2 arrays representing <string key, int value> pairs,
 * and creates a DS tree of values keyed by <key>
 *
 * @param elem_size provisioned size of strings in the input arrays
 * @param nelems number of actual elements in the input arrays
 * @param keys the static input array of string keys
 * @param values the static input array ofinteger values
 * @return a pointer to a ds_tree if successful, NULL otherwise
 */
ds_tree_t *
schema2itree(size_t elem_size, size_t nelems,
             char keys[][elem_size],
             int values[]);

/**
 * @brief frees a string_ipair
 *
 * @param pair the <string, int> pair to free
 */
void
free_str_ipair(struct str_ipair *pair);

/**
 * @brief frees a strings array container
 *
 * @param set the strings array container
 */
void
free_str_tree(ds_tree_t *tree);

/**
 * @brief frees a <string key, int value> tree
 *
 * @param set the <string key, int value> tree
 */
void
free_str_itree(ds_tree_t *tree);
#endif /* __OVSDB_UTILS_H__ */
