/*
 * Copyright (c) 2026 Avnet, Inc.
 * SPDX-License-Identifier: MIT
 *
 * Snapshot capture -> annotate -> color PNG -> Telemetry Files upload.
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
#define SNAP_CH    3 /* RGB888 */

/* Live frame + detections from the vision pipeline. */
extern uint8_t *gp_next_buffer;
extern uint32_t face_detection_box_count(void);
extern void face_detection_box_get(uint32_t i, int16_t *x, int16_t *y,
                                   int16_t *w, int16_t *h, float *score);

/* Working buffers in SDRAM: 480x480 RGB888 (675 KB) + PNG out (~680 KB). */
static uint8_t s_rgb[SNAP_W * SNAP_H * SNAP_CH]
    BSP_PLACE_IN_SECTION(BSP_UNINIT_SECTION_PREFIX ".sdram_noinit");
static uint8_t s_png[SNAP_W * SNAP_H * SNAP_CH + SNAP_H + 8192]
    BSP_PLACE_IN_SECTION(BSP_UNINIT_SECTION_PREFIX ".sdram_noinit");

static inline int clampi(int v, int lo, int hi)
{
    return (v < lo) ? lo : ((v > hi) ? hi : v);
}

static inline void put_px(int x, int y)
{
    uint8_t *p = &s_rgb[((size_t) y * SNAP_W + (size_t) x) * SNAP_CH];
    p[0] = 0x00; /* box color: green */
    p[1] = 0xFF;
    p[2] = 0x00;
}

static void prv_hline(int x0, int x1, int y)
{
    if ((y < 0) || (y >= SNAP_H)) {
        return;
    }
    x0 = clampi(x0, 0, SNAP_W - 1);
    x1 = clampi(x1, 0, SNAP_W - 1);
    for (int x = x0; x <= x1; x++) {
        put_px(x, y);
    }
}

static void prv_vline(int x, int y0, int y1)
{
    if ((x < 0) || (x >= SNAP_W)) {
        return;
    }
    y0 = clampi(y0, 0, SNAP_H - 1);
    y1 = clampi(y1, 0, SNAP_H - 1);
    for (int y = y0; y <= y1; y++) {
        put_px(x, y);
    }
}

int iotc_snapshot_capture_upload(char *msg, size_t msg_size)
{
    const uint16_t *fb = (const uint16_t *) gp_next_buffer;

    if (fb == NULL) {
        if (msg) {
            snprintf(msg, msg_size, "no camera frame available");
        }
        return -11; /* -EAGAIN */
    }

    /* RGB565 center crop -> RGB888 (bit replication for full 0..255 range). */
    for (int y = 0; y < SNAP_H; y++) {
        const uint16_t *row = fb + (size_t) y * CAM_W + CROP_OFF_X;
        uint8_t *dst = &s_rgb[(size_t) y * SNAP_W * SNAP_CH];
        for (int x = 0; x < SNAP_W; x++) {
            uint16_t p = row[x];
            uint8_t r5 = (uint8_t) ((p >> 11) & 0x1F);
            uint8_t g6 = (uint8_t) ((p >> 5) & 0x3F);
            uint8_t b5 = (uint8_t) (p & 0x1F);
            dst[0] = (uint8_t) ((r5 << 3) | (r5 >> 2));
            dst[1] = (uint8_t) ((g6 << 2) | (g6 >> 4));
            dst[2] = (uint8_t) ((b5 << 3) | (b5 >> 2));
            dst += SNAP_CH;
        }
    }

    /* Draw current detections (model coords -> crop coords: x2.5). */
    uint32_t n = face_detection_box_count();
    float top_score = 0.0f;
    for (uint32_t i = 0; i < n; i++) {
        int16_t mx, my, mw, mh;
        float score;
        face_detection_box_get(i, &mx, &my, &mw, &mh, &score);
        if (score > top_score) {
            top_score = score;
        }
        int x = (mx * SCALE_NUM) / SCALE_DEN;
        int y = (my * SCALE_NUM) / SCALE_DEN;
        int w = (mw * SCALE_NUM) / SCALE_DEN;
        int h = (mh * SCALE_NUM) / SCALE_DEN;
        for (int t = 0; t < BOX_THICK; t++) {
            prv_hline(x, x + w, y + t);
            prv_hline(x, x + w, y + h - t);
            prv_vline(x + t, y, y + h);
            prv_vline(x + w - t, y, y + h);
        }
    }

    int png_len = png_encode(s_rgb, SNAP_W, SNAP_H, SNAP_CH, s_png, sizeof(s_png));
    if (png_len <= 0) {
        if (msg) {
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
    if (msg) {
        if (rc == 0) {
            snprintf(msg, msg_size, "%s uploaded (%d KB, %u face%s)",
                     name, png_len / 1024, (unsigned) n, (n == 1) ? "" : "s");
        } else {
            snprintf(msg, msg_size, "upload failed (%d)", rc);
        }
    }
    return rc;
}
