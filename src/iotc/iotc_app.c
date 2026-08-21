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

/* Latest results from the vision pipeline (FaceDetection.cc). */
extern uint32_t face_detection_box_count(void);
extern void face_detection_box_get(uint32_t i, int16_t *x, int16_t *y,
                                   int16_t *w, int16_t *h, float *score);

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
        /* Phase 4: annotate + JPEG encode + Telemetry Files upload. */
        iotcl_mqtt_send_cmd_ack(ack_id, IOTCL_C2D_EVT_CMD_FAILED, "snapshot: not yet implemented");
        return;
    }
    if (0 == strcmp(cmd, "model-info"))
    {
        iotcl_mqtt_send_cmd_ack(ack_id, IOTCL_C2D_EVT_CMD_SUCCESS_WITH_ACK,
                                "builtin yolo-fastest-192 v1");
        return;
    }
    iotcl_mqtt_send_cmd_ack(ack_id, IOTCL_C2D_EVT_CMD_FAILED, "unknown command");
}

static void prv_on_ota(IotclC2dEventData data)
{
    /* Phase 5: AI Model Management push (ct:2) lands here. */
    const char *url = iotcl_c2d_get_ota_url(data, 0);
    IOTC_PRINT("IOTC: OTA/model push received (url: %s) - not yet implemented\r\n",
               url ? url : "?");
    const char *ack_id = iotcl_c2d_get_ack_id(data);
    if (ack_id)
    {
        iotcl_mqtt_send_ota_ack(ack_id, IOTCL_C2D_EVT_OTA_FAILED,
                                "model store not yet implemented");
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

    iotcl_telemetry_set_number(msg, "vision.face_count", (double) n);
    iotcl_telemetry_set_number(msg, "vision.score", (double) (top_score * 100.0f));
    iotcl_telemetry_set_string(msg, "vision.state", (n > 0) ? "face" : "clear");
    iotcl_telemetry_set_string(msg, "model.name", "yolo-fastest-192");
    iotcl_telemetry_set_number(msg, "model.ver", 1);
    iotcl_telemetry_set_string(msg, "model.src", "builtin");
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
                    IOTC_PRINT("IOTC: init failed\r\n");
                    s_state = IOTC_APP_FAILED;
                    break;
                }
                if (0 != iotconnect_sdk_connect())
                {
                    IOTC_PRINT("IOTC: MQTT connect failed\r\n");
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
            if ((xTaskGetTickCount() - s_last_pub) >= pdMS_TO_TICKS(s_period_s * 1000))
            {
                s_last_pub = xTaskGetTickCount();
                prv_publish_telemetry();
            }
            break;

        case IOTC_APP_STARTING:
        case IOTC_APP_FAILED:
        default:
            break;
    }
#endif /* IOTC_CFG_ENABLED */
}
