/*
 * Copyright (c) 2020-2026 Avnet, Inc.
 * SPDX-License-Identifier: MIT
 *
 * MQTT-over-TLS transport seam, FSP/FreeRTOS port: coreMQTT over the FSP
 * MbedTLS/PKCS11 transport (mutual TLS; device identity comes from the
 * PKCS#11 store on LittleFS/OSPI). Runs the process/keepalive loop on its
 * own task and forwards inbound PUBLISH payloads to iotc-c-lib's C2D
 * processor.
 */

#include "iotc_mqtt_client.h"

#include <stdio.h>
#include <string.h>

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "core_mqtt.h"
#include "transport_mbedtls_pkcs11.h"
#include "core_pkcs11_config.h"

#include "iotcl.h"
#include "iotcl_log.h"
#include "iotcl_c2d.h"
#include "iotc_time.h"

#define IOTC_MQTT_BUF_SIZE       (8 * 1024)
#define IOTC_MQTT_KEEPALIVE_S    60
#define IOTC_MQTT_CONNACK_MS     5000
#define IOTC_MQTT_TASK_STACK     2048 /* words */
#define IOTC_MQTT_TASK_PRIO      (tskIDLE_PRIORITY + 2)

struct NetworkContext
{
    TlsTransportParams_t *pParams;
};

static TlsTransportParams_t s_tls_params;
static NetworkContext_t s_net = {0};
static MQTTContext_t s_mqtt;
static uint8_t s_mqtt_buf[IOTC_MQTT_BUF_SIZE];
static iotc_mqtt_status_cb_t s_status_cb;
static volatile bool s_connected;
static volatile bool s_stop;
static TaskHandle_t s_pump_task;
static const iotc_mqtt_config_t *s_cfg;

/* coreMQTT is not thread-safe: serialize ProcessLoop vs publish/subscribe. */
static SemaphoreHandle_t s_api_mutex;
static StaticSemaphore_t s_api_mutex_buf;

static uint32_t prv_time_ms(void)
{
    return (uint32_t) (xTaskGetTickCount() * portTICK_PERIOD_MS);
}

static void prv_event_cb(MQTTContext_t *ctx, MQTTPacketInfo_t *packet, MQTTDeserializedInfo_t *info)
{
    (void) ctx;
    if ((packet->type & 0xF0U) == MQTT_PACKET_TYPE_PUBLISH)
    {
        MQTTPublishInfo_t *pub = info->pPublishInfo;
        IOTCL_INFO("MQTT: C2D message (%u bytes)", (unsigned) pub->payloadLength);
        (void) iotcl_c2d_process_event_with_length((const uint8_t *) pub->pPayload,
                                                   pub->payloadLength);
    }
    else if ((packet->type == MQTT_PACKET_TYPE_PUBACK) && s_status_cb)
    {
        s_status_cb(IOTC_MQTT_EVT_DELIVERED);
    }
}

