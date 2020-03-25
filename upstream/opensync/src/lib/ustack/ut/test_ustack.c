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

#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <netinet/ip_icmp.h>
#include <netinet/icmp6.h>
#include "log.h"
#include "net_header_parse.h"
#include "target.h"
#include "unity.h"

#include "pcap.c"

struct net_header_parser g_parser;
const char *test_name = "ustack_tests";

#define NET_HDR_BUFF_SIZE 128 + (2 * INET6_ADDRSTRLEN + 128) + 256

static char log_buf[NET_HDR_BUFF_SIZE] = { 0 };

/**
 * @brief Converts a bytes array in a hex dump file wireshark can import.
 *
 * Dumps the array in a file that can then be imported by wireshark.
 * Useful to visualize the packet content.
 */
void create_hex_dump(const char *fname, const uint8_t *buf, size_t len)
{
    int line_number = 0;
    bool new_line = true;
    size_t i;
    FILE *f;

    f = fopen(fname, "w+");

    if (f == NULL) return;

    for (i = 0; i < len; i++)
    {
	 new_line = (i == 0 ? true : ((i % 8) == 0));
	 if (new_line)
	 {
	      if (line_number) fprintf(f, "\n");
	      fprintf(f, "%06x", line_number);
	      line_number += 8;
	 }
         fprintf(f, " %02x", buf[i]);
    }
    fprintf(f, "\n");
    fclose(f);

    return;
}

/**
 * @brief Convenient wrapper
 *
 * Dumps the packet content in /tmp/ustack_tests_<pkt name> for
 * wireshark consumption and sets g_parser data fields.
 * @params pkt the C structure containing an exported packet capture
 */
