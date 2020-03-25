OSA Data Structures
====================

This module provides the following data structures:

- Single linked lists
- Double linked lists
- Red-black trees

This data structure implementation is mostly written as inline functions with a pinch of macros thrown in. The functions bodies are mostly inline functions.
Compared to traditional pure-macro implementations (eg. BSD queues), static inline function tend to be easier to read and easier to debug.

One advantage of macros is that it can do limited type checking; the data structure functions mostly take a **void pointer** as parameter. Usually this would be dangerous,
so in order to guard the programmer against some of the most common mistakes, the functions that take a **void pointer** as parameter also require that a field name of the data
structure node is specified.

Here's an example:
```C
/* Example insert */
struct test
{
    int       value;
    ds_list_t node;
};

ds_list_t my_list = DS_LIST_INIT(struct test, node);

struct test my_test;

ds_list_insert(&my_list, &my_test, node);
```

As you can see above, without a `ds_list_t` filed, it is impossible to insert `struct test` into the list.

Containers
----------

A node is the basic definition of an element contained within a data structure. Each structure has its own node type:

- Single linked lists: `ds_list_node_t`
- Double linked lists: `ds_dlist_node_t`
- Red-black trees: `ds_tree_node_t`

Nodes contain no actual data. In order to attach useful information to it, you have to embed it within a structure, for example:

```C
strcut list_data
{
    int                 data_value;
    ds_list_node_t      data_node;
};
```
The node field is required when initializing, inserting or removing from the data structure. This node parameter is required as this guards
from mistakenly inserting random structures into the list.

This is an example structure that can be a member of a single linked list, a double linked list and a tree *simultaneously*.

```C
struct my_data
{
    int                 value;              /* The value of this node   */

    /*
     * Data structure nodes
     */
    ds_list_node_t      lnode;              /* Single list node data    */
    ds_dlist_node_t     dnode;              /* Dobule list data         */
    ds_tree_node_t      tnode;              /* Tree node data           */
};

/*
 * Example initialization
 */
ds_list_t   my_list     = DS_LIST_INIT(struct my_data, lnode);
ds_dlist_t  my_dlist    = DS_DLIST_INIT(struct my_data, dnode);
ds_tree_t   my_tree     = DS_TREE_INIT(struct my_data, dnode, compare_my_data);

/*
 * Example insertions
 */
struct my_data data;

ds_list_insert_head(&my_list, &data, lnode);
ds_dlist_insert_head(&my_list, &data, dnode);
ds_tree_insert(&my_list, &data, tnode, "my_key");

```
Single Linked Lists
===================

Single linked lists are geared towards speed and space optimization at the expense of features. Single linked lists support only insertions
at the head of the list or positional inserts with iterators. Nodes can be only removed from the head (`ds_list_remove_head()`) or with
iterators (`ds_list_iremove()`).

This data structure is most suitable for implementing simple queues or keeping a simple list of things in a space efficient manner.

To use single linked list, include the following header:

```C
#include "ds_list.h"
```
Initialization
--------------

```C
/* Static initializer for single linked lists */
ds_list_t list = DS_LIST_INIT(struct my_data, lnode);

/* Declare an iterator */
ds_list_iter_t iter;

/* Run-time initilizer */
ds_list_init(&list, struct my_data, lnode);

```

Insert
------

```C
struct my_data single_node;

/*
 * Add single_node to the head of the list
 */
ds_list_insert_head(&list, &single_node, lnode);

/*
 * Insert at the end of the list using an iterator
 */
strucy my_data *data;

/* Traverse the list */
for (data = ds_list_ibegin(&iter, &list); data != NULL; data = ds_list_inext(&iter));

/* Insert at the tail */
ds_list_iinsert(&iter, &single_node, lnode);
```

Remove
------

Single linked lists only support removal from the head or using iterators.

```C
/*
 * Remove from the head of the structure
 */
ds_list_remove_head(&list);
```

