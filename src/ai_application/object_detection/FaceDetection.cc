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

#include <cmath>
#include <cstring>
#include <cstdio>
#include <new>
#include <vector>

extern "C" {
#include "hal_data.h"
#include "common_util.h"
#include "ai_application_config.h"
#include "console_output/console_output.h"
#include "model_store/model_store.h"
#include "time_counter/time_counter.h"
#include "../image_classification/Labels.h"

bool face_detection_init(void);
vision_ai_app_err_t face_detection_run(void);

/* Last NPU invoke duration in microseconds (DWT cycle counter). */
volatile uint32_t g_ai_inference_time_us;

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

/* Staging area for the ACTIVE model's flatbuffer. Lives in SDRAM like the
 * frame buffers; 16-byte alignment required by TFLM. */
#define MODEL_STAGING_MAX MODEL_MAX_BLOB
static uint8_t s_model_staging[MODEL_STAGING_MAX] BSP_ALIGN_VARIABLE(16)
    BSP_PLACE_IN_SECTION(BSP_UNINIT_SECTION_PREFIX ".sdram_noinit");
static size_t s_model_len;

/* Incoming IOTV blob awaiting hot-swap (written by the MQTT task, consumed
 * by the AI thread at a safe point between inferences). */
static uint8_t s_model_pending[MODEL_MAX_BLOB] BSP_ALIGN_VARIABLE(16)
    BSP_PLACE_IN_SECTION(BSP_UNINIT_SECTION_PREFIX ".sdram_noinit");
static volatile size_t s_pending_len; /* nonzero => swap requested */

/* Active model metadata for telemetry/commands. */
static char s_model_name[16] = "builtin";
static uint16_t s_model_ver = 1;
static char s_model_src[8] = "builtin"; /* builtin | flash | cloud */

/* Tensor arena: worst-case size for the models we intend to hot-swap
 * (largest: MobileNet v1 0.5 at ~590 KiB per the vela report). */
static uint8_t s_arena[0xA0000] BSP_ALIGN_VARIABLE(16);

static arm::app::YoloFastestModel s_model;
static uint8_t s_gray[FD_INPUT_W * FD_INPUT_H];

/* Model family, decided from the input tensor shape at load time:
 * 192x192x1 -> YOLO face detector, 224x224x3 -> ImageNet classifier. */
typedef enum { MODEL_KIND_DETECTOR = 0, MODEL_KIND_CLASSIFIER } model_kind_t;
static model_kind_t s_model_kind;

/* Latest classification result (classifier models only). */
static char s_class_label[32] = "-";
static int s_class_pct;

/*
 * Map a class index to a display label. The ImageNet table (Labels.c) has
 * 1000 entries starting at class "Tench"; the stock MobileNet quant models
 * output 1001 classes with index 0 = background, so those need a -1 shift
 * (showing the neighbouring class otherwise - looks like random labels).
 * Tiny 2-class outputs are person detectors (visual wake words layout).
 */
static const char *prv_class_label(size_t idx, size_t n_classes)
{
    if (n_classes <= 2) {
        return (idx == 1) ? "person" : "no person";
    }
    size_t n_labels = (size_t) getLabelSize();
    if (n_classes == n_labels + 1) { /* background at index 0 */
        if (idx == 0) {
            return "background";
        }
        idx--;
    }
    return (idx < n_labels) ? getLabelPtr()[idx] : "?";
}

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

/* (Re)initialise TFLM from the staging buffer. */
static bool prv_model_apply(size_t len)
{
    s_model_len = len;
    /* Frame buffers use write-through SDRAM; clean D-cache anyway so the NPU
     * (a separate bus master) sees the staged model. */
    SCB_CleanDCache_by_Addr(s_model_staging, (int32_t) len);

    /* Model construction is one-shot per instance; destroy and placement-new
     * a fresh wrapper over the same static storage for hot-swap (frees the
     * previous heap-allocated MicroInterpreter; the arena is reused). */
    s_model.~YoloFastestModel();
    new (&s_model) arm::app::YoloFastestModel();
    if (!s_model.Init(s_arena, sizeof(s_arena), s_model_staging, (uint32_t) len)) {
        FD_PRINT("FD: TFLM model init FAILED\r\n");
        return false;
    }

    /* Classify the model family by its input shape; reject shapes the
     * capture pipeline cannot feed. dims = [1, H, W, C]. */
    TfLiteTensor *in = s_model.GetInputTensor(0);
    if ((in == NULL) || (in->dims->size != 4)) {
        FD_PRINT("FD: model rejected: unexpected input tensor\r\n");
        return false;
    }
    int h = in->dims->data[1], w = in->dims->data[2], c = in->dims->data[3];
    if ((h == FD_INPUT_H) && (w == FD_INPUT_W) && (c == 1) &&
        (2 == s_model.GetNumOutputs())) {
        s_model_kind = MODEL_KIND_DETECTOR;
    } else if ((1 == s_model.GetNumOutputs()) && ((c == 1) || (c == 3)) &&
               (h >= 32) && (h <= AI_INPUT_IMAGE_HEIGHT) &&
               (w >= 32) && (w <= AI_INPUT_IMAGE_WIDTH)) {
        /* Any single-output color or grayscale classifier up to the camera
         * staging size: the input is resampled from the 224x224 RGB frame. */
        s_model_kind = MODEL_KIND_CLASSIFIER;
    } else {
        FD_PRINT("FD: model rejected: unsupported shape %dx%dx%d/%u-out\r\n",
                 h, w, c, (unsigned) s_model.GetNumOutputs());
        return false;
    }
    s_box_count = 0;
    strncpy(s_class_label, "-", sizeof(s_class_label));
    s_class_pct = 0;

    FD_PRINT("FD: model \"%s\" v%u (%s, %u bytes) loaded: %s, ethos-u: %s\r\n",
             s_model_name, (unsigned) s_model_ver, s_model_src, (unsigned) len,
             (s_model_kind == MODEL_KIND_CLASSIFIER) ? "classifier" : "face detector",
             s_model.ContainsEthosUOperator() ? "yes" : "no");
    return true;
}

static void prv_load_builtin_to_staging(void)
{
    const uint8_t *builtin = arm::app::object_detection::GetModelPointer();
    size_t len = arm::app::object_detection::GetModelLen();
    memcpy(s_model_staging, builtin, len);
    s_model_len = len;
    strncpy(s_model_name, "builtin", sizeof(s_model_name) - 1);
    s_model_ver = 1;
    strncpy(s_model_src, "builtin", sizeof(s_model_src) - 1);
}

bool face_detection_init(void)
{
    /* Prefer a model persisted in OSPI flash; fall back to the builtin. */
    size_t blob_len = 0;
    int rc = model_store_load(s_model_pending, sizeof(s_model_pending), &blob_len);
    if (0 == rc)
    {
        const struct iotv_hdr *h = (const struct iotv_hdr *) s_model_pending;
        memcpy(s_model_staging, s_model_pending + IOTV_HDR_LEN, h->model_len);
        memcpy(s_model_name, h->name, sizeof(s_model_name));
        s_model_name[sizeof(s_model_name) - 1] = 0;
        s_model_ver = h->model_ver;
        strncpy(s_model_src, "flash", sizeof(s_model_src) - 1);
        if (prv_model_apply(h->model_len))
        {
            return true;
        }
        FD_PRINT("FD: stored model failed to load; reverting to builtin\r\n");
        (void) model_store_erase();
    }
    else
    {
        FD_PRINT("FD: no stored model (%d); using builtin\r\n", rc);
    }

    prv_load_builtin_to_staging();
    bool ok = prv_model_apply(s_model_len);

#define MODEL_STORE_SELFTEST 0
#if MODEL_STORE_SELFTEST
    /* One-shot persistence test: seed the OSPI store with the builtin model
     * wrapped as IOTV "selftest" v2. The NEXT boot must report
     * model "selftest" v2 (flash). Disable before demo release. */
    if (ok)
    {
        memcpy(s_model_pending + IOTV_HDR_LEN, s_model_staging, s_model_len);
        iotv_wrap_in_place(s_model_pending, s_model_len, 2, "selftest");
        int src = model_store_save(s_model_pending, s_model_len + IOTV_HDR_LEN);
        FD_PRINT("FD: selftest: model_store_save -> %d (reboot to verify flash load)\r\n", src);
    }
#endif
    return ok;
}

/*
 * Hot-swap request from another task (cloud model push). blob is a full
 * IOTV envelope. Returns NULL on success (queued) or a reason string.
 */
extern "C" const char *face_detection_request_swap(const uint8_t *iotv_blob, size_t len)
{
    const char *reason = iotv_validate(iotv_blob, len);
    if (reason)
    {
        return reason;
    }
    if (s_pending_len)
    {
        return "swap already pending";
    }
    if (iotv_blob != s_model_pending)
    {
        memcpy(s_model_pending, iotv_blob, len);
    }
    s_pending_len = len; /* consumed by the AI thread */
    return NULL;
}

/* Download target for the cloud push: write straight into the pending
 * buffer, then call face_detection_request_swap(face_detection_pending_buf(), n). */
extern "C" uint8_t *face_detection_pending_buf(size_t *size)
{
    if (size)
    {
        *size = sizeof(s_model_pending);
    }
    return s_model_pending;
}

extern "C" void face_detection_revert(void)
{
    (void) model_store_erase();
    const uint8_t *builtin = arm::app::object_detection::GetModelPointer();
    size_t len = arm::app::object_detection::GetModelLen();
    /* Wrap builtin as IOTV into pending so the swap path is uniform. */
    memcpy(s_model_pending + IOTV_HDR_LEN, builtin, len);
    iotv_wrap_in_place(s_model_pending, len, 1, "builtin");
    s_pending_len = len + IOTV_HDR_LEN;
}

extern "C" void face_detection_model_info(const char **name, unsigned *ver,
                                          const char **src, unsigned *size_b)
{
    if (name) { *name = s_model_name; }
    if (ver) { *ver = s_model_ver; }
    if (src) { *src = s_model_src; }
    if (size_b) { *size_b = (unsigned) s_model_len; }
}

/* Classifier-mode result. Returns false when a detector model is active
 * (label/pct untouched). */
extern "C" bool face_detection_class_info(const char **label, int *pct)
{
    if (s_model_kind != MODEL_KIND_CLASSIFIER) {
        return false;
    }
    if (label) { *label = s_class_label; }
    if (pct) { *pct = s_class_pct; }
    return true;
}

/* Called on the AI thread between inferences. */
static void prv_swap_if_pending(void)
{
    if (0 == s_pending_len)
    {
        return;
    }
    size_t len = s_pending_len;
    const struct iotv_hdr *h = (const struct iotv_hdr *) s_model_pending;

    FD_PRINT("FD: hot-swapping to model \"%.15s\" v%u (%u bytes)\r\n",
             h->name, (unsigned) h->model_ver, (unsigned) h->model_len);

    memcpy(s_model_staging, s_model_pending + IOTV_HDR_LEN, h->model_len);
    memcpy(s_model_name, h->name, sizeof(s_model_name));
    s_model_name[sizeof(s_model_name) - 1] = 0;
    s_model_ver = h->model_ver;
    bool from_builtin = (0 == strncmp(h->name, "builtin", 7));
    strncpy(s_model_src, from_builtin ? "builtin" : "cloud", sizeof(s_model_src) - 1);

    if (prv_model_apply(h->model_len))
    {
        if (!from_builtin)
        {
            if (0 == model_store_save(s_model_pending, len))
            {
                strncpy(s_model_src, "cloud", sizeof(s_model_src) - 1);
                FD_PRINT("FD: model persisted to OSPI flash store\r\n");
            }
            else
            {
                FD_PRINT("FD: WARNING: model persist failed (still running from RAM)\r\n");
            }
        }
    }
    else
    {
        FD_PRINT("FD: swap failed; reloading builtin\r\n");
        prv_load_builtin_to_staging();
        (void) prv_model_apply(s_model_len);
    }
    s_pending_len = 0;
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
    prv_swap_if_pending();

    if (!s_model.IsInited()) {
        return VISION_AI_APP_ERR_AI_INFERENCE;
    }

    TfLiteTensor *input = s_model.GetInputTensor(0);

    if (s_model_kind == MODEL_KIND_CLASSIFIER) {
        /* Resample the camera thread's 224x224 RGB888 staging frame to the
         * classifier's input (any WxH, RGB or luma), nearest-neighbour.
         * int8-quantised inputs take value-128, done as ^0x80. */
        const uint8_t *rgb = (const uint8_t *) model_buffer_int8;
        const int th = input->dims->data[1];
        const int tw = input->dims->data[2];
        const int tc = input->dims->data[3];
        const uint8_t flip = s_model.IsDataSigned() ? 0x80 : 0x00;
        uint8_t *dst = input->data.uint8; /* aliases data.int8 */
        size_t o = 0;
        for (int y = 0; y < th; y++) {
            const int sy = (y * AI_INPUT_IMAGE_HEIGHT) / th;
            const uint8_t *row = rgb + (size_t) sy * AI_INPUT_IMAGE_WIDTH * 3;
            for (int x = 0; x < tw; x++) {
                const uint8_t *px =
                    row + (size_t) ((x * AI_INPUT_IMAGE_WIDTH) / tw) * 3;
                if (tc == 3) {
                    dst[o++] = (uint8_t) (px[0] ^ flip);
                    dst[o++] = (uint8_t) (px[1] ^ flip);
                    dst[o++] = (uint8_t) (px[2] ^ flip);
                } else {
                    uint8_t g8 = (uint8_t) ((77u * px[0] + 150u * px[1] +
                                             29u * px[2]) >> 8);
                    dst[o++] = (uint8_t) (g8 ^ flip);
                }
            }
        }
    } else {
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
    }

    /* Time the NPU invoke with the app's 100 us tick counter: the U55
     * finishes this model in ~1 ms, below the 1 ms resolution the donor
     * used (which is why its readout showed 0). The DWT cycle counter is
     * debug-gated on this core, so use the peripheral timer instead. */
    uint32_t t0 = TimeCounter_CurrentCountGet();
    bool inference_ok = s_model.RunInference();
    g_ai_inference_time_us = (TimeCounter_CurrentCountGet() - t0) * 100U;
    /* Keep the donor field alive for anything still reading it (rounded up
     * so a sub-ms inference no longer reads 0). */
    application_processing_time.ai_inference_time_ms = (g_ai_inference_time_us + 999U) / 1000U;

    if (!inference_ok) {
        return VISION_AI_APP_ERR_AI_INFERENCE;
    }

    if (s_model_kind == MODEL_KIND_CLASSIFIER) {
        TfLiteTensor *out = s_model.GetOutputTensor(0);
        if (out == NULL) {
            return VISION_AI_APP_ERR_AI_INFERENCE;
        }
        /* Top-1 over the quantised class vector. */
        size_t n_classes = out->bytes;
        size_t best = 0;
        int best_q = -256;
        for (size_t i = 0; i < n_classes; i++) {
            int q = s_model.IsDataSigned() ? (int) out->data.int8[i]
                                           : (int) out->data.uint8[i];
            if (q > best_q) {
                best_q = q;
                best = i;
            }
        }
        float score = out->params.scale *
                      ((float) best_q - (float) out->params.zero_point);
        /* v1-style quant models emit post-softmax probabilities (scale
         * 1/256); v2-style emit LOGITS, where scale*(q-zp) can be >> 1
         * (showed as e.g. "722%"). If the top value cannot be a
         * probability, softmax the dequantised vector: with the max as
         * reference, prob(best) = 1 / sum(exp(x_i - x_best)). */
        if (score > 1.001f) {
            float sum = 0.0f;
            for (size_t i = 0; i < n_classes; i++) {
                int q = s_model.IsDataSigned() ? (int) out->data.int8[i]
                                               : (int) out->data.uint8[i];
                float x = out->params.scale *
                          ((float) q - (float) out->params.zero_point);
                sum += expf(x - score);
            }
            score = 1.0f / sum;
        }
        int pct = (int) (score * 100.0f + 0.5f);
        if (pct > 100) {
            pct = 100;
        }
        const char *label = prv_class_label(best, n_classes);

        s_box_count = 0;
        int changed = (0 != strncmp(s_class_label, label, sizeof(s_class_label) - 1));
        snprintf(s_class_label, sizeof(s_class_label), "%s", label);
        s_class_pct = pct;
        if (changed && pct >= 30) {
            FD_PRINT("IC: %s (%d%%)\r\n", s_class_label, pct);
        }
        return VISION_AI_APP_SUCCESS;
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
