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
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "network.h"
#include "log.h"

/*
 * Convert an ip struct into a str. Like NTOA, this uses a single
 * buffer, so freeing it need not be freed, but it can only be used once
 * per statement.
 */
static char IP_STR_BUFF[INET6_ADDRSTRLEN];

/* Parse the ethernet headers, and return the payload position (0 on error). */
uint32_t
eth_parse(struct pcap_pkthdr *header, uint8_t *packet,
          eth_info * eth, eth_config * conf)
{
    uint32_t pos = 0;

    if (header->len < 14)
    {
        LOGD("Truncated Packet(eth)\n");
        return 0;
    }

    while (pos < 6)
    {
        eth->dstmac.addr[pos] = packet[pos];
        eth->srcmac.addr[pos] = packet[pos+6];
        pos++;
    }
    pos = pos + 6;

    /* Skip the extra 2 byte field inserted in "Linux Cooked" captures. */
    if (conf->datalink == DLT_LINUX_SLL) {
        pos = pos + 2;
    }

    /* Skip VLAN tagging */
    if (packet[pos] == 0x81 && packet[pos+1] == 0) pos = pos + 4;

    eth->ethtype = (packet[pos] << 8) + packet[pos+1];
    pos = pos + 2;

    return pos;
}


/*
 * Parse the IPv4 header. May point p_packet to a new packet data array,
 * which means zero is a valid return value. Sets p_packet to NULL on error.
 * See RFC791
 */
uint32_t
ipv4_parse(uint32_t pos, struct pcap_pkthdr *header,
           uint8_t ** p_packet, ip_info * ip, ip_config * conf)
{

    uint32_t h_len;
    ip_fragment * frag = NULL;
    uint8_t frag_mf;
    uint16_t frag_offset;

    /* For convenience and code consistency, dereference the packet **. */
    uint8_t * packet = *p_packet;

    if (header-> len - pos < 20)
    {
        LOGD("Truncated Packet(ipv4)");
        *p_packet = NULL;
        return 0;
    }

    ip->ip_header_pos = pos;
    h_len = packet[pos] & 0x0f;
    ip->length = (packet[pos+2] << 8) + packet[pos+3] - h_len*4;
    ip->proto = packet[pos+9];
    IPv4_MOVE(ip->src, packet + pos + 12);
    IPv4_MOVE(ip->dst, packet + pos + 16);

    /* Set if NOT the last fragment. */
    frag_mf = (packet[pos+6] & 0x20) >> 5;
    /* Offset for this data in the fragment. */
    frag_offset = ((packet[pos+6] & 0x1f) << 11) + (packet[pos+7] << 3);

    if (frag_mf == 1 || frag_offset != 0)
    {
        frag = malloc(sizeof(ip_fragment));
        if (frag == NULL)
        {
            *p_packet = NULL;
            return 0;
        }

        frag->start = frag_offset;
        /*
         * We don't try to deal with endianness here, since it
         * won't matter as long as we're consistent.
         */
        frag->islast = !frag_mf;
        frag->id = *((uint16_t *)(packet + pos + 4));
        frag->src = ip->src;
        frag->dst = ip->dst;
        frag->end = frag->start + ip->length;
        frag->data = malloc(sizeof(uint8_t) * ip->length);
        frag->next = frag->child = NULL;
        memcpy(frag->data, packet + pos + 4*h_len, ip->length);
        /*
         * Add the fragment to the list.
         * If this completed the packet, it is returned.
         */
        frag = ip_frag_add(frag, conf);
        if (frag != NULL)
        {
            /* Update the IP info on the reassembled data. */
            header->len = ip->length = frag->end - frag->start;
            *p_packet = frag->data;
            free(frag);

            return 0;
        }
        /* Signals that there is no more work to do on this packet. */
        *p_packet = NULL;
        return 0;
    }

    /* move the position up past the options section. */
    return pos + 4*h_len;

}

