/*
 * Copyright (c) 2026 Avnet, Inc.
 * SPDX-License-Identifier: MIT
 *
 * BSD/lwIP socket shim over FreeRTOS+TCP for the KVS WebRTC stack.
 * Sockets are exposed as small integer fds backed by a fixed table of
 * FreeRTOS+TCP Socket_t handles. IPv4 only.
 */
#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "FreeRTOS_IP.h"
#include "FreeRTOS_Sockets.h"
#include "FreeRTOS_DNS.h"

#ifdef sin_addr
#undef sin_addr /* FreeRTOS+TCP back-compat macro poisons BSD structs */
#endif

#include "lwip/sockets.h"
#include "lwip/netdb.h"
#include "lwip_netconf.h"

#include <sys/time.h> /* struct timeval */

#define SHIM_FD_BASE 4 /* keep clear of stdio fds */

typedef struct
{
    Socket_t sock;
    bool in_use;
    bool nonblock;
    bool is_tcp;
} shim_slot_t;

static shim_slot_t s_slots[KVS_SHIM_MAX_SOCKETS];
static SemaphoreHandle_t s_lock;
/* Per-task select() socket sets. A single shared set deadlocks when two
 * tasks (the wslay websocket recv loop and the ICE socket listener) block
 * in FreeRTOS_select() concurrently: they clobber each other's FD bits and
 * both wait on the same event group. */
#define KVS_SHIM_MAX_SELECT_TASKS 6
typedef struct
{
    TaskHandle_t task;
    SocketSet_t set;
} shim_select_slot_t;
static shim_select_slot_t s_select_sets[KVS_SHIM_MAX_SELECT_TASKS];

static SocketSet_t prv_task_select_set(void)
{
    TaskHandle_t me = xTaskGetCurrentTaskHandle();

    xSemaphoreTake(s_lock, portMAX_DELAY);
    for (int i = 0; i < KVS_SHIM_MAX_SELECT_TASKS; i++)
    {
        if (s_select_sets[i].task == me)
        {
            xSemaphoreGive(s_lock);
            return s_select_sets[i].set;
        }
    }
    for (int i = 0; i < KVS_SHIM_MAX_SELECT_TASKS; i++)
    {
        if (s_select_sets[i].task == NULL)
        {
            SocketSet_t set = FreeRTOS_CreateSocketSet();
            if (set != NULL)
            {
                s_select_sets[i].task = me;
                s_select_sets[i].set = set;
            }
            xSemaphoreGive(s_lock);
            return set;
        }
    }
    xSemaphoreGive(s_lock);
    return NULL;
}

static void prv_init(void)
{
    static bool s_inited = false;
    if (!s_inited)
    {
        taskENTER_CRITICAL();
        if (!s_inited)
        {
            s_lock = xSemaphoreCreateMutex();
            s_inited = true;
        }
        taskEXIT_CRITICAL();
    }
}

static shim_slot_t *prv_slot(int fd)
{
    int i = fd - SHIM_FD_BASE;
    if ((i < 0) || (i >= KVS_SHIM_MAX_SOCKETS) || !s_slots[i].in_use)
    {
        return NULL;
    }
    return &s_slots[i];
}

uint16_t lwip_htons(uint16_t x)
{
    return (uint16_t) ((x << 8) | (x >> 8));
}

uint32_t lwip_htonl(uint32_t x)
{
    return ((x & 0x000000FFu) << 24) | ((x & 0x0000FF00u) << 8) |
           ((x & 0x00FF0000u) >> 8) | ((x & 0xFF000000u) >> 24);
}

/* BSD sockaddr_in <-> freertos_sockaddr (both keep port/addr in net order) */
static void prv_to_frtos(const struct sockaddr *sa, struct freertos_sockaddr *fa)
{
    const struct sockaddr_in *in = (const struct sockaddr_in *) sa;
    memset(fa, 0, sizeof(*fa));
    fa->sin_len = sizeof(*fa);
    fa->sin_family = FREERTOS_AF_INET;
    fa->sin_port = in->sin_port;
    fa->sin_address.ulIP_IPv4 = in->sin_addr.s_addr;
}

