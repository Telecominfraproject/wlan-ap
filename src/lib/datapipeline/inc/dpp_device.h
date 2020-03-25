/*
Copyright (c) 2015, Plume Design Inc. All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
   1. Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
   2. Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
   3. Neither the name of the Plume Design Inc. nor the
      names of its contributors may be used to endorse or promote products
      derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL Plume Design Inc. BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#ifndef __HAVE_DPP_DEVICE_H__
#define __HAVE_DPP_DEVICE_H__

#include "ds.h"
#include "ds_dlist.h"

#include "dpp_types.h"

#define DPP_DEVICE_TX_CHAINMASK_MAX      3
#define DPP_DEVICE_TOP_MAX               10


typedef enum
{
    DPP_DEVICE_LOAD_AVG_ONE = 0,
    DPP_DEVICE_LOAD_AVG_FIVE,
    DPP_DEVICE_LOAD_AVG_FIFTEEN,
    DPP_DEVICE_LOAD_AVG_QTY
} dpp_device_load_avg_t;


/* Memory utilization: [kB]  */
typedef struct
{
    uint32_t mem_total;
    uint32_t mem_used;
    uint32_t swap_total;
    uint32_t swap_used;
} dpp_device_memutil_t;


typedef enum
{
    DPP_DEVICE_FS_TYPE_ROOTFS = 0,
    DPP_DEVICE_FS_TYPE_TMPFS = 1,
    DPP_DEVICE_FS_TYPE_QTY
} dpp_device_fs_type_t;


/* Filesystem utilization per FS-type: [kB]  */
typedef struct
{
    dpp_device_fs_type_t  fs_type;
    uint32_t              fs_total;
    uint32_t              fs_used;
} dpp_device_fsutil_t;


/* CPU utilization: [percent]:  */
typedef struct
{
    uint32_t cpu_util;
} dpp_device_cpuutil_t;


/* Per-process CPU/MEM utilization:  */
typedef struct
{
    uint32_t  pid;
    char      cmd[64];
    uint32_t  util;    /* for cpu: [%CPU] [0..100]; for mem: [kB]  */
} dpp_device_ps_util_t;



typedef struct
{
    double                          load[DPP_DEVICE_LOAD_AVG_QTY];
    uint32_t                        uptime;

    dpp_device_memutil_t            mem_util;
    dpp_device_cpuutil_t            cpu_util;
    dpp_device_fsutil_t             fs_util[DPP_DEVICE_FS_TYPE_QTY];


    dpp_device_ps_util_t            top_cpu[DPP_DEVICE_TOP_MAX];
    uint32_t                        n_top_cpu;
    dpp_device_ps_util_t            top_mem[DPP_DEVICE_TOP_MAX];
    uint32_t                        n_top_mem;
} dpp_device_record_t;

typedef struct
{
    radio_type_t                    type;
    int32_t                         value;
    ds_dlist_node_t                 node;
} dpp_device_temp_t;

typedef struct 
{
    radio_type_t                    type;
    uint32_t                        value;
} dpp_device_txchainmask_t;

typedef struct
{
    dpp_device_txchainmask_t        radio_txchainmasks[DPP_DEVICE_TX_CHAINMASK_MAX];
    uint32_t                        txchainmask_qty;
    int32_t                         fan_rpm;
    uint64_t                        timestamp_ms;
    ds_dlist_node_t                 node;
} dpp_device_thermal_record_t;

static inline dpp_device_temp_t * dpp_device_temp_record_alloc()
{
    dpp_device_temp_t *record = NULL;

    record = malloc(sizeof(dpp_device_temp_t));
    if (record)
    {
        memset(record, 0, sizeof(dpp_device_temp_t));
    }

    return record;
}

static inline void dpp_device_temp_record_free(dpp_device_temp_t *record)
{
    if (NULL != record)
    {
        free(record);
    }
}

static inline dpp_device_thermal_record_t * dpp_device_thermal_record_alloc()
{
    dpp_device_thermal_record_t *record = NULL;

    record = malloc(sizeof(dpp_device_thermal_record_t));
    if (record)
    {
        memset(record, 0, sizeof(dpp_device_thermal_record_t));
    }

    return record;
}

static inline void dpp_device_thermal_record_free(dpp_device_thermal_record_t *record)
{
    if (NULL != record)
    {
        free(record);
    }
}

typedef struct
{
    ds_dlist_t                      temp;
    dpp_device_record_t             record;
    ds_dlist_t                      thermal_records;
    uint64_t                        timestamp_ms;
} dpp_device_report_data_t;

#endif /* __HAVE_DPP_DEVICE_H__ */
