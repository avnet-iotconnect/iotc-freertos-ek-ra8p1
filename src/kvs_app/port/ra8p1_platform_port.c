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
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

#include "iotc_time.h"

/* ── Console logging (SdkLog target; see port/log_service.h) ────────────── */

extern int print_to_console( char *p_data ); /* fsp_err_t, but int-compatible */

void kvs_log_printf( const char *fmt, ... )
{
    /* Per-task buffer would be nicer, but log lines are short and a mutex
     * keeps concurrent KVS tasks from interleaving mid-line. */
    static SemaphoreHandle_t s_mtx;
    static char s_buf[ 256 ];
    va_list ap;

    if( s_mtx == NULL )
    {
        s_mtx = xSemaphoreCreateMutex();
    }
    if( ( s_mtx != NULL ) && ( xSemaphoreTake( s_mtx, pdMS_TO_TICKS( 200 ) ) != pdTRUE ) )
    {
        return; /* drop the line rather than block a media/network task */
    }

    va_start( ap, fmt );
    vsnprintf( s_buf, sizeof( s_buf ), fmt, ap );
    va_end( ap );
    ( void ) print_to_console( s_buf );

    if( s_mtx != NULL )
    {
        ( void ) xSemaphoreGive( s_mtx );
    }
}

/* Character sink for the vendored stack's raw diagnostic traces (replaces
 * the STM32 USART register pokes). Line-buffered; flushed on newline or
 * when full. Diagnostic-only paths, so the shared buffer race is benign. */
void kvs_log_putc( char ch )
{
    static char s_line[ 128 ];
    static size_t s_len;

    if( ( ch == '\n' ) || ( s_len >= sizeof( s_line ) - 2U ) )
    {
        if( ch != '\n' )
        {
            s_line[ s_len++ ] = ch;
        }
        s_line[ s_len ] = '\0';
        if( s_len > 0U )
        {
            kvs_log_printf( "%s\r\n", s_line );
        }
        s_len = 0U;
        return;
    }
    if( ch != '\r' )
    {
        s_line[ s_len++ ] = ch;
    }
}

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

/* ── Heap helpers for the vendored stack ────────────────────────────────── */
/* libsrtp's config.h maps malloc/calloc/free here so SRTP session churn
 * stays on the FreeRTOS heap instead of the 64 KB libc heap. */

void *kvs_port_malloc( unsigned int size )
{
    return pvPortMalloc( size );
}

void *kvs_port_calloc( unsigned int n, unsigned int size )
{
    size_t total = ( size_t ) n * ( size_t ) size;
    void *p = pvPortMalloc( total );

    if( p != NULL )
    {
        memset( p, 0, total );
    }
    return p;
}

void kvs_port_free( void *ptr )
{
    if( ptr != NULL )
    {
        vPortFree( ptr );
    }
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

/* ── Assert reporting ───────────────────────────────────────────────────── */
/* Strong override of the FSP weak __assert_func: the default hits BKPT
 * silently (a hard fault with no debugger), which cost a debug session to
 * attribute. Print the location first, then trap. */
void __assert_func( const char *file, int line, const char *func,
                    const char *expr )
{
    kvs_log_printf( "ASSERT %s:%d %s(): %s\r\n",
                    file ? file : "?", line,
                    func ? func : "?", expr ? expr : "?" );
    __asm volatile ( "bkpt 0" );
    for( ; ; )
    {
    }
}