Double Linked Lists
===================

Double linked lists are somewhat slower than single linked lists, however they provide more features. For example, they supporte in-place removal and
tail insertion. Compared to single linked lists, each node takes up more space (2 pointers compared to 1 pointer in the case of single linked lists).

To use double linked list, include the following header:

```C
#include "ds_dlist.h"
```

Initialization
--------------

```C
/* Static initializer for double linked lists */
ds_dlist_t dlist = DS_DLIST_INIT(struct my_data, dnode);

/* Declare an iterator */
ds_dlist_iter_t iter;

/* Run-time initilizer */
ds_dlist_init(&dlist, struct my_data, dnode);
```

Insert
------

Double linked lists support head and tail inserts, as well as iterator positional inserts.

```C
/* Insert at head */
ds_dlist_insert_head(&list, &my_data, dnode);

/* Insert at tail */
ds_dlist_insert_tail(&list, &my_data, dnode);
```

Remove
------

Double linked lists support head, tail removes and iterator positional removals. One significant advantage over single linked lists
is that they  also support in-place removal. This means that it is possible to remove nodes directly, without the use of iterators, as long
as you know the pointer to the node.

```C
/* Remove at head */
ds_dlist_remove_head(&list);

/* Remove at tail */
ds_dlist_remove_tail(&list);

/* Insert element */
struct my_data random_node;
ds_dlist_insert_tail(&list, &random_node, dnode);

/* ...  Insert more elements at the tail of the list ... */

/* In-place remove */
ds_dlist_remove(&list, &random_node, dnode);
```

Red-Black Trees
===============

