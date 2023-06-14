// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: 2023 Thibaut Varène <hacks@slashdirt.org>

/*
 Basic RFC5176-compliant Dynamic Authorization Server daemon.

 This daemon expects to find the RADIUS shared secret as option "das_secret" in the relevant uspot config section.
 The listening UDP port is 3799 if not overriden via "das_port" configuration option.
 Implementation is not thread-safe.

 This implementation supports both Disconnect-Request and CoA-Request messages.
 This implementation supports a limited subset of valid RADIUS attributes.
 In particular it does not support: Service-Type or Message-Authenticator.
 In the table below, the following identification attributes are not supported:
 Acct-Multi-Session-Id, NAS-IPv6-Address, NAS-Port, Vendor-Specific, Acct-Multi-Session-Id,
 NAS-Port-Id, Framed-Interface-Id, Framed-IPv6-Prefix.
 It also does not send Error-Cause.
 It will however preserve Proxy-State, State and Class attributes in the response.
 Per RFC, non-supported attributes found in incoming messages will result in a NAK response.

 Per RFC5176:

 NAS identification attributes
 Attribute              #   Reference  Description
 ---------             ---  ---------  -----------
 NAS-IP-Address         4   [RFC2865]  The IPv4 address of the NAS.
 NAS-Identifier        32   [RFC2865]  String identifying the NAS.
 NAS-IPv6-Address      95   [RFC3162]  The IPv6 address of the NAS.

 Session identification attributes
 Attribute              #   Reference  Description
 ---------             ---  ---------  -----------
 User-Name              1   [RFC2865]  The name of the user associated with one or more sessions.
 NAS-Port               5   [RFC2865]  The port on which a session is terminated.
 Framed-IP-Address      8   [RFC2865]  The IPv4 address associated with a session.
 Vendor-Specific       26   [RFC2865]  One or more vendor-specific identification attributes.
 Called-Station-Id     30   [RFC2865]  The link address to which a session is connected.
 Calling-Station-Id    31   [RFC2865]  The link address from which one or more sessions are connected.
 Acct-Session-Id       44   [RFC2866]  The identifier uniquely identifying a session on the NAS.
 Acct-Multi-Session-Id 50   [RFC2866]  The identifier uniquely identifying related sessions.
 NAS-Port-Id           87   [RFC2869]  String identifying the port where a session is.
 Chargeable-User-      89   [RFC4372]  The CUI associated with one
 Identity                              or more sessions.
 Framed-Interface-Id   96   [RFC3162]  The IPv6 Interface Identifier associated with a session,
				       always sent with Framed-IPv6-Prefix.
 Framed-IPv6-Prefix    97   [RFC3162]  The IPv6 prefix associated with a session, always sent
				       with Framed-Interface-Id.

 */

#define _GNU_SOURCE

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include <radcli/radcli.h>	// for RADIUS attribute ids - no linkage

#include <libubox/uloop.h>
#include <libubox/usock.h>
#include <libubox/ulog.h>
#include <libubox/md5.h>

#include <libubus.h>
#include <uci.h>

#define RAD_PROX_BUFLEN		(4 * 1024)

#define freeconst(p)		free((void *)(uintptr_t)(p))

// RFC5176 DAE for RADIUS codes
enum dae_codes {
	DISCONNECT_REQUEST = 40,
	DISCONNECT_ACK,
	DISCONNECT_NAK,
	COA_REQUEST,
	COA_ACK,
	COA_NAK,
};

/*
 The packet format consists of the following fields: Code, Identifier,
 Length, Authenticator, and Attributes in Type-Length-Value (TLV)
 format. The Code field is one octet, The Identifier field is one octet,
 The Length field is two octets, The Authenticator field is sixteen (16) octets.
 All fields hold the same meaning as those described in RADIUS [RFC2865].
 */
struct radius_header {
	uint8_t code;
	uint8_t id;
	uint16_t len;
	uint8_t auth[16];
} __attribute__((packed));

/*
 A summary of the Attribute format is shown below.  The fields are
 transmitted from left to right. The Type field is one octet,
 The Length field is one octet, The Value field is zero or more octets.
  0                   1                   2
  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0
 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-
 |     Type      |    Length     |  Value ...
 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-
 */
struct radius_tlv {
	uint8_t id;
	uint8_t len;
	uint8_t data[];
} __attribute__((packed));

