/*
 * Copyright (c) 2026 Avnet, Inc.
 * SPDX-License-Identifier: MIT
 *
 * getaddrinfo() shim over FreeRTOS+TCP DNS (IPv4 only).
 */
#ifndef KVS_LWIP_SHIM_NETDB_H
#define KVS_LWIP_SHIM_NETDB_H

#include "lwip/sockets.h"

#ifdef __cplusplus
extern "C" {
#endif

#define AI_PASSIVE     0x01
#define AI_CANONNAME   0x02
#define AI_NUMERICHOST 0x04

#define EAI_FAIL     4
#define EAI_MEMORY   6
#define EAI_NONAME   8
#define EAI_FAMILY   5
#define EAI_SERVICE  9

struct addrinfo
{
    int ai_flags;
    int ai_family;
    int ai_socktype;
    int ai_protocol;
    socklen_t ai_addrlen;
    struct sockaddr *ai_addr;
    char *ai_canonname;
    struct addrinfo *ai_next;
};

int getaddrinfo(const char *nodename, const char *servname,
                const struct addrinfo *hints, struct addrinfo **res);
void freeaddrinfo(struct addrinfo *ai);
const char *gai_strerror(int errcode);

#ifdef __cplusplus
}
#endif

#endif /* KVS_LWIP_SHIM_NETDB_H */
