/*
    Copyright 2025 Quectel Wireless Solutions Co.,Ltd

    Quectel hereby grants customers of Quectel a license to use, modify,
    distribute and publish the Software in binary form provided that
    customers shall have no right to reverse engineer, reverse assemble,
    decompile or reduce to source code form any portion of the Software. 
    Under no circumstances may customers modify, demonstrate, use, deliver 
    or disclose any portion of the Software in source code form.
*/

//https://source.codeaurora.org/quic/la/platform/vendor/qcom-opensource/dataservices/tree/rmnetctl
#include <sys/socket.h>
#include <stdint.h>
#include <linux/netlink.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <linux/rtnetlink.h>
#include <linux/gen_stats.h>
#include <net/if.h>
#include <asm/types.h>
#include <linux/rmnet_data.h>
#include "QMIThread.h"

#undef RMNET_EGRESS_FORMAT_AGGREGATION //QSDK SPF11.4
#define RMNET_EGRESS_FORMAT_AGGREGATION         (1U << 31)
#ifndef RMNET_FLAGS_INGRESS_MAP_CKSUMV5
#define RMNET_FLAGS_INGRESS_MAP_CKSUMV5           (1U << 5)
#define RMNET_FLAGS_EGRESS_MAP_CKSUMV5            (1U << 6)
#endif

#define RMNETCTL_SUCCESS 0
#define RMNETCTL_LIB_ERR 1
#define RMNETCTL_KERNEL_ERR 2
#define RMNETCTL_INVALID_ARG 3

#define CONFIG_QRTR_IPQ_NSS_OFFLOAD //defined for ipq nss offload support

enum rmnetctl_error_codes_e {
    RMNETCTL_API_SUCCESS = 0,

    RMNETCTL_API_FIRST_ERR = 1,
    RMNETCTL_API_ERR_MESSAGE_SEND = 3,
    RMNETCTL_API_ERR_MESSAGE_RECEIVE = 4,

    RMNETCTL_INIT_FIRST_ERR = 5,
    RMNETCTL_INIT_ERR_PROCESS_ID = RMNETCTL_INIT_FIRST_ERR,
    RMNETCTL_INIT_ERR_NETLINK_FD = 6,
    RMNETCTL_INIT_ERR_BIND = 7,

    RMNETCTL_API_SECOND_ERR = 9,
    RMNETCTL_API_ERR_HNDL_INVALID = RMNETCTL_API_SECOND_ERR,
    RMNETCTL_API_ERR_RETURN_TYPE = 13,

    RMNETCTL_API_THIRD_ERR = 25,
    /* Failed to copy data into netlink message */
    RMNETCTL_API_ERR_RTA_FAILURE = RMNETCTL_API_THIRD_ERR,
};

struct rmnetctl_hndl_s {
     uint32_t pid;
     uint32_t transaction_id;
     int netlink_fd;
     struct sockaddr_nl src_addr, dest_addr;
};
typedef struct rmnetctl_hndl_s rmnetctl_hndl_t;

#define NLMSG_TAIL(nmsg) \
    ((struct rtattr *) (((char *)(nmsg)) + NLMSG_ALIGN((nmsg)->nlmsg_len)))

struct nlmsg {
    struct nlmsghdr nl_addr;
    struct ifinfomsg ifmsg;
    char data[500];
};

#define MIN_VALID_PROCESS_ID 0
#define MIN_VALID_SOCKET_FD 0
#define KERNEL_PROCESS_ID 0
#define UNICAST 0

/* IFLA Attributes for the RT RmNet driver */
enum {
    RMNETCTL_IFLA_UNSPEC,
    RMNETCTL_IFLA_MUX_ID,
    RMNETCTL_IFLA_FLAGS,
    RMNETCTL_IFLA_DFC_QOS,
    RMNETCTL_IFLA_UPLINK_PARAMS,
    RMNETCTL_IFLA_NSS_OFFLOAD,
    __RMNETCTL_IFLA_MAX,
};

struct rmnetctl_uplink_params {
    uint16_t byte_count;
    uint8_t packet_count;
    uint8_t features;
    uint32_t time_limit;
};

