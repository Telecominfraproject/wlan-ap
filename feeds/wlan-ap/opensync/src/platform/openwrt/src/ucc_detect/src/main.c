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

static unsigned int mcgroups;		/* Mask of groups */

static log_severity_t cmdm_log_severity = LOG_SEVERITY_INFO;

static void prep_nl_sock(struct nl_sock** nlsock)
{
	int family_id, grp_id;
	unsigned int bit = 0;
	mcgroups |= 1 << (0); //group 0 Kir-change

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

static int print_rx_msg(struct nl_msg *msg, void* arg)
{
	struct nlattr *attr[GENL_UCC_ATTR_MAX+1];

	struct wc_capture_buf *rbuf;

	genlmsg_parse(nlmsg_hdr(msg), 0, attr, 
			GENL_UCC_ATTR_MAX, genl_ucc_policy);

	rbuf =	(struct wc_capture_buf *)nla_data(attr[GENL_UCC_ATTR_MSG]);

	LOGI("print_rx_msg: rbuf=%p", rbuf);
	if (!attr[GENL_UCC_ATTR_MSG]) {
		fprintf(stdout, "Kernel sent empty message!!\n");
		return NL_OK;
	}
/*
	fprintf(stdout, "Kernel says: %llu %llu %llu %d %d %d %d %d %d %d %d %d [%x:%x:%x:%x:%x:%x]\n", 
rbuf->TimeStamp,
rbuf->tsInUs,
rbuf->SessionId,
rbuf->Type,
rbuf->From,
rbuf->Len,
rbuf->Channel,
rbuf->Direction,
rbuf->Rssi,
rbuf->DataRate,
rbuf->Count,
rbuf->wifiIf,
rbuf->staMac[0],
rbuf->staMac[1],
rbuf->staMac[2],
rbuf->staMac[3],
rbuf->staMac[4],
rbuf->staMac[5]);
*/ 

/*
	fprintf(stdout, "Kernel says: %s \n", 
		nla_get_string(attr[GENL_UCC_ATTR_MSG]));

*/

	return NL_OK;
}



static int skip_seq_check(struct nl_msg *msg, void *arg)
{
	return NL_OK;
}



void netlink_listen(void) {
	struct nl_sock* nlsock = NULL;
	struct nl_cb *cb = NULL;
	int ret;

	LOGI("Netlink listen initialize");
	prep_nl_sock(&nlsock);

	/* prep the cb */
	cb = nl_cb_alloc(NL_CB_DEFAULT);
	nl_cb_set(cb, NL_CB_SEQ_CHECK, NL_CB_CUSTOM, skip_seq_check, NULL);
	nl_cb_set(cb, NL_CB_VALID, NL_CB_CUSTOM, print_rx_msg, NULL);
	LOGI("Netlink listening");
	do {
		ret = nl_recvmsgs(nlsock, cb);
	} while (!ret);
	
	nl_cb_put(cb);
    nl_socket_free(nlsock);

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

//	task_init();
	netlink_listen();
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

