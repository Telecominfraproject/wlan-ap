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

#include "ovsdb_utils.h"
#include "log.h"
#include "target.h"
#include "unity.h"


const char *test_name = "fsm_utils_tests";

/**
 * @brief static array of ovsdb string sets.
 *
 * An OVSDB set of strings is presented to the managers as
 * an array of character strings
 */
char str_arrays[][4][32] =
{
    {
        "set0_entry0",
        "set0_entry1",
        "set0_entry2",
    },
};

/**
 * @brief static array of ovsdb maps.
 *
 * An OVSDB map of strings is presented to the managers as
 * two arrays of character strings.
 */
char str_pairs[][2][4][32] =
{
    {
        {
            "set0_key0",
            "set0_key1",
            "set0_key2",
        },
        {
            "set0_value1",
            "set0_value2",
            "set0_value3",
        },
    },
};


/**
 * @brief static array of ovsdb integer sets.
 *
 * An OVSDB set of integers is presented to the managers as
 * an array of integers.
 */
int int_arrays[][4] =
{
    {
        19,
        17,
        31,
    },
};

/**
 * @brief setUp() is called by the Unity framework before each test
 */
void
setUp(void)
{
    return;
}


/**
 * @brief tearDown() is called by the Unity framework after each test
 */
void
tearDown(void)
{
    return;
}


/**
 * @brief test schema2str_set
 *
 * Validates the conversion of an OVSDB string set to a str_set structure.
 */
void
test_schema2str_set(void)
{
    struct str_set *converted;
    size_t elem_size;
    size_t nelems;
    char **out;
    size_t i;

    /* Gather the conversion paramters */
    elem_size = sizeof(str_arrays[0][0]);
    nelems = 3;

    /* Execute the conversion */
    converted = schema2str_set(elem_size, nelems, str_arrays[0]);

    /* Validate the conversion success */
    TEST_ASSERT_NOT_NULL(converted);

    /* Validate the converted object content */
    out = converted->array;
    for (i = 0; i < nelems; i++)
    {
        TEST_ASSERT_EQUAL_STRING(str_arrays[0][i], *out++);
    }

    /* Free the converted object */
    free_str_set(converted);
}


/**
 * @brief test schema2tree
 *
 * Validates the conversion of an OVSDB string map to a str_pair structure.
 */
void
test_schema2tree(void)
{
    struct str_pair *pair;
    ds_tree_t *converted;
    size_t elem_size;
    char *in_value;
    size_t nelems;
    char *in_key;
    size_t i;

    /* Gather the conversion paramters */
    elem_size = sizeof(str_pairs[0][0][0]);
    nelems = 3;

    /* Execute the conversion */
    converted = schema2tree(elem_size, elem_size, nelems,
                            str_pairs[0][0], str_pairs[0][1]);

    /* Validate the conversion success */
    TEST_ASSERT_NOT_NULL(converted);

    /* Validate the converted object content */
    for (i = 0; i < nelems; i++)
    {
        in_key = str_pairs[0][0][i];
        in_value = str_pairs[0][1][i];
        pair = ds_tree_find(converted, (void *)in_key);
        TEST_ASSERT_NOT_NULL(pair);
        TEST_ASSERT_EQUAL_STRING(in_value, pair->value);
    }

    /* Free the converted object */
    free_str_tree(converted);
}


/**
 * @brief test schema2str_int
 *
 * Validates the conversion of an OVSDB inter set to a int_set structure.
 */
void
test_schema2int_set(void)
{
    struct int_set *converted;
    size_t nelems;
    size_t i;
    int *out;

    /* Gather the conversion paramters */
    nelems = 3;

    /* Execute the conversion */
    converted = schema2int_set(nelems, int_arrays[0]);

    /* Validate the conversion success */
    TEST_ASSERT_NOT_NULL(converted);

    /* Validate the converted object content */
    out = converted->array;
    for (i = 0; i < nelems; i++)
    {
        TEST_ASSERT_EQUAL_INT(int_arrays[0][i], *out++);
    }

    /* Free the converted object */
    free_int_set(converted);
}


/**
 * @brief test schema2itree
 *
 * Validates the conversion of an OVSDB interger map to a str_ipair structure.
 */
void
test_schema2itree(void)
{
    size_t i, nelems, elem_size;
    struct str_ipair *pair;
    ds_tree_t *converted;
    int in_value;
    char *in_key;

    /* Gather the conversion paramters */
    elem_size = sizeof(str_pairs[0][0][0]);
    nelems = 3;

    /* Execute the conversion */
    converted = schema2itree(elem_size, nelems,
                            str_pairs[0][0], int_arrays[0]);

    /* Validate the conversion success */
    TEST_ASSERT_NOT_NULL(converted);

    /* Validate the converted object content */
    for (i = 0; i < nelems; i++)
    {
        in_key = str_pairs[0][0][i];
        in_value = int_arrays[0][i];
        pair = ds_tree_find(converted, (void *)in_key);
        TEST_ASSERT_NOT_NULL(pair);
        TEST_ASSERT_EQUAL_INT(in_value, pair->value);
    }

    /* Free the converted object */
    free_str_itree(converted);
}

int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    target_log_open("TEST", LOG_OPEN_STDOUT);
    log_severity_set(LOG_SEVERITY_INFO);

    UnityBegin(test_name);

    RUN_TEST(test_schema2str_set);
    RUN_TEST(test_schema2tree);
    RUN_TEST(test_schema2int_set);
    RUN_TEST(test_schema2itree);

    return UNITY_END();
}
