/* SPDX-License-Identifier BSD-3-Clause */

#define _GNU_SOURCE
#include <stdarg.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <netinet/in.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <jansson.h>
#include <ev.h>
#include <evsched.h>
#include <syslog.h>
#include <getopt.h>
#include <interap/interAPcomm.h>
#include <sys/ioctl.h>
#include <arpa/inet.h>

#include "ds_tree.h"
#include "log.h"
#include "os.h"
#include "os_socket.h"
#include "ovsdb.h"
#include "evext.h"
#include "os_backtrace.h"
#include "json_util.h"
#include "target.h"
#include "nl_ucc.h"
#include "ucc.h"
#include "dppline.h"

#define MODULE_ID LOG_MODULE_ID_MAIN
#define IAC_VOIP_PORT 50000

static ev_io nl_io;
static ev_io iac_io;
struct nl_sock* nlsock = NULL;
static log_severity_t cmdm_log_severity = LOG_SEVERITY_INFO;

static void prep_nl_sock(struct nl_sock** nlsock, unsigned int mcgroups)
{
	int family_id, grp_id;
	unsigned int bit = 0;

	*nlsock = nl_socket_alloc();
	if(!*nlsock) {
		fprintf(stderr, "Unable to alloc nl socket!\n");
		exit(EXIT_FAILURE);
	}

	/* disable seq checks on multicast sockets */
	nl_socket_disable_seq_check(*nlsock);
	nl_socket_disable_auto_ack(*nlsock);

	/* connect to genl */
	if (genl_connect(*nlsock)) {
		fprintf(stderr, "Unable to connect to genl!\n");
		goto exit_err;
	}

	/* resolve the generic nl family id*/
	family_id = genl_ctrl_resolve(*nlsock, GENL_UCC_FAMILY_NAME);
	if(family_id < 0){
		fprintf(stderr, "Unable to resolve family name!\n");
		goto exit_err;
	}

	if (!mcgroups)
		return;

	while (bit < sizeof(unsigned int)) {
		if (!(mcgroups & (1 << bit)))
			goto next;

		grp_id = genl_ctrl_resolve_grp(*nlsock, GENL_UCC_FAMILY_NAME,
				genl_ucc_mcgrp_names[bit]);

		if (grp_id < 0)	{
			fprintf(stderr, "Unable to resolve group name for %u!\n",
				(1 << bit));
            goto exit_err;
		}
		if (nl_socket_add_membership(*nlsock, grp_id)) {
			fprintf(stderr, "Unable to join group %u!\n", 
				(1 << bit));
            goto exit_err;
		}
next:
		bit++;
	}

    return;

exit_err:
    nl_socket_free(*nlsock); // this call closes the socket as well
    exit(EXIT_FAILURE);
}