#define TLVP_DATA_LEN(tlvp)	(((tlvp)->len) - 2)	// TLV data len is len - 2*sizeof(uint8_t)

static struct {
	struct ubus_context *ctx;
	const char *uspot;
	const char *secret;
	const char *port;
	struct uloop_fd ufd;
} das;

// uint32 from AVP
static const void * toval_u32(const struct radius_tlv *tlv)
{
	static uint32_t buf;

	if (6 != tlv->len)
		return NULL;

	buf = ntohl(*((uint32_t *)tlv->data));
	return &buf;
}

// human-readable IPv4 string from AVP
static const void * toval_ip(const struct radius_tlv *tlv)
{
	struct in_addr addr;

	if (6 != tlv->len)
		return NULL;

	addr.s_addr = *((uint32_t *)tlv->data);
	return inet_ntoa(addr);
}

// string (with only printable characters) from AVP
static const void * toval_str(const struct radius_tlv *tlv)
{
	static char buf[256];		// fits maximum tlv->len-2 + '\0'
	const uint8_t *data = tlv->data;
	const uint8_t len = TLVP_DATA_LEN(tlv);

	memset(buf, 0, sizeof(buf));

	for (int i = 0; i < len; i++) {
		if (!isprint(data[i]))
			return NULL;	// we don't expect to deal with non-printable chars

		buf[i] = (char)data[i];
	}

	return buf;
}

// AVP matching table for uspot callback
static const struct {
	const char *name;
	const int type;
	/**
	 * Callback for data processing.
	 * Takes a pointer to current valid radius_tlv. Returns a static pointer to data to pass to blobmsg_add_*().
	 */
	const void * (*const toval)(const struct radius_tlv *);
} radius_attrids[256] = {
	[PW_USER_NAME] = { .name = "username", .type = BLOBMSG_TYPE_STRING, .toval = toval_str, },
	[PW_NAS_IP_ADDRESS] = { .name = "nas_ip", .type = BLOBMSG_TYPE_STRING, .toval = toval_ip, },
	[PW_FRAMED_IP_ADDRESS] = { .name = "client_ip", .type = BLOBMSG_TYPE_STRING, .toval = toval_ip, },
	[PW_SESSION_TIMEOUT] = { .name = "Session-Timeout", .type = BLOBMSG_TYPE_INT32, .toval = toval_u32, },
	[PW_IDLE_TIMEOUT] = { .name = "Idle-Timeout", .type = BLOBMSG_TYPE_INT32, .toval = toval_u32, },
	[PW_CALLED_STATION_ID] = { .name = "called_station", .type = BLOBMSG_TYPE_STRING, .toval = toval_str, },
	[PW_CALLING_STATION_ID] = { .name = "calling_station", .type = BLOBMSG_TYPE_STRING, .toval = toval_str,},
	[PW_NAS_IDENTIFIER] = { .name = "nas_id", .type = BLOBMSG_TYPE_STRING, .toval = toval_str, },
	[PW_ACCT_SESSION_ID] = { .name = "acct_session", .type = BLOBMSG_TYPE_STRING, .toval = toval_str, },
	[PW_ACCT_INTERIM_INTERVAL] = { .name = "Acct-Interim-Interval", .type = BLOBMSG_TYPE_INT32, .toval = toval_u32, },
	[PW_CHARGEABLE_USER_IDENTITY] = { .name = "cui", .type = BLOBMSG_TYPE_STRING, .toval = toval_str, },
};

struct das_request {
	const struct radius_header *inbuf;		///< input (received) RADIUS packet buffer
	struct radius_header *outbuf;			///< allocated output buffer for RADIUS response, at least same size as inbuf
	const char *uspot_method;			///< uspot method to invoke to process the request
	const uint8_t code_ack, code_nak;		///< RADIUS response ACK/NAK codes
	int (* attrid_not_supported)(uint8_t id);	///< AVP filter callback, returns true if AVP id is not supported
	void (* ubus_cb)(struct ubus_request *, int, struct blob_attr *);	///< ubus_invoke() reply callback
};

