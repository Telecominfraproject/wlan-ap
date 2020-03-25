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
#include <resolv.h>
#include <netdb.h>

#include "log.h"
#include "cm2.h"

void cm2_free_addr_list(cm2_addr_t *addr)
{
    if (addr->ai_list) freeaddrinfo(addr->ai_list);
    addr->ai_list = NULL;
    addr->ai_curr = NULL;
}

int cm2_getaddrinfo(char *hostname, struct addrinfo **res, char *msg)
{
    struct addrinfo hints;
    int ret;

    // force reload resolv.conf
    res_init();

    // hints
    memset(&hints, 0, sizeof(hints));
    hints.ai_family   = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM; // Otherwise addresses are duplicated

    // resolve
    ret = getaddrinfo(hostname, NULL, &hints, res);
    if (ret != 0)
    {
        LOGE("%s DNS lookup failed (%s)", msg, gai_strerror(ret));
    }
    return ret;
}

bool cm2_resolve(cm2_dest_e dest)
{
    cm2_addr_t *addr = cm2_get_addr(dest);
    char *dstr = cm2_dest_name(dest);
    int ret;

    addr->updated = false;
    addr->resolved = false;

    if (!addr->valid)
    {
        return false;
    }

    LOGI("sync resolving %s '%s'", dstr, addr->resource);

    cm2_free_addr_list(addr);

    struct addrinfo *ai, *found = NULL;

    // resolve
    ret = cm2_getaddrinfo(addr->hostname, &addr->ai_list, dstr);
    if (ret != 0)
    {
        addr->ai_list = NULL;
        return false;
    }

    char buf[2048] = "", *bp = buf;
    size_t bsize = sizeof(buf);
    ai = addr->ai_list;
    while (ai)
    {
        if (ai->ai_addr->sa_family == AF_INET)
        {
            char buffer[INET_ADDRSTRLEN] = "";
            struct sockaddr_in   *sain;
            sain = (struct sockaddr_in *)ai->ai_addr;
            const char* result = inet_ntop(AF_INET, &sain->sin_addr, buffer, sizeof(buffer));
            if (result)
            {
                if (!found) found = ai;
                append_snprintf(&bp, &bsize, "%s ", buffer);
            }
        }
        else if (ai->ai_addr->sa_family == AF_INET6)
        {
            char buffer[INET6_ADDRSTRLEN] = "";
            struct sockaddr_in6  *sain6;
            sain6 = (struct sockaddr_in6 *)ai->ai_addr;
            const char* result = inet_ntop(AF_INET6, &sain6->sin6_addr, buffer, sizeof(buffer));
            if (result)
            {
                if(!found) found = ai;
                append_snprintf(&bp, &bsize, "[%s] ", buffer);
            }
        }
        ai = ai->ai_next;
    }
    if (!found)
    {
        LOGE("DNS did not return any usable addresses");
        return false;
    }
    LOGI("resolved %s '%s': %s", dstr, addr->hostname, buf);
    addr->resolved = true;
    addr->ai_curr = NULL;
    return true;
}

bool cm2_resolve_handle_process(void)
{
    return true;
}

struct addrinfo* cm2_get_next_addrinfo(cm2_addr_t *addr)
{
    struct addrinfo *ai;
    if (addr->ai_curr)
    {
        ai = addr->ai_curr->ai_next;
    }
    else
    {
        ai = addr->ai_list;
    }
    while (ai)
    {
        if ((ai->ai_addr->sa_family == AF_INET) || (ai->ai_addr->sa_family == AF_INET6)) break;
        ai = ai->ai_next;
    }
    if (!ai)
    {
        LOGE("No more addresses left");
        return NULL;
    }
    addr->ai_curr = ai;
    return ai;
}

bool cm2_write_target_addr(cm2_addr_t *addr, struct addrinfo *ai)
{
    if (!ai) return false;
    char target[256];

    if (ai->ai_addr->sa_family == AF_INET)
    {
        struct sockaddr_in  *sain = (struct sockaddr_in *)ai->ai_addr;
        char buffer[INET_ADDRSTRLEN] = "";
        const char* result = inet_ntop(AF_INET, &sain->sin_addr, buffer, sizeof(buffer));
        if (result == 0)
            return false;

        snprintf(target, sizeof(target), "%s:%s:%d",
                addr->proto,
                buffer,
                addr->port);
    }
    else if (ai->ai_addr->sa_family == AF_INET6)
    {
        struct sockaddr_in6  *sain6 = (struct sockaddr_in6 *)ai->ai_addr;
        char buffer[INET6_ADDRSTRLEN] = "";
        const char* result = inet_ntop(AF_INET6, &sain6->sin6_addr, buffer, sizeof(buffer));
        if (result == 0)
            return false;

        snprintf(target, sizeof(target), "%s:[%s]:%d",
                 addr->proto,
                 buffer,
                 addr->port);
    }
    else
        return false;

    bool ret = cm2_ovsdb_set_Manager_target(target);
    if (ret)
    {
        addr->ai_curr = ai;
        LOGI("Trying to connect to: %s : %s", cm2_curr_dest_name(), target);
    }
    return ret;
}

bool cm2_write_current_target_addr(void)
{
    cm2_addr_t *addr = cm2_curr_addr();
    struct addrinfo *ai = addr->ai_curr;

    if (!ai)
        ai = cm2_get_next_addrinfo(addr);

    return cm2_write_target_addr(addr, ai);
}

bool cm2_write_next_target_addr(void)
{
    cm2_addr_t *addr = cm2_curr_addr();
    struct addrinfo *ai = cm2_get_next_addrinfo(addr);
    return cm2_write_target_addr(addr, ai);
}
