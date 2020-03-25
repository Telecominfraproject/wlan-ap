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

#ifndef FSM_UTILS_H_INCLUDED
#define FSM_UTILS_H_INCLUDED

enum fsm_dpi_state
{
    FSM_DPI_CLEAR    = 0,
    FSM_DPI_INSPECT  = 1,
    FSM_DPI_PASSTHRU = 2,
    FSM_DPI_DROP     = 3,
};

/**
 * @brief FSM DPI APIs using 5 tuples
 */
int fsm_set_ip_dpi_state(
        void *ctx,
        void *src_ip,
        void *dst_ip,
        uint16_t src_port,
        uint16_t dst_port,
        uint8_t proto,
        uint16_t family,
        enum fsm_dpi_state state
);

int fsm_set_ip_dpi_state_timeout(
        void *ctx,
        void *src_ip,
        void *dst_ip,
        uint16_t src_port,
        uint16_t dst_port,
        uint8_t proto,
        uint16_t family,
        enum fsm_dpi_state state,
        uint32_t timeout
);

int fsm_set_icmp_dpi_state(
        void *ctx,
        void *src_ip,
        void *dst_ip,
        uint16_t id,
        uint8_t type,
        uint8_t code,
        uint16_t family,
        enum fsm_dpi_state state
);

int fsm_set_icmp_dpi_state_timeout(
        void *ctx,
        void *src_ip,
        void *dst_ip,
        uint16_t id,
        uint8_t type,
        uint8_t code,
        uint16_t family,
        enum fsm_dpi_state state,
        uint32_t timeout
);

/**
 * @brief FSM DPI APIs using pcap data
 */
int fsm_set_dpi_state(
        void *ctx,
        struct net_header_parser *net_hdr,
        enum fsm_dpi_state state
);

int fsm_set_dpi_state_timeout(
        void *ctx,
        struct net_header_parser *net_hdr,
        enum  fsm_dpi_state state,
        uint32_t timeout
);

#endif