// Process DAS request AVPs
static int
das_request_process(const struct das_request *drq)
{
	const struct radius_header *hdr = drq->inbuf;
	const struct radius_tlv *tlv = (struct radius_tlv *)(drq->inbuf+1);
	uint16_t attrs_len = ntohs(hdr->len) - sizeof(*hdr);
	struct radius_header *hdrout = drq->outbuf;
	struct radius_tlv *tlvout = (struct radius_tlv *)(drq->outbuf+1);
	uint16_t outlen;
	struct blob_buf b = {};
	uint32_t uspotid;
	int ret = -1;
	void *c;

	// copy the original header content
	memcpy(hdrout, hdr, sizeof(*hdrout));
	// reset len
	outlen = sizeof(*hdrout);
	hdrout->len = htons(outlen);

	if (blob_buf_init(&b, 0))
		goto failnoblob;

	if (blobmsg_add_string(&b, "uspot", das.uspot))
		goto fail;

	c = blobmsg_open_table(&b, "request");
	if (!c)
		goto fail;

	// parse AVPs
	while (attrs_len >= sizeof(*tlv)) {
		uint8_t id = tlv->id;

		// sanity check
		if (attrs_len < tlv->len || tlv->len < sizeof(*tlv)) {
			ULOG_ERR("invalid TLV length\n");
			goto fail;
		}

		// abort on unsupported AVP
		if (drq->attrid_not_supported(id)) {
			ULOG_ERR("unsupported RADIUS AVP: %d\n", id);
			goto fail;
		}

		// copy passthrough AVPs to response - preserve order
		switch (id) {
			case PW_CLASS:
			case PW_STATE:
			case PW_PROXY_STATE:
				memcpy(tlvout, tlv, tlv->len);
				tlvout = (struct radius_tlv *)((char *)tlvout + tlv->len);
				outlen += tlv->len;
				break;
			default:
				break;
		}

		// parse known attributes for uspot call
		if (radius_attrids[id].name) {
			switch (radius_attrids[id].type) {
				case BLOBMSG_TYPE_STRING:
					const char *str = radius_attrids[id].toval(tlv);
					if (!str)
						goto fail;
					if (blobmsg_add_string(&b, radius_attrids[id].name, str))
						goto fail;
					break;
				case BLOBMSG_TYPE_INT32:
					const uint32_t *val = radius_attrids[id].toval(tlv);
					if (!val)
						goto fail;
					if (blobmsg_add_u32(&b, radius_attrids[id].name, *val))
						goto fail;
					break;
				default:
					break;
			}
		}

		attrs_len -= tlv->len;
		tlv = (const struct radius_tlv *)((char *)tlv + tlv->len);
	}

	blobmsg_close_table(&b, c);

	// make ubus call to uspot with prepared blobmsg
	if (ubus_lookup_id(das.ctx, "uspot", &uspotid)) {
		ULOG_ERR("failed to look up uspot object\n");
		goto fail;
	}
	if (ubus_invoke(das.ctx, uspotid, drq->uspot_method, b.head, drq->ubus_cb, &ret, 3000))
		ret = -1;

	// process ubus reply, set response code
fail:
	blob_buf_free(&b);
failnoblob:
	hdrout->len = htons(outlen);
	hdrout->code = ret ? drq->code_nak : drq->code_ack;
	return ret;
}

enum {
	RETURN_FOUND,
	__RETURN_MAX,
};

static const struct blobmsg_policy uspot_disconnect_policy[__RETURN_MAX] = {
	[RETURN_FOUND] = { .name = "found", .type = BLOBMSG_TYPE_BOOL, },
};

static void
uspot_das_cb(struct ubus_request *req, int type, struct blob_attr *msg)
{
	struct blob_attr *tb[__RETURN_MAX];
	int *retval = (int *)req->priv;

	if (blobmsg_parse(uspot_disconnect_policy, __RETURN_MAX, tb, blob_data(msg), blob_len(msg))) {
		*retval = -1;
		return;
	}

	if (!tb[RETURN_FOUND]) {
		*retval = -1;
		return;
	}

	*retval = !blobmsg_get_bool(tb[RETURN_FOUND]);
}


