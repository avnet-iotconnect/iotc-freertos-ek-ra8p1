/*
* Copyright (c) 2020 - 2025 Renesas Electronics Corporation and/or its affiliates
*
* SPDX-License-Identifier: BSD-3-Clause
*/
/**********************************************************************************************************************
 * File Name    : image_classification_screen_mipi.c
 * Version      : .
 * Description  : The image classification screen display on the parallel lcd.
 *********************************************************************************************************************/
/***************************************************************************************************************************
 * Includes   <System Includes> , "Project Includes"
 ***************************************************************************************************************************/

#include "hal_data.h"
#include <stdio.h>

#include "common_util.h"
#include "display_layer.h"
#include "bg_font_18_full.h"
#include "time_counter.h"
#include "display_layer_config.h"
#include "ai_application_config.h"
#include "Labels.h"

/***************************************************************************************************************************
 * Macro definitions
 ***************************************************************************************************************************/
#define MAX_STR_LEN 15
#define INFERENCE_ROW_HEIGHT    30

#define TEXT_AREA_WIDTH         320
#define TEXT_AREA_HEIGHT        560

#define PERCENTAGE_OFF          50


/***************************************************************************************************************************
 * Typedef definitions
 ***************************************************************************************************************************/

/***************************************************************************************************************************
 * Imported global variables and functions (from other files)
 ***************************************************************************************************************************/

/***************************************************************************************************************************
 * Exported global variables and functions (to be accessed by other files)
 ***************************************************************************************************************************/

void  do_image_classification_screen(bool ai_result_new);
display_runtime_cfg_t glcd_layer_change_2;


/***************************************************************************************************************************
 * Private global variables and functions
 ***************************************************************************************************************************/
static void process_str(const char* input, char* output, int max_len);
static void print_inf_time(void);
static bool overlay_drawn = false;
static char local_str[5][32] = {0};
static char local_prob[5][8] = {0};


/*********************************************************************************************************************
 *  print_inf_time function
 *  This function prints the time used in the previously finished inference.
 *  @param      None
 *  @retval     None
***********************************************************************************************************************/
static void print_inf_time(void)
{
    extern volatile uint32_t g_ai_inference_time_us;
    uint32_t us = g_ai_inference_time_us;
    uint32_t infer_fps = (us > 0) ? (1000000U / us) : 0;
    uint32_t cam_ms = application_processing_time.camera_image_capture_time_ms;
    uint32_t cam_fps = (cam_ms > 0) ? (1000U / cam_ms) : 0;

    char buf[36];
    snprintf(buf, sizeof(buf), "%lu us / %lu fps    ",
             (unsigned long) us, (unsigned long) infer_fps);
    print_bg_font_18(d2_handle, 20, 245, NORMAL_FONT_SCALING, buf);
    snprintf(buf, sizeof(buf), "Camera: %lu fps    ", (unsigned long) cam_fps);
    print_bg_font_18(d2_handle, 20, 272, NORMAL_FONT_SCALING, buf);
}

/*********************************************************************************************************************
 *  process_str
 *  This function process the Label string.
 *  @param      input: Label
 *  @param      output: string
 *  @retval     None
***********************************************************************************************************************/
static void process_str(const char* input, char* output, int max_len) {
    int i;
    for (i = 0; input[i] != '\0' && i < max_len - 1; i++) {
        if (input[i] == ',') {
            break;
        }
        output[i] = input[i];
    }
    for(; i < max_len - 1; i++){
        output[i] = ' ';
    }
    output[max_len - 1] = '\0';
}

