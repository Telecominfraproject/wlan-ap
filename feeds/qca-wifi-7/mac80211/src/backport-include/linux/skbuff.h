#ifndef __BACKPORT_SKBUFF_H
#define __BACKPORT_SKBUFF_H
#include_next <linux/skbuff.h>
#include <linux/version.h>

#ifdef CPTCFG_MAC80211_ATHMEMDEBUG
#include <linux/athdebug_skbuff.h>
#endif

#if LINUX_VERSION_IS_LESS(4,20,0)
static inline struct sk_buff *__skb_peek(const struct sk_buff_head *list_)
{
	return list_->next;
}

#if !LINUX_VERSION_IN_RANGE(4,19,10, 4,20,0) && \
    !LINUX_VERSION_IN_RANGE(4,14,217, 4,15,0)
static inline void skb_mark_not_on_list(struct sk_buff *skb)
{
	skb->next = NULL;
}
#endif /* 4.19.10 <= x < 4.20 || 4.14.217 <= x < 4.15 */

#if !LINUX_VERSION_IN_RANGE(4,19,10, 4,20,0)
static inline void skb_list_del_init(struct sk_buff *skb)
{
	__list_del_entry((struct list_head *)&skb->next);
	skb_mark_not_on_list(skb);
}
#endif /* 4.19.10 <= x < 4.20 */
#endif /* < 4.20 */

#if LINUX_VERSION_IS_LESS(5,4,0)
/**
 * skb_frag_off() - Returns the offset of a skb fragment
 * @frag: the paged fragment
 */
#define skb_frag_off LINUX_BACKPORT(skb_frag_off)
static inline unsigned int skb_frag_off(const skb_frag_t *frag)
{
	return frag->page_offset;
}

#define nf_reset_ct LINUX_BACKPORT(nf_reset_ct)
static inline void nf_reset_ct(struct sk_buff *skb)
{
	nf_reset(skb);
}
#endif

#ifndef skb_list_walk_safe
#define skb_list_walk_safe(first, skb, next_skb)				\
	for ((skb) = (first), (next_skb) = (skb) ? (skb)->next : NULL; (skb); 	\
	     (skb) = (next_skb), (next_skb) = (skb) ? (skb)->next : NULL)
#endif

#if LINUX_VERSION_IS_LESS(5,6,0) &&			\
	!LINUX_VERSION_IN_RANGE(5,4,69, 5,5,0) &&	\
	!LINUX_VERSION_IN_RANGE(4,19,149, 4,20,0) &&	\
	!LINUX_VERSION_IN_RANGE(4,14,200, 4,15,0)
/**
 *	skb_queue_len_lockless	- get queue length
 *	@list_: list to measure
 *
 *	Return the length of an &sk_buff queue.
 *	This variant can be used in lockless contexts.
 */
#define skb_queue_len_lockless LINUX_BACKPORT(skb_queue_len_lockless)
static inline __u32 skb_queue_len_lockless(const struct sk_buff_head *list_)
{
	return READ_ONCE(list_->qlen);
}
#endif /* < 5.6.0 */

#if LINUX_VERSION_IS_LESS(5,10,54)
#define skb_get_kcov_handle LINUX_BACKPORT(skb_get_kcov_handle)
static inline u64 skb_get_kcov_handle(struct sk_buff *skb)
{
	return 0;
}
#endif

#if LINUX_VERSION_IS_LESS(5,11,0)
#define napi_build_skb build_skb
#endif

#if LINUX_VERSION_IS_LESS(5,18,6)
static inline struct sk_buff *LINUX_BACKPORT(skb_recv_datagram)(struct sock *sk, unsigned int flags, int *err)
{
	return skb_recv_datagram(sk, flags & ~MSG_DONTWAIT, flags & MSG_DONTWAIT, err);
}
#define skb_recv_datagram LINUX_BACKPORT(skb_recv_datagram)
#endif /* < 5.17 */

#if LINUX_VERSION_IS_LESS(5,4,0)
#define skb_queue_empty_lockless LINUX_BACKPORT(skb_queue_empty_lockless)
/**
 *	skb_queue_empty_lockless - check if a queue is empty
 *	@list: queue head
 *
 *	Returns true if the queue is empty, false otherwise.
 *	This variant can be used in lockless contexts.
 */
static inline bool skb_queue_empty_lockless(const struct sk_buff_head *list)
{
	return READ_ONCE(list->next) == (const struct sk_buff *) list;
}
#endif /* < 5.4 */

#endif /* __BACKPORT_SKBUFF_H */