/*
 A Disconnect-Request MUST contain only NAS and session identification
 attributes.  If other attributes are included in a Disconnect-
 Request, implementations MUST send a Disconnect-NAK; an Error-Cause
 Attribute with value "Unsupported Attribute" MAY be included.

 In CoA-Request and Disconnect-Request packets, all attributes MUST
 be treated as mandatory.
 A NAS MUST respond to a Disconnect-Request containing one or more
 unsupported attributes or Attribute values with a Disconnect-NAK;
 an Error-Cause Attribute with value 401 (Unsupported Attribute) or
 407 (Invalid Attribute Value) MAY be included.

 Disconnect Messages attributes
 Request   ACK      NAK   #   Attribute
 0-1       0        0     1   User-Name (Note 1)
 0-1       0        0     4   NAS-IP-Address (Note 1)		// NAS ident - supported
 0-1       0        0     5   NAS-Port (Note 1)			// sesssion ident - NOT supported
 0         0        0     6   Service-Type
 0         0        0     8   Framed-IP-Address (Note 1)	// session ident - supported
 0+        0        0    18   Reply-Message (Note 2)		// NOT supported
 0         0        0    24   State
 0+        0        0    25   Class (Note 4)			// sent back unmodified
 0+        0        0    26   Vendor-Specific (Note 7)		// NOT supported
 0-1       0        0    30   Called-Station-Id (Note 1)	// session ident - supported
 0-1       0        0    31   Calling-Station-Id (Note 1)	// session ident - supported
 0-1       0        0    32   NAS-Identifier (Note 1)		// NAS ident - supported
 0+        0+       0+   33   Proxy-State			// sent back unmodified
 0-1       0        0    44   Acct-Session-Id (Note 1)		// session ident - supported
 0-1       0-1      0    49   Acct-Terminate-Cause		// RFC only mentions ACK: ignored in Req
 0-1       0        0    50   Acct-Multi-Session-Id (Note 1)	// NOT supported
 0-1       0-1      0-1  55   Event-Timestamp			// for replay protection - NOT supported
 0         0        0    61   NAS-Port-Type
 0+        0-1      0    79   EAP-Message (Note 2)		// NOT supported
 0-1       0-1      0-1  80   Message-Authenticator		// NOT supported
 0-1       0        0    87   NAS-Port-Id (Note 1)		// session ident - NOT supported
 0-1       0        0    89   Chargeable-User-Identity (Note 1)	// session ident - supported
 0-1       0        0    95   NAS-IPv6-Address (Note 1)		// NAS ident - NOT supported
 0         0        0    96   Framed-Interface-Id (Note 1)
 0         0        0    97   Framed-IPv6-Prefix (Note 1)
 0         0        0+  101   Error-Cause			// NOT implemented
 Request   ACK      NAK   #   Attribute
 */
static int
disconnect_attrid_not_supported(uint8_t id)
{
	int ret;

	switch (id) {
		// allowed
		case PW_USER_NAME:
		case PW_NAS_IP_ADDRESS:
		case PW_NAS_IDENTIFIER:
		case PW_CLASS:
		case PW_CALLED_STATION_ID:
		case PW_CALLING_STATION_ID:
		case PW_PROXY_STATE:
		case PW_ACCT_SESSION_ID:
		case PW_ACCT_TERMINATE_CAUSE:
		case PW_CHARGEABLE_USER_IDENTITY:
			ret = 0;
			break;
		// allowed but not implemented
		case PW_NAS_PORT:
		case PW_REPLY_MESSAGE:
		case PW_VENDOR_SPECIFIC:
		case PW_ACCT_MULTI_SESSION_ID:
		case PW_EVENT_TIMESTAMP:
		case PW_EAP_MESSAGE:
		case PW_MESSAGE_AUTHENTICATOR:
		case PW_NAS_PORT_ID_STRING:
		case PW_NAS_IPV6_ADDRESS:
		// not allowed
		default:
			ret = 1;
			break;
	}

	return ret;
}

/**
 * Handle disconnect messages.
 * This function processes a Disconnect-Request packet and calls uspot to remove the matched client, if any.
 * Non implemented attributes that are allowed by the RFC result in a Disconnect-NAK reply and no further processing.
 * @param inbuf the received RADIUS DAE packet
 * @param outbuf output buffer for DAS reply, must be at least same size as inbuf
 * @return 0 for success
 *
\verbatim
 Disconnect Messages

 A Disconnect-Request packet is sent by the Dynamic Authorization
 Client in order to terminate user session(s) on a NAS and discard all
 associated session context.  The Disconnect-Request packet is sent to
 UDP port 3799, and identifies the NAS as well as the user session(s)
 to be terminated by inclusion of the identification attributes
 described in Section 3.

 The NAS responds to a Disconnect-Request packet sent by a Dynamic
 Authorization Client with a Disconnect-ACK if all associated session
 context is discarded and the user session(s) are no longer connected,
 or a Disconnect-NAK, if the NAS was unable to disconnect one or more
 sessions and discard all associated session context.
\endverbatim
 */
