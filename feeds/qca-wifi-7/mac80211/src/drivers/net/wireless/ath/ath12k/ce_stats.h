/* SPDX-License-Identifier: BSD-3-Clause-Clear */
/* Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries. */

#ifndef ATH12K_CE_STATS_H
#define ATH12K_CE_STATS_H

struct ath12k_base;
#define MAX_CE_STATS_RECORDS 20
/**
 * enum ce_buckets - CE tasklet time buckets
 * @CE_BUCKET_500_US: tasklet bucket to store 0-0.5ms
 * @CE_BUCKET_1_MS: tasklet bucket to store 0.5-1ms
 * @CE_BUCKET_2_MS: tasklet bucket to store 1-2ms
 * @CE_BUCKET_5_MS: tasklet bucket to store 2-5ms
 * @CE_BUCKET_10_MS: tasklet bucket to store 5-10ms
 * @CE_BUCKET_BEYOND: tasklet bucket to store > 10ms
 * @CE_BUCKET_MAX: enum max value
 */
enum ce_buckets {
	CE_BUCKET_500_US,
	CE_BUCKET_1_MS,
	CE_BUCKET_2_MS,
	CE_BUCKET_5_MS,
	CE_BUCKET_10_MS,
	CE_BUCKET_BEYOND,
	CE_BUCKET_MAX,
};

struct ath12k_ce_stats {
	unsigned int record_index;
	/* Timestamp when tasklet/work queue is scheduled from IRQ */
	u64 sched_entry_ts;
	/* Timestamp when tasklet/work queue is started execution */
	u64 exec_entry_ts;
	/* Last N number of tasklets/work queue scheduled time */
	u64 sched_time_record[MAX_CE_STATS_RECORDS];
	/* Last N number of tasklets//work queue execution time */
	u64 exec_time_record[MAX_CE_STATS_RECORDS];
	u64 exec_bucket[CE_BUCKET_MAX];
	u64 exec_time, exec_ms;
	u64 sched_time, sched_ms;
	u64 sched_bucket[CE_BUCKET_MAX];
	u64 exec_last_update[CE_BUCKET_MAX];
	u64 sched_last_update[CE_BUCKET_MAX];
};

void ath12k_record_exec_entry_ts(struct ath12k_base *ab, int ce_id);
void ath12k_record_sched_entry_ts(struct ath12k_base *ab, int ce_id);
void ath12k_update_ce_stats_bucket(struct ath12k_base *ab, int ce_id);
#endif
