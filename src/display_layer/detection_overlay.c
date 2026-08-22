/*
 * detection_overlay.c - draw face-detection boxes into the camera frame
 * buffer (640x480 RGB565) before it is handed to the GLCDC.
 *
 * Model coordinates are 192x192 over the center 480x480 crop of the
 * 640x480 camera frame: cam_x = 80 + 2.5*x, cam_y = 2.5*y.
 */

#include <stdint.h>
#include <stddef.h>

#define CAM_W        640
#define CAM_H        480
#define CROP_OFF_X   80
#define SCALE_NUM    5
#define SCALE_DEN    2 /* 2.5 = 5/2 */
#define BOX_COLOR    0x07E0 /* RGB565 green */
#define BOX_THICK    3

extern uint32_t face_detection_box_count(void);
extern void face_detection_box_get(uint32_t i, int16_t *x, int16_t *y,
                                   int16_t *w, int16_t *h, float *score);

static inline int clampi(int v, int lo, int hi)
{
    return (v < lo) ? lo : ((v > hi) ? hi : v);
}

static void draw_hline(uint16_t *fb, int x0, int x1, int y)
{
    if ((y < 0) || (y >= CAM_H))
    {
        return;
    }
    x0 = clampi(x0, 0, CAM_W - 1);
    x1 = clampi(x1, 0, CAM_W - 1);
    uint16_t *row = fb + (size_t) y * CAM_W;
    for (int x = x0; x <= x1; x++)
    {
        row[x] = BOX_COLOR;
    }
}

static void draw_vline(uint16_t *fb, int x, int y0, int y1)
{
    if ((x < 0) || (x >= CAM_W))
    {
        return;
    }
    y0 = clampi(y0, 0, CAM_H - 1);
    y1 = clampi(y1, 0, CAM_H - 1);
    for (int y = y0; y <= y1; y++)
    {
        fb[(size_t) y * CAM_W + x] = BOX_COLOR;
    }
}

void detection_overlay_draw(void *frame_buffer)
{
    uint16_t *fb = (uint16_t *) frame_buffer;
    uint32_t n = face_detection_box_count();

    for (uint32_t i = 0; i < n; i++)
    {
        int16_t mx, my, mw, mh;
        float score;
        face_detection_box_get(i, &mx, &my, &mw, &mh, &score);

        int x = CROP_OFF_X + (mx * SCALE_NUM) / SCALE_DEN;
        int y = (my * SCALE_NUM) / SCALE_DEN;
        int w = (mw * SCALE_NUM) / SCALE_DEN;
        int h = (mh * SCALE_NUM) / SCALE_DEN;

        for (int t = 0; t < BOX_THICK; t++)
        {
            draw_hline(fb, x, x + w, y + t);
            draw_hline(fb, x, x + w, y + h - t);
            draw_vline(fb, x + t, y, y + h);
            draw_vline(fb, x + w - t, y, y + h);
        }
    }
}
