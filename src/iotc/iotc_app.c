/*
 * iotc_app.c - IoTConnect application glue for the EK-RA8P1 Vision AI demo.
 *
 * Driven from net_thread: once the network is up, sync time, run
 * discovery/identity, connect MQTT, then publish vision telemetry
 * periodically and dispatch cloud-to-device commands.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "FreeRTOS.h"
#include "task.h"

#include "app_config.h"
#include "iotconnect.h"
#include "iotc_time.h"

#include "iotcl.h"
#include "iotcl_log.h"
#include "iotcl_c2d.h"
#include "iotcl_telemetry.h"

#include "console_output/console_output.h"

#include "iotc_dra_client.h"
#include "iotc_snapshot.h"
#include "iotcl_util.h"
#include "model_store/model_store.h"

/* Vision pipeline interface (FaceDetection.cc). */
extern uint32_t face_detection_box_count(void);
extern void face_detection_box_get(uint32_t i, int16_t *x, int16_t *y,
                                   int16_t *w, int16_t *h, float *score);
extern const char *face_detection_request_swap(const uint8_t *iotv_blob, size_t len);
extern uint8_t *face_detection_pending_buf(size_t *size);
extern void face_detection_revert(void);
extern void face_detection_model_info(const char **name, unsigned *ver,
                                      const char **src, unsigned *size_b);

#define IOTC_TELEMETRY_PERIOD_S_DEFAULT 10

static char s_print[160];
#define IOTC_PRINT(...)                                     \
    do {                                                    \
        snprintf(s_print, sizeof(s_print), __VA_ARGS__);    \
        print_to_console(s_print);                          \
    } while (0)

typedef enum
{
    IOTC_APP_IDLE = 0,
    IOTC_APP_STARTING,
    IOTC_APP_RUNNING,
    IOTC_APP_FAILED,
} iotc_app_state_t;

static iotc_app_state_t s_state;
static uint32_t s_period_s = IOTC_TELEMETRY_PERIOD_S_DEFAULT;
static TickType_t s_last_pub;
static uint32_t s_msgs_sent;

/* Snapshot requests are deferred: the command callback runs on the MQTT pump
 * task, but the upload takes seconds (TLS x3 + ~230 KB PUT) and must not
 * stall the process loop. The net thread performs it from iotc_app_poll(). */
static volatile bool s_snapshot_pending;
static char *s_snapshot_ack_id;

static void prv_on_command(IotclC2dEventData data)
{
    const char *cmd = iotcl_c2d_get_command(data);
    const char *ack_id = iotcl_c2d_get_ack_id(data);
    if (!cmd)
    {
        return;
    }
    IOTC_PRINT("IOTC: command \"%s\"\r\n", cmd);

    if (0 == strncmp(cmd, "set-interval ", 13))
    {
        int v = atoi(&cmd[13]);
        if (v >= 1 && v <= 3600)
        {
            s_period_s = (uint32_t) v;
            iotcl_mqtt_send_cmd_ack(ack_id, IOTCL_C2D_EVT_CMD_SUCCESS_WITH_ACK, "interval set");
            return;
        }
        iotcl_mqtt_send_cmd_ack(ack_id, IOTCL_C2D_EVT_CMD_FAILED, "bad interval");
        return;
    }
    if (0 == strcmp(cmd, "snapshot"))
    {
        if (s_snapshot_pending)
        {
            iotcl_mqtt_send_cmd_ack(ack_id, IOTCL_C2D_EVT_CMD_FAILED,
                                    "snapshot already in progress");
            return;
        }
        iotcl_free(s_snapshot_ack_id);
        s_snapshot_ack_id = ack_id ? iotcl_strdup(ack_id) : NULL;
        s_snapshot_pending = true; /* net thread picks it up */
        return;
    }
    if (0 == strcmp(cmd, "model-info"))
    {
        const char *name; const char *src; unsigned ver, size_b;
        face_detection_model_info(&name, &ver, &src, &size_b);
        char info[96];
        snprintf(info, sizeof(info), "%s v%u (%s, %u bytes)", name, ver, src, size_b);
        iotcl_mqtt_send_cmd_ack(ack_id, IOTCL_C2D_EVT_CMD_SUCCESS_WITH_ACK, info);
        return;
    }
    if (0 == strcmp(cmd, "model-revert"))
    {
        face_detection_revert();
        iotcl_mqtt_send_cmd_ack(ack_id, IOTCL_C2D_EVT_CMD_SUCCESS_WITH_ACK,
                                "reverting to builtin model");
        return;
    }
    iotcl_mqtt_send_cmd_ack(ack_id, IOTCL_C2D_EVT_CMD_FAILED, "unknown command");
}