static int
das_disconnect_request(const struct radius_header *inbuf, struct radius_header *outbuf)
{
	struct das_request drq = {
		.inbuf = inbuf,
		.outbuf = outbuf,
		.uspot_method = "das_disconnect",
		.code_ack = DISCONNECT_ACK,
		.code_nak = DISCONNECT_NAK,
		.attrid_not_supported = disconnect_attrid_not_supported,
		.ubus_cb = uspot_das_cb,
	};

	return das_request_process(&drq);
}


/*
 Change-of-Authorization Messages attributes
 Request   ACK      NAK   #   Attribute
 0-1       0        0     1   User-Name (Note 1)		// supported
 0-1       0        0     4   NAS-IP-Address (Note 1)		// supported
 0-1       0        0     5   NAS-Port (Note 1)
 0-1       0        0-1   6   Service-Type
 0-1       0        0     7   Framed-Protocol (Note 3)
 0-1       0        0     8   Framed-IP-Address (Notes 1, 6)	// supported
 0-1       0        0     9   Framed-IP-Netmask (Note 3)
 0-1       0        0    10   Framed-Routing (Note 3)
 0+        0        0    11   Filter-ID (Note 3)
 0-1       0        0    12   Framed-MTU (Note 3)
 0+        0        0    13   Framed-Compression (Note 3)
 0+        0        0    14   Login-IP-Host (Note 3)
 0-1       0        0    15   Login-Service (Note 3)
 0-1       0        0    16   Login-TCP-Port (Note 3)
 0+        0        0    18   Reply-Message (Note 2)
 0-1       0        0    19   Callback-Number (Note 3)
 0-1       0        0    20   Callback-Id (Note 3)
 0+        0        0    22   Framed-Route (Note 3)
 0-1       0        0    23   Framed-IPX-Network (Note 3)
 0-1       0-1      0-1  24   State				// supported
 0+        0        0    25   Class (Note 3)			// supported
 0+        0        0    26   Vendor-Specific (Note 7)
 0-1       0        0    27   Session-Timeout (Note 3)		// supported
 0-1       0        0    28   Idle-Timeout (Note 3)		// supported
 0-1       0        0    29   Termination-Action (Note 3)
 0-1       0        0    30   Called-Station-Id (Note 1)	// supported
 0-1       0        0    31   Calling-Station-Id (Note 1)	// supported
 0-1       0        0    32   NAS-Identifier (Note 1)		// supported
 0+        0+       0+   33   Proxy-State			// supported
 0-1       0        0    34   Login-LAT-Service (Note 3)
 0-1       0        0    35   Login-LAT-Node (Note 3)
 0-1       0        0    36   Login-LAT-Group (Note 3)
 0-1       0        0    37   Framed-AppleTalk-Link (Note 3)
 0+        0        0    38   Framed-AppleTalk-Network (Note 3)
 0-1       0        0    39   Framed-AppleTalk-Zone (Note 3)
 0-1       0        0    44   Acct-Session-Id (Note 1)		// supported
 0-1       0        0    50   Acct-Multi-Session-Id (Note 1)
 0-1       0-1      0-1  55   Event-Timestamp
 0+        0        0    56   Egress-VLANID (Note 3)
 0-1       0        0    57   Ingress-Filters (Note 3)
 0+        0        0    58   Egress-VLAN-Name (Note 3)
 0-1       0        0    59   User-Priority-Table (Note 3)
 0-1       0        0    61   NAS-Port-Type (Note 3)
 0-1       0        0    62   Port-Limit (Note 3)
 0-1       0        0    63   Login-LAT-Port (Note 3)
 0+        0        0    64   Tunnel-Type (Note 5)
 0+        0        0    65   Tunnel-Medium-Type (Note 5)
 0+        0        0    66   Tunnel-Client-Endpoint (Note 5)
 0+        0        0    67   Tunnel-Server-Endpoint (Note 5)
 0+        0        0    69   Tunnel-Password (Note 5)
 0-1       0        0    71   ARAP-Features (Note 3)
 0-1       0        0    72   ARAP-Zone-Access (Note 3)
 0+        0        0    78   Configuration-Token (Note 3)
 0+        0-1      0    79   EAP-Message (Note 2)
 0-1       0-1      0-1  80   Message-Authenticator
 0+        0        0    81   Tunnel-Private-Group-ID (Note 5)
 0+        0        0    82   Tunnel-Assignment-ID (Note 5)
 0+        0        0    83   Tunnel-Preference (Note 5)
 0-1       0        0    85   Acct-Interim-Interval (Note 3)	// supported
 0-1       0        0    87   NAS-Port-Id (Note 1)
 0-1       0        0    88   Framed-Pool (Note 3)
 0-1       0        0    89   Chargeable-User-Identity (Note 1)	// supported
 0+        0        0    90   Tunnel-Client-Auth-ID (Note 5)
 0+        0        0    91   Tunnel-Server-Auth-ID (Note 5)
 0-1       0        0    92   NAS-Filter-Rule (Note 3)
 0         0        0    94   Originating-Line-Info
 0-1       0        0    95   NAS-IPv6-Address (Note 1)
 0-1       0        0    96   Framed-Interface-Id (Notes 1, 6)
 0+        0        0    97   Framed-IPv6-Prefix (Notes 1, 6)
 0+        0        0    98   Login-IPv6-Host (Note 3)
 0+        0        0    99   Framed-IPv6-Route (Note 3)
 0-1       0        0   100   Framed-IPv6-Pool (Note 3)
 0         0        0+  101   Error-Cause
 0+        0        0   123   Delegated-IPv6-Prefix (Note 3)

 (Note 1) Where NAS or session identification attributes are included
 in Disconnect-Request or CoA-Request packets, they are used for
 identification purposes only.  These attributes MUST NOT be used for
 purposes other than identification (e.g., within CoA-Request packets
 to request authorization changes).

 (Note 3) When included within a CoA-Request, these attributes
 represent an authorization change request.  When one of these
 attributes is omitted from a CoA-Request, the NAS assumes that the
 attribute value is to remain unchanged.  Attributes included in a
 CoA-Request replace all existing values of the same attribute(s).
 */
