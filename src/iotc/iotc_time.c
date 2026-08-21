/*
 * Copyright (c) 2020-2026 Avnet, Inc.
 * SPDX-License-Identifier: MIT
 *
 * Wall-clock seam, FSP/FreeRTOS port. Minimal SNTP exchange over a
 * FreeRTOS+TCP UDP socket; latches a UTC-epoch offset against the FreeRTOS
 * tick. time() (via _gettimeofday) serves mbedTLS certificate validity
 * checks and iotc-c-lib telemetry timestamps.
 */

#include "iotc_time.h"

#include <string.h>
#include <sys/time.h>

#include "FreeRTOS.h"
#include "task.h"
#include "FreeRTOS_IP.h"
#include "FreeRTOS_Sockets.h"

#define SNTP_PORT          123
#define SNTP_PACKET_SIZE   48
#define SNTP_UNIX_OFFSET   2208988800ULL /* 1900 -> 1970 */

static volatile int64_t s_epoch_at_boot; /* UTC seconds minus uptime seconds */
static volatile bool s_synced;

int iotc_time_sync(const char *sntp_server, uint32_t timeout_ms)
{
    int rc = -1;
    uint32_t ip = FreeRTOS_gethostbyname(sntp_server);
    if (0 == ip)
    {
        return -1;
    }

    Socket_t sock = FreeRTOS_socket(FREERTOS_AF_INET, FREERTOS_SOCK_DGRAM, FREERTOS_IPPROTO_UDP);
    if (FREERTOS_INVALID_SOCKET == sock)
    {
        return -1;
    }

    TickType_t to = pdMS_TO_TICKS(timeout_ms);
    FreeRTOS_setsockopt(sock, 0, FREERTOS_SO_RCVTIMEO, &to, sizeof(to));
    FreeRTOS_setsockopt(sock, 0, FREERTOS_SO_SNDTIMEO, &to, sizeof(to));

    struct freertos_sockaddr dest;
    memset(&dest, 0, sizeof(dest));
    dest.sin_family = FREERTOS_AF_INET;
    dest.sin_port = FreeRTOS_htons(SNTP_PORT);
    dest.sin_address.ulIP_IPv4 = ip;
    dest.sin_len = sizeof(dest);

    uint8_t pkt[SNTP_PACKET_SIZE];
    memset(pkt, 0, sizeof(pkt));
    pkt[0] = 0x1B; /* LI=0, VN=3, Mode=3 (client) */

    if (FreeRTOS_sendto(sock, pkt, sizeof(pkt), 0, &dest, sizeof(dest)) == SNTP_PACKET_SIZE)
    {
        struct freertos_sockaddr src;
        uint32_t srclen = sizeof(src);
        int32_t n = FreeRTOS_recvfrom(sock, pkt, sizeof(pkt), 0, &src, &srclen);
        if (n >= SNTP_PACKET_SIZE)
        {
            /* Transmit timestamp seconds, bytes 40..43, big endian. */
            uint64_t secs = ((uint64_t) pkt[40] << 24) | ((uint64_t) pkt[41] << 16) |
                            ((uint64_t) pkt[42] << 8) | pkt[43];
            if (secs > SNTP_UNIX_OFFSET)
            {
                int64_t now = (int64_t) (secs - SNTP_UNIX_OFFSET);
                int64_t uptime_s = (int64_t) (xTaskGetTickCount() / configTICK_RATE_HZ);
                s_epoch_at_boot = now - uptime_s;
                s_synced = true;
                rc = 0;
            }
        }
    }

    FreeRTOS_closesocket(sock);
    return rc;
}

bool iotc_time_is_synced(void)
{
    return s_synced;
}

int64_t iotc_time_now(void)
{
    if (!s_synced)
    {
        return 0;
    }
    return s_epoch_at_boot + (int64_t) (xTaskGetTickCount() / configTICK_RATE_HZ);
}

/* libc back end: time() -> gettimeofday() -> this stub. mbedTLS reads
 * certificate validity via time(). */
int _gettimeofday(struct timeval *tv, void *tz)
{
    (void) tz;
    if (tv)
    {
        tv->tv_sec = (time_t) iotc_time_now();
        tv->tv_usec = 0;
    }
    return 0;
}
