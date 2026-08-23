/*
 * Copyright (c) 2026 Avnet, Inc.
 * SPDX-License-Identifier: MIT
 *
 * One-shot on-target benchmark of software H.264 encoding (minih264,
 * baseline profile, scalar C) on live camera frames. Gates the KVS video
 * streaming feature: prints ms/frame and achievable fps for QVGA and VGA.
 * Enabled with H264_BENCH=1 (needs IOTC_CFG_NO_BUILTIN_MODEL=1 for MRAM).
 */
#include "app_config.h"

#if H264_BENCH

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "bsp_api.h"
#include "FreeRTOS.h"
#include "task.h"
#include "console_output/console_output.h"
#include "time_counter/time_counter.h"

#define H264E_ENABLE_NEON 0
#define MINIH264_IMPLEMENTATION
#include "minih264e.h"

#define CAM_W 640
#define CAM_H 480
#define BENCH_FRAMES 30

extern uint8_t *gp_next_buffer; /* live 640x480 RGB565 */

/* Working memory pools. The D-cache is disabled in this project (Ethernet
 * driver constraint), so SDRAM accesses are uncached and slow; the benchmark
 * therefore runs each resolution twice where possible: once from internal
 * SRAM (QVGA fits) and once from SDRAM, to quantify the memory penalty. */
static uint8_t s_pool_sdram[6 * 1024 * 1024] BSP_ALIGN_VARIABLE(64)
    BSP_PLACE_IN_SECTION(BSP_UNINIT_SECTION_PREFIX ".sdram_noinit");
static uint8_t s_pool_sram[480 * 1024] BSP_ALIGN_VARIABLE(64);
static uint8_t *s_pool;
static size_t s_pool_size;
static size_t s_pool_used;

static void *prv_alloc(size_t n)
{
    n = (n + 63U) & ~63U;
    if (s_pool_used + n > s_pool_size)
    {
        return NULL;
    }
    void *p = &s_pool[s_pool_used];
    s_pool_used += n;
    return p;
}

/* RGB565 -> I420 (BT.601), optional 2x decimation (640x480 -> 320x240). */
static void prv_rgb565_to_i420(const uint16_t *src, int dec,
                               uint8_t *y, uint8_t *u, uint8_t *v,
                               int w, int h)
{
    for (int j = 0; j < h; j++)
    {
        const uint16_t *row = src + (size_t) (j * dec) * CAM_W;
        uint8_t *yr = y + (size_t) j * w;
        for (int i = 0; i < w; i++)
        {
            uint16_t p = row[i * dec];
            int r = ((p >> 11) & 0x1F) << 3;
            int g = ((p >> 5) & 0x3F) << 2;
            int b = (p & 0x1F) << 3;
            yr[i] = (uint8_t) ((77 * r + 150 * g + 29 * b) >> 8);
            if (((j & 1) == 0) && ((i & 1) == 0))
            {
                size_t ci = (size_t) (j / 2) * (size_t) (w / 2) + (size_t) (i / 2);
                u[ci] = (uint8_t) (128 + ((-43 * r - 85 * g + 128 * b) >> 8));
                v[ci] = (uint8_t) (128 + ((128 * r - 107 * g - 21 * b) >> 8));
            }
        }
    }
}

static char s_line[160];
#define BENCH_PRINT(...)                                \
    do {                                                \
        snprintf(s_line, sizeof(s_line), __VA_ARGS__);  \
        print_to_console(s_line);                       \
    } while (0)

