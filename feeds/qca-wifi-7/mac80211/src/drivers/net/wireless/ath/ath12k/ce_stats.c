// SPDX-License-Identifier: BSD-3-Clause-Clear
/* Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries. */

#include "dp_rx.h"
#include "debug.h"
#include "hif.h"
#include "hal.h"

void ath12k_record_sched_entry_ts(struct ath12k_base *ab, int ce_id)
{
	struct ath12k_ce_pipe *pipe = &ab->ce.ce_pipe[ce_id];
	struct ath12k_ce_stats *stats = pipe->ce_stats;

	if (stats)
		stats->sched_entry_ts = ktime_to_us(ktime_get());
}

void ath12k_record_exec_entry_ts(struct ath12k_base *ab, int ce_id)
{
	struct ath12k_ce_pipe *pipe = &ab->ce.ce_pipe[ce_id];
	struct ath12k_ce_stats *stats = pipe->ce_stats;

	if (stats)
		stats->exec_entry_ts = ktime_to_us(ktime_get());
}

static  enum ce_buckets ath12k_get_ce_stats_bucket_index(u64 time_us)
{
	u64 time_ms;
	enum ce_buckets index = CE_BUCKET_MAX;

	time_ms = do_div(time_us, 1000);

	if (time_ms > 10)
		index = CE_BUCKET_BEYOND;
	else if (time_ms > 5)
		index = CE_BUCKET_10_MS;
	else if (time_ms > 2)
		index = CE_BUCKET_5_MS;
	else if (time_ms > 1)
		index = CE_BUCKET_2_MS;
	else if (time_us > 500)
		index = CE_BUCKET_1_MS;
	else
		index = CE_BUCKET_500_US;

	return index;
}

void ath12k_update_ce_stats_bucket(struct ath12k_base *ab, int ce_id)
{
	struct ath12k_ce_pipe *pipe = &ab->ce.ce_pipe[ce_id];
	struct ath12k_ce_stats *stats = pipe->ce_stats;
	u32 index;
	u64 exec_time;
	u64 sched_time;
	u64 curr_time = ktime_to_us(ktime_get());
	enum ce_buckets bucket_index;

	if (!stats)
		return;

	exec_time = (curr_time - stats->exec_entry_ts);
	sched_time = (stats->exec_entry_ts - stats->sched_entry_ts);

	index = stats->record_index;
	index = (index + 1) % MAX_CE_STATS_RECORDS;

	stats->sched_time_record[index] = sched_time;
	stats->exec_time_record[index] = exec_time;
	stats->record_index = index;

	bucket_index =  ath12k_get_ce_stats_bucket_index(exec_time);
	stats->exec_bucket[bucket_index]++;
	stats->exec_last_update[bucket_index] = curr_time;

	bucket_index =  ath12k_get_ce_stats_bucket_index(sched_time);
	stats->sched_bucket[bucket_index]++;
	stats->sched_last_update[bucket_index] = curr_time;
}