static void prv_pump(void *arg)
{
    (void) arg;
    while (!s_stop)
    {
        xSemaphoreTake(s_api_mutex, portMAX_DELAY);
        MQTTStatus_t st = MQTT_ProcessLoop(&s_mqtt);
        xSemaphoreGive(s_api_mutex);
        if ((MQTTSuccess != st) && (MQTTNeedMoreBytes != st))
        {
            IOTCL_ERROR(st, "MQTT: process loop error");
            s_connected = false;
            if (s_status_cb)
            {
                s_status_cb(IOTC_MQTT_EVT_DISCONNECTED);
            }
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    s_pump_task = NULL;
    vTaskDelete(NULL);
}

int iotc_mqtt_client_connect(const iotc_mqtt_config_t *cfg)
{
    if (s_connected)
    {
        return 0;
    }
    s_cfg = cfg;
    s_status_cb = cfg->status_cb;

    if (NULL == s_api_mutex)
    {
        s_api_mutex = xSemaphoreCreateMutexStatic(&s_api_mutex_buf);
    }

    memset(&s_tls_params, 0, sizeof(s_tls_params));
    s_net.pParams = &s_tls_params;

    NetworkCredentials_t creds;
    memset(&creds, 0, sizeof(creds));
    creds.pRootCa = (const unsigned char *) cfg->ca_cert_pem;
    creds.rootCaSize = cfg->ca_cert_pem ? strlen(cfg->ca_cert_pem) + 1 : 0;
    creds.pClientCertLabel = pkcs11configLABEL_DEVICE_CERTIFICATE_FOR_TLS;
    creds.pPrivateKeyLabel = pkcs11configLABEL_DEVICE_PRIVATE_KEY_FOR_TLS;

    uint16_t port = cfg->port ? cfg->port : 8883;
    TlsTransportStatus_t ts = TLS_FreeRTOS_Connect(&s_net, cfg->host, port, &creds, 10000, 10000);
    if (TLS_TRANSPORT_SUCCESS != ts)
    {
        IOTCL_ERROR(ts, "MQTT: TLS connect failed");
        return -1;
    }

    TransportInterface_t xport = {0};
    xport.pNetworkContext = &s_net;
    xport.send = TLS_FreeRTOS_send;
    xport.recv = TLS_FreeRTOS_recv;

    MQTTFixedBuffer_t fixed = {.pBuffer = s_mqtt_buf, .size = sizeof(s_mqtt_buf)};
    if (MQTTSuccess != MQTT_Init(&s_mqtt, &xport, prv_time_ms, prv_event_cb, &fixed))
    {
        TLS_FreeRTOS_Disconnect(&s_net);
        return -1;
    }

    /* Required for QoS1: without stateful-QoS records every QoS1 subscribe
     * and publish returns MQTTBadParameter. */
    static MQTTPubAckInfo_t s_outgoing_records[10];
    static MQTTPubAckInfo_t s_incoming_records[10];
    if (MQTTSuccess != MQTT_InitStatefulQoS(&s_mqtt,
                                            s_outgoing_records, 10,
                                            s_incoming_records, 10))
    {
        TLS_FreeRTOS_Disconnect(&s_net);
        return -1;
    }

    MQTTConnectInfo_t ci;
    memset(&ci, 0, sizeof(ci));
    ci.cleanSession = true;
    ci.pClientIdentifier = cfg->client_id;
    ci.clientIdentifierLength = (uint16_t) strlen(cfg->client_id);
    ci.keepAliveSeconds = IOTC_MQTT_KEEPALIVE_S;
    if (cfg->username)
    {
        ci.pUserName = cfg->username;
        ci.userNameLength = (uint16_t) strlen(cfg->username);
    }

    bool session_present = false;
    if (MQTTSuccess != MQTT_Connect(&s_mqtt, &ci, NULL, IOTC_MQTT_CONNACK_MS, &session_present))
    {
        IOTCL_ERROR(0, "MQTT: CONNECT failed");
        TLS_FreeRTOS_Disconnect(&s_net);
        return -1;
    }

    if (cfg->sub_c2d)
    {
        MQTTSubscribeInfo_t sub;
        memset(&sub, 0, sizeof(sub));
        sub.qos = MQTTQoS1;
        sub.pTopicFilter = cfg->sub_c2d;
        sub.topicFilterLength = (uint16_t) strlen(cfg->sub_c2d);
        MQTTStatus_t st = MQTT_Subscribe(&s_mqtt, &sub, 1, MQTT_GetPacketId(&s_mqtt));
        if (MQTTSuccess != st)
        {
            IOTCL_ERROR((int) st, "MQTT: subscribe failed");
            IOTCL_INFO("MQTT: c2d topic was \"%s\"", cfg->sub_c2d);
        }
    }

    s_stop = false;
    s_connected = true;
    if (pdPASS != xTaskCreate(prv_pump, "iotc_mqtt", IOTC_MQTT_TASK_STACK, NULL,
                              IOTC_MQTT_TASK_PRIO, &s_pump_task))
    {
        iotc_mqtt_client_disconnect();
        return -1;
    }

    if (s_status_cb)
    {
        s_status_cb(IOTC_MQTT_EVT_CONNECTED);
    }
    return 0;
}

void iotc_mqtt_client_disconnect(void)
{
    s_stop = true;
    if (s_connected)
    {
        (void) MQTT_Disconnect(&s_mqtt);
        TLS_FreeRTOS_Disconnect(&s_net);
        s_connected = false;
        if (s_status_cb)
        {
            s_status_cb(IOTC_MQTT_EVT_DISCONNECTED);
        }
    }
}

bool iotc_mqtt_client_is_connected(void)
{
    return s_connected;
}

void iotc_mqtt_client_publish(const char *topic, const char *json_str)
{
    if (!s_connected || !topic || !json_str)
    {
        return;
    }
    MQTTPublishInfo_t pub;
    memset(&pub, 0, sizeof(pub));
    pub.qos = (s_cfg && s_cfg->qos == 0) ? MQTTQoS0 : MQTTQoS1;
    pub.pTopicName = topic;
    pub.topicNameLength = (uint16_t) strlen(topic);
    pub.pPayload = json_str;
    pub.payloadLength = strlen(json_str);

    xSemaphoreTake(s_api_mutex, portMAX_DELAY);
    MQTTStatus_t st = MQTT_Publish(&s_mqtt, &pub, MQTT_GetPacketId(&s_mqtt));
    xSemaphoreGive(s_api_mutex);
    if (MQTTSuccess != st)
    {
        IOTCL_ERROR((int) st, "MQTT: publish failed");
        if (s_status_cb)
        {
            s_status_cb(IOTC_MQTT_EVT_SEND_FAILED);
        }
    }
}
