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
#include "iotcl_certs.h"
#include "iotcl_telemetry.h"

#include "console_output/console_output.h"
#include "common/common_util.h" /* application_processing_time */

#include "iotc_dra_client.h"
#include "iotc_snapshot.h"
#include "iotc_config.h"
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
extern bool face_detection_class_info(const char **label, int *pct);

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

    /* Dashboard Video Streaming tab Start/Stop (ct:112/113). These carry no
     * "cmd" field and only need a success ack: the actual stream start/stop
     * is driven by the viewer's WebRTC signaling connection. */
    {
        int et = iotcl_c2d_get_event_type(data);
        if ((112 == et) || (113 == et))
        {
            IOTC_PRINT("IOTC: %s Video (ct=%d)\r\n",
                       (112 == et) ? "Start" : "Stop", et);
            if (ack_id)
            {
                iotcl_mqtt_send_cmd_ack(ack_id, IOTCL_C2D_EVT_CMD_SUCCESS_WITH_ACK,
                                        "OK");
            }
            return;
        }
    }

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
/* Model pushes are deferred to the net thread (like snapshots): the
 * download's TLS session needs ~40 KB of heap, which is not available
 * while a KVS video session is streaming — attempting it live fails the
 * TLS setup. The worker below waits for the stream to go idle. */
static char *s_model_host;
static char *s_model_res;
static char *s_model_ack_id;
static volatile bool s_model_pending;
static TickType_t s_model_wait_since;

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
    if (s_model_pending)
    {
        if (ack_id)
        {
            iotcl_mqtt_send_ota_ack(ack_id, IOTCL_C2D_EVT_OTA_FAILED,
                                    "another model push is in progress");
        }
        return;
    }

    if (ack_id)
    {
        iotcl_mqtt_send_ota_ack(ack_id, IOTCL_C2D_EVT_OTA_DOWNLOADING, NULL);
    }

    iotcl_free(s_model_host);
    iotcl_free(s_model_res);
    iotcl_free(s_model_ack_id);
    s_model_host = iotcl_strdup(host);
    s_model_res = iotcl_strdup(res);
    s_model_ack_id = ack_id ? iotcl_strdup(ack_id) : NULL;
    s_model_wait_since = xTaskGetTickCount();
    s_model_pending = true; /* net thread picks it up */
}

/* Runs on the net thread once video streaming is idle. */
static void prv_model_push_execute(void)
{
    const char *host = s_model_host;
    const char *res = s_model_res;
    const char *ack_id = s_model_ack_id;

    size_t cap = 0;
    uint8_t *buf = face_detection_pending_buf(&cap);
    size_t got = 0;
    /* The signed model URL is S3 (*.amazonaws.com), which chains to Amazon
     * Root CA 1 - NOT the GoDaddy root the DRA default covers. DNS/TLS to
     * S3 fails transiently now and then; retry a few times before giving
     * the deployment up. */
    int rc = -1;
    for (int attempt = 1; (attempt <= 3) && (0 != rc); attempt++)
    {
        if (attempt > 1)
        {
            IOTC_PRINT("IOTC: model download retry %d/3\r\n", attempt);
            vTaskDelay(pdMS_TO_TICKS(3000));
        }
        rc = iotc_https_download_large(host, res, IOTCL_AMAZON_ROOT_CA1,
                                       30000, buf, cap, &got);
    }
    if (0 != rc)
    {
        IOTC_PRINT("IOTC: model download failed (%d)\r\n", rc);
        if (ack_id)
        {
            iotcl_mqtt_send_ota_ack(ack_id, IOTCL_C2D_EVT_OTA_DOWNLOAD_FAILED, "download failed");
        }
        goto done;
    }
    IOTC_PRINT("IOTC: model downloaded (%u bytes)\r\n", (unsigned) got);

    /* The platform's AI Models upload may serve the file zip-wrapped. Unwrap
     * a single STORED (uncompressed) entry in place; pack_model.py emits
     * exactly that. Deflated zips are rejected (no inflate on-device). */
    if ((got >= 34) && (0 == memcmp(buf, "PK\x03\x04", 4)))
    {
        uint16_t method = (uint16_t) (buf[8] | (buf[9] << 8));
        uint32_t csize = (uint32_t) buf[18] | ((uint32_t) buf[19] << 8) |
                         ((uint32_t) buf[20] << 16) | ((uint32_t) buf[21] << 24);
        uint16_t nlen = (uint16_t) (buf[26] | (buf[27] << 8));
        uint16_t xlen = (uint16_t) (buf[28] | (buf[29] << 8));
        size_t data_off = 30U + nlen + xlen;
        if ((method != 0) || (csize == 0) || (data_off + csize > got))
        {
            IOTC_PRINT("IOTC: zip is not a STORED archive; repack with pack_model.py\r\n");
            if (ack_id)
            {
                iotcl_mqtt_send_ota_ack(ack_id, IOTCL_C2D_EVT_OTA_DOWNLOAD_FAILED,
                                        "zip must contain one STORED .iotv entry");
            }
            goto done;
        }
        memmove(buf, buf + data_off, csize);
        got = csize;
        IOTC_PRINT("IOTC: unwrapped STORED zip -> %u bytes\r\n", (unsigned) got);
    }

    const char *reason = face_detection_request_swap(buf, got);
    if (reason)
    {
        IOTC_PRINT("IOTC: model rejected: %s\r\n", reason);
        if (ack_id)
        {
            iotcl_mqtt_send_ota_ack(ack_id, IOTCL_C2D_EVT_OTA_DOWNLOAD_FAILED, reason);
        }
        goto done;
    }
    if (ack_id)
    {
        iotcl_mqtt_send_ota_ack(ack_id, IOTCL_C2D_EVT_OTA_DOWNLOAD_DONE,
                                "model queued for hot-swap");
    }

done:
    iotcl_free(s_model_host);
    iotcl_free(s_model_res);
    iotcl_free(s_model_ack_id);
    s_model_host = NULL;
    s_model_res = NULL;
    s_model_ack_id = NULL;
    s_model_pending = false;
}

