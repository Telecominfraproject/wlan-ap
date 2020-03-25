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

#include "ds_tree.h"
#include "ovsdb_utils.h"
#include "log.h"

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
               char schema_set[][elem_size])
{
    struct str_set *set;
    char **array;
    char *item;
    bool loop;
    size_t i;

    if (nelems == 0) return NULL;

    array = calloc(nelems, sizeof(*array));
    if (array == NULL) return NULL;

    set = calloc(1, sizeof(*set));
    if (set == NULL) goto err_free_array;

    i = 0;
    do {
        item = schema_set[i];
        array[i] = strdup(item);
        loop = (array[i] != NULL);
        i++;
        loop &= (i < nelems);
    } while (loop);

    if (i != nelems) goto err_free_array_items;

    set->array = array;
    set->nelems = nelems;

    return set;

err_free_array_items:
    for (i = 0; i < nelems; i++) free(array[i]);
    free(set);

err_free_array:
    free(array);

    return NULL;
}


/**
 * @brief frees a strings array container
 *
 * @param set the strings array container
 */
void
free_str_set(struct str_set *set)
{
    char **array_iter;
    size_t nelems;
    char **array;
    size_t i;

    if (set == NULL) return;

    nelems = set->nelems;
    array = set->array;
    array_iter = array;
    for (i = 0; i < nelems; i++) free(*array_iter++);
    free(array);
    free(set);
}

/**
 * @brief comparing routine for a string pair tree
 *
 * @param a the first string of the comparison
 * @param b the second string of the comparison
 * @return the string comparision result
 */
int
str_tree_cmp(void *a, void *b)
{
    const char *key_a = (char *)a;
    const char *key_b = (char *)b;

    return strcmp(key_a, key_b);
}


/**
 * @brief allocates a string pair for inclusion in a string pair tree
 *
 * @param key the key element of the pair
 * @param value the value element of the pair
 * @return a string pair pointer if successfull, NULL otherwise
 */
struct str_pair *
get_pair(const char *key, const char *value)
{
    struct str_pair *pair;

    pair = calloc(1, sizeof(*pair));
    if (pair == NULL) return NULL;

    pair->key = strdup(key);
    if (pair->key == NULL) goto err_free_pair;

    pair->value = strdup(value);
    if (pair->value == NULL) goto err_free_key;

    return pair;

err_free_key:
    free(pair->key);

err_free_pair:
    free(pair);

    return NULL;
}


/**
 * @brief : convert 2 static arrays of in a dynamically allocated tree
 *
 * Takes a set of 2 arrays representing <string key, string value> pairs,
 * and creates a DS tree of values keyed by <key>
 *
 * @param elem_size provisioned size of strings in the input arrays
 * @param nelems number of actual elements in the input arrays
 * @param keys the static input array of string keys
 * @param values the static input array of string values
 * @return a pointer to a ds_tree if successful, NULL otherwise
 */
ds_tree_t *
schema2tree(size_t key_size, size_t value_size, size_t nelems,
            char keys[][key_size],
            char values[][value_size])
{
    struct str_pair *pair;
    ds_tree_t *tree;
    char *value;
    char *key;
    bool loop;
    size_t i;

    if (nelems == 0) return NULL;

    tree = calloc(1, sizeof(*tree));
    if (tree == NULL) return NULL;

    ds_tree_init(tree, str_tree_cmp, struct str_pair, pair_node);

    i = 0;
    do {
        key = keys[i];
        value = values[i];
        pair = get_pair(key, value);
        loop = (pair != NULL);
        ds_tree_insert(tree, pair, pair->key);
        i++;
        loop &= (i < nelems);
    } while (loop);

    if (i != nelems) goto err_free_tree;

    return tree;

err_free_tree:
    free_str_tree(tree);

    return NULL;
}


/**
 * @brief frees a string_pair
 *
 * @param pair the string pair to free
 */
void
free_str_pair(struct str_pair *pair)
{
    if (pair == NULL) return;

    free(pair->key);
    free(pair->value);
    free(pair);
}


/**
 * @brief frees a strings array container
 *
 * @param set the strings array container
 */
void
free_str_tree(ds_tree_t *tree)
{
    struct str_pair *to_remove;
    struct str_pair *pair;

    if (tree == NULL) return;

    pair = ds_tree_head(tree);
    while (pair != NULL)
    {
        to_remove = pair;
        pair = ds_tree_next(tree, pair);
        ds_tree_remove(tree, to_remove);
        free_str_pair(to_remove);
    }

    free(tree);
    return;
}


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
schema2int_set(size_t nelems, int schema_set[])
{
    struct int_set *set;
    int *array;
    size_t i;

    array = calloc(nelems, sizeof(*array));
    if (array == NULL) return NULL;

    set = calloc(1, sizeof(*set));
    if (set == NULL) goto err_free_array;

    for (i = 0; i < nelems; i++) array[i] = schema_set[i];

    set->array = array;
    set->nelems = nelems;

    return set;

err_free_array:
    free(array);

    return NULL;
}


/**
 * @brief frees a strings array container
 *
 * @param set the strings array container
 */
void
free_int_set(struct int_set *set)
{
    if (set == NULL) return;

    free(set->array);
    free(set);
}

/**
 * @brief allocates a string pair for inclusion in a ipair tree
 *
 * @param key the key element of the pair
 * @param value the value element of the pair
 * @return a string pair pointer if successfull, NULL otherwise
 */
struct str_ipair *
get_ipair(const char *key, int value)
{
    struct str_ipair *pair;

    pair = calloc(1, sizeof(*pair));
    if (pair == NULL) return NULL;

    pair->key = strdup(key);
    if (pair->key == NULL) goto err_free_pair;

    pair->value = value;

    return pair;

err_free_pair:
    free(pair);

    return NULL;
}


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
             int values[])
{
    struct str_ipair *pair;
    ds_tree_t *tree;
    char *key;
    int value;
    bool loop;
    size_t i;

    if (nelems == 0) return NULL;

    tree = calloc(1, sizeof(*tree));
    if (tree == NULL) return NULL;

    ds_tree_init(tree, str_tree_cmp, struct str_ipair, pair_node);

    i = 0;
    do {
        key = keys[i];
        value = values[i];
        pair = get_ipair(key, value);
        loop = (pair != NULL);
        ds_tree_insert(tree, pair, pair->key);
        i++;
        loop &= (i < nelems);
    } while (loop);

    if (i != nelems) goto err_free_tree;

    return tree;

err_free_tree:
    free_str_itree(tree);

    return NULL;
}


/**
 * @brief frees a string_ipair
 *
 * @param pair the <string, int> pair to free
 */
void
free_str_ipair(struct str_ipair *pair)
{
    if (pair == NULL) return;

    free(pair->key);
    free(pair);
}


/**
 * @brief frees a <string key, int value> tree
 *
 * @param set the <string key, int value> tree
 */
void
free_str_itree(ds_tree_t *tree)
{
    struct str_ipair *to_remove;
    struct str_ipair *pair;

    if (tree == NULL) return;

    pair = ds_tree_head(tree);
    while (pair != NULL)
    {
        to_remove = pair;
        pair = ds_tree_next(tree, pair);
        ds_tree_remove(tree, to_remove);
        free_str_ipair(to_remove);
    }

    free(tree);
    return;
}