static void prv_from_frtos(const struct freertos_sockaddr *fa, struct sockaddr *sa,
                           socklen_t *salen)
{
    struct sockaddr_in in;
    memset(&in, 0, sizeof(in));
    in.sin_len = sizeof(in);
    in.sin_family = AF_INET;
    in.sin_port = fa->sin_port;
    in.sin_addr.s_addr = fa->sin_address.ulIP_IPv4;
    if (sa != NULL && salen != NULL)
    {
        socklen_t n = (*salen < (socklen_t) sizeof(in)) ? *salen : (socklen_t) sizeof(in);
        memcpy(sa, &in, n);
        *salen = (socklen_t) sizeof(in);
    }
}

static int prv_map_err(BaseType_t e)
{
    if (e == -pdFREERTOS_ERRNO_EWOULDBLOCK || e == 0) { return EWOULDBLOCK; }
    if (e == -pdFREERTOS_ERRNO_ENOMEM) { return ENOMEM; }
    if (e == -pdFREERTOS_ERRNO_ENOTCONN) { return ENOTCONN; }
    if (e == -pdFREERTOS_ERRNO_EINVAL) { return EINVAL; }
    return EIO;
}

int socket(int domain, int type, int protocol)
{
    (void) protocol;
    prv_init();
    if (domain != AF_INET)
    {
        errno = EAFNOSUPPORT;
        return -1;
    }
    BaseType_t ft = (type == SOCK_STREAM) ? FREERTOS_SOCK_STREAM : FREERTOS_SOCK_DGRAM;
    BaseType_t fp = (type == SOCK_STREAM) ? FREERTOS_IPPROTO_TCP : FREERTOS_IPPROTO_UDP;
    Socket_t s = FreeRTOS_socket(FREERTOS_AF_INET, ft, fp);
    if ((s == NULL) || (s == FREERTOS_INVALID_SOCKET))
    {
        errno = ENOMEM;
        return -1;
    }
    xSemaphoreTake(s_lock, portMAX_DELAY);
    for (int i = 0; i < KVS_SHIM_MAX_SOCKETS; i++)
    {
        if (!s_slots[i].in_use)
        {
            s_slots[i].in_use = true;
            s_slots[i].sock = s;
            s_slots[i].nonblock = false;
            s_slots[i].is_tcp = (type == SOCK_STREAM);
            xSemaphoreGive(s_lock);
            return SHIM_FD_BASE + i;
        }
    }
    xSemaphoreGive(s_lock);
    (void) FreeRTOS_closesocket(s);
    errno = ENFILE;
    return -1;
}

int bind(int s, const struct sockaddr *name, socklen_t namelen)
{
    (void) namelen;
    shim_slot_t *sl = prv_slot(s);
    if (!sl) { errno = EBADF; return -1; }
    struct freertos_sockaddr fa;
    prv_to_frtos(name, &fa);
    /* FreeRTOS+TCP binds to the device address; wildcard is implied. */
    fa.sin_address.ulIP_IPv4 = 0;
    if (FreeRTOS_bind(sl->sock, &fa, sizeof(fa)) != 0)
    {
        errno = EADDRINUSE;
        return -1;
    }
    return 0;
}

int connect(int s, const struct sockaddr *name, socklen_t namelen)
{
    (void) namelen;
    shim_slot_t *sl = prv_slot(s);
    if (!sl) { errno = EBADF; return -1; }
    struct freertos_sockaddr fa;
    prv_to_frtos(name, &fa);
    if (!sl->is_tcp)
    {
        /* UDP "connect" just latches the default peer: FreeRTOS+TCP has no
         * connected-UDP mode; the callers always use sendto/recvfrom, so
         * nothing to do beyond an implicit bind. */
        struct freertos_sockaddr local = {0};
        local.sin_len = sizeof(local);
        local.sin_family = FREERTOS_AF_INET;
        (void) FreeRTOS_bind(sl->sock, &local, sizeof(local));
        return 0;
    }
    BaseType_t rc = FreeRTOS_connect(sl->sock, &fa, sizeof(fa));
    if (rc != 0)
    {
        errno = prv_map_err(rc);
        return -1;
    }
    return 0;
}

