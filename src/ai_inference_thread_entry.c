/*
* Copyright (c) 2020 - 2025 Renesas Electronics Corporation and/or its affiliates
*
* SPDX-License-Identifier: BSD-3-Clause
*/
/**********************************************************************************************************************
 * File Name    : ai_inference_thread_entry.c
 * Description  : This file defines the entry function of the ai inference thread and activates the inference.
 **********************************************************************************************************************/


/***********************************************************************************************************************
 * Includes
 **********************************************************************************************************************/

#include "ai_inference_thread.h"
#include <stdio.h>
#include "common_util.h"
#include "app_config.h"
#include "common_data.h"
#include "ai_application_config.h"
#include "time_counter.h"
#include "console_output.h"
#include "tensorflow/lite/micro/cortex_m_generic/debug_log_callback.h"


/***************************************************************************************************************************
 * Macro definitions
 ***************************************************************************************************************************/
#define AI_THREAD_YIELD                     (400)

/***************************************************************************************************************************
 * Typedef definitions
 ***************************************************************************************************************************/
/***************************************************************************************************************************
 * Imported global variables and functions (from other files)
 ***************************************************************************************************************************/

extern bool face_detection_init(void);
extern vision_ai_app_err_t face_detection_run(void);
extern uint32_t face_detection_box_count(void);
extern bool face_detection_class_info(const char **label, int *pct);

/***************************************************************************************************************************
 * Detection LED (led-auto command)
 ***************************************************************************************************************************/

/*
 * The board's green user LED follows the detection state while enabled: lit
 * whenever a face or a person is in frame, dark otherwise. Updated here, once
 * per inference, so it tracks detections at the pipeline's own rate and works
 * with no display attached.
 *
 * Off by default, so a board behaves exactly as before until asked.
 *
 * This lives in the inference thread rather than a module of its own because
 * the generated build files list every source explicitly; a new directory
 * would be dropped the next time the Smart Configurator regenerates them.
 */
static volatile bool s_led_auto;

void led_auto_set(bool on)
{
    s_led_auto = on;
    if (!on)
    {
        /* Leave the LED dark when switching the feature off, rather than
         * frozen in whatever state the last detection left it. */
        (void) R_IOPORT_PinWrite(&g_ioport_ctrl, USER_LED_GREEN, BSP_IO_LEVEL_LOW);
    }
}

bool led_auto_get(void)
{
    return s_led_auto;
}

static void prv_led_auto_update(void)
{
    if (!s_led_auto)
    {
        return;
    }

    /* Classifier models report a label (the 2-class person detector emits
     * "person"/"no person"); detector models report boxes. */
    const char *label = NULL;
    bool detected;
    if (face_detection_class_info(&label, NULL))
    {
        detected = (NULL != label) && (0 == strcmp(label, "person"));
    }
    else
    {
        detected = (face_detection_box_count() > 0U);
    }

    (void) R_IOPORT_PinWrite(&g_ioport_ctrl, USER_LED_GREEN,
                             detected ? BSP_IO_LEVEL_HIGH : BSP_IO_LEVEL_LOW);
}

/***************************************************************************************************************************
 * Exported global variables and functions (to be accessed by other files)
 ***************************************************************************************************************************/

/* Inference engine input buffer */

/* In SDRAM: frees ~144 KB of SRAM for the FreeRTOS heap (KVS WebRTC). The
 * buffer is written sequentially by pre-processing and read by the NPU via
 * AXI, so the uncached-SDRAM penalty is small. */
int8_t model_buffer_int8[AI_INPUT_IMAGE_WIDTH * AI_INPUT_IMAGE_HEIGHT * AI_INPUT_IMAGE_BYTE_PER_PIXEL]
    BSP_ALIGN_VARIABLE(8) BSP_PLACE_IN_SECTION(BSP_UNINIT_SECTION_PREFIX ".sdram_noinit");

uint32_t model_buffer_int8_size = sizeof(model_buffer_int8);
st_ai_classification_point_t g_ai_classification[AI_MAX_DETECTION_NUM] = {};

