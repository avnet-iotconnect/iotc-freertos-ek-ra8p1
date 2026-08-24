/*
 * Copyright (c) 2026 Avnet, Inc.
 * SPDX-License-Identifier: MIT
 *
 * BSD/lwIP-compatible socket API shim over FreeRTOS+TCP for the vendored
 * KVS WebRTC stack (which was written against lwIP sockets). IPv4 only;
 * AF_INET6 socket creation fails cleanly. Implementation: kvs_lwip_shim.c.
 */
#ifndef KVS_LWIP_SHIM_SOCKETS_H
#define KVS_LWIP_SHIM_SOCKETS_H

#include <stdint.h>
#include <stddef.h>
#include <errno.h>
#include <sys/select.h>
#include <sys/time.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---- address families / socket types ------------------------------------ */
#define AF_UNSPEC       0
#define AF_INET         2
#define AF_INET6        10
#define PF_INET         AF_INET
#define PF_INET6        AF_INET6

#define SOCK_STREAM     1
#define SOCK_DGRAM      2

#define IPPROTO_IP      0
#define IPPROTO_TCP     6
#define IPPROTO_UDP     17

/* ---- setsockopt --------------------------------------------------------- */
#define SOL_SOCKET      0xfff
#define SO_REUSEADDR    0x0004
#define SO_KEEPALIVE    0x0008
#define SO_BROADCAST    0x0020
#define SO_SNDBUF       0x1001
#define SO_RCVBUF       0x1002
#define SO_SNDTIMEO     0x1005
#define SO_RCVTIMEO     0x1006
#define SO_ERROR        0x1007
#define TCP_NODELAY     0x01

/* ---- fcntl -------------------------------------------------------------- */
#ifndef O_NONBLOCK
#define O_NONBLOCK      0x4000
#endif
#ifndef F_GETFL
#define F_GETFL         3
#endif
#ifndef F_SETFL
#define F_SETFL         4
#endif

/* ---- misc constants ----------------------------------------------------- */
#define INADDR_ANY          ((uint32_t) 0x00000000UL)
#define IPADDR_ANY          ((uint32_t) 0x00000000UL)
#define INADDR_NONE         ((uint32_t) 0xFFFFFFFFUL)
#define INADDR_LOOPBACK     ((uint32_t) 0x7F000001UL)
#define INET_ADDRSTRLEN     16
#define INET6_ADDRSTRLEN    46
#define MSG_DONTWAIT        0x08
#define MSG_PEEK            0x01
#define SHUT_RDWR           2

#ifndef EWOULDBLOCK
#define EWOULDBLOCK EAGAIN
#endif
#ifndef EINPROGRESS
#define EINPROGRESS 119
#endif
#ifndef EAFNOSUPPORT
#define EAFNOSUPPORT 106
#endif

/* ---- types -------------------------------------------------------------- */
typedef uint32_t socklen_t;
typedef uint8_t  sa_family_t;
typedef uint16_t in_port_t;
typedef uint32_t in_addr_t;

struct in_addr
{
    in_addr_t s_addr;
};

struct in6_addr
{
    union
    {
        uint32_t u32_addr[4];
        uint8_t u8_addr[16];
    } un;
#define s6_addr un.u8_addr
};

struct sockaddr
{
    uint8_t sa_len;
    sa_family_t sa_family;
    char sa_data[14];
};

struct sockaddr_in
{
    uint8_t sin_len;
    sa_family_t sin_family;
    in_port_t sin_port;      /* network byte order */
    struct in_addr sin_addr; /* network byte order */
    char sin_zero[8];
};

/* Tell the vendored ICE controller not to re-define its own stub. */
#define ICE_CONTROLLER_SOCKADDR_IN6_STUB_DEFINED

struct sockaddr_in6
{
    uint8_t sin6_len;
    sa_family_t sin6_family;
    in_port_t sin6_port;
    uint32_t sin6_flowinfo;
    struct in6_addr sin6_addr;
    uint32_t sin6_scope_id;
};

struct sockaddr_storage
{
    uint8_t s2_len;
    sa_family_t ss_family;
    char s2_data1[2];
    uint32_t s2_data2[3];
    uint32_t s2_data3[3];
};

/* ---- byte order --------------------------------------------------------- */
uint16_t lwip_htons(uint16_t x);
uint32_t lwip_htonl(uint32_t x);
#ifndef htons
#define htons(x) lwip_htons(x)
#define ntohs(x) lwip_htons(x)
#define htonl(x) lwip_htonl(x)
#define ntohl(x) lwip_htonl(x)
#endif

/* fd_set / FD_* / select come from <sys/select.h> (picolibc) */
#define KVS_SHIM_MAX_SOCKETS 16

/* ---- API ---------------------------------------------------------------- */
int socket(int domain, int type, int protocol);
int bind(int s, const struct sockaddr *name, socklen_t namelen);
int connect(int s, const struct sockaddr *name, socklen_t namelen);
int close(int s);
int shutdown(int s, int how);
int setsockopt(int s, int level, int optname, const void *optval, socklen_t optlen);
int getsockopt(int s, int level, int optname, void *optval, socklen_t *optlen);
int getsockname(int s, struct sockaddr *name, socklen_t *namelen);
int getpeername(int s, struct sockaddr *name, socklen_t *namelen);
int fcntl(int s, int cmd, ...);
int select(int maxfdp1, fd_set *readset, fd_set *writeset, fd_set *exceptset,
           struct timeval *timeout);
int sendto(int s, const void *data, size_t size, int flags,
           const struct sockaddr *to, socklen_t tolen);
int recvfrom(int s, void *mem, size_t len, int flags,
             struct sockaddr *from, socklen_t *fromlen);
int send(int s, const void *data, size_t size, int flags);
int recv(int s, void *mem, size_t len, int flags);

/* lwIP-style aliases used in some sources */
#define lwip_socket socket
#define lwip_close close
#define lwip_select select
#define closesocket close

const char *inet_ntop(int af, const void *src, char *dst, socklen_t size);
int inet_pton(int af, const char *src, void *dst);
in_addr_t inet_addr(const char *cp);

#ifdef __cplusplus
}
#endif

#endif /* KVS_LWIP_SHIM_SOCKETS_H */