#define PREPARE_UT(pkt)                                         \
    {                                                           \
        char fname[128];                                        \
        size_t len = sizeof(pkt);                               \
                                                                \
        snprintf(fname, sizeof(fname), "/tmp/%s_%s.txtpcap",    \
                 test_name, #pkt);                              \
        create_hex_dump(fname, pkt, len);                       \
        g_parser.packet_len = len;                              \
        g_parser.data = (uint8_t *)pkt;                         \
    }


void setUp(void)
{
    memset(&g_parser, 0, sizeof(g_parser));
    return;
}

void tearDown(void)
{
    memset(&g_parser, 0, sizeof(g_parser));
    return;
}

/**
 * @brief Parsing pcap.c's pkt16574.
 *
 * pkt16574 is a ICMpv6 packet over vlan4 with multiple ipv6 extension headers.
 * See pcap.json for details.
 */
void test_icmpv6_pcap_parse(void)
{
    struct net_header_parser *parser;
    struct eth_header *eth_header;
    struct ip6_hdr *hdr;
    struct icmp6_hdr *icmphdr;
    size_t len;

    PREPARE_UT(pkt16574);
    parser = &g_parser;

    /* Validate parsing success */
    len = net_header_parse(parser);
    TEST_ASSERT_TRUE(len != 0);

    LOGI("%s: %s", __func__,
         net_header_fill_info_buf(log_buf, NET_HDR_BUFF_SIZE, parser));

    /* validate vlan id */
    eth_header = &parser->eth_header;
    TEST_ASSERT_EQUAL_UINT(4, eth_header->vlan_id);

    /* Validate hopping through ipv6 extension headers */
    hdr = net_header_get_ipv6_hdr(parser);
    TEST_ASSERT_NOT_NULL(hdr);
    TEST_ASSERT_EQUAL_INT(IPPROTO_ICMPV6, parser->ip_protocol);

    /* Validate the icmpv6 request */
    icmphdr = (struct icmp6_hdr *)(parser->data);
    len = sizeof(*icmphdr);
    TEST_ASSERT_EQUAL_UINT(parser->packet_len, parser->parsed + len);
    TEST_ASSERT_EQUAL_UINT(ICMP6_ECHO_REQUEST, icmphdr->icmp6_type);
}

/**
 * @brief Parsing pcap.c's pkt16608
 *
 * pkt16608 is a TCP/IPv4 packet over vlan4.
 * It carries the data "Bonjour"
 * See pcap.json for details.
 */
void test_tcp_ipv4(void)
{
    struct net_header_parser *parser;
    struct eth_header *eth_header;
    struct iphdr *hdr;
    size_t len;
    char *expected_data = "Bonjour\n";
    char data[strlen(expected_data) + 1];

    PREPARE_UT(pkt16608);
    parser = &g_parser;

    /* Validate parsing success */
    len = net_header_parse(parser);
    TEST_ASSERT_TRUE(len != 0);

    LOGI("%s: %s", __func__,
         net_header_fill_info_buf(log_buf, NET_HDR_BUFF_SIZE, parser));

    /* validate vlan id */
    eth_header = &parser->eth_header;
    TEST_ASSERT_EQUAL_UINT(4, eth_header->vlan_id);

    hdr = net_header_get_ipv4_hdr(parser);
    TEST_ASSERT_NOT_NULL(hdr);
    TEST_ASSERT_EQUAL_INT(IPPROTO_TCP, parser->ip_protocol);

    memset(data, 0, sizeof(data));
    memcpy(data, parser->data, strlen(expected_data));
    TEST_ASSERT_EQUAL_STRING(expected_data, data);
}


/**
 * @brief Parsing pcap.c's pkt1200
 *
 * pkt1200 is a TCP/IPv6 packet over vlan4.
 * It carries the data "Bonjour"
 * See pcap.json for details.
 */
void test_tcp_ipv6(void)
{
    struct net_header_parser *parser;
    struct eth_header *eth_header;
    struct ip6_hdr *hdr;
    size_t len;
    char *expected_data = "Bonjour\n";
    char data[strlen(expected_data) + 1];

    parser = &g_parser;
    PREPARE_UT(pkt1200);

    /* Validate parsing success */
    len = net_header_parse(parser);
    TEST_ASSERT_TRUE(len != 0);

    LOGI("%s: %s", __func__,
         net_header_fill_info_buf(log_buf, NET_HDR_BUFF_SIZE, parser));

    /* validate vlan id */
    eth_header = &parser->eth_header;
    TEST_ASSERT_EQUAL_UINT(4, eth_header->vlan_id);

    hdr = net_header_get_ipv6_hdr(parser);
    TEST_ASSERT_NOT_NULL(hdr);
    TEST_ASSERT_EQUAL_INT(IPPROTO_TCP, parser->ip_protocol);

    memset(data, 0, sizeof(data));
    memcpy(data, parser->data, strlen(expected_data));
    TEST_ASSERT_EQUAL_STRING(expected_data, data);
}


/**
 * @brief Parsing pcap.c's pkt9568
 *
 * pkt9568 is a UDP/IPv4 packet over vlan4.
 * It carries the data "Bonjour"
 * See pcap.json for details.
 */
void test_udp_ipv4(void)
{
    struct net_header_parser *parser;
    struct eth_header *eth_header;
    struct iphdr *hdr;
    size_t len;
    char *expected_data = "Bonjour\n";
    char data[strlen(expected_data) + 1];

    parser = &g_parser;
    PREPARE_UT(pkt9568);

    /* Validate parsing success */
    len = net_header_parse(parser);
    TEST_ASSERT_TRUE(len != 0);

    LOGI("%s: %s", __func__,
         net_header_fill_info_buf(log_buf, NET_HDR_BUFF_SIZE, parser));

    /* validate vlan id */
    eth_header = &parser->eth_header;
    TEST_ASSERT_EQUAL_UINT(4, eth_header->vlan_id);

    hdr = net_header_get_ipv4_hdr(parser);
    TEST_ASSERT_NOT_NULL(hdr);
    TEST_ASSERT_EQUAL_INT(IPPROTO_UDP, parser->ip_protocol);

    memset(data, 0, sizeof(data));
    memcpy(data, parser->data, strlen(expected_data));
    TEST_ASSERT_EQUAL_STRING(expected_data, data);
}


/**
 * @brief Parsing pcap.c's pkt12176
 *
 * pkt12176 is a ARP request over vlan4.
 * See pcap.json for details.
 */
void test_arp_request(void)
{
    struct net_header_parser *parser;
    struct eth_header *eth_header;
    struct arphdr *hdr;
    int ethertype;
    size_t len;

    parser = &g_parser;
    PREPARE_UT(pkt12176);

    /* Validate parsing success */
    len = net_header_parse(parser);
    TEST_ASSERT_TRUE(len != 0);

    LOGI("%s: %s", __func__,
         net_header_fill_info_buf(log_buf, NET_HDR_BUFF_SIZE, parser));

    /* validate vlan id */
    eth_header = &parser->eth_header;
    TEST_ASSERT_EQUAL_UINT(4, eth_header->vlan_id);

    ethertype = net_header_get_ethertype(parser);
    TEST_ASSERT_EQUAL_INT(ETH_P_ARP, ethertype);
    hdr = (struct arphdr *)(parser->eth_pld.payload);
    TEST_ASSERT_EQUAL_UINT(ARPOP_REQUEST, ntohs(hdr->ar_op));
}

/**
 * @brief Parsing pcap.c's pkt244
 *
 * pkt244 is a ICMP4 request
 * See pcap.json for details.
 */
void test_icmp4_request(void)
{
    struct net_header_parser *parser;
    size_t len;
    struct iphdr *ip_hdr;
    char *src_ip = "10.2.22.238";
    char *dst_ip = "69.147.88.8";
    struct in_addr src_addr;
    struct in_addr dst_addr;
    struct icmphdr *icmp_hdr;

    parser = &g_parser;
    PREPARE_UT(pkt244);

    /* Validate parsing success */
    len = net_header_parse(parser);
    TEST_ASSERT_TRUE(len != 0);

    LOGI("%s: %s", __func__,
         net_header_fill_info_buf(log_buf, NET_HDR_BUFF_SIZE, parser));

    /* validate ip hdr */
    ip_hdr = net_header_get_ipv4_hdr(parser);
    TEST_ASSERT_NOT_NULL(ip_hdr);
    TEST_ASSERT_EQUAL_INT(IPPROTO_ICMP, parser->ip_protocol);
    inet_pton(AF_INET, src_ip, &src_addr);
    inet_pton(AF_INET, dst_ip, &dst_addr);
    LOGD("src addr exp: %d act: %d", src_addr.s_addr, ip_hdr->saddr);
    TEST_ASSERT_EQUAL_UINT32(src_addr.s_addr, ip_hdr->saddr);
    LOGD("dst addr exp: %d act: %d", dst_addr.s_addr, ip_hdr->daddr);
    TEST_ASSERT_EQUAL_UINT32(dst_addr.s_addr, ip_hdr->daddr);

    /* icmp hdr validation */
    TEST_ASSERT_NOT_NULL(parser->eth_pld.payload);
    icmp_hdr = (struct icmphdr *)(parser->data);
    TEST_ASSERT_EQUAL_UINT8(8, icmp_hdr->type);
    TEST_ASSERT_EQUAL_UINT8(0, icmp_hdr->code);
    TEST_ASSERT_EQUAL_UINT16(3320, icmp_hdr->un.echo.id);
}

/**
 * @brief Parsing pcap.c's pkt245
 *
 * pkt245 is a ICMP4 reply.
 * See pcap.json for details.
 */
void test_icmp4_reply(void)
{
    struct net_header_parser *parser;
    size_t len;
    struct iphdr *ip_hdr;
    char *src_ip = "69.147.88.8";
    char *dst_ip = "10.2.22.238";
    struct in_addr src_addr;
    struct in_addr dst_addr;
    struct icmphdr *icmp_hdr;

    parser = &g_parser;
    PREPARE_UT(pkt245);

    /* Validate parsing success */
    len = net_header_parse(parser);
    TEST_ASSERT_TRUE(len != 0);

    LOGI("%s: %s", __func__,
         net_header_fill_info_buf(log_buf, NET_HDR_BUFF_SIZE, parser));

    /* validate ip hdr */
    ip_hdr = net_header_get_ipv4_hdr(parser);
    TEST_ASSERT_NOT_NULL(ip_hdr);
    TEST_ASSERT_EQUAL_INT(IPPROTO_ICMP, parser->ip_protocol);
    inet_pton(AF_INET, src_ip, &src_addr);
    inet_pton(AF_INET, dst_ip, &dst_addr);
    TEST_ASSERT_EQUAL_UINT32(src_addr.s_addr, ip_hdr->saddr);
    TEST_ASSERT_EQUAL_UINT32(dst_addr.s_addr, ip_hdr->daddr);

    /* icmp hdr validation */
    TEST_ASSERT_NOT_NULL(parser->eth_pld.payload);
    icmp_hdr = (struct icmphdr *)(parser->data);
    TEST_ASSERT_EQUAL_UINT8(0, icmp_hdr->type);
    TEST_ASSERT_EQUAL_UINT8(0, icmp_hdr->code);
}


int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    target_log_open("TEST", LOG_OPEN_STDOUT);
    log_severity_set(LOG_SEVERITY_INFO);

    UnityBegin(test_name);
    RUN_TEST(test_tcp_ipv4);
    RUN_TEST(test_tcp_ipv6);
    RUN_TEST(test_udp_ipv4);
    RUN_TEST(test_arp_request);
    RUN_TEST(test_icmpv6_pcap_parse);
    RUN_TEST(test_icmp4_request);
    RUN_TEST(test_icmp4_reply);
    return UNITY_END();
}
