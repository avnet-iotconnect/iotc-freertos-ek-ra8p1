/*
 * Copyright (c) 2026 Avnet, Inc.
 * SPDX-License-Identifier: MIT
 *
 * Snapshot capture -> annotate -> grayscale PNG -> Telemetry Files upload.
 */
#include <stdio.h>
#include <string.h>

#include "bsp_api.h"

#include "iotc_snapshot.h"
#include "iotc_file_upload.h"
#include "iotc_time.h"
#include "png_gray.h"

/* Camera frame geometry (matches detection_overlay.c): the model sees the
 * center 480x480 of the 640x480 RGB565 frame, model coords scale by 2.5. */
#define CAM_W      640
#define CAM_H      480
#define SNAP_W     480
#define SNAP_H     480
#define CROP_OFF_X ((CAM_W - SNAP_W) / 2)
#define SCALE_NUM  5
#define SCALE_DEN  2
#define BOX_THICK  3
#define BOX_GRAY   0xFF

/* Live frame + detections from the vision pipeline. */
extern uint8_t *gp_next_buffer;
extern uint32_t face_detection_box_count(void);
extern void face_detection_box_get(uint32_t i, int16_t *x, int16_t *y,
                                   int16_t *w, int16_t *h, float *score);

/* Working buffers in SDRAM: 480x480 gray (225 KB) + PNG out (~232 KB). */
static uint8_t s_gray[SNAP_W * SNAP_H]
    BSP_PLACE_IN_SECTION(BSP_UNINIT_SECTION_PREFIX ".sdram_noinit");
static uint8_t s_png[SNAP_W * SNAP_H + (SNAP_W * SNAP_H) / 8192 + 4096]
    BSP_PLACE_IN_SECTION(BSP_UNINIT_SECTION_PREFIX ".sdram_noinit");

static inline int clampi(int v, int lo, int hi)
{
    return (v < lo) ? lo : ((v > hi) ? hi : v);
}

static void prv_hline(int x0, int x1, int y)
{
    if ((y < 0) || (y >= SNAP_H))
    {
        return;
    }
    x0 = clampi(x0, 0, SNAP_W - 1);
    x1 = clampi(x1, 0, SNAP_W - 1);
    memset(&s_gray[(size_t) y * SNAP_W + x0], BOX_GRAY, (size_t) (x1 - x0 + 1));
}

static void prv_vline(int x, int y0, int y1)
{
    if ((x < 0) || (x >= SNAP_W))
    {
        return;
    }
    y0 = clampi(y0, 0, SNAP_H - 1);
    y1 = clampi(y1, 0, SNAP_H - 1);
    for (int y = y0; y <= y1; y++)
    {
        s_gray[(size_t) y * SNAP_W + x] = BOX_GRAY;
    }
}

int iotc_snapshot_capture_upload(char *msg, size_t msg_size)
{
    const uint16_t *fb = (const uint16_t *) gp_next_buffer;

    if (fb == NULL)
    {
        if (msg)
        {
            snprintf(msg, msg_size, "no camera frame available");
        }
        return -11; /* -EAGAIN */
    }

    /* RGB565 center crop -> 8-bit luma (BT.601 integer weights). */
    for (int y = 0; y < SNAP_H; y++)
    {
        const uint16_t *row = fb + (size_t) y * CAM_W + CROP_OFF_X;
        uint8_t *dst = &s_gray[(size_t) y * SNAP_W];
        for (int x = 0; x < SNAP_W; x++)
        {
            uint16_t p = row[x];
            unsigned r = (unsigned) ((p >> 11) & 0x1F) << 3;
            unsigned g = (unsigned) ((p >> 5) & 0x3F) << 2;
            unsigned b = (unsigned) (p & 0x1F) << 3;
            dst[x] = (uint8_t) ((299U * r + 587U * g + 114U * b) / 1000U);
        }
    }

    /* Draw current detections (model coords -> crop coords: x2.5). */
    uint32_t n = face_detection_box_count();
    float top_score = 0.0f;
    for (uint32_t i = 0; i < n; i++)
    {
        int16_t mx, my, mw, mh;
        float score;
        face_detection_box_get(i, &mx, &my, &mw, &mh, &score);
        if (score > top_score)
        {
            top_score = score;
        }
        int x = (mx * SCALE_NUM) / SCALE_DEN;
        int y = (my * SCALE_NUM) / SCALE_DEN;
        int w = (mw * SCALE_NUM) / SCALE_DEN;
        int h = (mh * SCALE_NUM) / SCALE_DEN;
        for (int t = 0; t < BOX_THICK; t++)
        {
            prv_hline(x, x + w, y + t);
            prv_hline(x, x + w, y + h - t);
            prv_vline(x + t, y, y + h);
            prv_vline(x + w - t, y, y + h);
        }
    }

    int png_len = png_gray_encode(s_gray, SNAP_W, SNAP_H, s_png, sizeof(s_png));
    if (png_len <= 0)
    {
        if (msg)
        {
            snprintf(msg, msg_size, "png encode failed (%d)", png_len);
        }
        return (png_len < 0) ? png_len : -5;
    }

    /* Epoch-stamped name so snapshots never collide across reboots. */
    char name[40];
    snprintf(name, sizeof(name), "snap-%lu.png",
             (unsigned long) iotc_time_now());

    char cf[96];
    snprintf(cf, sizeof(cf), "{\"face_count\":%u,\"score\":%d}",
             (unsigned) n, (int) (top_score * 100.0f));

    int rc = iotc_fu_upload(name, s_png, (size_t) png_len, "image/png", cf);
    if (msg)
    {
        if (rc == 0)
        {
            snprintf(msg, msg_size, "%s uploaded (%d KB, %u face%s)",
                     name, png_len / 1024, (unsigned) n, (n == 1) ? "" : "s");
        }
        else
        {
            snprintf(msg, msg_size, "upload failed (%d)", rc);
        }
    }
    return rc;
}
