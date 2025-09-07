/*
 * nslookup_lede - musl compatible replacement for busybox nslookup
 *
 * Copyright (C) 2017 Jo-Philipp Wich <jo@mein.io>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

//config:config NSLOOKUP_OPENWRT
//config:	bool "nslookup_openwrt"
//config:	depends on !NSLOOKUP
//config:	default y
//config:	help
//config:	  nslookup is a tool to query Internet name servers (LEDE flavor).
//config:
//config:config FEATURE_NSLOOKUP_OPENWRT_LONG_OPTIONS
//config:       bool "Enable long options"
//config:       default y
//config:       depends on NSLOOKUP_OPENWRT && LONG_OPTS
//config:       help
//config:         Support long options for the nslookup applet.

//applet:IF_NSLOOKUP_OPENWRT(APPLET(nslookup, BB_DIR_USR_BIN, BB_SUID_DROP))

//kbuild:lib-$(CONFIG_NSLOOKUP_OPENWRT) += nslookup_lede.o

//usage:#define nslookup_lede_trivial_usage
//usage:       "[HOST] [SERVER]"
//usage:#define nslookup_lede_full_usage "\n\n"
//usage:       "Query the nameserver for the IP address of the given HOST\n"
//usage:       "optionally using a specified DNS server"
//usage:
//usage:#define nslookup_lede_example_usage
//usage:       "$ nslookup localhost\n"
//usage:       "Server:     default\n"
//usage:       "Address:    default\n"
//usage:       "\n"
//usage:       "Name:       debian\n"
//usage:       "Address:    127.0.0.1\n"

#include <stdio.h>
#include <resolv.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <poll.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <netdb.h>

#include <libubox/ulog.h>

#define ENABLE_FEATURE_IPV6	1

typedef struct len_and_sockaddr {
	socklen_t len;
	union {
		struct sockaddr sa;
		struct sockaddr_in sin;
#if ENABLE_FEATURE_IPV6
		struct sockaddr_in6 sin6;
#endif
	} u;
} len_and_sockaddr;

struct ns {
	const char *name;
	len_and_sockaddr addr;
	int failures;
	int replies;
};

struct query {
	const char *name;
	size_t qlen, rlen;
	unsigned char query[512], reply[512];
	unsigned long latency;
	int rcode, n_ns;
};

static const char *rcodes[] = {
	"NOERROR",
	"FORMERR",
	"SERVFAIL",
	"NXDOMAIN",
	"NOTIMP",
	"REFUSED",
	"YXDOMAIN",
	"YXRRSET",
	"NXRRSET",
	"NOTAUTH",
	"NOTZONE",
	"RESERVED11",
	"RESERVED12",
	"RESERVED13",
	"RESERVED14",
	"RESERVED15",
	"BADVERS"
};

static unsigned int default_port = 53;
static unsigned int default_retry = 1;
static unsigned int default_timeout = 2;


static int parse_reply(const unsigned char *msg, size_t len, int *bb_style_counter)
{
	ns_msg handle;
	ns_rr rr;
	int i, n, rdlen;
	const char *format = NULL;
	char astr[INET6_ADDRSTRLEN], dname[MAXDNAME];
	const unsigned char *cp;

	if (ns_initparse(msg, len, &handle) != 0) {
		//fprintf(stderr, "Unable to parse reply: %s\n", strerror(errno));
		return -1;
	}

	for (i = 0; i < ns_msg_count(handle, ns_s_an); i++) {
		if (ns_parserr(&handle, ns_s_an, i, &rr) != 0) {
			//fprintf(stderr, "Unable to parse resource record: %s\n", strerror(errno));
			return -1;
		}

		rdlen = ns_rr_rdlen(rr);

		switch (ns_rr_type(rr))
		{
		case ns_t_a:
			if (rdlen != 4) {
				//fprintf(stderr, "Unexpected A record length\n");
				return -1;
			}
			inet_ntop(AF_INET, ns_rr_rdata(rr), astr, sizeof(astr));
			printf("Name:\t%s\nAddress: %s\n", ns_rr_name(rr), astr);
			break;

#if ENABLE_FEATURE_IPV6
		case ns_t_aaaa:
			if (rdlen != 16) {
				//fprintf(stderr, "Unexpected AAAA record length\n");
				return -1;
			}
			inet_ntop(AF_INET6, ns_rr_rdata(rr), astr, sizeof(astr));
			printf("%s\thas AAAA address %s\n", ns_rr_name(rr), astr);
			break;
#endif

		case ns_t_ns:
			if (!format)
				format = "%s\tnameserver = %s\n";
			/* fall through */

		case ns_t_cname:
			if (!format)
				format = "%s\tcanonical name = %s\n";
			/* fall through */

		case ns_t_ptr:
			if (!format)
				format = "%s\tname = %s\n";
			if (ns_name_uncompress(ns_msg_base(handle), ns_msg_end(handle),
				ns_rr_rdata(rr), dname, sizeof(dname)) < 0) {
				//fprintf(stderr, "Unable to uncompress domain: %s\n", strerror(errno));
				return -1;
			}
			printf(format, ns_rr_name(rr), dname);
			break;

		case ns_t_mx:
			if (rdlen < 2) {
				fprintf(stderr, "MX record too short\n");
				return -1;
			}
			n = ns_get16(ns_rr_rdata(rr));
			if (ns_name_uncompress(ns_msg_base(handle), ns_msg_end(handle),
				ns_rr_rdata(rr) + 2, dname, sizeof(dname)) < 0) {
				//fprintf(stderr, "Cannot uncompress MX domain: %s\n", strerror(errno));
				return -1;
			}
			printf("%s\tmail exchanger = %d %s\n", ns_rr_name(rr), n, dname);
			break;

		case ns_t_txt:
			if (rdlen < 1) {
				//fprintf(stderr, "TXT record too short\n");
				return -1;
			}
			n = *(unsigned char *)ns_rr_rdata(rr);
			if (n > 0) {
				memset(dname, 0, sizeof(dname));
				memcpy(dname, ns_rr_rdata(rr) + 1, n);
				printf("%s\ttext = \"%s\"\n", ns_rr_name(rr), dname);
			}
			break;

		case ns_t_soa:
			if (rdlen < 20) {
				//fprintf(stderr, "SOA record too short\n");
				return -1;
			}

			printf("%s\n", ns_rr_name(rr));

			cp = ns_rr_rdata(rr);
			n = ns_name_uncompress(ns_msg_base(handle), ns_msg_end(handle),
			                       cp, dname, sizeof(dname));

			if (n < 0) {
				//fprintf(stderr, "Unable to uncompress domain: %s\n", strerror(errno));
				return -1;
			}

			printf("\torigin = %s\n", dname);
			cp += n;

			n = ns_name_uncompress(ns_msg_base(handle), ns_msg_end(handle),
			                       cp, dname, sizeof(dname));

			if (n < 0) {
				//fprintf(stderr, "Unable to uncompress domain: %s\n", strerror(errno));
				return -1;
			}

			printf("\tmail addr = %s\n", dname);
			cp += n;

			printf("\tserial = %lu\n", ns_get32(cp));
			cp += 4;

			printf("\trefresh = %lu\n", ns_get32(cp));
			cp += 4;

			printf("\tretry = %lu\n", ns_get32(cp));
			cp += 4;

			printf("\texpire = %lu\n", ns_get32(cp));
			cp += 4;

			printf("\tminimum = %lu\n", ns_get32(cp));
			break;

		default:
			break;
		}
	}

	return i;
}