Red-black trees are the holy grail of data structures. This is probably the most general purpose data-structure ever invented as it is blazing fast
no matter what you throw at it. Only specialized data structures can beat it in terms of raw speed. As this is a binary search tree it means that
elements are sorted. This may or not may be a desirable property (you cannot implement queues with it, and you shouldn't).

To use red-black trees, include the following header:

```C
#include "ds_tree.h"
```

Initialization
--------------
Red-black trees are a sorted list, this means that the data structure requires a compare function to compare the lexicographical order of keys. They key compare
function has to be specified at initialization and its definition loosely resembles strcmp():

```C
typedef int ds_key_cmp_t(void *a, void *b);
```

The function should return a negative number if a < b, 0 if a == b or a positive number of a > b.

```C
/*
 * The keys are pointers to the data_value field in struct tree_node
 */
int tree_compare(void *a, void *b)
{
    /* Calculate difference between integers */
    return *(int *)a - *(int *)b;
}

/* Static initializer for red-black tress */
ds_tree_t tree = ADS_TREE_INIT(struct my_data, tnode, tree_compare);

/* Runtime initialization */
ds_tree_init(&tree, struct my_data, tnode, tree_compare);

/* Declare an iterator */
ds_tree_iter_t iter;
```
Insert
------
Red-black trees elements are stored in a lexicographical order. This means that the insertion order is never preserved. As such, it makes no sense to implement
positional iterator inserts.

Insertion is only possible by specifying a key:

```C
/* They key can be part of the structure  */
ds_tree_insert(&tree, &string_data, data_node, &string_data.key);

/* Or it can be a pointer to a static string */
ds_tree_insert(&tree, &string_data, data_node, "Hello World");
```
Note that only the pointer to the key is stored in the data structure, therefore the user must ensure that the key's storage isn't freed prematurely. A good practice
is to store the key data inside the container structure.

Find
----
This is the only data structure that supports element lookup. Simply use the key that was used during insert to find the specified node.

```C
struct tree_node *data = ds_tree_find(&tree, key);
```

Remove
------
Red-black trees support only in-place and iterator removal. If you wish to delete a specific element, you can look it up using the `ds_tree_find()` method and then
doing an in-place remove.

```C
/*
 * Example in-place remove after a find
 */

struct my_data* data = ds_tree_find(&tree, "hello");
ds_tree_remove(&tree, data, tnode);
```
Iterators
---------
Iterators are primarily used to traverse the data structure. The API is unified between all the data structures and one data structure can be switched with another
simply by renaming the iterator functions.

### Iterator Traversal ###
The main iterator API functions are `ds_TYPE_ibegin()` and `ds_TYPE_inext()`. The `ds_TYPE_begin()` function initializes the iterator and sets the position to the
first element in the data structure. The `ds_TYPE_inext()` function moves the iterator to the next position. This is an example how to print every node in a list:

```C
struct my_data *data;

for (data = ds_list_ibegin(&iter, &my_list); data != NULL; data = ds_list_inext(&iter))
{
    printf("%d\n", data->data_value);
}
```

### Iterator Remove ###
All of the data structures also support the iterator remove function, `ds_TYPE_iremove()`, which deletes the element at the current position. Since the element that
was just deleted is no longer valid, this function moves the iterator forward and returns the next node. As such, this function is similar to `ds_TYPE_inext()`
except that it also deletes the current node. This means that the programmer must be careful not to use `ds_TYPE_inext()` right after a `ds_TYPE_iremove()`.

This is an example of a selective removal using iterators:

```C
strucy my_data *data;

/* Remove a single value */
for (data = ds_list_ibegin(&iter, &list); data != NULL;)
{
    if (data->data_value == 5)
    {
        data = ds_list_iremove(&iter);
    }
    else
    {
        data = ds_list_inext(&iter);
    }
}

/* Remove all elements from a list */
for (data = ds_list_ibegin(&iter, &list); data != NULL; data = ds_list_iremove(&iter));
```

*Note: If you use ds_list_inext() after ds_list_iremove() you're effectively skipping an element as ds_list_iremove() moves forward as well.*

### Iterator Insert ###
Some data structures (single and double lists) support iterator positional insert by implementing the `ds_TYPE_iinsert()` function. This function inserts
a node *before* the current element or at the tail of the list of the current element is NULL.

Example of a sorted insert using `ds_list_iinsert()`:

```C
ds_list_iter_t      iter;
struct my_data      node4;
strucy my_data*     data;

/*
 * Assume that the list contains the "0,1,2,3,5,6,7,8,9" elements
 * and we want to insert "4"
 */
node4.data_value = 4;

for (data = ds_list_ibegin(&iter, list); data != NULL; data = ds_list_inext(&iter))
{
    /* If the current value is greater than "4", break out */
    if (data->data_value > node4.data_value) break;
}

/*
 * This will work even if node is NULL and we reached the end of the list above,
 * the element will be inserted at the tail
 */
ds_list_iinsert(&iter, &node4, data_node);
```


Quick Reference
===============

|                |    Single Lists   |     Double Lists     |   Red-Black Trees     |     Description
|--------------: | :---------------: | :------------------: | :-------------------: | :---------------------------------------------------------------------------------------
|*Header*        |     ds_list.h     |       ds_dlist.h     |       ds_tree.h       |     Include header
|*Prefix*        |    `ds_list`      |      `ds_dlist`      |      `ds_tree`        |     Function/types prefix
|insert          |                   |                      |         x             |     Insert by key
|find            |                   |                      |         x             |     Find by key
|remove          |                   |        x             |         x             |     In-place remove of a node
|insert_head     |      x            |        x             |                       |     Insert before first element
|remove_head     |      x            |        x             |                       |     Remove first element
|insert_tail     |                   |        x             |                       |     Insert after last element
|remove_tail     |                   |        x             |                       |     Remove last element
|ibegin          |      x            |        x             |         x             |     Initialize the iterator and return the first node
|inext           |      x            |        x             |         x             |     Get next node and move the iterator position forward
|iinsert         |      x            |        x             |                       |     Insert right before the current iterator position
|iremove         |      x            |        x             |         x             |     Get next node while removing the node at the current iterator position


