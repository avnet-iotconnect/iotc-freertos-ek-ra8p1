/*
 * Copyright (c) 2026 Avnet, Inc.
 * SPDX-License-Identifier: MIT
 *
 * EK-RA8P1 media source port for the KVS WebRTC stack — implements the
 * AppMediaSourcePort_* interface (examples/app_media_source).
 *
 * Pipeline:
 *   camera (640x480 RGB565, gp_next_buffer, shared with display/AI)
 *     -> 2x decimate + RGB565->I420 (BT.601)         [software]
 *     -> minih264 baseline encode, QVGA              [software, ~90 ms/frame]
 *     -> onVideoFrameReady callback -> PeerConnection RTP
 *
 * The encoder's hot state (persist + scratch, ~460 KB) lives in internal
 * SRAM — measured 84 ms/frame vs 420+ ms with those pools in the uncached
 * SDRAM (D-cache is off for the Ethernet driver). YUV working frames and
 * the encoded output go to SDRAM.
 *
 * The encode task runs only while at least one WebRTC peer is connected
 * (ucStreaming, gated by Start/Stop); the camera itself runs continuously
 * for the vision pipeline regardless.
 */
#include <stdio.h>
#include <string.h>
#include <stdint.h>

#include "bsp_api.h"
#include "FreeRTOS.h"
#include "task.h"

#include "app_media_source_port.h"

#include "logging_levels.h"
#define LOG_LEVEL LOG_INFO
#include "logging.h"

#define H264E_ENABLE_NEON 0
#define MINIH264_IMPLEMENTATION
#include "../../video/minih264e.h"

/* ── Configuration ──────────────────────────────────────────────────────── */

#define CAM_W            640
#define CAM_H            480
#define ENC_W            320
#define ENC_H            240
#define ENC_DECIMATE     2
#define ENC_GOP          30
#define ENC_QP_MIN       26
#define ENC_QP_MAX       37
#define ENC_SPEED        10
/* Soft frame-rate cap; actual rate is encode-bound (~8-10 fps QVGA). */
#define ENC_MIN_FRAME_MS 100U
/* Target ~500 kbit/s at ~8 fps. */
#define ENC_TARGET_FRAME_BYTES 8000

#define MEDIA_TASK_STACK_WORDS ( 4096U )
#define MEDIA_TASK_PRIORITY    ( 1U )

/* Live camera frame published by camera_control.c. */
extern uint8_t *gp_next_buffer;

/* ── Buffers ────────────────────────────────────────────────────────────── */

/* Encoder persist + scratch: measured 459 KB for QVGA. Keep in SRAM. */
static uint8_t s_enc_pool_sram[ 470U * 1024U ] BSP_ALIGN_VARIABLE( 64 );

/* I420 working frame + encoded output: SDRAM (read/written once per pixel). */
static uint8_t s_yuv_frame[ ( ENC_W * ENC_H * 3 ) / 2 ] BSP_ALIGN_VARIABLE( 64 )
    BSP_PLACE_IN_SECTION( BSP_UNINIT_SECTION_PREFIX ".sdram_noinit" );
static uint8_t s_encoded_copy[ 128U * 1024U ] BSP_ALIGN_VARIABLE( 64 )
    BSP_PLACE_IN_SECTION( BSP_UNINIT_SECTION_PREFIX ".sdram_noinit" );

/* ── State ──────────────────────────────────────────────────────────────── */

typedef struct
{
    OnFrameReadyToSend_t pfnOnVideoFrame;
    void *               pvVideoCtx;
    TaskHandle_t         xTaskHandle;
    volatile uint8_t     ucRunning;    /* task alive                     */
    volatile uint8_t     ucStreaming;  /* peers present — encode+send    */
    volatile uint8_t     ucForceIdr;   /* keyframe on next encode        */

    H264E_persist_t *    pxPersist;
    H264E_scratch_t *    pxScratch;
    uint8_t              ucEncInited;
} Ra8p1MediaCtx_t;

static Ra8p1MediaCtx_t s_ctx;

/* ── RGB565 -> I420, 2x decimation (640x480 -> 320x240, BT.601) ─────────── */

