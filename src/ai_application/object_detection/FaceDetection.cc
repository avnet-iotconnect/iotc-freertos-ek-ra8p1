/*
 * FaceDetection.cc - TFLM + Ethos-U55 face detection for the IOTCONNECT
 * EK-RA8P1 Vision AI demo.
 *
 * Replaces the MERA/RUHMI code-generated inference path with a TensorFlow
 * Lite Micro interpreter running a Vela-compiled YOLO Fastest 192x192 face
 * model. The interpreter takes the model as a pointer + length at runtime;
 * to prove the model-hot-swap mechanism (IoTConnect AI Model Management,
 * Phase 5) the built-in model is first staged into an SDRAM buffer and the
 * interpreter is initialised from THAT copy - the same path a cloud-pushed
 * model will take after download from OSPI flash.
 */

#include "YoloFastestModel.hpp"
#include "DetectorPostProcessing.h"
#include "log_macros.h"

#include <cstring>
#include <cstdio>
#include <vector>

extern "C" {
#include "hal_data.h"
#include "common_util.h"
#include "ai_application_config.h"
#include "console_output/console_output.h"

bool face_detection_init(void);
vision_ai_app_err_t face_detection_run(void);

/* Camera output staged by camera_thread: AI_INPUT_IMAGE_WIDTH x HEIGHT RGB888. */
extern int8_t model_buffer_int8[];
}

namespace arm {
namespace app {
namespace object_detection {
extern const uint8_t *GetModelPointer();
extern size_t GetModelLen();
} // namespace object_detection
} // namespace app
} // namespace arm

#define FD_INPUT_W   192
#define FD_INPUT_H   192
#define FD_MAX_BOXES 8

/* Staging area for runtime-loaded models (builtin now; cloud-pushed later).
 * Lives in SDRAM like the frame buffers; 16-byte alignment required by TFLM. */
#define MODEL_STAGING_MAX (1024 * 1024)
static uint8_t s_model_staging[MODEL_STAGING_MAX] BSP_ALIGN_VARIABLE(16)
    BSP_PLACE_IN_SECTION(BSP_UNINIT_SECTION_PREFIX ".sdram_noinit");
static size_t s_model_len;

/* Tensor arena: worst-case size for the models we intend to hot-swap. */
static uint8_t s_arena[0x80000] BSP_ALIGN_VARIABLE(16);

static arm::app::YoloFastestModel s_model;
static uint8_t s_gray[FD_INPUT_W * FD_INPUT_H];

/* Latest detection results, for telemetry (Phase 3) and display overlay. */
typedef struct {
    int16_t x, y, w, h; /* in model input coordinates (192x192) */
    float score;
} fd_box_t;
static fd_box_t s_boxes[FD_MAX_BOXES];
static uint32_t s_box_count;

extern "C" uint32_t face_detection_box_count(void) { return s_box_count; }
extern "C" void face_detection_box_get(uint32_t i, int16_t *x, int16_t *y,
                                       int16_t *w, int16_t *h, float *score)
{
    if (i < s_box_count) {
        *x = s_boxes[i].x; *y = s_boxes[i].y;
        *w = s_boxes[i].w; *h = s_boxes[i].h;
        *score = s_boxes[i].score;
    }
}

static char s_line[192];
#define FD_PRINT(...)                                   \
    do {                                                \
        snprintf(s_line, sizeof(s_line), __VA_ARGS__);  \
        print_to_console(s_line);                       \
    } while (0)