/*
 * Parse the IPv6 header. May point p_packet to a new packet data array,
 * which means zero is a valid return value. Sets p_packet to NULL on error.
 * See RFC2460
 */
uint32_t
ipv6_parse(uint32_t pos, struct pcap_pkthdr *header,
           uint8_t ** p_packet, ip_info * ip, ip_config * conf)
{

    /* For convenience and code consistency, dereference the packet **. */
    uint8_t * packet = *p_packet;

    /* In case the IP packet is a fragment. */
    ip_fragment * frag = NULL;
    uint32_t header_len = 0;

    if (header->len < (pos + 40))
    {
        LOGD("Truncated Packet(ipv6)");
        *p_packet = NULL;

        return 0;
    }

    ip->length = (packet[pos+4] << 8) + packet[pos+5];
    IPv6_MOVE(ip->src, packet + pos + 8);
    IPv6_MOVE(ip->dst, packet + pos + 24);

    /*
     * Jumbo grams will have a length of zero. We'll choose to ignore those,
     * and any other zero length packets.
     */
    if (ip->length == 0)
    {
        LOGD("Zero Length IP packet, possible Jumbo Payload");
        *p_packet=NULL; return 0;
    }

    uint8_t next_hdr = packet[pos+6];
    pos += 40;

    /*
     * We pretty much have no choice but to parse all extended sections,
     * since there is nothing to tell where the actual data is.
     */
    uint8_t done = 0;
    while (done == 0)
    {
        switch (next_hdr)
        {
            /*
             *Handle hop-by-hop, dest, and routing options.
             * Yay for consistent layouts.
             */
        case IPPROTO_HOPOPTS:
        case IPPROTO_DSTOPTS:
        case IPPROTO_ROUTING:
            if (header->len < (pos + 16))
            {
                LOGD("Truncated Packet(ipv6)");
                *p_packet = NULL; return 0;
            }
            next_hdr = packet[pos];
            /* The headers are 16 bytes longer. */
            header_len += 16;
            pos += packet[pos+1] + 1;
            break;
        case 51: /* Authentication Header. See RFC4302 */
            if (header->len < (pos + 2))
            {
                LOGD("Truncated Packet(ipv6)");
                *p_packet = NULL; return 0;
            }
            next_hdr = packet[pos];
            header_len += (packet[pos+1] + 2) * 4;
            pos += (packet[pos+1] + 2) * 4;
            if (header->len < pos)
            {
                LOGD("Truncated Packet(ipv6)");
                *p_packet = NULL; return 0;
            }
            break;
        case 50: /* ESP Protocol. See RFC4303. */
            /* We don't support ESP. */
            LOGD("Unsupported protocol: IPv6 ESP");
            if (frag != NULL) free(frag);
            *p_packet = NULL; return 0;
        case 135: /* IPv6 Mobility See RFC 6275 */
            if (header->len < (pos + 2))
            {
                LOGD("Truncated Packet(ipv6)");
                *p_packet = NULL; return 0;
            }
            next_hdr = packet[pos];
            header_len += packet[pos+1] * 8;
            pos += packet[pos+1] * 8;
            if (header->len < pos)
            {
                LOGD("Truncated Packet(ipv6)");
                *p_packet = NULL; return 0;
            }
            break;
        case IPPROTO_FRAGMENT:
            /* IP fragment. */
            next_hdr = packet[pos];
            frag = malloc(sizeof(ip_fragment));
            /* Get the offset of the data for this fragment. */
            frag->start = (packet[pos+2] << 8) + (packet[pos+3] & 0xf4);
            frag->islast = !(packet[pos+3] & 0x01);
            /*
             * We don't try to deal with endianness here, since it
             * won't matter as long as we're consistent.
             */
            frag->id = *(uint32_t *)(packet+pos+4);
            /* The headers are 8 bytes longer. */
            header_len += 8;
            pos += 8;
            break;
        case TCP:
        case UDP:
            done = 1;
            break;
        default:
            LOGD("Unsupported IPv6 proto(%u)", next_hdr);
            *p_packet = NULL; return 0;
        }
    }

    /* check for int overflow */
    if (header_len > ip->length)
    {
        LOGD("Malformed packet(ipv6)");
      *p_packet = NULL;

      return 0;
    }

    ip->proto = next_hdr;
    ip->length = ip->length - header_len;

    /* Handle fragments. */
    if (frag != NULL)
    {
        frag->src = ip->src;
        frag->dst = ip->dst;
        frag->end = frag->start + ip->length;
        frag->next = frag->child = NULL;
        frag->data = malloc(sizeof(uint8_t) * ip->length);
        memcpy(frag->data, packet+pos, ip->length);
        /*
         * Add the fragment to the list.
         * If this completed the packet, it is returned.
         */
        frag = ip_frag_add(frag, conf);
        if (frag != NULL)
        {
            header->len = ip->length = frag->end - frag->start;
            *p_packet = frag->data;
            free(frag);

            return 0;
        }
        /* Signals that there is no more work to do on this packet. */
        *p_packet = NULL;

        return 0;
    }
    else
    {
        return pos;
    }

}