static void rta_parse(struct rtattr **tb, int maxtype, struct rtattr *head,
              int len)
{
    struct rtattr *rta;

    memset(tb, 0, sizeof(struct rtattr *) * maxtype);
    for (rta = head; RTA_OK(rta, len);
         rta = RTA_NEXT(rta, len)) {
        __u16 type = rta->rta_type & NLA_TYPE_MASK;

        if (type > 0 && type < maxtype)
            tb[type] = rta;
    }
}

static struct rtattr *rta_find(struct rtattr *rta, int attrlen, uint16_t type)
{
    for (; RTA_OK(rta, attrlen); rta = RTA_NEXT(rta, attrlen)) {
        if (rta->rta_type == (type & NLA_TYPE_MASK))
            return rta;
    }

    return NULL;
}

static struct rtattr * rta_put(struct nlmsg *req,int type, int len, void *data)
{
    struct rtattr *attrinfo = NLMSG_TAIL(&req->nl_addr);

    attrinfo->rta_type = type;
    attrinfo->rta_len = RTA_ALIGN(RTA_LENGTH(len));
    if (data && len)
        memcpy(RTA_DATA(attrinfo), data, len);
    req->nl_addr.nlmsg_len = NLMSG_ALIGN(req->nl_addr.nlmsg_len) +
                RTA_ALIGN(RTA_LENGTH(len));
    return attrinfo;
}

static int rmnet_get_ack(rmnetctl_hndl_t *hndl, uint16_t *error_code)
{
    struct nlack {
        struct nlmsghdr ackheader;
        struct nlmsgerr ackdata;
        char   data[256];

    } ack;
    int i;

    if (!hndl || !error_code)
        return RMNETCTL_INVALID_ARG;

    if ((i = recv(hndl->netlink_fd, &ack, sizeof(ack), 0)) < 0) {
        *error_code = errno;
        return RMNETCTL_API_ERR_MESSAGE_RECEIVE;
    }

    /*Ack should always be NLMSG_ERROR type*/
    if (ack.ackheader.nlmsg_type == NLMSG_ERROR) {
        if (ack.ackdata.error == 0) {
            *error_code = RMNETCTL_API_SUCCESS;
            return RMNETCTL_SUCCESS;
        } else {
            *error_code = -ack.ackdata.error;
            return RMNETCTL_KERNEL_ERR;
        }
    }

    *error_code = RMNETCTL_API_ERR_RETURN_TYPE;
    return RMNETCTL_API_FIRST_ERR;
}

static int rtrmnet_ctl_init(rmnetctl_hndl_t **hndl, uint16_t *error_code)
{
    struct sockaddr_nl __attribute__((__may_alias__)) *saddr_ptr;
    int netlink_fd = -1;
    pid_t pid = 0;

    if (!hndl || !error_code)
        return RMNETCTL_INVALID_ARG;

    *hndl = (rmnetctl_hndl_t *)malloc(sizeof(rmnetctl_hndl_t));
    if (!*hndl) {
        *error_code = RMNETCTL_API_ERR_HNDL_INVALID;
        return RMNETCTL_LIB_ERR;
    }

    memset(*hndl, 0, sizeof(rmnetctl_hndl_t));

    pid = getpid();
    if (pid  < MIN_VALID_PROCESS_ID) {
        free(*hndl);
        *error_code = RMNETCTL_INIT_ERR_PROCESS_ID;
        return RMNETCTL_LIB_ERR;
    }
    (*hndl)->pid = KERNEL_PROCESS_ID;
    netlink_fd = socket(AF_NETLINK, SOCK_RAW, NETLINK_ROUTE);
    if (netlink_fd < MIN_VALID_SOCKET_FD) {
        free(*hndl);
        *error_code = RMNETCTL_INIT_ERR_NETLINK_FD;
        return RMNETCTL_LIB_ERR;
    }

    (*hndl)->netlink_fd = netlink_fd;

    memset(&(*hndl)->src_addr, 0, sizeof(struct sockaddr_nl));

    (*hndl)->src_addr.nl_family = AF_NETLINK;
    (*hndl)->src_addr.nl_pid = (*hndl)->pid;

    saddr_ptr = &(*hndl)->src_addr;
    if (bind((*hndl)->netlink_fd,
        (struct sockaddr *)saddr_ptr,
        sizeof(struct sockaddr_nl)) < 0) {
        close((*hndl)->netlink_fd);
        free(*hndl);
        *error_code = RMNETCTL_INIT_ERR_BIND;
        return RMNETCTL_LIB_ERR;
    }

    memset(&(*hndl)->dest_addr, 0, sizeof(struct sockaddr_nl));

    (*hndl)->dest_addr.nl_family = AF_NETLINK;
    (*hndl)->dest_addr.nl_pid = KERNEL_PROCESS_ID;
    (*hndl)->dest_addr.nl_groups = UNICAST;

    return RMNETCTL_SUCCESS;
}