static int parse_nsaddr(const char *addrstr, len_and_sockaddr *lsa)
{
	char *eptr, *hash, ifname[IFNAMSIZ];
	unsigned int port = default_port;
	unsigned int scope = 0;

	hash = strchr(addrstr, '#');

	if (hash) {
		*hash++ = '\0';
		port = strtoul(hash, &eptr, 10);

		if (eptr == hash || *eptr != '\0' || port > 65535) {
			errno = EINVAL;
			return -1;
		}
	}

	hash = strchr(addrstr, '%');

	if (hash) {
		for (eptr = ++hash; *eptr != '\0' && *eptr != '#'; eptr++) {
			if ((eptr - hash) >= IFNAMSIZ) {
				errno = ENODEV;
				return -1;
			}

			ifname[eptr - hash] = *eptr;
		}

		ifname[eptr - hash] = '\0';
		scope = if_nametoindex(ifname);

		if (scope == 0) {
			errno = ENODEV;
			return -1;
		}
	}

#if ENABLE_FEATURE_IPV6
	if (inet_pton(AF_INET6, addrstr, &lsa->u.sin6.sin6_addr)) {
		lsa->u.sin6.sin6_family = AF_INET6;
		lsa->u.sin6.sin6_port = htons(port);
		lsa->u.sin6.sin6_scope_id = scope;
		lsa->len = sizeof(lsa->u.sin6);
		return 0;
	}
#endif

	if (!scope && inet_pton(AF_INET, addrstr, &lsa->u.sin.sin_addr)) {
		lsa->u.sin.sin_family = AF_INET;
		lsa->u.sin.sin_port = htons(port);
		lsa->len = sizeof(lsa->u.sin);
		return 0;
	}

	errno = EINVAL;
	return -1;
}

