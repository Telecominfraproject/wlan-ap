#include <stdbool.h>
#include <stdlib.h>

#include "osn_types.h"
#include "const.h"
#include <libubox/avl.h>

#define OSYNC_MAX_CACHED_DHCP_EVENTS 500
extern uint32_t cached_dhcp_events_nr;

/*
 * ===========================================================================
 *  DHCP event definitions
 * ===========================================================================
 */

enum inet_dhcp_event_ubus_type {
	INET_UBUS_DHCP_ACK,
	INET_UBUS_DHCP_NAK,
	INET_UBUS_DHCP_OFFER,
	INET_UBUS_DHCP_INFORM,
	INET_UBUS_DHCP_DECLINE,
	INET_UBUS_DHCP_REQUEST,
	INET_UBUS_DHCP_DISCOVER,
	INET_UBUS_DHCP_RELEASE,
};

struct inet_dhcp_event_data {
	int type;
	uint32_t x_id;
	uint32_t vlan_id;
	osn_ip_addr_t dhcp_server_ip;
	osn_ip_addr_t client_ip;
	osn_ip_addr_t relay_ip;
	osn_mac_addr_t device_mac_address;
	osn_ip_addr_t subnet_mask;
	osn_ip_addr_t primary_dns;
	osn_ip_addr_t secondary_dns;
	uint32_t lease_time;
	uint32_t renewal_time;
	uint32_t rebinding_time;
	uint32_t time_offset;
	osn_ip_addr_t gateway_ip;
	char source_mac_address[C_MACADDR_LEN];
	bool from_internal;
	char hostname[C_HOSTNAME_LEN];
	uint64_t timestamp_ms;
};

struct dhcp_common_data {
	uint32_t x_id;
	uint32_t vlan_id;
	char dhcp_server_ip[C_IP4ADDR_LEN];
	char client_ip[C_IP4ADDR_LEN];
	char relay_ip[C_IP4ADDR_LEN];
	char device_mac_address[C_MACADDR_LEN];
	uint64_t timestamp_ms;
};

struct dhcp_ack_event {
	struct dhcp_common_data dhcp_common;
	char subnet_mask[C_IP4ADDR_LEN];
	char primary_dns[C_IP4ADDR_LEN];
	char secondary_dns[C_IP4ADDR_LEN];
	uint32_t lease_time;
	uint32_t renewal_time;
	uint32_t rebinding_time;
	uint32_t time_offset;
	char gateway_ip[C_IP4ADDR_LEN];
};

struct dhcp_nak_event {
	struct dhcp_common_data dhcp_common;
	bool from_internal;
};

struct dhcp_offer_event {
	struct dhcp_common_data dhcp_common;
	bool from_internal;
};

struct dhcp_inform_event {
	struct dhcp_common_data dhcp_common;
};

struct dhcp_decline_event {
	struct dhcp_common_data dhcp_common;
};

struct dhcp_request_event {
	struct dhcp_common_data dhcp_common;
	char hostname[C_HOSTNAME_LEN];
};

struct dhcp_discover_event {
	struct dhcp_common_data dhcp_common;
	char hostname[C_HOSTNAME_LEN];
};

struct dhcp_transaction {
	int type;
	union {
		struct dhcp_ack_event dhcp_ack;
		struct dhcp_nak_event dhcp_nak;
		struct dhcp_offer_event dhcp_offer;
		struct dhcp_inform_event dhcp_inform;
		struct dhcp_decline_event dhcp_decline;
		struct dhcp_request_event dhcp_request;
		struct dhcp_discover_event dhcp_discover;
	} u;
};

struct dhcp_event_avl_rec {
	uint32_t x_id;
	struct dhcp_transaction *dhcp_records;
	size_t dhcp_records_nr;
	struct avl_node avl;
};

int inet_dhcp_event_ubus_handle(struct inet_dhcp_event_data *data);
