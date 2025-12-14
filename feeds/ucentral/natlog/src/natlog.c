#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/netfilter.h>
#include <linux/netfilter_ipv4.h>
#include <linux/skbuff.h>
#include <linux/ip.h>
#include <linux/tcp.h>
#include <linux/udp.h>
#include <linux/netfilter/nfnetlink.h>
#include <net/netfilter/nf_conntrack.h>
#include <net/netfilter/nf_log.h>
#include <linux/etherdevice.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("KMK");
MODULE_DESCRIPTION("Kernel module to log client request SNAT and server response DNAT using nf_conntrack");

static struct nf_hook_ops nat_hook_ops_pre;
static struct nf_hook_ops nat_hook_ops_post;

static void log_nat_info(struct nf_conn *ct, struct sk_buff *skb, unsigned int hooknum)
{
    struct nf_conntrack_tuple *orig_tuple, *reply_tuple;
    char *proto_name;
    __u16 sport, dport, nat_sport, nat_dport;
    __u32 saddr, daddr, nat_saddr, nat_daddr;
    unsigned char *mac_addr;
    char mac_str[18];

    if (!ct)
        return;

    orig_tuple = &ct->tuplehash[IP_CT_DIR_ORIGINAL].tuple;
    reply_tuple = &ct->tuplehash[IP_CT_DIR_REPLY].tuple;

    switch (orig_tuple->dst.protonum) {
        case IPPROTO_TCP:
            proto_name = "TCP";
            break;
        case IPPROTO_UDP:
            proto_name = "UDP";
            break;
        default:
            return;
    }

    saddr = orig_tuple->src.u3.ip;
    daddr = orig_tuple->dst.u3.ip;
    sport = ntohs(orig_tuple->src.u.all);
    dport = ntohs(orig_tuple->dst.u.all);

    nat_saddr = reply_tuple->dst.u3.ip;
    nat_daddr = reply_tuple->src.u3.ip;
    nat_sport = ntohs(reply_tuple->dst.u.all);
    nat_dport = ntohs(reply_tuple->src.u.all);

    if (hooknum == NF_INET_POST_ROUTING && (ct->status & IPS_SRC_NAT)) {
        if (!skb_mac_header_was_set(skb))
            return;
        mac_addr = skb_mac_header(skb);
        snprintf(mac_str, sizeof(mac_str), "%02x:%02x:%02x:%02x:%02x:%02x",
                 mac_addr[6], mac_addr[7], mac_addr[8], mac_addr[9], mac_addr[10], mac_addr[11]);
        printk(KERN_INFO "NAT_LOG: %s SRC MAC: %s; Original: %pI4:%u -> %pI4:%u, NAT: %pI4:%u -> %pI4:%u\n",
               proto_name, mac_str,
               &saddr, sport, &daddr, dport,
               &nat_saddr, nat_sport, &daddr, dport);
    }
}

static unsigned int nat_hook_func(void *priv,
                                 struct sk_buff *skb,
                                 const struct nf_hook_state *state)
{
    struct nf_conn *ct;
    enum ip_conntrack_info ct_info;

    ct = nf_ct_get(skb, &ct_info);
    if (!ct) {
        printk(KERN_DEBUG "NAT_LOG: No conntrack info for packet\n");
        return NF_ACCEPT;
    }

    if (ct->status & IPS_NAT_MASK) {
        log_nat_info(ct, skb, state->hook);
    }

    return NF_ACCEPT;
}

static int __init nat_logger_init(void)
{
    int ret;

    nat_hook_ops_post.hook = nat_hook_func;
    nat_hook_ops_post.pf = PF_INET;
    nat_hook_ops_post.hooknum = NF_INET_POST_ROUTING;
    nat_hook_ops_post.priority = NF_IP_PRI_NAT_SRC;

    ret = nf_register_net_hook(&init_net, &nat_hook_ops_post);
    if (ret) {
        printk(KERN_ERR "NAT_LOG: Failed to register POST_ROUTING hook: %d\n", ret);
        nf_unregister_net_hook(&init_net, &nat_hook_ops_post);
        return ret;
    }

    printk(KERN_INFO "NAT_LOG: Module loaded successfully\n");
    return 0;
}

static void __exit nat_logger_exit(void)
{
    nf_unregister_net_hook(&init_net, &nat_hook_ops_post);
    printk(KERN_INFO "NAT_LOG: Module unloaded\n");
}

module_init(nat_logger_init);
module_exit(nat_logger_exit);