void update_detection_result(uint16_t index, signed short  x, signed short  y, signed short  w, signed short  h);

/***************************************************************************************************************************
 * Private global variables and functions
 ***************************************************************************************************************************/

/*********************************************************************************************************************
 *  Read the image classification result to a buffer which will be used by the display function.
 *  @param[IN]   index: index of the image in the result list
 *  @param[IN]   category: image category identified
 *  @param[IN]   probability: confidence percentage for this category
 *  @retval      None
***********************************************************************************************************************/
void update_classification_result(unsigned index, unsigned short category, float probability)
{
    if(index < AI_MAX_DETECTION_NUM)
    {
        g_ai_classification[index].prob = probability;
        g_ai_classification[index].category = category;
    }
    if(index == (AI_MAX_DETECTION_NUM - 1)) {
        float sum = (float)(1e-6);
        for(uint32_t i = 0; i < AI_MAX_DETECTION_NUM; i++) {
            sum += g_ai_classification[i].prob;
        }
        for(uint32_t i = 0; i < AI_MAX_DETECTION_NUM; i++) {
            g_ai_classification[i].prob /= sum;
        }
    }
}

/* This function can be called to print debug message */
static void print_log(const char* s)
{
    print_to_console(s);
}

/*********************************************************************************************************************
 *  AI thread entry function. The image will be processed prior to inference.
 *  The image processing and inference is repeatedly carried out in this thread.
 *  @param[IN]      void *pvParameters, contains TaskHandle_t, not used.
 *  @retval      None
***********************************************************************************************************************/
void ai_inference_thread_entry(void *pvParameters)
{
    FSP_PARAMETER_NOT_USED (pvParameters);

    vision_ai_app_err_t vision_ai_status = VISION_AI_APP_SUCCESS;

    /* Wait for display and camera initialization complete */
    xEventGroupWaitBits(g_ai_app_event, (HARDWARE_DISPLAY_INIT_DONE | HARDWARE_CAMERA_INIT_DONE), pdFALSE, pdTRUE, portMAX_DELAY);

    RM_ETHOSU_Open(&g_rm_ethosu0_ctrl, &g_rm_ethosu0_cfg);

    xEventGroupSetBits(g_ai_app_event, HARDWARE_ETHOSU_INIT_DONE);

    RegisterDebugLogCallback(print_log);

    if(true != face_detection_init())
    {
        handle_error(VISION_AI_APP_ERR_AI_INIT);
    }

    xEventGroupSetBits(g_ai_app_event, SOFTWARE_AI_INFERENCE_INIT_DONE);

   


    while (true)
    {
        xEventGroupWaitBits(g_ai_app_event, AI_INFERENCE_INPUT_IMAGE_READY, pdTRUE, pdTRUE, portMAX_DELAY);

#if H264_BENCH
        {
            /* One-shot: measure software H.264 encode on live frames before
             * inference starts competing for the CPU. */
            extern void h264_bench_run(void);
            static bool s_bench_done = false;
            if (!s_bench_done)
            {
                s_bench_done = true;
                h264_bench_run();
            }
        }
#endif

        for(int i = 0; i < AI_MAX_DETECTION_NUM; i++)
        {
            memset(&g_ai_classification[i], 0, sizeof(g_ai_classification[i]));
        }
        vision_ai_status = face_detection_run();

        if(VISION_AI_APP_ERR_AI_INFERENCE == vision_ai_status)
        {
            handle_error(VISION_AI_APP_ERR_AI_INFERENCE);
        }

        prv_led_auto_update();

        xEventGroupSetBits(g_ai_app_event, AI_INFERENCE_RESULT_UPDATED);
         /*
         * Yield to the display thread. The AI thread does not need to run faster than human reaction and response time,
         * so a relatively larger delay is used. This value should not be too low; otherwise, the display thread
         * is negatively influenced. This value should be reevaluated if the system is updated.
         */
        vTaskDelay(AI_THREAD_YIELD);
    }
}