/* Get current ip broadcast address */
int get_current_ip(char *ip, char *iface) {

	int fd;
	struct ifreq ifr;

	if ((fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		LOGI("Error: socket failed");
		return -1;
	}

	ifr.ifr_addr.sa_family = AF_INET;
	strncpy(ifr.ifr_name, iface, IFNAMSIZ-1);

	if ((ioctl(fd, SIOCGIFBRDADDR, &ifr)) < 0) {
		LOGI("Error: ioctl failed");
		return -1;
	}

	memcpy(ip, inet_ntoa(((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr),
	       16);

	close(fd);

	return 0;
}

static int rx_msg(struct nl_msg *msg, void* arg)
{
	struct nlattr *attr[GENL_UCC_ATTR_MAX+1];

	struct voip_session *data;
	genlmsg_parse(nlmsg_hdr(msg), 0, attr, 
			GENL_UCC_ATTR_MAX, genl_ucc_policy);

	data =	(struct voip_session *)nla_data(attr[GENL_UCC_ATTR_MSG]);

	if (!attr[GENL_UCC_ATTR_MSG]) {
		fprintf(stdout, "Kernel sent empty message!!\n");
		return NL_OK;
	}

	char *dst_ip = malloc(16);
	memset(dst_ip, 0, 16);
	if((get_current_ip(dst_ip, IAC_IFACE)) < 0) {
		LOGI("Error: Cannot get IP for %s", IAC_IFACE);
		return NL_OK;
	}
	interap_send(IAC_VOIP_PORT, dst_ip, data, sizeof(struct voip_session));

	return NL_OK;
}



static int skip_seq_check(struct nl_msg *msg, void *arg)
{
	return NL_OK;
}

static void nl_event(struct ev_loop *ev, struct ev_io *io, int event)
{
    struct nl_cb *cb = NULL;
    /* prep the cb */
    cb = nl_cb_alloc(NL_CB_DEFAULT);
    nl_cb_set(cb, NL_CB_SEQ_CHECK, NL_CB_CUSTOM, skip_seq_check, NULL);
    nl_cb_set(cb, NL_CB_VALID, NL_CB_CUSTOM, rx_msg, NULL);
    nl_recvmsgs(nlsock, cb);

    nl_cb_put(cb);

}

void netlink_listen(struct ev_loop *loop) {
	unsigned int mcgroups = 0;

	LOGI("Netlink listen initialize");
	mcgroups |= 1 << (GENL_UCC_MCGRP0);
	mcgroups |= 1 << (GENL_UCC_MCGRP1);
	prep_nl_sock(&nlsock, mcgroups);

	ev_io_init(&nl_io, nl_event, nlsock->s_fd, EV_READ);
	ev_io_start(loop, &nl_io);

	LOGI("Netlink listening");
}

static int send_msg_to_kernel(struct nl_sock *sock, void *data,  unsigned int len)
{
	struct nl_msg* msg;
	int family_id, err = 0;

	family_id = genl_ctrl_resolve(sock, GENL_UCC_FAMILY_NAME);
	if(family_id < 0){
		LOGI("Unable to resolve family name!\n");
		exit(EXIT_FAILURE);
	}

	msg = nlmsg_alloc();
	if (!msg) {
		LOGI("failed to allocate netlink message\n");
		exit(EXIT_FAILURE);
	}

	if(!genlmsg_put(msg, NL_AUTO_PID, NL_AUTO_SEQ, family_id, 0, 
		NLM_F_REQUEST, GENL_UCC_INTAP_MSG, 0)) {
		LOGI("failed to put nl hdr!\n");
		err = -ENOMEM;
		goto out;
	}

	err = nla_put(msg, GENL_UCC_ATTR_INTAP_MSG, len , data);
	if (err) {
		LOGI("failed to put nl data!\n");
		goto out;
	}

	err = nl_send_auto_complete(sock, msg);
	if (err < 0) {
		LOGI("failed to send nl message!\n");
	}
out:
	nlmsg_free(msg);
	return err;
}

int recv_process(void *data, ssize_t len) {
	struct nl_sock *nlsock;
	unsigned int mcgroups = 0;

	prep_nl_sock(&nlsock, mcgroups);
	send_msg_to_kernel(nlsock, data, sizeof(struct voip_session));

	return 0;
}

int main(int argc, char ** argv)
{
	struct ev_loop *loop = EV_DEFAULT;

	if (os_get_opt(argc, argv, &cmdm_log_severity))
		return -1;

	target_log_open("UCCM", 0);
	LOGI("Starting UCC manager - UCCM");
	log_severity_set(cmdm_log_severity);
	log_register_dynamic_severity(loop);

	backtrace_init();

	json_memdbg_init(loop);
#if 0
	if (!dpp_init())
	{
        	LOG(ERR,
            	"Initializing SM "
            	"(Failed to init DPP library)");
		return -1;
	}

	if (!uccm_mqtt_init())
	{
		LOG(ERR,
		"Initializing SM "
		"(Failed to start MQTT)");
		return -1;
	}
#endif
	if (!ovsdb_init_loop(loop, "UCCM")) {
		LOGEM("Initializing UCCM (Failed to initialize OVSDB)");
		return -1;
	}
	evsched_init(loop);

	callback cb = recv_process;
	LOGI("Call interap_recv");
	if( interap_recv(IAC_VOIP_PORT, cb, sizeof(struct voip_session),
			 loop, &iac_io) < 0)
		LOGI("Error: Failed InterAP receive");

//	task_init();
	netlink_listen(loop);
//	command_ubus_init(loop);

	ev_run(loop, 0);

	if (!ovsdb_stop_loop(loop))
		LOGE("Stopping UCCM (Failed to stop OVSDB");
#if 0
	uccm_mqtt_stop();
#endif
	ev_default_destroy();

	LOGN("Exiting UCCM");

	return 0;
}