static unsigned long mtime(void)
{
	struct timespec ts;
	clock_gettime(CLOCK_REALTIME, &ts);
	return (unsigned long)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

#if ENABLE_FEATURE_IPV6
static void to_v4_mapped(len_and_sockaddr *a)
{
	if (a->u.sa.sa_family != AF_INET)
		return;

	memcpy(a->u.sin6.sin6_addr.s6_addr + 12,
	       &a->u.sin.sin_addr, 4);

	memcpy(a->u.sin6.sin6_addr.s6_addr,
	       "\0\0\0\0\0\0\0\0\0\0\xff\xff", 12);

	a->u.sin6.sin6_family = AF_INET6;
	a->u.sin6.sin6_flowinfo = 0;
	a->u.sin6.sin6_scope_id = 0;
	a->len = sizeof(a->u.sin6);
}
#endif


/*
 * Function logic borrowed & modified from musl libc, res_msend.c
 */

static int send_queries(struct ns *ns, int n_ns, struct query *queries, int n_queries)
{
	int fd;
	int timeout = default_timeout * 1000, retry_interval, servfail_retry = 0;
	len_and_sockaddr from = { };
#if ENABLE_FEATURE_IPV6
	int one = 1;
#endif
	int recvlen = 0;
	int n_replies = 0;
	struct pollfd pfd;
	unsigned long t0, t1, t2;
	int nn, qn, next_query = 0;

	from.u.sa.sa_family = AF_INET;
	from.len = sizeof(from.u.sin);

#if ENABLE_FEATURE_IPV6
	for (nn = 0; nn < n_ns; nn++) {
		if (ns[nn].addr.u.sa.sa_family == AF_INET6) {
			from.u.sa.sa_family = AF_INET6;
			from.len = sizeof(from.u.sin6);
			break;
		}
	}
#endif

	/* Get local address and open/bind a socket */
	fd = socket(from.u.sa.sa_family, SOCK_DGRAM|SOCK_CLOEXEC|SOCK_NONBLOCK, 0);

#if ENABLE_FEATURE_IPV6
	/* Handle case where system lacks IPv6 support */
	if (fd < 0 && from.u.sa.sa_family == AF_INET6 && errno == EAFNOSUPPORT) {
		fd = socket(AF_INET, SOCK_DGRAM|SOCK_CLOEXEC|SOCK_NONBLOCK, 0);
		from.u.sa.sa_family = AF_INET;
	}
#endif

	if (fd < 0)
		return -1;

	if (bind(fd, &from.u.sa, from.len) < 0) {
		close(fd);
		return -1;
	}

#if ENABLE_FEATURE_IPV6
	/* Convert any IPv4 addresses in a mixed environment to v4-mapped */
	if (from.u.sa.sa_family == AF_INET6) {
		setsockopt(fd, IPPROTO_IPV6, IPV6_V6ONLY, &one, sizeof(one));

		for (nn = 0; nn < n_ns; nn++)
			to_v4_mapped(&ns[nn].addr);
	}
#endif

	pfd.fd = fd;
	pfd.events = POLLIN;
	retry_interval = timeout / default_retry;
	t0 = t2 = mtime();
	t1 = t2 - retry_interval;

	for (; t2 - t0 < timeout; t2 = mtime()) {
		if (t2 - t1 >= retry_interval) {
			for (qn = 0; qn < n_queries; qn++) {
				if (queries[qn].rlen)
					continue;

				for (nn = 0; nn < n_ns; nn++) {
					sendto(fd, queries[qn].query, queries[qn].qlen,
					       MSG_NOSIGNAL, &ns[nn].addr.u.sa, ns[nn].addr.len);
				}
			}

			t1 = t2;
			servfail_retry = 2 * n_queries;
		}

		/* Wait for a response, or until time to retry */
		if (poll(&pfd, 1, t1+retry_interval-t2) <= 0)
			continue;

		while (1) {
			recvlen = recvfrom(fd, queries[next_query].reply,
			                   sizeof(queries[next_query].reply), 0,
			                   &from.u.sa, &from.len);

			/* read error */
			if (recvlen < 0)
				break;

			/* Ignore non-identifiable packets */
			if (recvlen < 4)
				continue;

			/* Ignore replies from addresses we didn't send to */
			for (nn = 0; nn < n_ns; nn++)
				if (memcmp(&from.u.sa, &ns[nn].addr.u.sa, from.len) == 0)
					break;

			if (nn >= n_ns)
				continue;

			/* Find which query this answer goes with, if any */
			for (qn = next_query; qn < n_queries; qn++)
				if (!memcmp(queries[next_query].reply, queries[qn].query, 2))
					break;

			if (qn >= n_queries || queries[qn].rlen)
				continue;

			queries[qn].rcode = queries[next_query].reply[3] & 15;
			queries[qn].latency = mtime() - t0;
			queries[qn].n_ns = nn;

			ns[nn].replies++;

			/* Only accept positive or negative responses;
			 * retry immediately on server failure, and ignore
			 * all other codes such as refusal. */
			switch (queries[qn].rcode) {
			case 0:
			case 3:
				break;

			case 2:
				if (servfail_retry && servfail_retry--) {
					ns[nn].failures++;
					sendto(fd, queries[qn].query, queries[qn].qlen,
					       MSG_NOSIGNAL, &ns[nn].addr.u.sa, ns[nn].addr.len);
				}
				/* fall through */

			default:
				continue;
			}

			/* Store answer */
			n_replies++;

			queries[qn].rlen = recvlen;

			if (qn == next_query) {
				while (next_query < n_queries) {
					if (!queries[next_query].rlen)
						break;

					next_query++;
				}
			}
			else {
				memcpy(queries[qn].reply, queries[next_query].reply, recvlen);
			}

			if (next_query >= n_queries)
				return n_replies;
		}
	}

	return n_replies;
}

static struct ns *add_ns(struct ns **ns, int *n_ns, const char *addr)
{
	char portstr[sizeof("65535")], *p;
	len_and_sockaddr a = { };
	struct ns *tmp;
	struct addrinfo *ai, *aip, hints = {
		.ai_flags = AI_NUMERICSERV,
		.ai_socktype = SOCK_DGRAM
	};

	if (parse_nsaddr(addr, &a)) {
		/* Maybe we got a domain name, attempt to resolve it using the standard
		 * resolver routines */

		p = strchr(addr, '#');
		snprintf(portstr, sizeof(portstr), "%hu",
		         (unsigned short)(p ? strtoul(p, NULL, 10) : default_port));

		if (!getaddrinfo(addr, portstr, &hints, &ai)) {
			for (aip = ai; aip; aip = aip->ai_next) {
				if (aip->ai_addr->sa_family != AF_INET &&
				    aip->ai_addr->sa_family != AF_INET6)
					continue;

#if ! ENABLE_FEATURE_IPV6
				if (aip->ai_addr->sa_family != AF_INET)
					continue;
#endif

				tmp = realloc(*ns, sizeof(**ns) * (*n_ns + 1));

				if (!tmp)
					return NULL;

				*ns = tmp;

				(*ns)[*n_ns].name = addr;
				(*ns)[*n_ns].replies = 0;
				(*ns)[*n_ns].failures = 0;
				(*ns)[*n_ns].addr.len = aip->ai_addrlen;

				memcpy(&(*ns)[*n_ns].addr.u.sa, aip->ai_addr, aip->ai_addrlen);

				(*n_ns)++;
			}

			freeaddrinfo(ai);

			return &(*ns)[*n_ns];
		}

		return NULL;
	}

	tmp = realloc(*ns, sizeof(**ns) * (*n_ns + 1));

	if (!tmp)
		return NULL;

	*ns = tmp;

	(*ns)[*n_ns].addr = a;
	(*ns)[*n_ns].name = addr;
	(*ns)[*n_ns].replies = 0;
	(*ns)[*n_ns].failures = 0;

	return &(*ns)[(*n_ns)++];
}

static struct query *add_query(struct query **queries, int *n_queries,
                               int type, const char *dname)
{
	struct query *tmp;
	ssize_t qlen;

	tmp = realloc(*queries, sizeof(**queries) * (*n_queries + 1));

	if (!tmp)
		return NULL;

	memset(&tmp[*n_queries], 0, sizeof(*tmp));

	qlen = res_mkquery(QUERY, dname, C_IN, type, NULL, 0, NULL,
	                   tmp[*n_queries].query, sizeof(tmp[*n_queries].query));

	tmp[*n_queries].qlen = qlen;
	tmp[*n_queries].name = dname;
	*queries = tmp;

	return &tmp[(*n_queries)++];
}

int main(int argc, char **argv)
{
	int rc = 1;
	struct ns *ns = NULL;
	struct query *queries = NULL;
	int n_ns = 0, n_queries = 0;
	int c = 0;

	char *url = "telecominfraproject.com";
	char *server = "127.0.0.1";
	int v6 = 0;

	while (1) {
		int option = getopt(argc, argv, "u:s:i:6");

		if (option == -1)
			break;

		switch (option) {
		case '6':
			v6 = 1;
			break;
		case 'u':
			url = optarg;
			break;
		case 's':
			server = optarg;
			break;
		default:
		case 'h':
			printf("Usage: dnsprobe OPTIONS\n"
			       "  -6 - use ipv6\n"
			       "  -u <url>\n"
			       "  -s <server>\n");
			return -1;
		}
	}

	ulog_open(ULOG_SYSLOG | ULOG_STDIO, LOG_DAEMON, "dnsprobe");

	ULOG_INFO("attempting to probe dns - %s %s %s\n",
		url, server, v6 ? "ipv6" : "");


	add_query(&queries, &n_queries, v6 ? T_AAAA : T_A, url);

	add_ns(&ns, &n_ns, server);

	rc = send_queries(&ns[0], 1, queries, n_queries);
	if (rc <= 0) {
		fprintf(stderr, "Failed to send queries: %s\n", strerror(errno));
		rc = -1;
		goto out;
	}

	if (queries[0].rcode != 0) {
		printf("** server can't find %s: %s\n", queries[0].name,
		rcodes[queries[0].rcode]);
		goto out;
	}

	if (queries[0].rlen) {
		c = parse_reply(queries[0].reply, queries[0].rlen, NULL);
	}

	if (c == 0)
		printf("*** Can't find %s: No answer\n", queries[0].name);
	else if (c < 0)
		printf("*** Can't find %s: Parse error\n", queries[0].name);
	else
		rc = 0;

out:
	if (n_ns)
		free(ns);

	if (n_queries)
		free(queries);

	return rc;
}