/*********************************************************************************************************************
 *  do_image_classification_screen function: display the camera image and image classification result on the mipi lcd
 *  @param       None
 *  @retval      None
***********************************************************************************************************************/
void do_image_classification_screen(bool ai_result_new)
{
    d2_point vpos = 355;
    d2_point hpos = 20;

    const char** labels = getLabelPtr();
    if(!(xEventGroupGetBits(g_ai_app_event) & DISPLAY_PAUSE))
    {

        graphics_start_frame();


        if (!overlay_drawn)
        {
            memset(fb_foreground, 0x0, 320*560*2);
            d2_framebuffer(d2_handle,
                           (void *)fb_foreground,         // Must match GLCDC framebuffer
                           TEXT_AREA_WIDTH,                  // pixel width
                           TEXT_AREA_WIDTH,                  // stride (pixels)
                           TEXT_AREA_HEIGHT,                 // height
                           d2_mode_rgb565);               // mode — must match GLCDC input format

            /* Start a new display list */
            d2_startframe(d2_handle);


            print_bg_font_18(d2_handle, 20,  25, IMAGE_CLASSIFICATION_FONT_SCALING, (char*)"/IOTCONNECT");

            print_bg_font_18(d2_handle, 20,  95, DISPLAY_FONT_SCALING, (char*)"Face Detection");

            print_bg_font_18(d2_handle, 20,  135, DISPLAY_FONT_SCALING, (char*)"Ethos-U55 NPU");

            /*show inference time in ms*/
            print_bg_font_18(d2_handle, 20, 205, DISPLAY_FONT_SCALING, (char*)"Ethos-U Inference Time:");

            print_bg_font_18(d2_handle, 20, 305, DISPLAY_FONT_SCALING, (char*)"Detections: ");

            /* Enable the back light */
            R_IOPORT_PinWrite(&g_ioport_ctrl, DISP_BLEN, BSP_IO_LEVEL_HIGH);
            overlay_drawn = true;
        }
        else if(ai_result_new)
        {
            extern uint32_t face_detection_box_count(void);
            extern void face_detection_box_get(uint32_t i, int16_t *x, int16_t *y,
                                               int16_t *w, int16_t *h, float *score);
            extern void face_detection_model_info(const char **name, unsigned *ver,
                                                  const char **src, unsigned *size_b);

            print_inf_time();

            const char *mname; const char *msrc; unsigned mver, msize;
            face_detection_model_info(&mname, &mver, &msrc, &msize);
            uint32_t n = face_detection_box_count();

            char line[40];
            snprintf(line, sizeof(line), "Model: %.12s v%u      ", mname, mver);
            print_bg_font_18(d2_handle, hpos, vpos, NORMAL_FONT_SCALING, line);
            snprintf(line, sizeof(line), "Source: %.8s      ", msrc);
            print_bg_font_18(d2_handle, hpos, vpos + INFERENCE_ROW_HEIGHT, NORMAL_FONT_SCALING, line);
            snprintf(line, sizeof(line), "Faces: %u      ", (unsigned) n);
            print_bg_font_18(d2_handle, hpos, vpos + INFERENCE_ROW_HEIGHT*2, NORMAL_FONT_SCALING, line);
            if (n > 0)
            {
                int16_t x, y, w, h; float score;
                face_detection_box_get(0, &x, &y, &w, &h, &score);
                snprintf(line, sizeof(line), "Best: %02d%%      ", (int)(score * 100.0f));
            }
            else
            {
                snprintf(line, sizeof(line), "Best: --      ");
            }
            print_bg_font_18(d2_handle, hpos, vpos + INFERENCE_ROW_HEIGHT*3, NORMAL_FONT_SCALING, line);
            (void) labels;
        }
        d2_endframe(d2_handle);
        d2_flushframe(d2_handle);
        /*Clean cache to ensure GLCDC sees latest images*/
        SCB_CleanDCache_by_Addr((uint32_t *)fb_foreground, TEXT_AREA_WIDTH * TEXT_AREA_HEIGHT * 2);

        (void)R_GLCDC_LayerChange(&g_plcd_display.p_ctrl, &glcd_layer_change_2, DISPLAY_FRAME_LAYER_2);

        }


        /* Wait for previous frame rendering to finish, then finalize this frame and flip the buffers */
        graphics_end_frame();
    }