/*
 * Add this ip fragment to the our list of fragments. If we complete
 * a fragmented packet, return it.
 * Limitations - Duplicate packets may end up in the list of fragments.
 *             - We aren't going to expire fragments, and we aren't going
 *                to save/load them like with TCP streams either. This may
 *                mean lost data.
 */
ip_fragment *
ip_frag_add(ip_fragment * this, ip_config * conf)
{
    ip_fragment ** curr = &(conf->ip_fragment_head);
    ip_fragment ** found = NULL;

    /* Find the matching fragment list. */
    while (*curr != NULL)
    {
        if ((*curr)->id == this->id &&
            IP_CMP((*curr)->src, this->src) &&
            IP_CMP((*curr)->dst, this->dst))
        {
            found = curr;
            break;
        }
        curr = &(*curr)->next;
    }

    /*
     * At this point curr will be the head of our matched chain of fragments,
     * and found will be the same. We'll use found as our pointer into this
     * chain, and curr to remember where it starts.
     * 'found' could also be NULL, meaning no match was found.
     */

    /* If there wasn't a matching list, then we're done. */
    if (found == NULL)
    {
        this->next = conf->ip_fragment_head;
        conf->ip_fragment_head = this;
        return NULL;
    }

    while (*found != NULL)
    {
        if ((*found)->start >= this->end)
        {
            /* It goes before, so put it there. */
            this->child = *found;
            this->next = (*found)->next;
            *found = this;
            break;
        }
        else if ((*found)->child == NULL &&
                 (*found)->end <= this->start)
        {
            (*found)->child = this;
            break;
        }
        found = &((*found)->child);
    }

    /*
     * We found no place for the fragment, which means it's a duplicate
     * (or the chain is screwed up...)
     */
    if (*found == NULL)
    {
        free(this);

        return NULL;
    }

    /* Now we try to collapse the list. */
    found = curr;
    while ((*found != NULL) && (*found)->child != NULL)
    {
        ip_fragment * child = (*found)->child;
        if ((*found)->end == child->start)
        {
            uint32_t child_len = child->end - child->start;
            uint32_t fnd_len = (*found)->end - (*found)->start;
            uint8_t * buff = malloc(sizeof(uint8_t) * (fnd_len + child_len));
            memcpy(buff, (*found)->data, fnd_len);
            memcpy(buff + fnd_len, child->data, child_len);
            (*found)->end = (*found)->end + child_len;
            (*found)->islast = child->islast;
            (*found)->child = child->child;
            /*
             * Free the old data and the child, and make the combined buffer
             * the new data for the merged fragment.
             */
            free((*found)->data);
            free(child->data);
            free(child);
            (*found)->data = buff;
        }
        else
        {
            found = &(*found)->child;
        }
    }

    /*
     * Check to see if we completely collapsed it.
     * *curr is the pointer to the first fragment.
     */
    if ((*curr)->islast != 0)
    {
        ip_fragment * ret = *curr;
        /* Remove this from the fragment list. */
        *curr = (*curr)->next;
        return ret;
    }

    /* This is what happens when we don't complete a packet. */
    return NULL;
}