/*
 * AI Model Management push (ct:2) / OTA. Downloads the blob into the vision
 * pipeline's pending buffer, validates the IOTV envelope (unwrapping a raw
 * .tflite or STORED zip if needed is done by pack_model.py conventions:
 * we push .iotv blobs), and queues the hot-swap. Runs on the MQTT pump
 * task; the AI thread applies the swap between inferences and persists the
 * model to the OSPI flash store.
 */
static void prv_on_ota(IotclC2dEventData data)
{
    const char *host = iotcl_c2d_get_ota_url_hostname(data, 0);
    const char *res = iotcl_c2d_get_ota_url_resource(data, 0);
    const char *ack_id = iotcl_c2d_get_ack_id(data);
    IOTC_PRINT("IOTC: model push from https://%s%s\r\n", host ? host : "?", res ? res : "");

    if (!host || !res)
    {
        if (ack_id)
        {
            iotcl_mqtt_send_ota_ack(ack_id, IOTCL_C2D_EVT_OTA_FAILED, "no download url");
        }
        return;
    }

    if (ack_id)
    {
        iotcl_mqtt_send_ota_ack(ack_id, IOTCL_C2D_EVT_OTA_DOWNLOADING, NULL);
    }

    size_t cap = 0;
    uint8_t *buf = face_detection_pending_buf(&cap);
    size_t got = 0;
    /* NULL ca: the signed URL is S3/CloudFront; default CA set covers it. */
    int rc = iotc_https_download_large(host, res, NULL, 30000, buf, cap, &got);
    if (0 != rc)
    {
        IOTC_PRINT("IOTC: model download failed (%d)\r\n", rc);
        if (ack_id)
        {
            iotcl_mqtt_send_ota_ack(ack_id, IOTCL_C2D_EVT_OTA_DOWNLOAD_FAILED, "download failed");
        }
        return;
    }
    IOTC_PRINT("IOTC: model downloaded (%u bytes)\r\n", (unsigned) got);

    const char *reason = face_detection_request_swap(buf, got);
    if (reason)
    {
        IOTC_PRINT("IOTC: model rejected: %s\r\n", reason);
        if (ack_id)
        {
            iotcl_mqtt_send_ota_ack(ack_id, IOTCL_C2D_EVT_OTA_DOWNLOAD_FAILED, reason);
        }
        return;
    }
    if (ack_id)
    {
        iotcl_mqtt_send_ota_ack(ack_id, IOTCL_C2D_EVT_OTA_DOWNLOAD_DONE,
                                "model queued for hot-swap");
    }
}

static void prv_publish_telemetry(void)
{
    IotclMessageHandle msg = iotcl_telemetry_create();
    if (!msg)
    {
        return;
    }

    uint32_t n = face_detection_box_count();
    float top_score = 0.0f;
    if (n > 0)
    {
        int16_t x, y, w, h;
        face_detection_box_get(0, &x, &y, &w, &h, &top_score);
    }

    const char *mname; const char *msrc; unsigned mver, msize;
    face_detection_model_info(&mname, &mver, &msrc, &msize);

    iotcl_telemetry_set_number(msg, "vision.face_count", (double) n);
    iotcl_telemetry_set_number(msg, "vision.score", (double) (top_score * 100.0f));
    iotcl_telemetry_set_string(msg, "vision.state", (n > 0) ? "face" : "clear");
    iotcl_telemetry_set_string(msg, "model.name", mname);
    iotcl_telemetry_set_number(msg, "model.ver", (double) mver);
    iotcl_telemetry_set_string(msg, "model.src", msrc);
    iotcl_telemetry_set_number(msg, "model.size_b", (double) msize);
    iotcl_telemetry_set_number(msg, "sys.uptime_s",
                               (double) (xTaskGetTickCount() / configTICK_RATE_HZ));
    iotcl_telemetry_set_number(msg, "sys.free_heap", (double) xPortGetFreeHeapSize());
    iotcl_telemetry_set_number(msg, "sys.msgs_sent", (double) s_msgs_sent);

    if (IOTCL_SUCCESS == iotcl_mqtt_send_telemetry(msg, false))
    {
        s_msgs_sent++;
    }
    iotcl_telemetry_destroy(msg);
}