int close(int s)
{
    shim_slot_t *sl = prv_slot(s);
    if (!sl) { errno = EBADF; return -1; }
    for (int i = 0; i < KVS_SHIM_MAX_SELECT_TASKS; i++)
    {
        if (s_select_sets[i].set != NULL)
        {
            FreeRTOS_FD_CLR(sl->sock, s_select_sets[i].set, eSELECT_ALL);
        }
    }
    (void) FreeRTOS_closesocket(sl->sock);
    sl->in_use = false;
    sl->sock = NULL;
    return 0;
}

int shutdown(int s, int how)
{
    (void) how;
    shim_slot_t *sl = prv_slot(s);
    if (!sl) { errno = EBADF; return -1; }
    (void) FreeRTOS_shutdown(sl->sock, FREERTOS_SHUT_RDWR);
    return 0;
}

static TickType_t prv_tv_to_ticks(const struct timeval *tv)
{
    uint64_t ms = (uint64_t) tv->tv_sec * 1000u + (uint64_t) tv->tv_usec / 1000u;
    return pdMS_TO_TICKS((uint32_t) ms);
}

int setsockopt(int s, int level, int optname, const void *optval, socklen_t optlen)
{
    (void) level; (void) optlen;
    shim_slot_t *sl = prv_slot(s);
    if (!sl) { errno = EBADF; return -1; }
    if ((optname == SO_RCVTIMEO) || (optname == SO_SNDTIMEO))
    {
        TickType_t t = prv_tv_to_ticks((const struct timeval *) optval);
        BaseType_t opt = (optname == SO_RCVTIMEO) ? FREERTOS_SO_RCVTIMEO
                                                  : FREERTOS_SO_SNDTIMEO;
        (void) FreeRTOS_setsockopt(sl->sock, 0, opt, &t, sizeof(t));
        return 0;
    }
    /* SO_REUSEADDR / SO_SNDBUF / SO_RCVBUF / TCP_NODELAY: accept silently */
    return 0;
}

int getsockopt(int s, int level, int optname, void *optval, socklen_t *optlen)
{
    (void) level;
    shim_slot_t *sl = prv_slot(s);
    if (!sl) { errno = EBADF; return -1; }
    if ((optname == SO_ERROR) && optval && optlen && (*optlen >= sizeof(int)))
    {
        *(int *) optval = 0;
        *optlen = sizeof(int);
        return 0;
    }
    errno = EINVAL;
    return -1;
}

int getsockname(int s, struct sockaddr *name, socklen_t *namelen)
{
    shim_slot_t *sl = prv_slot(s);
    if (!sl) { errno = EBADF; return -1; }
    struct freertos_sockaddr fa;
    memset(&fa, 0, sizeof(fa));
    (void) FreeRTOS_GetLocalAddress(sl->sock, &fa);
    if (fa.sin_address.ulIP_IPv4 == 0)
    {
        /* Bound to ANY: report the device address for ICE candidates. */
        fa.sin_address.ulIP_IPv4 = FreeRTOS_GetIPAddress();
    }
    prv_from_frtos(&fa, name, namelen);
    return 0;
}

int getpeername(int s, struct sockaddr *name, socklen_t *namelen)
{
    shim_slot_t *sl = prv_slot(s);
    if (!sl) { errno = EBADF; return -1; }
    struct freertos_sockaddr fa;
    memset(&fa, 0, sizeof(fa));
    (void) FreeRTOS_GetRemoteAddress(sl->sock, &fa);
    prv_from_frtos(&fa, name, namelen);
    return 0;
}

int fcntl(int s, int cmd, ...)
{
    shim_slot_t *sl = prv_slot(s);
    if (!sl) { errno = EBADF; return -1; }
    if (cmd == F_GETFL)
    {
        return sl->nonblock ? O_NONBLOCK : 0;
    }
    if (cmd == F_SETFL)
    {
        va_list ap;
        va_start(ap, cmd);
        int flags = va_arg(ap, int);
        va_end(ap);
        sl->nonblock = (flags & O_NONBLOCK) != 0;
        TickType_t t = sl->nonblock ? 0 : portMAX_DELAY;
        (void) FreeRTOS_setsockopt(sl->sock, 0, FREERTOS_SO_RCVTIMEO, &t, sizeof(t));
        TickType_t ts = sl->nonblock ? 0 : pdMS_TO_TICKS(10000);
        (void) FreeRTOS_setsockopt(sl->sock, 0, FREERTOS_SO_SNDTIMEO, &ts, sizeof(ts));
        return 0;
    }
    errno = EINVAL;
    return -1;
}

