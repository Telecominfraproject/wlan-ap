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

#ifndef __OS_SOCKET__H__
#define __OS_SOCKET__H__

#include <stdbool.h>

#include "target.h"

#define SOCKET_ADDR_ANY        "0.0.0.0"
#define SOCKET_ADDR_LOCALHOST  "127.0.0.1"
#define OVSDB_SOCK_PATH        TARGET_OVSDB_SOCK_PATH
#define ENV_OVSDB_SOCK_PATH    "PLUME_OVSDB_SOCK_PATH"

typedef enum
{
    OS_SOCK_TYPE_UDP,
    OS_SOCK_TYPE_TCP
}
os_sock_type;

extern bool socket_set_keepalive(int fd);

/*
 * Socket related definitions
 */
typedef bool socket_cbk_t(int fd,
                          char *msg,
                          size_t msgsz,
                          void *ctx);

/**
 * Socket Related definitions
 */
int32_t server_socket_create(os_sock_type sock_type,
                             char *listen_addr,
                             uint32_t server_port);

int32_t client_socket_create(os_sock_type sock_type);

bool client_connect(int32_t sock_fd,
                    char *server_addr,
                    uint32_t port);

int32_t tcp_server_listen(int32_t sock_fd);

/* open ovsdb server socket */
int ovsdb_conn();
bool ovsdb_disconn(int sock_fd);

#endif  /* #define __OS_SOCKET__H__ */
