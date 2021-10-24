#ifndef __BPF_QOSIFY_H
#define __BPF_QOSIFY_H

#ifndef QOSIFY_FLOW_BUCKET_SHIFT
#define QOSIFY_FLOW_BUCKET_SHIFT	13
#endif

#define QOSIFY_FLOW_BUCKETS		(1 << QOSIFY_FLOW_BUCKET_SHIFT)

/* rodata per-instance flags */
#define QOSIFY_INGRESS			(1 << 0)
#define QOSIFY_IP_ONLY			(1 << 1)

/* global config data */
struct qosify_config {
	uint8_t dscp_prio;
	uint8_t dscp_bulk;
	uint8_t dscp_icmp;

	uint8_t bulk_trigger_timeout;
	uint16_t bulk_trigger_pps;

	uint16_t prio_max_avg_pkt_len;
};

#endif