int select(int maxfdp1, fd_set *readset, fd_set *writeset, fd_set *exceptset,
           struct timeval *timeout)
{
    (void) exceptset;
    prv_init();
    SocketSet_t sel = prv_task_select_set();
    if (sel == NULL)
    {
        errno = ENOMEM;
        return -1;
    }

    int upper = maxfdp1 - SHIM_FD_BASE;
    if (upper > KVS_SHIM_MAX_SOCKETS) { upper = KVS_SHIM_MAX_SOCKETS; }

    for (int i = 0; i < upper; i++)
    {
        if (!s_slots[i].in_use) { continue; }
        int fd = SHIM_FD_BASE + i;
        EventBits_t bits = 0;
        if (readset && FD_ISSET(fd, readset)) { bits |= eSELECT_READ; }
        if (writeset && FD_ISSET(fd, writeset)) { bits |= eSELECT_WRITE; }
        if (bits)
        {
            FreeRTOS_FD_SET(s_slots[i].sock, sel, bits);
        }
        else
        {
            FreeRTOS_FD_CLR(s_slots[i].sock, sel, eSELECT_ALL);
        }
    }

    TickType_t to = timeout ? prv_tv_to_ticks(timeout) : portMAX_DELAY;
    (void) FreeRTOS_select(sel, to);

    int nready = 0;
    fd_set rout, wout;
    FD_ZERO(&rout);
    FD_ZERO(&wout);
    for (int i = 0; i < upper; i++)
    {
        if (!s_slots[i].in_use) { continue; }
        int fd = SHIM_FD_BASE + i;
        EventBits_t got = FreeRTOS_FD_ISSET(s_slots[i].sock, sel);
        if (readset && FD_ISSET(fd, readset) && (got & eSELECT_READ))
        {
            FD_SET(fd, &rout);
            nready++;
        }
        if (writeset && FD_ISSET(fd, writeset) && (got & eSELECT_WRITE))
        {
            FD_SET(fd, &wout);
            nready++;
        }
        FreeRTOS_FD_CLR(s_slots[i].sock, sel, eSELECT_ALL);
    }
    if (readset) { *readset = rout; }
    if (writeset) { *writeset = wout; }
    return nready;
}

int sendto(int s, const void *data, size_t size, int flags,
           const struct sockaddr *to, socklen_t tolen)
{
    (void) flags; (void) tolen;
    shim_slot_t *sl = prv_slot(s);
    if (!sl) { errno = EBADF; return -1; }
    struct freertos_sockaddr fa;
    prv_to_frtos(to, &fa);
    int32_t rc = FreeRTOS_sendto(sl->sock, data, size, 0, &fa, sizeof(fa));
    if (rc <= 0)
    {
        errno = prv_map_err(rc);
        return -1;
    }
    return rc;
}

int recvfrom(int s, void *mem, size_t len, int flags,
             struct sockaddr *from, socklen_t *fromlen)
{
    shim_slot_t *sl = prv_slot(s);
    if (!sl) { errno = EBADF; return -1; }
    struct freertos_sockaddr fa;
    memset(&fa, 0, sizeof(fa));
    uint32_t fl = ((flags & MSG_DONTWAIT) || sl->nonblock) ? 0 : 0;
    (void) fl;
    if (flags & MSG_DONTWAIT)
    {
        TickType_t t0 = 0;
        (void) FreeRTOS_setsockopt(sl->sock, 0, FREERTOS_SO_RCVTIMEO, &t0, sizeof(t0));
    }
    uint32_t falen = sizeof(fa);
    int32_t rc = FreeRTOS_recvfrom(sl->sock, mem, len, 0, &fa, &falen);
    if (rc < 0)
    {
        errno = prv_map_err(rc);
        return -1;
    }
    if (rc == 0)
    {
        errno = EWOULDBLOCK;
        return -1;
    }
    if (from != NULL && fromlen != NULL)
    {
        prv_from_frtos(&fa, from, fromlen);
    }
    return rc;
}

