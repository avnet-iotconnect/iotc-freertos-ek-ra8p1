/*
 * Copyright (c) 2026 Avnet, Inc.
 * SPDX-License-Identifier: MIT
 *
 * EK-RA8P1 platform glue for the vendored KVS WebRTC stack:
 *  - clock_gettime / sntp_get_lasttime over the project's iotc_time seam
 *    (iotc_time_sync() has already latched the SNTP epoch offset before the
 *    KVS task starts).
 *  - Ameba platform hooks (crypto_init, platform_set_malloc_free) as no-ops:
 *    mbedTLS here is the full FSP software build.
 *  - vPetWatchdog: referenced throughout the vendored examples layer; this
 *    project runs without an independent watchdog, so it is a no-op.
 */
#include <stddef.h>
#include <time.h>
#include <sys/time.h>

#include "FreeRTOS.h"
#include "task.h"

#include "iotc_time.h"

/* ── Wall clock ─────────────────────────────────────────────────────────── */
/* iotc_time_now() gives whole UTC seconds (epoch offset + tick). Derive the
 * sub-second part from the tick counter so consecutive calls are monotonic
 * within the second. */

int clock_gettime( clockid_t clk_id, struct timespec *tp )
{
    ( void ) clk_id;
    if( tp != NULL )
    {
        TickType_t ticks = xTaskGetTickCount();
        uint32_t ms = ( uint32_t ) ( ticks % configTICK_RATE_HZ ) *
                      portTICK_PERIOD_MS;
        tp->tv_sec  = ( time_t ) iotc_time_now();
        tp->tv_nsec = ( long ) ( ms % 1000U ) * 1000000L;
    }
    return 0;
}

/* networking_utils.c projects (sec,usec,tick) forward using the current tick
 * delta; returning "now" with the current tick makes the delta zero. */
void sntp_get_lasttime( long long *sec, long long *usec, unsigned int *tick )
{
    struct timespec ts;

    ( void ) clock_gettime( 0, &ts );
    if( sec != NULL )
    {
        *sec = ( long long ) ts.tv_sec;
    }
    if( usec != NULL )
    {
        *usec = ( long long ) ( ts.tv_nsec / 1000L );
    }
    if( tick != NULL )
    {
        *tick = ( unsigned int ) xTaskGetTickCount();
    }
}

void sntp_init( void )
{
    /* Time is already synced by iotc_time_sync() before KVS starts. */
}

/* ── Ameba platform hooks ───────────────────────────────────────────────── */

int crypto_init( void )
{
    return 0;
}

int platform_set_malloc_free( void * ( *malloc_func )( size_t ),
                              void ( *free_func )( void * ) )
{
    ( void ) malloc_func;
    ( void ) free_func;
    return 0;
}

/* ── Watchdog ───────────────────────────────────────────────────────────── */

void vPetWatchdog( void )
{
}
