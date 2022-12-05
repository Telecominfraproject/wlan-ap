/*
 * Copyright (c) 2014, 2019 Qualcomm Technologies, Inc.
 *
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <limits.h>

#include <linux/nl80211.h>
#include <linux/netlink.h>
#include <net/if.h>

#include <netlink/genl/genl.h>
#include <netlink/genl/family.h>
#include <netlink/genl/ctrl.h>
#include <netlink/msg.h>
#include <netlink/attr.h>

#define ATH11K_TM_DATA_MAX_LEN          5000
#define ATH11K_WMI_UNIT_TEST_MAX_NUM_ARGS 100

#define WMI_TAG_ARRAY_UINT32	16
#define WMI_TAG_VDEV_SET_PARAM_CMD	95
#define WMI_TAG_PDEV_SET_PARAM_CMD	82
#define WMI_TAG_UNIT_TEST_CMD		327

#define WMI_TLV_HDR_SIZE	(sizeof(unsigned int))

#define WMI_TLV_LEN_SHIFT	0
#define WMI_TLV_LEN_MASK	0x0000ffff

#define WMI_TLV_TAG_SHIFT	16
#define WMI_TLV_TAG_MASK	0xffff0000

#define FIELD_PREP(a, b)	(((a) << (b##_SHIFT)) & (b##_MASK))

#define WMI_GRP_FWTEST 0x1f
#define WMI_GRP_PDEV   0x04
#define WMI_GRP_VDEV   0x05
#define WMI_TLV_CMD(grp_id) (((grp_id) << 12) | 0x1)
#define WMI_UNIT_TEST_CMDID (WMI_TLV_CMD(WMI_GRP_FWTEST) + 2)
#define WMI_VDEV_SET_PARAM_CMDID (WMI_TLV_CMD(WMI_GRP_VDEV) + 7)
#define WMI_PDEV_SET_PARAM_CMDID (WMI_TLV_CMD(WMI_GRP_PDEV) + 2)

enum ath11k_tm_attr {
	__ATH11K_TM_ATTR_INVALID	= 0,
	ATH11K_TM_ATTR_CMD		= 1,
	ATH11K_TM_ATTR_DATA		= 2,
	ATH11K_TM_ATTR_WMI_CMDID	= 3,
	ATH11K_TM_ATTR_VERSION_MAJOR	= 4,
	ATH11K_TM_ATTR_VERSION_MINOR	= 5,
	ATH11K_TM_ATTR_WMI_OP_VERSION	= 6,

	/* keep last */
	__ATH11K_TM_ATTR_AFTER_LAST,
	ATH11K_TM_ATTR_MAX              = __ATH11K_TM_ATTR_AFTER_LAST - 1,
};

/* All ath11k testmode interface commands specified in
 * ATH11K_TM_ATTR_CMD
 */
enum ath11k_tm_cmd {
	/* Returns the supported ath11k testmode interface version in
	 * ATH11K_TM_ATTR_VERSION. Always guaranteed to work. User space
	 * uses this to verify it's using the correct version of the
	 * testmode interface
	 */
	ATH11K_TM_CMD_GET_VERSION = 0,

	/* Boots the UTF firmware, the netdev interface must be down at the
	 * time.
	 */
	ATH11K_TM_CMD_TESTMODE_START = 1,

	/* Shuts down the UTF firmware and puts the driver back into OFF
	 * state.
	 */
	ATH11K_TM_CMD_TESTMODE_STOP = 2,

	/* The command used to transmit a FW test WMI command to the firmware
	 * and the event to receive WMI events from the firmware. Without
	 * struct wmi_cmd_hdr header, only the WMI payload. Command id is
	 * provided with ATH11K_TM_ATTR_WMI_CMDID and payload in
	 * ATH11K_TM_ATTR_DATA.
	 */
	ATH11K_TM_CMD_WMI_FW_TEST = 3,