int send(int s, const void *data, size_t size, int flags)
{
    (void) flags;
    shim_slot_t *sl = prv_slot(s);
    if (!sl) { errno = EBADF; return -1; }
    int32_t rc = FreeRTOS_send(sl->sock, data, size, 0);
    if (rc < 0)
    {
        errno = prv_map_err(rc);
        return -1;
    }
    return rc;
}

int recv(int s, void *mem, size_t len, int flags)
{
    (void) flags;
    shim_slot_t *sl = prv_slot(s);
    if (!sl) { errno = EBADF; return -1; }
    int32_t rc = FreeRTOS_recv(sl->sock, mem, len, 0);
    if (rc < 0)
    {
        errno = prv_map_err(rc);
        return -1;
    }
    if ((rc == 0) && sl->nonblock)
    {
        errno = EWOULDBLOCK;
        return -1;
    }
    return rc;
}

/* ---- name resolution ----------------------------------------------------- */

int getaddrinfo(const char *nodename, const char *servname,
                const struct addrinfo *hints, struct addrinfo **res)
{
    uint32_t ip = FreeRTOS_inet_addr(nodename);
    if (ip == 0)
    {
        ip = FreeRTOS_gethostbyname(nodename);
    }
    if (ip == 0)
    {
        return EAI_NONAME;
    }
    struct addrinfo *ai = pvPortMalloc(sizeof(struct addrinfo) + sizeof(struct sockaddr_in));
    if (ai == NULL)
    {
        return EAI_MEMORY;
    }
    struct sockaddr_in *sa = (struct sockaddr_in *) (ai + 1);
    memset(ai, 0, sizeof(*ai) + sizeof(*sa));
    sa->sin_len = sizeof(*sa);
    sa->sin_family = AF_INET;
    sa->sin_addr.s_addr = ip;
    sa->sin_port = servname ? lwip_htons((uint16_t) atoi(servname)) : 0;
    ai->ai_family = AF_INET;
    /* Honor the caller's requested socket type: the TCP wrapper passes
     * SOCK_STREAM and feeds ai_socktype straight into socket(). */
    ai->ai_socktype = ((hints != NULL) && (hints->ai_socktype != 0))
                          ? hints->ai_socktype : SOCK_STREAM;
    ai->ai_protocol = (ai->ai_socktype == SOCK_STREAM) ? IPPROTO_TCP
                                                       : IPPROTO_UDP;
    ai->ai_addrlen = sizeof(*sa);
    ai->ai_addr = (struct sockaddr *) sa;
    *res = ai;
    return 0;
}

void freeaddrinfo(struct addrinfo *ai)
{
    if (ai != NULL)
    {
        vPortFree(ai);
    }
}

const char *gai_strerror(int errcode)
{
    (void) errcode;
    return "getaddrinfo error";
}

/* ---- address text conversion --------------------------------------------- */

const char *inet_ntop(int af, const void *src, char *dst, socklen_t size)
{
    if (af != AF_INET || size < INET_ADDRSTRLEN)
    {
        return NULL;
    }
    const uint8_t *b = (const uint8_t *) src;
    snprintf(dst, size, "%u.%u.%u.%u", b[0], b[1], b[2], b[3]);
    return dst;
}

int inet_pton(int af, const char *src, void *dst)
{
    if (af != AF_INET)
    {
        return -1;
    }
    uint32_t ip = FreeRTOS_inet_addr(src);
    if (ip == 0 && strcmp(src, "0.0.0.0") != 0)
    {
        return 0;
    }
    memcpy(dst, &ip, 4);
    return 1;
}

in_addr_t inet_addr(const char *cp)
{
    return FreeRTOS_inet_addr(cp);
}

/* ---- lwip_netconf -------------------------------------------------------- */

uint8_t *LwIP_GetIP(int interface_index)
{
    (void) interface_index;
    static uint8_t s_ip[4];
    uint32_t ip = FreeRTOS_GetIPAddress(); /* network byte order */
    memcpy(s_ip, &ip, 4);
    return s_ip;
}