static int rtrmnet_ctl_deinit(rmnetctl_hndl_t *hndl)
{
    if (!hndl)
        return RMNETCTL_SUCCESS;

    close(hndl->netlink_fd);
    free(hndl);

    return RMNETCTL_SUCCESS;
}

static int rtrmnet_ctl_newvnd(rmnetctl_hndl_t *hndl, char *devname, char *vndname,
               uint16_t *error_code, uint8_t  index,
               uint32_t flagconfig, uint32_t ul_agg_cnt, uint32_t ul_agg_size)
{
    struct rtattr *datainfo, *linkinfo;
    struct ifla_vlan_flags flags;
    int devindex;
    char *kind = "rmnet";
    struct nlmsg req;
    short id;

    if (!hndl || !devname || !vndname || !error_code)
        return RMNETCTL_INVALID_ARG;

    memset(&req, 0, sizeof(req));
    req.nl_addr.nlmsg_type = RTM_NEWLINK;
    req.nl_addr.nlmsg_len = NLMSG_LENGTH(sizeof(struct ifinfomsg));
    req.nl_addr.nlmsg_flags = NLM_F_REQUEST | NLM_F_CREATE | NLM_F_EXCL |
                  NLM_F_ACK;
    req.nl_addr.nlmsg_seq = hndl->transaction_id;
    hndl->transaction_id++;

    /* Get index of devname*/
    devindex = if_nametoindex(devname);
    if (devindex < 0) {
        *error_code = errno;
        return RMNETCTL_KERNEL_ERR;
    }

    /* Setup link attr with devindex as data */
    rta_put(&req, IFLA_LINK, sizeof(devindex), &devindex);
    rta_put(&req, IFLA_IFNAME, strlen(vndname) + 1, vndname);

    /* Set up IFLA info kind  RMNET that has linkinfo and type */
    linkinfo = rta_put(&req, IFLA_LINKINFO, 0, NULL);
    rta_put(&req, IFLA_INFO_KIND, strlen(kind), kind);

    datainfo = rta_put(&req, IFLA_INFO_DATA, 0, NULL);
    id = index;
    rta_put(&req, RMNETCTL_IFLA_MUX_ID, sizeof(id), &id);

    if (flagconfig != 0) {
        flags.mask  = flagconfig;
        flags.flags = flagconfig;

        rta_put(&req, RMNETCTL_IFLA_FLAGS, sizeof(flags), &flags);
    }

#ifdef CONFIG_QRTR_IPQ_NSS_OFFLOAD
    if (!access("/sys/module/rmnet_nss/", F_OK)) {
        uint8_t offload = 1;

        rta_put(&req, RMNETCTL_IFLA_NSS_OFFLOAD, sizeof(offload), &offload);
    }
#endif

    if (ul_agg_cnt > 1) {
            struct rmnetctl_uplink_params ul_agg;

            ul_agg.byte_count = ul_agg_size;
            ul_agg.packet_count = ul_agg_cnt;
            ul_agg.features = 0;
            ul_agg.time_limit = 3000000;

            rta_put(&req, RMNETCTL_IFLA_UPLINK_PARAMS, sizeof(ul_agg), &ul_agg);
    }

    datainfo->rta_len = (char *)NLMSG_TAIL(&req.nl_addr) - (char *)datainfo;
    linkinfo->rta_len = (char *)NLMSG_TAIL(&req.nl_addr) - (char *)linkinfo;

    if (send(hndl->netlink_fd, &req, req.nl_addr.nlmsg_len, 0) < 0) {
        *error_code = RMNETCTL_API_ERR_MESSAGE_SEND;
        return RMNETCTL_LIB_ERR;
    }

    return rmnet_get_ack(hndl, error_code);
}