	/* The command used to transmit a FTM WMI command to the firmware
	 * and the event to receive WMI events from the firmware.The data
	 * received  only contain the payload, Need to add the tlv
	 * header and send the cmd to fw with commandid WMI_PDEV_UTF_CMDID.
	 */
	ATH11K_TM_CMD_WMI_FTM = 4,

};

struct nl80211_socket_info {
	struct nl_sock *nl_sock;
	int genl_family_id;
};

struct wmi_unit_test_cmd {
	unsigned int tlv_header;
	unsigned int vdev_id;
	unsigned int module_id;
	unsigned int num_args;
	unsigned int diag_token;
};

struct wmi_vdev_pdev_set_param_cmd {
	unsigned int tlv_header;
	unsigned int vdev_pdev_id;
	unsigned int param_id;
	unsigned int param_value;
};

enum wmi_unit_test_cmd_type {
	ATH11K_WMI_FW_UNIT_TEST_CMD,
	ATH11K_WMI_VDEV_SET_PARAM_CMD,
	ATH11K_WMI_PDEV_SET_PARAM_CMD,
};

int init_nl_sock(struct nl80211_socket_info *sock)
{
	int ret;

	sock->nl_sock = nl_socket_alloc();
	if (!sock->nl_sock) {
		fprintf(stderr, "Failed to create nl socket\n");
		return -ENOMEM;
	}

	if (genl_connect(sock->nl_sock)) {
		fprintf(stderr, "Failed to connect to nl socket\n");
		ret = -ENOLINK;
		goto free_nl_sock;
	}

	nl_socket_set_buffer_size(sock->nl_sock, 0, ATH11K_TM_DATA_MAX_LEN);

	sock->genl_family_id = genl_ctrl_resolve(sock->nl_sock, "nl80211");
	if (sock->genl_family_id < 0) {
		fprintf(stderr, "genl family not found!!\n");
		ret = -ENOENT;
		goto free_nl_sock;
	}

	return 0;
free_nl_sock:
	nl_socket_free(sock->nl_sock);
	return ret;
}

void usage(char *argv[])
{
	fprintf(stderr, "Usage: %s -t <cmd type> -i <wlanX> -m <moduleid> -v <vdevid> <argument list>\n",
		argv[0]);
}

static int nl_cb_error_handler(struct sockaddr_nl *nla, struct nlmsgerr *err, void *arg)
{
	int *ret = arg;
	*ret = err->error;
	return NL_STOP;
}

static int nl_cb_finish_handler(struct nl_msg *msg, void *arg)
{
	int *ret = arg;
	*ret = 0;
	return NL_SKIP;
}

static int nl_cb_ack_handler(struct nl_msg *msg, void *arg)
{
	int *ret = arg;
	*ret = 0;
	return NL_STOP;
}

void *ath11k_fwtest_fill_cmd_buf(unsigned int module_id, unsigned int vdev_id,
				 int num_args, char *argv[], int optind, int *buf_sz)
{
	struct wmi_unit_test_cmd *cmd;
	unsigned int *buf;
	int buf_size, i;

	buf_size = sizeof(struct wmi_unit_test_cmd) + WMI_TLV_HDR_SIZE + num_args * sizeof(unsigned int);
	*buf_sz = buf_size;

	buf = (unsigned int *) malloc(buf_size);
	if (!buf) {
		fprintf(stderr, "Failed to allocate memory\n");
		return NULL;
	}

	memset((void *)buf, 0, buf_size);
	cmd = (struct wmi_unit_test_cmd *) buf;

	cmd->tlv_header = FIELD_PREP(WMI_TAG_UNIT_TEST_CMD, WMI_TLV_TAG) |
			  FIELD_PREP(sizeof(struct wmi_unit_test_cmd) - WMI_TLV_HDR_SIZE, WMI_TLV_LEN);

	cmd->module_id = module_id;
	cmd->vdev_id = vdev_id;
	cmd->num_args = num_args;
	cmd->diag_token = 0;

	buf = (unsigned int *)((char *)buf + sizeof(struct wmi_unit_test_cmd));
	*buf = FIELD_PREP(WMI_TAG_ARRAY_UINT32, WMI_TLV_TAG) |
	       FIELD_PREP(num_args * sizeof(unsigned int), WMI_TLV_LEN);
	buf++;

	for (i = 0; i < num_args; i++, buf++)
		*buf = strtoul(argv[optind + i], NULL, 0);

	return cmd;
}

