/* SPDX-License-Identifier: BSD-3-Clause */

#include "dns.h"

static char name_buffer[MAX_NAME_LEN + 1];

static int
scan_name(const uint8_t *buffer, int len)
{
	int offset = 0;

	while (len && (*buffer != '\0')) {
		int l = *buffer;

		if (IS_COMPRESSED(l))
			return offset + 2;

		if (l + 1 > len) return -1;
		len -= l + 1;
		offset += l + 1;
		buffer += l + 1;
	}

	if (!len || !offset || (*buffer != '\0'))
		return -1;

	return offset + 1;
}

static struct dns_header*
dns_consume_header(uint8_t **data, int *len)
{
	struct dns_header *h = (struct dns_header *) *data;

	if (*len < sizeof(struct dns_header))
		return NULL;

	h->id = be16_to_cpu(h->id);
	h->flags = be16_to_cpu(h->flags);
	h->questions = be16_to_cpu(h->questions);
	h->answers = be16_to_cpu(h->answers);
	h->authority = be16_to_cpu(h->authority);
	h->additional = be16_to_cpu(h->additional);

	*len -= sizeof(struct dns_header);
	*data += sizeof(struct dns_header);

	return h;
}

static struct dns_question*
dns_consume_question(uint8_t **data, int *len)
{
	struct dns_question *q = (struct dns_question *) *data;

	if (*len < sizeof(struct dns_question))
		return NULL;

	q->type = be16_to_cpu(q->type);
	q->class = be16_to_cpu(q->class);

	*len -= sizeof(struct dns_question);
	*data += sizeof(struct dns_question);

	return q;
}

static struct dns_answer*
dns_consume_answer(uint8_t **data, int *len)
{
	struct dns_answer *a = (struct dns_answer *) *data;

	if (*len < sizeof(struct dns_answer))
		return NULL;

	a->type = be16_to_cpu(a->type);
	a->class = be16_to_cpu(a->class);
	a->ttl = be32_to_cpu(a->ttl);
	a->rdlength = be16_to_cpu(a->rdlength);

	*len -= sizeof(struct dns_answer);
	*data += sizeof(struct dns_answer);

	return a;
}

static char *
dns_consume_name(const uint8_t *base, int blen, uint8_t **data, int *len)
{
	int nlen = scan_name(*data, *len);

	if (nlen < 1)
		return NULL;

	if (dn_expand(base, base + blen, *data, name_buffer, MAX_NAME_LEN) < 0) {
		perror("dns_consume_name/dn_expand");
		return NULL;
	}

	*len -= nlen;
	*data += nlen;

	return name_buffer;
}

static int
parse_answer(uint8_t *buffer, int len, uint8_t **b, int *rlen)
{
	char *name = dns_consume_name(buffer, len, b, rlen);
	struct dns_answer *a;
	uint8_t *rdata;
	char ipbuf[33];

	if (*rlen < 0) {
		fprintf(stderr, "dropping: bad answer - bad length\n");
		return -1;
	}

	a = dns_consume_answer(b, rlen);
	if (!a) {
		fprintf(stderr, "dropping: bad answer - bad buffer\n");
		return -1;
	}

	if (!name) {
		return 0;
	}

	if ((a->class & ~CLASS_FLUSH) != CLASS_IN) {
		fprintf(stderr, "dropping: class\n");
		return -1;
	}

	rdata = *b;
	if (a->rdlength > *rlen) {
		fprintf(stderr, "dropping: bad answer - bad rlen\n");
		return -1;
	}

	*rlen -= a->rdlength;
	*b += a->rdlength;

	switch (a->type) {
	case TYPE_A:
		if (a->rdlength != 4)
			return 0;

		if (!inet_ntop(AF_INET, rdata, ipbuf, sizeof(ipbuf)))
			return 0;
		break;

	case TYPE_AAAA:
		if (a->rdlength != 16)
			return 0;

		if (!inet_ntop(AF_INET6, rdata, ipbuf, sizeof(ipbuf)))
			return 0;
		break;

	default:
		return 0;
	}

	ubus_notify_qosify(name, ipbuf, a->type, a->ttl);
	printf("%s %s %" PRIu32 "\n", name, ipbuf, a->ttl);

	return 0;
}

void
dns_handle_packet(uint8_t *buffer, int len)
{
	struct dns_header *h;
	uint8_t *b = buffer;
	int rlen = len;

	h = dns_consume_header(&b, &rlen);
	if (!h) {
		fprintf(stderr, "dropping: bad header\n");
		return;
	}

	if (!(h->flags & FLAG_RESPONSE))
		return;

	if (!h->answers)
		return;

	while (h->questions-- > 0) {
		char *name = dns_consume_name(buffer, len, &b, &rlen);
		struct dns_question *q;

		if (!name || rlen < 0)
			return;

		q = dns_consume_question(&b, &rlen);
		if (!q) {
			fprintf(stderr, "dropping: bad question\n");
			return;
		}
	}

	while (h->answers-- > 0)
		if (parse_answer(buffer, len, &b, &rlen))
			return;

/*	while (h->authority-- > 0)
		if (parse_answer(buffer, len, &b, &rlen))
			return;

	while (h->additional-- > 0)
		if (parse_answer(buffer, len, &b, &rlen))
			return;
*/
}
