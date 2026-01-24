/*
    Copyright 2025 Quectel Wireless Solutions Co.,Ltd

    Quectel hereby grants customers of Quectel a license to use, modify,
    distribute and publish the Software in binary form provided that
    customers shall have no right to reverse engineer, reverse assemble,
    decompile or reduce to source code form any portion of the Software. 
    Under no circumstances may customers modify, demonstrate, use, deliver 
    or disclose any portion of the Software in source code form.
*/

#ifndef __QUECTEL_LIST_H__
#define __QUECTEL_LIST_H__
struct qlistnode
{
    struct qlistnode *next;
    struct qlistnode *prev;
};

#define qnode_to_item(node, container, member) \
    (container *) (((char*) (node)) - offsetof(container, member))

#define qlist_for_each(node, list) \
    for (node = (list)->next; node != (list); node = node->next)

#define qlist_empty(list) ((list) == (list)->next)
#define qlist_head(list) ((list)->next)
#define qlist_tail(list) ((list)->prev)

static void qlist_init(struct qlistnode *node)
{
    node->next = node;
    node->prev = node;
}

static void qlist_add_tail(struct qlistnode *head, struct qlistnode *item)
{
    item->next = head;
    item->prev = head->prev;
    head->prev->next = item;
    head->prev = item;
}

static void qlist_remove(struct qlistnode *item)
{
    item->next->prev = item->prev;
    item->prev->next = item->next;
}
#endif