bool face_detection_init(void)
{
    /* Stage the builtin model into SDRAM - runtime load, not linked address. */
    const uint8_t *builtin = arm::app::object_detection::GetModelPointer();
    size_t len = arm::app::object_detection::GetModelLen();
    if (len > sizeof(s_model_staging)) {
        FD_PRINT("FD: model too large for staging (%u bytes)\r\n", (unsigned) len);
        return false;
    }
    memcpy(s_model_staging, builtin, len);
    s_model_len = len;
    /* Frame buffers use write-through SDRAM; clean D-cache anyway so the NPU
     * (a separate bus master) sees the staged model. */
    SCB_CleanDCache_by_Addr(s_model_staging, (int32_t) len);

    if (!s_model.Init(s_arena, sizeof(s_arena), s_model_staging, (uint32_t) len)) {
        FD_PRINT("FD: TFLM model init FAILED\r\n");
        return false;
    }
    FD_PRINT("FD: YOLO Fastest 192x192 loaded from SDRAM staging (%u bytes), "
             "ethos-u delegated: %s\r\n",
             (unsigned) len, s_model.ContainsEthosUOperator() ? "yes" : "no");
    return true;
}

/* Nearest-neighbour downscale of the camera's RGB888 square frame to
 * FD_INPUT_W x FD_INPUT_H with RGB -> luma conversion in one pass. */
static void rgb224_to_gray192(const uint8_t *rgb)
{
    for (int y = 0; y < FD_INPUT_H; y++) {
        const int sy = (y * AI_INPUT_IMAGE_HEIGHT) / FD_INPUT_H;
        const uint8_t *row = rgb + (size_t) sy * AI_INPUT_IMAGE_WIDTH * 3;
        uint8_t *dst = &s_gray[(size_t) y * FD_INPUT_W];
        for (int x = 0; x < FD_INPUT_W; x++) {
            const int sx = (x * AI_INPUT_IMAGE_WIDTH) / FD_INPUT_W;
            const uint8_t *px = row + (size_t) sx * 3;
            dst[x] = (uint8_t) ((77u * px[0] + 150u * px[1] + 29u * px[2]) >> 8);
        }
    }
}

vision_ai_app_err_t face_detection_run(void)
{
    if (!s_model.IsInited()) {
        return VISION_AI_APP_ERR_AI_INFERENCE;
    }

    TfLiteTensor *input = s_model.GetInputTensor(0);

    rgb224_to_gray192((const uint8_t *) model_buffer_int8);

    /* Fill the input tensor: grayscale, then shift to int8 if quantised signed. */
    const size_t want = (size_t) FD_INPUT_W * FD_INPUT_H;
    size_t n = (input->bytes < want) ? input->bytes : want;
    if (s_model.IsDataSigned()) {
        int8_t *d = input->data.int8;
        for (size_t i = 0; i < n; i++) {
            d[i] = (int8_t) ((int) s_gray[i] - 128);
        }
    } else {
        memcpy(input->data.uint8, s_gray, n);
    }

    if (!s_model.RunInference()) {
        return VISION_AI_APP_ERR_AI_INFERENCE;
    }

    TfLiteTensor *outputs[2] = {s_model.GetOutputTensor(0), s_model.GetOutputTensor(1)};
    if ((NULL == outputs[0]) || (NULL == outputs[1])) {
        return VISION_AI_APP_ERR_AI_INFERENCE;
    }

    std::vector<arm::app::DetectionResult> results;
    runPostProcessing(s_gray, outputs, results);

    s_box_count = 0;
    for (size_t i = 0; i < results.size() && i < FD_MAX_BOXES; i++) {
        s_boxes[i].x = (int16_t) results[i].m_x0;
        s_boxes[i].y = (int16_t) results[i].m_y0;
        s_boxes[i].w = (int16_t) results[i].m_w;
        s_boxes[i].h = (int16_t) results[i].m_h;
        s_boxes[i].score = (float) results[i].m_normalisedVal;
        s_box_count++;
    }

    if (s_box_count > 0) {
        FD_PRINT("FD: %u face(s):", (unsigned) s_box_count);
        for (uint32_t i = 0; i < s_box_count; i++) {
            FD_PRINT(" [%d,%d %dx%d %d%%]", s_boxes[i].x, s_boxes[i].y,
                     s_boxes[i].w, s_boxes[i].h, (int) (s_boxes[i].score * 100.0f));
        }
        FD_PRINT("\r\n");
    }

    return VISION_AI_APP_SUCCESS;
}