void *ath11k_wmi_vdev_pdev_set_cmd_fill_buf(unsigned int type, unsigned int vdev_pdev_id,
					    char *argv[], int optind, int *buf_sz)
{
	struct wmi_vdev_pdev_set_param_cmd *cmd;
	unsigned int *buf;
	int buf_size;
	int tag;

	buf_size = sizeof(struct wmi_vdev_pdev_set_param_cmd);
	*buf_sz = buf_size;

	buf = (unsigned int *) malloc(buf_size);
	if (!buf) {
		fprintf(stderr, "Failed to allocate memory\n");
		return NULL;
	}

	memset((void *)buf, 0, buf_size);
	cmd = (struct wmi_vdev_pdev_set_param_cmd *) buf;

	if (type == ATH11K_WMI_VDEV_SET_PARAM_CMD)
		tag = WMI_TAG_VDEV_SET_PARAM_CMD;
	else
		tag = WMI_TAG_PDEV_SET_PARAM_CMD;

	cmd->tlv_header = FIELD_PREP(tag, WMI_TLV_TAG) |
			  FIELD_PREP(sizeof(struct wmi_vdev_pdev_set_param_cmd) - WMI_TLV_HDR_SIZE, WMI_TLV_LEN);

	cmd->vdev_pdev_id = vdev_pdev_id;
	cmd->param_id = strtoul(argv[optind], NULL, 0);
	cmd->param_value = strtoul(argv[optind + 1], NULL, 0);

	return cmd;
}