static int
coa_attrid_not_supported(uint8_t id)
{
	int ret;

	switch (id) {
		// allowed & implemented
		case PW_USER_NAME:
		case PW_NAS_IP_ADDRESS:
		case PW_FRAMED_IP_ADDRESS:
		case PW_NAS_IDENTIFIER:
		case PW_STATE:
		case PW_CLASS:
		case PW_SESSION_TIMEOUT:	// change
		case PW_IDLE_TIMEOUT:		// change
		case PW_CALLED_STATION_ID:
		case PW_CALLING_STATION_ID:
		case PW_PROXY_STATE:
		case PW_ACCT_SESSION_ID:
		case PW_ACCT_INTERIM_INTERVAL:	// change
		case PW_CHARGEABLE_USER_IDENTITY:
			ret = 0;
			break;
		// everything else
		default:
			ret = 1;
			break;
	}

	return ret;
}

/**
 * Handle CoA messages.
 * This function processes a CoA-Request packet and calls uspot to handle the matched client, if any.
 * Non implemented attributes that are allowed by the RFC result in a CoA-NAK reply and no further processing.
 * @param inbuf the received RADIUS DAE packet
 * @param outbuf output buffer for DAS reply, must be at least same size as inbuf
 * @return 0 for success
 *
\verbatim
 Change-of-Authorization (CoA) Messages

  CoA-Request packets contain information for dynamically changing
  session authorizations.  Typically, this is used to change data
  filters.  The data filters can be of either the ingress or egress
  kind, and are sent in addition to the identification attributes as
  described in Section 3.

  The NAS responds to a CoA-Request sent by a Dynamic Authorization
  Client with a CoA-ACK if the NAS is able to successfully change the
  authorizations for the user session(s), or a CoA-NAK if the CoA-
  Request is unsuccessful.  A NAS MUST respond to a CoA-Request
  including a Service-Type Attribute with an unsupported value with a
  CoA-NAK; an Error-Cause Attribute with value "Unsupported Service"
  SHOULD be included.
\endverbatim
 */
static int
das_coa_request(const struct radius_header *inbuf, struct radius_header *outbuf)
{
	struct das_request drq = {
		.inbuf = inbuf,
		.outbuf = outbuf,
		.uspot_method = "das_coa",
		.code_ack = COA_ACK,
		.code_nak = COA_NAK,
		.attrid_not_supported = coa_attrid_not_supported,
		.ubus_cb = uspot_das_cb,
	};

	return das_request_process(&drq);
}