static void prv_bench_one(int w, int h, int dec, const char *tag, bool use_sram)
{
    s_pool = use_sram ? s_pool_sram : s_pool_sdram;
    s_pool_size = use_sram ? sizeof(s_pool_sram) : sizeof(s_pool_sdram);
    s_pool_used = 0;

    H264E_create_param_t cp;
    memset(&cp, 0, sizeof(cp));
    cp.width = w;
    cp.height = h;
    cp.gop = 30;
    cp.const_input_flag = 1;

    int persist_sz = 0, scratch_sz = 0;
    int rc = H264E_sizeof(&cp, &persist_sz, &scratch_sz);
    if (rc)
    {
        BENCH_PRINT("H264 %s: sizeof failed (%d)\r\n", tag, rc);
        return;
    }
    H264E_persist_t *persist = prv_alloc((size_t) persist_sz);
    H264E_scratch_t *scratch = prv_alloc((size_t) scratch_sz);
    /* Input YUV always in SDRAM (read once per pixel; the encoder's hot
     * state is persist+scratch, which is what the SRAM run exercises). */
    s_pool = s_pool_sdram;
    s_pool_size = sizeof(s_pool_sdram);
    s_pool_used = (use_sram) ? 0 : s_pool_used;
    uint8_t *yb = prv_alloc((size_t) w * h);
    uint8_t *ub = prv_alloc((size_t) w * h / 4);
    uint8_t *vb = prv_alloc((size_t) w * h / 4);
    if (!persist || !scratch || !yb || !ub || !vb)
    {
        BENCH_PRINT("H264 %s: pool exhausted (persist %d scratch %d)\r\n",
                    tag, persist_sz, scratch_sz);
        return;
    }
    rc = H264E_init(persist, &cp);
    if (rc)
    {
        BENCH_PRINT("H264 %s: init failed (%d)\r\n", tag, rc);
        return;
    }

    H264E_run_param_t rp;
    memset(&rp, 0, sizeof(rp));
    rp.encode_speed = 10;
    rp.qp_min = 26;
    rp.qp_max = 34;

    H264E_io_yuv_t io;
    io.yuv[0] = yb; io.stride[0] = w;
    io.yuv[1] = ub; io.stride[1] = w / 2;
    io.yuv[2] = vb; io.stride[2] = w / 2;

    uint32_t conv_ticks = 0, enc_ticks = 0;
    size_t total_bytes = 0;
    for (int f = 0; f < BENCH_FRAMES; f++)
    {
        uint32_t t0 = TimeCounter_CurrentCountGet();
        prv_rgb565_to_i420((const uint16_t *) gp_next_buffer, dec, yb, ub, vb, w, h);
        uint32_t t1 = TimeCounter_CurrentCountGet();

        uint8_t *coded = NULL;
        int coded_sz = 0;
        rc = H264E_encode(persist, scratch, &rp, &io, &coded, &coded_sz);
        uint32_t t2 = TimeCounter_CurrentCountGet();
        if (rc)
        {
            BENCH_PRINT("H264 %s: encode failed (%d) at frame %d\r\n", tag, rc, f);
            return;
        }
        conv_ticks += t1 - t0;
        enc_ticks += t2 - t1;
        total_bytes += (size_t) coded_sz;
    }

    /* 100 us per tick */
    uint32_t conv_ms10 = conv_ticks / BENCH_FRAMES;    /* in 100us units */
    uint32_t enc_ms10 = enc_ticks / BENCH_FRAMES;
    uint32_t total_ms10 = conv_ms10 + enc_ms10;
    BENCH_PRINT("H264 %s: convert %lu.%lu ms, encode %lu.%lu ms -> %lu.%lu ms/frame"
                " (~%lu fps), avg %u B/frame, persist %d KB scratch %d KB\r\n",
                tag,
                (unsigned long) conv_ms10 / 10, (unsigned long) conv_ms10 % 10,
                (unsigned long) enc_ms10 / 10, (unsigned long) enc_ms10 % 10,
                (unsigned long) total_ms10 / 10, (unsigned long) total_ms10 % 10,
                (unsigned long) ((total_ms10 > 0) ? (10000U / total_ms10) : 0),
                (unsigned) (total_bytes / BENCH_FRAMES), persist_sz / 1024,
                scratch_sz / 1024);
}

void h264_bench_run(void)
{
    /* The display buffer pointer is published a few frames after the camera
     * starts; wait up to 5 s for the first one. */
    for (int i = 0; (i < 100) && (gp_next_buffer == NULL); i++)
    {
        vTaskDelay(pdMS_TO_TICKS(50));
    }
    if (gp_next_buffer == NULL)
    {
        print_to_console("H264 bench: no camera frame\r\n");
        return;
    }
    print_to_console("\r\nH264 software-encode benchmark (minih264 baseline, "
                     "scalar C, live frames):\r\n");
    prv_bench_one(320, 240, 2, "QVGA/SRAM ", true);
    prv_bench_one(320, 240, 2, "QVGA/SDRAM", false);
    prv_bench_one(640, 480, 1, "VGA /SDRAM", false);
    print_to_console("H264 bench done\r\n\r\n");
}

#else /* !H264_BENCH */

void h264_bench_run(void)
{
}

#endif /* H264_BENCH */