/* Free the lists of IP fragments. */
void
ip_frag_free(ip_config *conf)
{
    ip_fragment * curr;
    ip_fragment * child;

    while (conf->ip_fragment_head != NULL)
    {
        curr = conf->ip_fragment_head;
        conf->ip_fragment_head = curr->next;
        while (curr != NULL)
        {
            child = curr->child;
            free(curr->data);
            free(curr);
            curr = child;
        }
    }
}


/* Parse the udp headers. */
uint32_t
udp_parse(uint32_t pos, struct pcap_pkthdr *header,
          uint8_t *packet, transport_info *udp)
{
    if (header->len - pos < 8)
    {
        LOGD("Truncated Packet(udp)");
        return 0;
    }

    udp->tpt_header_pos = pos;
    udp->srcport = (packet[pos] << 8) + packet[pos+1];
    udp->dstport = (packet[pos+2] << 8) + packet[pos+3];
    udp->length = (packet[pos+4] << 8) + packet[pos+5];
    udp->udp_checksum = (packet[pos+6] << 8) + packet[pos+7];
    udp->udp_csum_ptr = &packet[pos + 6];
    udp->transport = UDP;

    return pos + 8;
}


/*
 * Convert an ip struct to a string. The returned buffer is internal,
 * and need not be freed.
 */
char *
iptostr(ip_addr * ip)
{
    if (ip->vers == IPv4)
    {
        inet_ntop(AF_INET, (const void *) &(ip->addr.v4),
                  IP_STR_BUFF, INET6_ADDRSTRLEN);
    }
    else
    { /* IPv6 */
        inet_ntop(AF_INET6, (const void *) &(ip->addr.v6),
                  IP_STR_BUFF, INET6_ADDRSTRLEN);
    }
    return IP_STR_BUFF;
}


static uint32_t
compute_pseudo_iph_checksum(uint8_t *packet, ip_info *ip,
                            transport_info *udp)
{
    uint32_t sum = 0;
    uint16_t udp_len = udp->length;
    uint8_t ip_vers = ip->src.vers;

    if (ip_vers == IPv4)
    {
        uint32_t ip_addr = ip->src.addr.v4.s_addr;
        sum += (ip_addr >> 16) & 0xffff;
        sum += ip_addr & 0xffff;

        ip_addr = ip->dst.addr.v4.s_addr;
        sum += (ip_addr >> 16) & 0xffff;
        sum += ip_addr & 0xffff;
    }
    else
    {   /*  IPv6 */
        uint16_t *ip_addr = ip->src.addr.v6.s6_addr16;
        int i;

        for (i = 0; i < 8; i++)
        {
            sum += *ip_addr++;
        }

        ip_addr = ip->dst.addr.v6.s6_addr16;
        for (i = 0; i < 8; i++)
        {
            sum += *ip_addr++;
        }
    }
    sum += htons(IPPROTO_UDP);
    sum += htons(udp_len);

    memset(udp->udp_csum_ptr, 0, 2);
    return sum;
}

uint16_t
compute_udp_checksum(uint8_t *packet, ip_info *ip, transport_info *udp)
{
    uint32_t sum = 0;
    uint16_t *udphdr = (uint16_t *)(packet + udp->tpt_header_pos);
    uint16_t udp_len = udp->length;
    uint16_t csum = 0;

    sum = compute_pseudo_iph_checksum(packet, ip, udp);
    while (udp_len > 1)
    {
        sum += *udphdr++;
        udp_len -= 2;
    }

    if(udp_len > 0)
    {
        sum += *udphdr & htons(0xff00);
    }

    while (sum >> 16)
    {
        sum = (sum & 0xffff) + (sum >> 16);
    }
    csum = sum;
    csum = ~csum;

    return (csum == 0 ? 0xffff : csum);
}