/**
 * Fill response authenticator.
 * From RFC5176: The Authenticator field in a Response packet (e.g., Disconnect-ACK, Disconnect-NAK, CoA-ACK, or CoA-NAK)
 * is called the Response Authenticator, and contains a one-way MD5 hash calculated over a stream of octets consisting of the Code,
 * Identifier, Length, the Request Authenticator field from the  packet being replied to, and the response attributes if any,  followed by
 * the shared secret.  The resulting 16-octet MD5 hash value is stored in the Authenticator field of the Response packet.
 *
 * @param hdr the populated response packet
 * @param secret the shared secret
 * @note input sanitization is left to caller
 */
static void
radius_rspauth_fill(struct radius_header *hdr, const char *secret)
{
	uint8_t hash[16];
	md5_ctx_t md5 = {};

	md5_begin(&md5);
	md5_hash(hdr, ntohs(hdr->len), &md5);
	md5_hash(secret, strlen(secret), &md5);
	md5_end(hash, &md5);

	memcpy(hdr->auth, hash, sizeof(hdr->auth));
}

/**
 * Verify request authenticator.
 * From RFC5176: The Request Authenticator is calculated the same way as for an Accounting-Request, specified in [RFC2866].
 * From RFC2866: The NAS and RADIUS accounting server share a secret.  The Request Authenticator field in Accounting-Request packets
 * contains a one-way MD5 hash calculated over a stream of octets consisting of the Code + Identifier + Length + 16 zero octets +
 * request attributes + shared secret (where + indicates concatenation).  The 16 octet MD5 hash value is stored in the Authenticator field
 * of the Accounting-Request packet.
 *
 * @param buf the received RADIUS DAS packet
 * @param secret the shared RADIUS secret
 * @return 0 if authenticator is valid, non-zero otherwise.
 * @note input sanitization is left to caller
 */
static int
radius_rqauth_check(const struct radius_header *buf, const char *secret)
{
	struct radius_header hdr;
	uint16_t len = ntohs(buf->len);
	uint8_t rqauth[16];
	uint8_t hash[16];
	md5_ctx_t md5 = {};

	memcpy(&hdr, buf, sizeof(hdr));
	memcpy(&rqauth, hdr.auth, sizeof(rqauth));	// save request authenticator
	memset(&hdr.auth, 0, sizeof(hdr.auth));		// zero-out authenticator for hashing

	md5_begin(&md5);
	md5_hash(&hdr, sizeof(hdr), &md5);

	len -= sizeof(hdr);
	buf++;

	md5_hash(buf, len, &md5);
	md5_hash(secret, strlen(secret), &md5);
	md5_end(hash, &md5);

	return memcmp(hash, rqauth, sizeof(hash));
}

/**
 * Parse a RADIUS DAE request.
 * Parses and processes Disconnect-Request and CoA-Request messages.
 * @param buf the received DAE packet
 * @param buflen buf length
 * @param sin the remote sockaddr
 * @param sinlen sin length
 * @param u the uloop socket fd
 */
static void
radius_dae_parse(const void *buf, unsigned int buflen, const struct sockaddr_storage *sin, socklen_t sinlen,
		 struct uloop_fd *u)
{
	static char bufout[RAD_PROX_BUFLEN];
	const struct radius_header *hdr = (struct radius_header *)buf;
	struct radius_header *hdrout = (struct radius_header *)bufout;
	uint16_t radius_len;

	if (buflen < sizeof(*hdr)) {
		ULOG_ERR("invalid packet length, %d\n", buflen);
		return;
	}

	/*
	Length
	    The Length field is two octets.  It indicates the length of the
	    packet including the Code, Identifier, Length, Authenticator, and
	    Attribute fields.  Octets outside the range of the Length field
	    MUST be treated as padding and ignored on reception.  If the
	    packet is shorter than the Length field indicates, it MUST be
	    silently discarded.  The minimum length is 20 and maximum length
	    is 4096.
	 */
	radius_len = ntohs(hdr->len);
	if (radius_len > buflen) {
		ULOG_ERR("truncated packet, expected %d got %d bytes\n", radius_len, buflen);
		return;
	}

	if ((radius_len < 20) || (radius_len > 4096)) {
		ULOG_ERR("invalid header length %d\n", radius_len);
		return;
	}

	/*
	Authenticator
	    A Dynamic Authorization Server MUST silently discard Disconnect-Request
	    or CoA-Request packets from untrusted sources.
	 */
	if (radius_rqauth_check(buf, das.secret)) {
		ULOG_ERR("invalid Request Authenticator\n");
		return;
	}