static void prv_rgb565_to_i420( const uint16_t *src,
                                uint8_t *y, uint8_t *u, uint8_t *v )
{
    for( int j = 0; j < ENC_H; j++ )
    {
        const uint16_t *row = src + ( size_t ) ( j * ENC_DECIMATE ) * CAM_W;
        uint8_t *yr = y + ( size_t ) j * ENC_W;
        for( int i = 0; i < ENC_W; i++ )
        {
            uint16_t p = row[ i * ENC_DECIMATE ];
            int r = ( ( p >> 11 ) & 0x1F ) << 3;
            int g = ( ( p >> 5 ) & 0x3F ) << 2;
            int b = ( p & 0x1F ) << 3;
            yr[ i ] = ( uint8_t ) ( ( 77 * r + 150 * g + 29 * b ) >> 8 );
            if( ( ( j & 1 ) == 0 ) && ( ( i & 1 ) == 0 ) )
            {
                size_t ci = ( size_t ) ( j / 2 ) * ( ENC_W / 2 ) +
                            ( size_t ) ( i / 2 );
                u[ ci ] = ( uint8_t ) ( 128 + ( ( -43 * r - 85 * g + 128 * b ) >> 8 ) );
                v[ ci ] = ( uint8_t ) ( 128 + ( ( 128 * r - 107 * g - 21 * b ) >> 8 ) );
            }
        }
    }
}

/* ── Encoder lifecycle ──────────────────────────────────────────────────── */

static int prv_encoder_init( void )
{
    H264E_create_param_t cp;
    int persist_sz = 0, scratch_sz = 0;

    if( s_ctx.ucEncInited )
    {
        return 0;
    }

    memset( &cp, 0, sizeof( cp ) );
    cp.width = ENC_W;
    cp.height = ENC_H;
    cp.gop = ENC_GOP;
    cp.const_input_flag = 1;

    if( 0 != H264E_sizeof( &cp, &persist_sz, &scratch_sz ) )
    {
        LogError( ( "[KVSMedia] H264E_sizeof failed" ) );
        return -1;
    }
    if( ( size_t ) ( persist_sz + scratch_sz + 128 ) > sizeof( s_enc_pool_sram ) )
    {
        LogError( ( "[KVSMedia] encoder pools %d+%d exceed SRAM pool",
                  persist_sz, scratch_sz ) );
        return -1;
    }
    s_ctx.pxPersist = ( H264E_persist_t * ) s_enc_pool_sram;
    s_ctx.pxScratch = ( H264E_scratch_t * )
        &s_enc_pool_sram[ ( ( size_t ) persist_sz + 63U ) & ~( size_t ) 63U ];

    if( 0 != H264E_init( s_ctx.pxPersist, &cp ) )
    {
        LogError( ( "[KVSMedia] H264E_init failed" ) );
        return -1;
    }
    s_ctx.ucEncInited = 1U;
    LogInfo( ( "[KVSMedia] H.264 encoder ready (QVGA, persist %d KB scratch %d KB)",
             persist_sz / 1024, scratch_sz / 1024 ) );
    return 0;
}

/* ── Capture + encode loop ──────────────────────────────────────────────── */