int rtrmnet_ctl_new_vnd(char *devname, char *vndname, uint8_t muxid,
               uint32_t qmap_version, uint32_t ul_agg_cnt, uint32_t ul_agg_size)
{
    struct rmnetctl_hndl_s *handle;
    uint16_t error_code;
    int return_code;
    uint32_t flagconfig = RMNET_FLAGS_INGRESS_DEAGGREGATION;

    dbg_time("%s dev: %s, vnd: %s, muxid: %d, qmap_version: %d",
        __func__, devname, vndname, muxid, qmap_version);

    if (ul_agg_cnt > 1)
        flagconfig |= RMNET_EGRESS_FORMAT_AGGREGATION;

    if (qmap_version == 9) { //QMAPV5
        flagconfig |= RMNET_FLAGS_INGRESS_MAP_CKSUMV5;
        flagconfig |= RMNET_FLAGS_EGRESS_MAP_CKSUMV5;
    }
    else if (qmap_version == 5) { //QMAPV1
    }
    else {
        dbg_time("%s donot support qmap_version %d\n", __func__, qmap_version);
        return -1001;
    }
    
    return_code = rtrmnet_ctl_init(&handle, &error_code);
    if (return_code != RMNETCTL_SUCCESS) {
        dbg_time("rtrmnet_ctl_init error_code: %d, return_code: %d, errno: %d (%s)",
            error_code, return_code, errno, strerror(errno));
    }

    return_code = rtrmnet_ctl_newvnd(handle, devname, vndname, &error_code,
        muxid, flagconfig, ul_agg_cnt, ul_agg_size);
    if (return_code == RMNETCTL_KERNEL_ERR)
        dbg_time("rtrmnet_ctl_newvnd RMNETCTL_KERNEL_ERR errno: %d (%s)",
            error_code, strerror(error_code));
    else if (return_code != RMNETCTL_SUCCESS) {
        dbg_time("rtrmnet_ctl_newvnd error_code: %d, return_code: %d",
            error_code, return_code);
    }

    rtrmnet_ctl_deinit(handle);

    return return_code;
}