	switch (hdr->code) {
		case DISCONNECT_REQUEST:
			das_disconnect_request(hdr, hdrout);
			break;
		case COA_REQUEST:
			das_coa_request(hdr, hdrout);
			break;
		default:
			ULOG_ERR("ignoring frame with invalid RADIUS code %d\n", hdr->code);
			return;
	}

	// we always get at least a header back - fill Response Authenticator
	radius_rspauth_fill(hdrout, das.secret);

	// send reply back to DAC
	if (sendto(u->fd, hdrout, ntohs(hdrout->len), MSG_DONTWAIT, (struct sockaddr *)sin, sinlen) < 0)
		ULOG_ERR("failed to deliver DAS reply frame\n");
}

static void
sock_recv(struct uloop_fd *u, unsigned int events)
{
	static char buf[RAD_PROX_BUFLEN];
	static struct sockaddr_storage sin;
	socklen_t sinlen = sizeof(sin);
	int len;

	do {
		len = recvfrom(u->fd, buf, sizeof(buf), 0, (struct sockaddr *)&sin, &sinlen);
		if (len < 0) {
			switch (errno) {
			case EAGAIN:
				return;
			case EINTR:
				continue;
			default:
				perror("recvfrom");
				uloop_fd_delete(u);
				return;
			}
		}

		radius_dae_parse(buf, (unsigned int)len, &sin, sinlen, u);
	} while (1);
}

static int
sock_open(const char *port)
{
	struct uloop_fd *u = &das.ufd;

	memset(u, 0, sizeof(*u));

	u->fd = usock(USOCK_UDP | USOCK_SERVER | USOCK_NONBLOCK, NULL, port);
	if (u->fd < 0) {
		perror("usock");
		return -1;
	}

	u->cb = sock_recv;

	uloop_fd_add(u, ULOOP_READ);

	return 0;
}

static int
load_config(void)
{
	struct uci_context *uci_ctx;
	struct uci_package *uci_uspot;
	struct uci_section *uci_s;
	const char *str;
	int ret = -1;

	uci_ctx = uci_alloc_context();
	if (!uci_ctx)
		return -1;

	if (uci_load(uci_ctx, "uspot", &uci_uspot) || !uci_uspot)
		goto fail;

	uci_s = uci_lookup_section(uci_ctx, uci_uspot, das.uspot);
	if (!uci_s)
		goto fail;

	str = uci_lookup_option_string(uci_ctx, uci_s, "das_secret");
	if (!str)
		goto fail;

	das.secret = strdup(str);
	if (!das.secret)
		goto fail;

	str = uci_lookup_option_string(uci_ctx, uci_s, "das_port");
	if (!str)
		str = "3799";	// RFC default

	das.port = strdup(str);
	if (!das.port)
		goto fail;

	ret = 0;
fail:
	uci_free_context(uci_ctx);
	return ret;
}

static void
usage(const char *name)
{
	printf("usage: %s -u <uspot>\n"
	       " -u <uspot>\t"	"use <uspot> configuration\n"
	       " -h\t\t"	"show this help message\n"
	       , name);
}

int main(int argc, char **argv)
{
	int ch;

	das.uspot = NULL;

	while ((ch = getopt(argc, argv, "hu:")) != -1) {
		switch (ch) {
			case 'h':
				usage(argv[0]);
				return 0;
			case 'u':
				das.uspot = optarg;
				break;
			default:
				usage(argv[0]);
				exit(-1);
		}
	}

	if (!das.uspot) {
		usage(argv[0]);
		exit(-1);
	}

	ulog_open(ULOG_STDIO | ULOG_SYSLOG, LOG_DAEMON, "uspot-das");

	if (load_config()) {
		ULOG_ERR("failed to load configuration\n");
		return -1;
	}

	if (uloop_init()) {
		ULOG_ERR("uloop_init() failed\n");
		return -1;
	}

	das.ctx = ubus_connect(NULL);
	if (!das.ctx) {
		ULOG_ERR("failed to connect to ubus\n");
		return -1;
	}

	if (sock_open(das.port)) {
		ULOG_ERR("failed to setup socket\n");
		return -1;
	}

	ubus_add_uloop(das.ctx);

	uloop_run();
	uloop_end();

	ubus_free(das.ctx);

	if (das.ufd.registered) {
		uloop_fd_delete(&das.ufd);
		close(das.ufd.fd);
	}

	freeconst(das.secret);
	freeconst(das.port);


	return 0;
}