/* Called from net_thread each loop iteration once the network is up. */
void iotc_app_poll(bool network_up)
{
#if !IOTC_CFG_ENABLED
    (void) network_up;
    return;
#else
    switch (s_state)
    {
        case IOTC_APP_IDLE:
            if (!network_up)
            {
                break;
            }
            s_state = IOTC_APP_STARTING;
            IOTC_PRINT("IOTC: starting (env=%s duid=%s)\r\n", IOTC_CFG_ENV, IOTC_CFG_DUID);

            if (0 != iotc_time_sync(IOTC_CFG_SNTP_SERVER, 10000))
            {
                IOTC_PRINT("IOTC: SNTP sync failed; retrying later\r\n");
                s_state = IOTC_APP_IDLE;
                break;
            }
            IOTC_PRINT("IOTC: time synced (%lld)\r\n", (long long) iotc_time_now());

            {
                IotConnectClientConfig cfg;
                iotconnect_sdk_init_config(&cfg);
                cfg.env = IOTC_CFG_ENV;
                cfg.cpid = IOTC_CFG_CPID;
                cfg.duid = IOTC_CFG_DUID;
                cfg.auth_info.type = IOTC_AT_X509;
                cfg.auth_info.cert_info.device_cert = IOTC_CFG_DEVICE_CERT_PEM;
                cfg.auth_info.cert_info.device_key = IOTC_CFG_DEVICE_KEY_PEM;
                cfg.cmd_cb = prv_on_command;
                cfg.ota_cb = prv_on_ota;

                if (0 != iotconnect_sdk_init(&cfg))
                {
                    IOTC_PRINT("IOTC: init failed (will retry)\r\n");
                    s_state = IOTC_APP_FAILED;
                    break;
                }
                if (0 != iotconnect_sdk_connect())
                {
                    IOTC_PRINT("IOTC: MQTT connect failed (will retry)\r\n");
                    s_state = IOTC_APP_FAILED;
                    break;
                }
            }
            IOTC_PRINT("IOTC: connected\r\n");
            s_state = IOTC_APP_RUNNING;
            s_last_pub = 0;
            break;

        case IOTC_APP_RUNNING:
            if (!iotconnect_sdk_is_connected())
            {
                IOTC_PRINT("IOTC: disconnected\r\n");
                s_state = IOTC_APP_FAILED; /* TODO: reconnect backoff */
                break;
            }
            if (s_snapshot_pending)
            {
                char result[96];
                IOTC_PRINT("IOTC: snapshot: capturing + uploading...\r\n");
                int rc = iotc_snapshot_capture_upload(result, sizeof(result));
                IOTC_PRINT("IOTC: snapshot: %s\r\n", result);
                if (s_snapshot_ack_id)
                {
                    iotcl_mqtt_send_cmd_ack(s_snapshot_ack_id,
                                            (0 == rc) ? IOTCL_C2D_EVT_CMD_SUCCESS_WITH_ACK
                                                      : IOTCL_C2D_EVT_CMD_FAILED,
                                            result);
                    iotcl_free(s_snapshot_ack_id);
                    s_snapshot_ack_id = NULL;
                }
                s_snapshot_pending = false;
            }
            if ((xTaskGetTickCount() - s_last_pub) >= pdMS_TO_TICKS(s_period_s * 1000))
            {
                s_last_pub = xTaskGetTickCount();
                prv_publish_telemetry();
            }
            break;

        case IOTC_APP_FAILED:
        {
            /* Retry from scratch after a backoff. */
            static TickType_t s_fail_at;
            if (0 == s_fail_at)
            {
                s_fail_at = xTaskGetTickCount();
            }
            else if ((xTaskGetTickCount() - s_fail_at) >= pdMS_TO_TICKS(20000))
            {
                s_fail_at = 0;
                iotconnect_sdk_deinit();
                s_state = IOTC_APP_IDLE;
                IOTC_PRINT("IOTC: retrying\r\n");
            }
            break;
        }

        case IOTC_APP_STARTING:
        default:
            break;
    }
#endif /* IOTC_CFG_ENABLED */
}
