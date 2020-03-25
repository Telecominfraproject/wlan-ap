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

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <strings.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/un.h>

#define LOG_MODULE_ID  LOG_MODULE_ID_OSA

#include "os.h"
#include "os_socket.h"
#include "log.h"


#define SORTED_ARRAY_INSERT(a, n, val)   \
{                                        \
    int32_t indx = n;                    \
    while (indx > 0)                     \
    {                                    \
        if (val > a[indx-1])             \
            break;                       \
        a[indx] = a[indx-1];             \
        indx--;                          \
    }                                    \
    a[indx] = val;                       \
}


#define SORTED_ARRAY_DELETE(a, n, val)   \
{                                        \
    int32_t indx;                        \
    for (indx=0; indx < n; indx++)       \
    {                                    \
        if (val == a[indx])              \
            break;                       \
    }                                    \
    for (; indx < n-1; indx++)           \
    {                                    \
        a[indx] = a[indx+1];             \
    }                                    \
}

int32_t server_socket_create(os_sock_type sock_type,
                             char *listen_addr,
                             uint32_t server_port)
{
    int32_t       sock_fd;
    struct sockaddr_in server_addr;

    if (sock_type == OS_SOCK_TYPE_UDP)
    {
        sock_fd = socket(AF_INET, SOCK_DGRAM, 0);
    }
    else
    {
        sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    }
    if (sock_fd == -1)
    {
        LOG(ERR, "Server socket creation failed::error=%s", strerror(errno));
        return -1;
    }

    bzero((char *)&server_addr, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port  = htons(server_port);
    if (inet_pton(AF_INET, listen_addr, &server_addr.sin_addr.s_addr) != 1)
    {
        LOG(ERR, "Server socket inet_pton() failed::error=%s", strerror(errno));
        return -1;
    }

    /*
     * SO_REUSEADDR must be set before binding the socket
     */
    int val = 1;
    if (setsockopt(sock_fd, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val)) < 0)
    {
        LOG(ERR, "Unable to set reuse-addr-flag::error=%s", strerror(errno));
        close(sock_fd);
        return -1;
    }


    if (bind(sock_fd, (const struct sockaddr *)&server_addr, sizeof(server_addr)) < 0 )
    {
        LOG(ERR, "Server socket binding failed::error=%s", strerror(errno));
        close(sock_fd);
        return -1;
    }

    return sock_fd;
}


int32_t client_socket_create(os_sock_type sock_type)
{
    int32_t sock_fd;

    if ( sock_type == OS_SOCK_TYPE_UDP)
    {
        sock_fd = socket(AF_INET, SOCK_DGRAM, 0);
    }
    else
    {
        sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    }
    if (sock_fd == -1)
    {
        LOG(ERR, "Client socket creation failed::error=%s", strerror(errno));
        return -1;
    }
    return sock_fd;
}

bool client_connect(int32_t sock_fd,
                                char *server_ip,
                                uint32_t port)
{
    struct sockaddr_in server_addr;

    bzero((char *)&server_addr, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port  = htons(port);
    if (inet_pton(AF_INET, server_ip, &server_addr.sin_addr) <= 0)
    {
        LOG(ERR, "Client: inet_pton() failed::error=%s", strerror(errno));
        return false;
    }

    if (connect(sock_fd, (struct sockaddr *)&server_addr,
                sizeof(server_addr)) < 0)
    {
        LOG(ERR, "Client connection failed::address=%s|port=%d|error=%s",
                      server_ip,
                      port,
                      strerror(errno));

        return false;
    }

    LOG(DEBUG, "Client connected::address=%s|port=%d", server_ip, port);

    return true;
}

int32_t tcp_server_listen(int32_t sock_fd)
{
#define MAX_NUM_CONNECTIONS 32
#define MAX_SEND_RECV_BUFFER_SIZE 1024
    int32_t       status;

    if (-1 == fcntl(sock_fd, F_SETFL, (fcntl(sock_fd, F_GETFL)) | O_NONBLOCK))
    {
        LOG(ERR, "Unable to set socket to non-blocking::error=%s", strerror(errno));
        status = -1;
        goto end;
    }

    if (-1 == fcntl(sock_fd, F_SETFD, fcntl(sock_fd, F_GETFD) | FD_CLOEXEC))
    {
        LOG(ERR, "Unable to set close-on-execflag::error=%s", strerror(errno));
        status = -1;
        goto end;
    }

    if ((status = listen(sock_fd, MAX_NUM_CONNECTIONS)) == -1)
    {
        LOG(ERR, "Server failed while listening to socket::socket=%s", strerror(errno));
        goto end;
    }

end:
    return status;
}

/**
 * Enable TCP keep-alive on a socket
 */
bool socket_set_keepalive(int fd)
{
    int opt;
    bool success = true;

    opt = 60;
    if (setsockopt(fd, IPPROTO_TCP, TCP_KEEPIDLE, &opt, sizeof(opt)) != 0)
    {
        LOG(WARNING, "Error setting TCP_KEEPIDLE::socket=%d", fd);
        success = false;
    }

    opt = 10;
    if (setsockopt(fd, IPPROTO_TCP, TCP_KEEPINTVL, &opt, sizeof(opt)) != 0)
    {
        LOG(WARNING, "Error setting TCP_KEEPINTVL::socket=%d", fd);
        success = false;
    }

    opt = 12;
    if (setsockopt(fd, IPPROTO_TCP, TCP_KEEPCNT, &opt, sizeof(opt)) != 0)
    {
        LOG(WARNING, "Error setting TCP_KEEPCNT::socket=%d", fd);
        success = false;
    }

    opt = 1;
    if (setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &opt, sizeof(opt)) != 0)
    {
        LOG(WARNING, "Error setting SO_KEEPALIVE::socket=%d", fd);
        success = false;
    }

    return success;
}


int ovsdb_conn()
{
    struct sockaddr_un peer_addr;
    int sock_fd;
    int success;
    socklen_t addrlen;

    sock_fd = socket(AF_UNIX, SOCK_STREAM, 0);

    if (sock_fd < 0)
    {
        LOG(ERR,"OVSDB connect, opening socket::error=%s", strerror(errno));
        return sock_fd;
    }

    memset(&peer_addr, 0, sizeof(peer_addr));

    peer_addr.sun_family = AF_UNIX;
    char *sock_path = getenv(ENV_OVSDB_SOCK_PATH);
    if (!sock_path || !*sock_path) sock_path = OVSDB_SOCK_PATH;
    strcpy(peer_addr.sun_path, sock_path);
    addrlen = strlen(peer_addr.sun_path) + sizeof(peer_addr.sun_family);

    success = connect(sock_fd, (const struct sockaddr *)&peer_addr, addrlen);

    if (0 != success)
    {
        LOG(ERR, "OVSDB connect, connection error::error=%s",strerror(errno));
        /* don't forge to close opened socket */
        close(sock_fd);
        return success;
    }

    return sock_fd;

}


bool ovsdb_disconn(int sock_fd)
{
    close(sock_fd);
    return true;
}
