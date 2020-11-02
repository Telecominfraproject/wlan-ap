/* SPDX-License-Identifier: BSD-3-Clause */
#ifndef _APC_IFACE_H_
#define _APC_IFACE_H_

#include "lib/lists.h"

extern list iface_list;

struct proto;

struct ifa
{
    node n;
    struct iface * iface;   /* Interface this address belongs to */
    ip_addr ip;             /* IP address of this host */
    ip_addr prefix;         /* Network prefix */
    unsigned pxlen;         /* Prefix length */
    ip_addr brd;            /* Broadcast address */
    unsigned flags;         /* Analogous to iface->flags */
};

struct iface
{
    node n;
    char name[16];
    unsigned flags;
    unsigned mtu;
    unsigned index;     /* OS-dependent interface index */
    list addrs;         /* Addresses assigned to this interface */
    struct ifa * addr;  /* Primary address */
    list neighbors;     /* All neighbors on this interface */
};

/* The Neighbor Cache */

typedef struct neighbor
{
    node n;                 /* Node in global neighbor list */
    node if_n;              /* Node in per-interface neighbor list */
    ip_addr addr;           /* Address of the neighbor */
    struct ifa *ifa;        /* Ifa on related iface */
    struct iface *iface;    /* Interface it's connected to */
    struct proto *proto;    /* Protocol this belongs to */
    void *data;             /* Protocol-specific data */
    unsigned flags;
    int scope;              /* Address scope, -1 for unreachable sticky neighbors */
} neighbor;

/*
 *	Interface Pattern Lists
 */

struct iface_patt {
  node n;
  list ipn_list;			/* A list of struct iface_patt_node */

  /* Protocol-specific data follow after this structure */
};

#endif
