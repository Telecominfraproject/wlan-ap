#ifndef DPP_NETWORK_PROBE_H_INCLUDED
#define DPP_NETWORK_PROBE_H_INCLUDED

#include "ds.h"
#include "ds_dlist.h"

#include "dpp_types.h"

#define MAX_IP_ADDR_SIZE 16
#define MAX_IF_NAME_SIZE 16

typedef enum
{
    SUD_down = 0,
    SUD_up = 1,
    SUD_error = 2
}StateUpDown_t;

/* dns probe metrics */
typedef struct
{
    char  serverIP[MAX_IP_ADDR_SIZE];
    StateUpDown_t state;
    uint32_t latency;
} dpp_dns_metrics_t;

/* VLAN probe metrics */
typedef struct
{
    char	              vlanIF[MAX_IF_NAME_SIZE];
    StateUpDown_t         dhcpState;
    uint32_t              dhcpLatency;
    StateUpDown_t         dnsState;
    uint32_t              dnsLatency;
    StateUpDown_t         obsV200_radiusState;
    uint32_t              obsV200_radiusLatency;
    dpp_dns_metrics_t     dnsProbeResults;
    uint32_t              dur_vlanIF;
    uint32_t              dur_dhcpState;
    uint32_t              dur_dhcpLatency;
    uint32_t              dur_dnsState;
    uint32_t              dur_dnsLatency;
    uint32_t              dur_dnsReport;
} dpp_vlan_metrics_t;


/* Radius probe metrics  */
typedef struct
{
    uint8_t serverIP[4];
    uint32_t noAnswer;
    uint32_t latencyMin;
    uint32_t latencyMax;
    uint32_t latencyAve;

    // -- duration
    uint32_t dur_serverIP;   //category Network
    uint32_t dur_noAnswer;   //category Network
    uint32_t dur_latencyMin;   //category Network
    uint32_t dur_latencyMax;   //category Network
    uint32_t dur_latencyAve;   //category Network
} dpp_radius_metrics_t;

typedef struct
{
    dpp_dns_metrics_t                dns_probe;
    dpp_vlan_metrics_t               vlan_probe;
    dpp_radius_metrics_t             radius_probe;
} dpp_network_probe_record_t;


typedef struct
{
    dpp_network_probe_record_t             record;
    uint64_t                               timestamp_ms;
} dpp_network_probe_report_data_t;

#endif /* DPP_NETWORK_PROBE_H_INCLUDED */