static int rtrmnet_ctl_getvnd(rmnetctl_hndl_t *hndl, char *vndname,
               uint16_t *error_code, uint16_t *mux_id,
               uint32_t *flagconfig, struct rmnetctl_uplink_params *ul_agg)
{
    struct nlmsg req;
    struct nlmsghdr *resp;
    struct rtattr *attrs, *linkinfo, *datainfo;
    struct rtattr *tb[__RMNETCTL_IFLA_MAX];
    unsigned int devindex = 0;
    int resp_len;

    memset(&req, 0, sizeof(req));

    if (!hndl || !vndname || !error_code || !(mux_id || flagconfig))
        return RMNETCTL_INVALID_ARG;

    req.nl_addr.nlmsg_type = RTM_GETLINK;
    req.nl_addr.nlmsg_len = NLMSG_LENGTH(sizeof(struct ifinfomsg));
    req.nl_addr.nlmsg_flags = NLM_F_REQUEST;
    req.nl_addr.nlmsg_seq = hndl->transaction_id;
    hndl->transaction_id++;

    /* Get index of vndname */
    devindex = if_nametoindex(vndname);
    if (devindex == 0) {
        *error_code = errno;
        return RMNETCTL_KERNEL_ERR;
    }

    req.ifmsg.ifi_index = devindex;
    if (send(hndl->netlink_fd, &req, req.nl_addr.nlmsg_len, 0) < 0) {
        *error_code = RMNETCTL_API_ERR_MESSAGE_SEND;
        return RMNETCTL_LIB_ERR;
    }

    resp_len = recv(hndl->netlink_fd, NULL, 0, MSG_PEEK | MSG_TRUNC);
    if (resp_len < 0) {
        *error_code = errno;
        return RMNETCTL_API_ERR_MESSAGE_RECEIVE;
    }

    resp = malloc((size_t)resp_len);
    if (!resp) {
        *error_code = errno;
        return RMNETCTL_LIB_ERR;
    }

    resp_len = recv(hndl->netlink_fd, (char *)resp, (size_t)resp_len, 0);
    if (resp_len < 0) {
        *error_code = errno;
        free(resp);
        return RMNETCTL_API_ERR_MESSAGE_RECEIVE;
    }

    /* Parse out the RT attributes */
    attrs = (struct rtattr *)((char *)NLMSG_DATA(resp) + NLMSG_ALIGN(sizeof(req.ifmsg)));
    linkinfo = rta_find(attrs, NLMSG_PAYLOAD(resp, sizeof(req.ifmsg)), IFLA_LINKINFO);
    if (!linkinfo) {
        free(resp);
        *error_code = RMNETCTL_API_ERR_RTA_FAILURE;
        return RMNETCTL_KERNEL_ERR;
    }

    datainfo = rta_find(RTA_DATA(linkinfo), RTA_PAYLOAD(linkinfo), IFLA_INFO_DATA);
    if (!datainfo) {
        free(resp);
        *error_code = RMNETCTL_API_ERR_RTA_FAILURE;
        return RMNETCTL_KERNEL_ERR;
    }

    /* Parse all the rmnet-specific information from the kernel */
    rta_parse(tb, __RMNETCTL_IFLA_MAX, RTA_DATA(datainfo), RTA_PAYLOAD(datainfo));
    if (tb[RMNETCTL_IFLA_MUX_ID] && mux_id) {
        *mux_id = *((uint16_t *)RTA_DATA(tb[RMNETCTL_IFLA_MUX_ID]));
        dbg_time("\tMux id: %d", *mux_id);
    }

    if (tb[RMNETCTL_IFLA_FLAGS] && flagconfig) {
        struct ifla_vlan_flags *flags;

        flags = (struct ifla_vlan_flags *)RTA_DATA(tb[RMNETCTL_IFLA_FLAGS]);
        *flagconfig = flags->flags;
        dbg_time("\tData format: 0x%x", *flagconfig);
    }

    if (tb[RMNETCTL_IFLA_UPLINK_PARAMS]) {
        *ul_agg = *(struct rmnetctl_uplink_params *)
        RTA_DATA(tb[RMNETCTL_IFLA_UPLINK_PARAMS]);

        dbg_time("\tUplink Aggregation parameters:");
        dbg_time("\t\tPacket limit: %u", ul_agg->packet_count);
        dbg_time("\t\tByte limit: %u", ul_agg->byte_count);
        dbg_time("\t\tTime limit (ns): %u", ul_agg->time_limit);
        dbg_time("\t\tFeatures : 0x%x", ul_agg->features);
    }

    free(resp);
    return RMNETCTL_API_SUCCESS;
}

int rtrmnet_ctl_get_vnd(char *vndname, int *muxid, int *qmap_version)
{
    struct rmnetctl_hndl_s *handle;
    uint16_t error_code;
    int return_code;
    uint32_t flagconfig = 0;
    uint16_t mux_id = 0;
    struct rmnetctl_uplink_params ul_agg;

    dbg_time("%s vnd: %s", __func__, vndname);
    
    return_code = rtrmnet_ctl_init(&handle, &error_code);
    if (return_code != RMNETCTL_SUCCESS) {
        dbg_time("rtrmnet_ctl_init error_code: %d, return_code: %d, errno: %d (%s)",
            error_code, return_code, errno, strerror(errno));
        return -1;
    }

    // rmnetcli -n getlink rmnet_data0
    return_code = rtrmnet_ctl_getvnd(handle, /*devname, */vndname, &error_code,
        &mux_id, &flagconfig, &ul_agg);
    if (return_code == RMNETCTL_KERNEL_ERR)
        dbg_time("rtrmnet_ctl_uplinkparam RMNETCTL_KERNEL_ERR errno: %d (%s)",
                error_code, strerror(error_code));
    else if (return_code != RMNETCTL_SUCCESS) {
        dbg_time("rtrmnet_ctl_uplinkparam error_code: %d, return_code: %d",
                error_code, return_code);
    }
    rtrmnet_ctl_deinit(handle);

    if (return_code == RMNETCTL_SUCCESS) {
        *muxid = mux_id;
        if (flagconfig&RMNET_FLAGS_INGRESS_DEAGGREGATION) {
            if ((flagconfig&RMNET_FLAGS_INGRESS_MAP_CKSUMV5) && (flagconfig&RMNET_FLAGS_EGRESS_MAP_CKSUMV5))
                *qmap_version = 9; //QMAPV5
            else
                *qmap_version = 5; //QMAPV1
        }
    }

    return return_code;
}

