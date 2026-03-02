/*
* Copyright (c) 2011-2012 Qualcomm Atheros Inc. All Rights Reserved.
* Qualcomm Atheros Proprietary and Confidential.
*/

/* private, os-specific things go here */
int tcmd_set_timer(struct tcmd_cfg *cfg);
/* reset timer and return 0 if still running, return -ETIMEDOUT if the tcmd
 * timer timed out */
int tcmd_reset_timer(struct tcmd_cfg *cfg);