static void prv_media_task( void *pvParam )
{
    Ra8p1MediaCtx_t *ctx = ( Ra8p1MediaCtx_t * ) pvParam;
    MediaFrame_t frame;
    uint32_t frame_no = 0;
    TickType_t last_hb = xTaskGetTickCount();

    LogInfo( ( "[KVSMedia] media task started" ) );

    while( ctx->ucRunning )
    {
        TickType_t t0 = xTaskGetTickCount();

        if( !ctx->ucStreaming || ( gp_next_buffer == NULL ) ||
            !ctx->ucEncInited )
        {
            vTaskDelay( pdMS_TO_TICKS( 100U ) );
            continue;
        }

        /* Convert the most recent camera frame. The camera thread flips
         * gp_next_buffer between ping-pong buffers; a mid-conversion flip
         * can tear one frame, which a lossy live stream tolerates. */
        prv_rgb565_to_i420( ( const uint16_t * ) gp_next_buffer,
                            &s_yuv_frame[ 0 ],
                            &s_yuv_frame[ ENC_W * ENC_H ],
                            &s_yuv_frame[ ( ENC_W * ENC_H * 5 ) / 4 ] );

        H264E_run_param_t rp;
        memset( &rp, 0, sizeof( rp ) );
        rp.encode_speed = ENC_SPEED;
        rp.qp_min = ENC_QP_MIN;
        rp.qp_max = ENC_QP_MAX;
        rp.desired_frame_bytes = ENC_TARGET_FRAME_BYTES;
        if( ctx->ucForceIdr )
        {
            ctx->ucForceIdr = 0U;
            rp.frame_type = H264E_FRAME_TYPE_KEY;
        }

        H264E_io_yuv_t io;
        io.yuv[ 0 ] = &s_yuv_frame[ 0 ];
        io.stride[ 0 ] = ENC_W;
        io.yuv[ 1 ] = &s_yuv_frame[ ENC_W * ENC_H ];
        io.stride[ 1 ] = ENC_W / 2;
        io.yuv[ 2 ] = &s_yuv_frame[ ( ENC_W * ENC_H * 5 ) / 4 ];
        io.stride[ 2 ] = ENC_W / 2;

        uint8_t *coded = NULL;
        int coded_sz = 0;
        int rc = H264E_encode( ctx->pxPersist, ctx->pxScratch, &rp, &io,
                               &coded, &coded_sz );
        if( ( rc == 0 ) && ( coded_sz > 0 ) &&
            ( ( size_t ) coded_sz <= sizeof( s_encoded_copy ) ) )
        {
            /* The encoder's output lives inside its scratch memory and is
             * clobbered by the next encode; RTP packetization + SRTP happen
             * synchronously inside the callback, but copy out anyway so a
             * slow send path never reads a half-overwritten frame. */
            memcpy( s_encoded_copy, coded, ( size_t ) coded_sz );

            frame.pData = s_encoded_copy;
            frame.size = ( uint32_t ) coded_sz;
            frame.timestampUs = ( uint64_t ) xTaskGetTickCount() *
                                portTICK_PERIOD_MS * 1000ULL;
            frame.trackKind = TRANSCEIVER_TRACK_KIND_VIDEO;
            frame.freeData = 0;

            if( ctx->pfnOnVideoFrame != NULL )
            {
                ( void ) ctx->pfnOnVideoFrame( ctx->pvVideoCtx, &frame );
            }
        }
        else if( rc != 0 )
        {
            LogWarn( ( "[KVSMedia] encode failed (%d)", rc ) );
        }

        /* Heartbeat every ~10 s of streaming. */
        frame_no++;
        if( ( xTaskGetTickCount() - last_hb ) >= pdMS_TO_TICKS( 10000U ) )
        {
            last_hb = xTaskGetTickCount();
            LogInfo( ( "[KVSMedia] f=%lu len=%d heap=%u hwm=%u",
                     ( unsigned long ) frame_no, coded_sz,
                     ( unsigned ) xPortGetFreeHeapSize(),
                     ( unsigned ) uxTaskGetStackHighWaterMark( NULL ) ) );
        }

        /* Pace: never faster than ENC_MIN_FRAME_MS, and always yield. */
        TickType_t spent = xTaskGetTickCount() - t0;
        if( spent < pdMS_TO_TICKS( ENC_MIN_FRAME_MS ) )
        {
            vTaskDelay( pdMS_TO_TICKS( ENC_MIN_FRAME_MS ) - spent );
        }
        else
        {
            vTaskDelay( 1 );
        }
    }

    s_ctx.xTaskHandle = NULL;
    vTaskDelete( NULL );
}

/* ── AppMediaSourcePort_* API ───────────────────────────────────────────── */

int32_t AppMediaSourcePort_Init( void )
{
    if( s_ctx.xTaskHandle != NULL )
    {
        return 0; /* already up */
    }

    memset( &s_ctx, 0, sizeof( s_ctx ) );

    if( 0 != prv_encoder_init() )
    {
        return -1;
    }

    s_ctx.ucRunning = 1U;
    if( pdPASS != xTaskCreate( prv_media_task,
                               "KVSMedia",
                               MEDIA_TASK_STACK_WORDS,
                               &s_ctx,
                               MEDIA_TASK_PRIORITY,
                               &s_ctx.xTaskHandle ) )
    {
        LogError( ( "[KVSMedia] media task create failed" ) );
        s_ctx.ucRunning = 0U;
        return -1;
    }
    return 0;
}

int32_t AppMediaSourcePort_Start( OnFrameReadyToSend_t pfnOnVideoFrame,
                                  void *pvVideoCtx,
                                  OnFrameReadyToSend_t pfnOnAudioFrame,
                                  void *pvAudioCtx )
{
    ( void ) pfnOnAudioFrame; /* no audio source on this board */
    ( void ) pvAudioCtx;

    s_ctx.pfnOnVideoFrame = pfnOnVideoFrame;
    s_ctx.pvVideoCtx = pvVideoCtx;
    s_ctx.ucForceIdr = 1U; /* joining viewer needs SPS/PPS + IDR */
    s_ctx.ucStreaming = 1U;
    LogInfo( ( "[KVSMedia] streaming ON" ) );
    return 0;
}

void AppMediaSourcePort_Stop( void )
{
    s_ctx.ucStreaming = 0U;
    LogInfo( ( "[KVSMedia] streaming OFF" ) );
}

void AppMediaSourcePort_Destroy( void )
{
    s_ctx.ucStreaming = 0U;
    /* Task and encoder state are kept for the next session. */
}

void AppMediaSourcePort_PlayAudioFrame( MediaFrame_t *pFrame )
{
    ( void ) pFrame; /* no audio output on this board */
}