int main(int argc, char *argv[])
{
	struct nl80211_socket_info sock;
	void *cmd = NULL;
	struct nl_msg *msg;
	struct nlattr *nest;
	struct nl_cb *cb;
	unsigned int cmd_id;
	char *ifname = NULL;
	int netdev_idx;
	unsigned int module_id = UINT_MAX;
	unsigned int vdev_id = UINT_MAX;
	unsigned int pdev_id = UINT_MAX;
	unsigned int  type = ATH11K_WMI_FW_UNIT_TEST_CMD;
	int num_args;
	int buf_size;
	int opt;
	int ret = 1;

	while (1) {
		opt = getopt(argc, argv, "t:i:m:v:p:h");
		if (opt < 0)
			break;

		switch (opt) {
		case 'i':
			ifname = optarg;
			break;
		case 't':
			type = strtoul(optarg, NULL, 0);
			break;
		case 'm':
			module_id = strtoul(optarg, NULL, 0);
			break;
		case 'v':
			vdev_id = strtoul(optarg, NULL, 0);
			break;
		case 'p':
			pdev_id = strtoul(optarg, NULL, 0);
			break;
		case 'h':
			/* fall through */
		default:
			usage(argv);
			return ret;
		}
	}

	if (!ifname || module_id == UINT_MAX ||
	    (type != ATH11K_WMI_PDEV_SET_PARAM_CMD && vdev_id == UINT_MAX)) {
		fprintf(stderr, "Mandatory options are missing!!\n");
		usage(argv);
		return -EINVAL;
	}

	if (optind >= argc) {
		fprintf(stderr, "Argument list missing!!\n");
		usage(argv);
		return ret;
	}

	num_args = argc - optind;
	if (num_args > ATH11K_WMI_UNIT_TEST_MAX_NUM_ARGS) {
		fprintf(stderr, "Arguments exceeded max limit, limit:%d\n",
			ATH11K_WMI_UNIT_TEST_MAX_NUM_ARGS);
		return ret;
	}


	netdev_idx = if_nametoindex(ifname);
	if (!netdev_idx) {
		fprintf(stderr, "%s not found\n", ifname);
		return -ENOENT;
	}

	ret = init_nl_sock(&sock);
	if (ret < 0) {
		fprintf(stderr, "Failed to initialize nl socket\n");
		return ret;
	}

	msg = nlmsg_alloc();
	if (!msg) {
		fprintf(stderr, "Failed to allocate nl message\n");
		ret = -ENOMEM;
		goto free_nl_sock;
	}

	cb = nl_cb_alloc(NL_CB_DEFAULT);
	if (!cb) {
		fprintf(stderr, "failed to allocate netlink callback\n");
		ret = -ENOMEM;
		goto free_nl_msg;
	}

	switch (type) {
	case ATH11K_WMI_FW_UNIT_TEST_CMD:
		cmd = ath11k_fwtest_fill_cmd_buf(module_id, vdev_id, num_args, argv, optind, &buf_size);
		cmd_id = WMI_UNIT_TEST_CMDID;
		break;
	case ATH11K_WMI_VDEV_SET_PARAM_CMD:
		cmd = ath11k_wmi_vdev_pdev_set_cmd_fill_buf(type, vdev_id, argv, optind, &buf_size);
		cmd_id = WMI_VDEV_SET_PARAM_CMDID;
		break;
	case ATH11K_WMI_PDEV_SET_PARAM_CMD:
		if (pdev_id == 0 || pdev_id == UINT_MAX)
			break;
		cmd = ath11k_wmi_vdev_pdev_set_cmd_fill_buf(type, pdev_id, argv, optind, &buf_size);
		cmd_id = WMI_PDEV_SET_PARAM_CMDID;
		break;
	default:
		fprintf(stderr, "Unknown fw unittest type\n");
		ret = -EINVAL;
		goto free_nl_msg;
	}

	if (!cmd) {
		fprintf(stderr, "failed to fill cmd buffer, please check inputs\n");
		ret = -EINVAL;
		goto free_nl_msg;
	}

	genlmsg_put(msg, 0, 0, sock.genl_family_id, 0, 0,
		    NL80211_CMD_TESTMODE, 0);

	NLA_PUT_U32(msg, NL80211_ATTR_IFINDEX, netdev_idx);

	nest = nla_nest_start(msg, NL80211_ATTR_TESTDATA);
	if (!nest) {
		fprintf(stderr, "Failed to nest test input\n");
		ret = -ENOMEM;
		goto free_cmd_buf;
	}

	NLA_PUT_U32(msg, ATH11K_TM_ATTR_CMD, ATH11K_TM_CMD_WMI_FW_TEST);
	NLA_PUT_U32(msg, ATH11K_TM_ATTR_WMI_CMDID, cmd_id);
	NLA_PUT(msg, ATH11K_TM_ATTR_DATA, buf_size, cmd);

	nla_nest_end(msg, nest);

	ret = nl_send_auto_complete(sock.nl_sock, msg);
	if (ret < 0) {
		fprintf(stderr, "Failed to send nl msg: %d\n", ret);
		goto free_cmd_buf;
	}

	ret = 1;

	nl_cb_err(cb, NL_CB_CUSTOM, nl_cb_error_handler, &ret);
	nl_cb_set(cb, NL_CB_FINISH, NL_CB_CUSTOM, nl_cb_finish_handler, &ret);
	nl_cb_set(cb, NL_CB_ACK, NL_CB_CUSTOM, nl_cb_ack_handler, &ret);

	while (ret > 0)
		nl_recvmsgs(sock.nl_sock, cb);
	if (ret < 0)
		fprintf(stderr, "command failed: %s (%d)\n", strerror(-ret), ret);

	goto free_cmd_buf;

nla_put_failure:
	fprintf(stderr, "nl put operation failed!!\n");
free_cmd_buf:
	free(cmd);
	nl_cb_put(cb);
free_nl_msg:
	nlmsg_free(msg);
free_nl_sock:
	nl_socket_free(sock.nl_sock);
	return ret;
}