static void prv_publish_telemetry(void)
{
    IotclMessageHandle msg = iotcl_telemetry_create();
    if (!msg)
    {
        IOTC_PRINT("IOTC: telemetry create failed (heap %u)\r\n",
                   (unsigned) xPortGetFreeHeapSize());
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

    /* Detector models report face count/state; classifier models report the
     * top-1 ImageNet label in vision.state with its confidence. */
    const char *class_label = NULL;
    int class_pct = 0;
    if (face_detection_class_info(&class_label, &class_pct))
    {
        iotcl_telemetry_set_number(msg, "vision.face_count", 0);
        iotcl_telemetry_set_number(msg, "vision.score", (double) class_pct);
        iotcl_telemetry_set_string(msg, "vision.state", class_label);
    }
    else
    {
        iotcl_telemetry_set_number(msg, "vision.face_count", (double) n);
        iotcl_telemetry_set_number(msg, "vision.score", (double) (top_score * 100.0f));
        iotcl_telemetry_set_string(msg, "vision.state", (n > 0) ? "face" : "clear");
    }
    iotcl_telemetry_set_string(msg, "model.name", mname);
    iotcl_telemetry_set_number(msg, "model.ver", (double) mver);
    iotcl_telemetry_set_string(msg, "model.src", msrc);
    iotcl_telemetry_set_number(msg, "model.size_b", (double) msize);
    {
        extern volatile uint32_t g_ai_inference_time_us;
        uint32_t infer_us = g_ai_inference_time_us;
        uint32_t infer_fps = (infer_us > 0) ? (1000000U / infer_us) : 0;
        uint32_t cam_ms = application_processing_time.camera_image_capture_time_ms;
        uint32_t cam_fps = (cam_ms > 0) ? (1000U / cam_ms) : 0;
        iotcl_telemetry_set_number(msg, "perf.infer_us", (double) infer_us);
        iotcl_telemetry_set_number(msg, "perf.infer_fps", (double) infer_fps);
        iotcl_telemetry_set_number(msg, "perf.cam_fps", (double) cam_fps);
    }
    iotcl_telemetry_set_number(msg, "sys.uptime_s",
                               (double) (xTaskGetTickCount() / configTICK_RATE_HZ));
    iotcl_telemetry_set_number(msg, "sys.free_heap", (double) xPortGetFreeHeapSize());
    iotcl_telemetry_set_number(msg, "sys.msgs_sent", (double) s_msgs_sent);
    {
        /* KVS WebRTC video state: off / wait / ready / live. */
        extern const char *iotc_kvs_state(void);
        iotcl_telemetry_set_string(msg, "video.state", iotc_kvs_state());
    }

    {
        int rc = iotcl_mqtt_send_telemetry(msg, false);
        if (IOTCL_SUCCESS == rc)
        {
            s_msgs_sent++;
        }
        else
        {
            IOTC_PRINT("IOTC: telemetry send failed (%d)\r\n", rc);
        }
    }
    iotcl_telemetry_destroy(msg);
}

/* Credentials resolved at connect time: the runtime-provisioned identity on
 * LittleFS (serial CLI, survives power cycles) takes precedence over any
 * identity compiled in via app_secrets.h. */
static iotc_config_identity_t s_stored;
static bool s_no_creds_reported;
static volatile bool s_restart_req;

/* Called by the CLI after configuration changes: reconnect with the
 * currently stored identity, no reboot needed. */
void iotc_app_request_restart(void)
{
    s_restart_req = true;
}

/* Called from net_thread each loop iteration once the network is up. */
void iotc_app_poll(bool network_up)
{
    if (s_restart_req)
    {
        s_restart_req = false;
        iotconnect_sdk_deinit();
        s_state = IOTC_APP_IDLE;
        s_no_creds_reported = false;
    }

    switch (s_state)
    {
        case IOTC_APP_IDLE:
        {
            if (!network_up)
            {
                break;
            }

            /* Resolve the identity source. */
            iotc_config_identity_free(&s_stored);
            bool use_stored = (0 == iotc_config_load(&s_stored));
            if (!use_stored && !IOTC_CFG_ENABLED)
            {
                if (!s_no_creds_reported)
                {
                    s_no_creds_reported = true;
                    IOTC_PRINT("IOTC: no credentials provisioned - use the serial "
                               "CLI (type 'help') to set env/cpid/duid/cert/key, "
                               "then 'apply'\r\n");
                }
                break;
            }

            s_state = IOTC_APP_STARTING;
            IOTC_PRINT("IOTC: starting (env=%s duid=%s, credentials: %s)\r\n",
                       use_stored ? s_stored.env : IOTC_CFG_ENV,
                       use_stored ? s_stored.duid : IOTC_CFG_DUID,
                       use_stored ? "stored" : "compiled");

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
                cfg.env = use_stored ? s_stored.env : IOTC_CFG_ENV;
                cfg.cpid = use_stored ? s_stored.cpid : IOTC_CFG_CPID;
                cfg.duid = use_stored ? s_stored.duid : IOTC_CFG_DUID;
                cfg.auth_info.type = IOTC_AT_X509;
                cfg.auth_info.cert_info.device_cert =
                    use_stored ? s_stored.cert_pem : IOTC_CFG_DEVICE_CERT_PEM;
                cfg.auth_info.cert_info.device_key =
                    use_stored ? s_stored.key_pem : IOTC_CFG_DEVICE_KEY_PEM;
                cfg.cmd_cb = prv_on_command;
                cfg.ota_cb = prv_on_ota;

                if (0 != iotconnect_sdk_init(&cfg))
                {
                    IOTC_PRINT("IOTC: init failed (will retry)\r\n");
                    s_state = IOTC_APP_FAILED;
                    break;
                }

                /* KVS WebRTC video: the identity hook has parsed d.p.vs by
                 * now (inside sdk_init). Hand over the device credentials
                 * (copied) and start the streaming task once. */
                {
                    extern void iotc_kvs_set_credentials(const char *cert_pem,
                                                         const char *key_pem);
                    extern bool iotc_kvs_config_ready(void);
                    extern void iotc_kvs_start_task(void);
                    if (iotc_kvs_config_ready())
                    {
                        iotc_kvs_set_credentials(cfg.auth_info.cert_info.device_cert,
                                                 cfg.auth_info.cert_info.device_key);
                        iotc_kvs_start_task();
                    }
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
        }

        case IOTC_APP_RUNNING:
        {
            /* Net-thread liveness heartbeat: if these stop while other
             * threads keep printing, the net thread is blocked; the buffer
             * count exposes FreeRTOS+TCP network-buffer exhaustion under
             * KVS video load. */
            static TickType_t s_hb;
            if ((xTaskGetTickCount() - s_hb) >= pdMS_TO_TICKS(30000))
            {
                extern UBaseType_t uxGetNumberOfFreeNetworkBuffers(void);
                s_hb = xTaskGetTickCount();
                IOTC_PRINT("IOTC: hb up=%lu netbuf=%u heap=%u conn=%d sent=%lu\r\n",
                           (unsigned long) (s_hb / configTICK_RATE_HZ),
                           (unsigned) uxGetNumberOfFreeNetworkBuffers(),
                           (unsigned) xPortGetFreeHeapSize(),
                           (int) iotconnect_sdk_is_connected(),
                           (unsigned long) s_msgs_sent);
            }
        }
            if (!iotconnect_sdk_is_connected())
            {
                IOTC_PRINT("IOTC: disconnected\r\n");
                s_state = IOTC_APP_FAILED; /* TODO: reconnect backoff */
                break;
            }
            {
                /* One-shot after each boot's first connect: exercise the
                 * Telemetry Files credentials fetch so upload problems show
                 * on the console without needing a dashboard command. */
                static bool s_fu_tested;
                extern int iotc_fu_selftest(void);
                if (!s_fu_tested)
                {
                    s_fu_tested = true;
                    (void) iotc_fu_selftest();
                }
            }
            if (s_model_pending)
            {
                /* The download's TLS session does not fit in the heap while
                 * a KVS viewer is streaming; wait for idle (or force after
                 * 5 minutes rather than dropping the deployment). */
                extern bool kvs_media_is_streaming(void);
                bool streaming = kvs_media_is_streaming();
                bool timed_out = (xTaskGetTickCount() - s_model_wait_since) >=
                                 pdMS_TO_TICKS(5U * 60U * 1000U);
                static TickType_t s_hint_at;
                if (streaming && !timed_out)
                {
                    if ((xTaskGetTickCount() - s_hint_at) >= pdMS_TO_TICKS(30000))
                    {
                        s_hint_at = xTaskGetTickCount();
                        IOTC_PRINT("IOTC: model push waiting for video "
                                   "streaming to stop\r\n");
                    }
                }
                else
                {
                    prv_model_push_execute();
                }
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
}
