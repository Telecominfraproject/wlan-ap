/* SPDX-License-Identifier: BSD-3-Clause */

#define WRITE_TO_FR                 0x40000000

#define DEBUG_LEVEL_1               0x00000001
#define DEBUG_LEVEL_2               0x00000002
#define DEBUG_LEVEL_3               0x00000004
#define DEBUG_LEVEL_4               0x00000008
#define APP_LOG_CAMI                0x00000010
#define APP_LOG_REDIR               0x00000020
#define APP_LOG_DMAN                0x00000040
#define APP_LOG_SYSLOG              0x00000080
#define APP_LOG_WCFCTL              0x00000100
#define APP_LOG_CORE                0x00000200
#define APP_LOG_WEBSOCK             0x00000400
#define APP_LOG_DHCP_DISC           0x00000800
#define APP_LOG_STATUS_AGENT_DISC   0x00001000
#define APP_LOG_APC                 0x00002000
#define APP_LOG_RADIUS_PROXY        0x00004000
#define APP_LOG_NANNY               0x00008000
#define APP_LOG_CP_UAP              0x00010000
#define APP_LOG_RADIUS_PROBE        0x00020000
#define APP_LOG_RTLS                0x00040000
#define APP_LOG_HOSTAP              0x00080000
#define APP_LOG_BESTAP              0x00100000

extern int wc_put_logline( unsigned int Mask, const char * format, ... );

