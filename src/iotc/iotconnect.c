/*
 * Copyright (c) 2020-2026 Avnet, Inc.
 * SPDX-License-Identifier: MIT
 *
 * IoTConnect SDK orchestrator, FSP/FreeRTOS port: credential provisioning
 * (PKCS#11 on LittleFS/OSPI) -> discovery/identity (DRA) -> MQTT connect ->
 * telemetry/c2d pump. Mirrors the IOTCONNECT generic-SDK orchestration flow.
 */

#include "iotconnect.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "FreeRTOS.h"
#include "task.h"

#include "hal_data.h"
#include "rm_littlefs_api.h"
#include "lfs.h"
#include "aws_dev_mode_key_provisioning.h"
#include "mbedtls/platform.h"

#include "iotcl.h"
#include "iotcl_log.h"
#include "iotcl_certs.h"
#include "iotcl_util.h"
#include "iotcl_telemetry.h"

#include "iotc_mqtt_client.h"
#include "iotc_dra_client.h"
#include "iotc_file_upload.h"
#include "iotc_time.h"

#include "iotc_fs.h"

/* Referenced by the generated rm_littlefs_spi_flash instance. */
void g_rm_littlefs_spi_flash0_callback(rm_littlefs_spi_flash_callback_args_t *p_args)
{
    FSP_PARAMETER_NOT_USED(p_args);
}

static struct
{
    IotConnectClientConfig cfg; /* deep copies below */
    char *env;
    char *cpid;
    char *duid;
    bool lib_inited;
} s_ctx;

static const char *const s_broker_ca = IOTCL_AMAZON_ROOT_CA1;
static const char *const s_dra_ca = IOTCL_CERT_GODADDY_SECURE_SERVER_CERTIFICATE_G2;

void iotconnect_sdk_init_config(IotConnectClientConfig *c)
{
    memset(c, 0, sizeof(*c));
    c->qos = 1;
    c->connection_type = IOTC_CT_AWS;
}

static int prv_provision_identity(const IotConnectAuthInfo *auth)
{
    if (!auth->cert_info.device_cert || !auth->cert_info.device_cert[0])
    {
        /* Nothing to write; assume the PKCS#11 store already holds identity. */
        return 0;
    }
    ProvisioningParams_t params;
    memset(&params, 0, sizeof(params));
    params.pucClientCertificate = (uint8_t *) auth->cert_info.device_cert;
    params.ulClientCertificateLength = (uint32_t) (auth->cert_info.device_cert_len
        ? auth->cert_info.device_cert_len : strlen(auth->cert_info.device_cert) + 1);
    params.pucClientPrivateKey = (uint8_t *) auth->cert_info.device_key;
    params.ulClientPrivateKeyLength = (uint32_t) (auth->cert_info.device_key_len
        ? auth->cert_info.device_key_len : strlen(auth->cert_info.device_key) + 1);

    {
        /* FSP requirement: initialise the crypto hardware, mbedTLS threading
         * (MBEDTLS_THREADING_ALT) and allocator before any mbedTLS/PSA use. */
        static bool s_platform_ready;
        if (!s_platform_ready)
        {
            int r = mbedtls_platform_setup(NULL);
            IOTCL_INFO("IOTC: mbedtls_platform_setup -> %d", r);
            if (0 != r)
            {
                return -1;
            }
            s_platform_ready = true;
        }
    }

    CK_RV rv = vAlternateKeyProvisioning(&params);
    if (CKR_OK != rv)
    {
        IOTCL_ERROR((int) rv, "IOTC: key provisioning failed");
        return -1;
    }
    IOTCL_INFO("IOTC: device identity provisioned to PKCS#11 store");
    return 0;
}

static void *prv_iotcl_malloc(size_t size)
{
    return pvPortMalloc(size);
}

static void prv_iotcl_free(void *ptr)
{
    vPortFree(ptr);
}

int iotconnect_sdk_init(IotConnectClientConfig *c)
{
    int status;

    /* Run iotc-c-lib (and its cJSON) on the FreeRTOS heap. The default is
     * libc malloc (the 64 KB newlib heap), which the KVS WebRTC stack can
     * exhaust during a video session - after which telemetry message
     * creation fails silently, forever. One-shot: allocator must never
     * change once anything is allocated. */
    {
        static bool s_mem_configured;
        if (!s_mem_configured)
        {
            s_mem_configured = true;
            iotcl_configure_dynamic_memory(prv_iotcl_malloc, prv_iotcl_free);
        }
    }

    s_ctx.cfg = *c;
    s_ctx.env = iotcl_strdup(c->env);
    s_ctx.cpid = iotcl_strdup(c->cpid);
    s_ctx.duid = iotcl_strdup(c->duid);

    if (0 != iotc_fs_init())
    {
        IOTCL_ERROR(0, "IOTC: filesystem init failed");
        return -1;
    }
    if (0 != prv_provision_identity(&c->auth_info))
    {
        return -1;
    }

    IotclClientConfig lib_cfg;
    iotcl_init_client_config(&lib_cfg);
    lib_cfg.device.instance_type = IOTCL_DCT_CUSTOM;
    lib_cfg.device.duid = s_ctx.duid;
    lib_cfg.device.cpid = s_ctx.cpid;
    lib_cfg.events.cmd_cb = c->cmd_cb;
    lib_cfg.events.ota_cb = c->ota_cb;
    lib_cfg.mqtt_send_cb = iotc_mqtt_client_publish;
    lib_cfg.time_fn = iotc_time_now;

    status = iotcl_init(&lib_cfg);
    if (IOTCL_SUCCESS != status)
    {
        return status;
    }
    s_ctx.lib_inited = true;

    iotc_dra_set_default_ca(c->auth_info.dra_ca ? c->auth_info.dra_ca : s_dra_ca);
    iotc_fu_set_ca(s_broker_ca); /* creds provider/STS/S3 all chain to Amazon Root CA 1 */

    iotc_dra_config_t dra = {
        .platform = (c->connection_type == IOTC_CT_AZURE) ? IOTC_DRA_PF_AZURE : IOTC_DRA_PF_AWS,
        .discovery_host = NULL,
        .cpid = s_ctx.cpid,
        .env = s_ctx.env,
        .duid = s_ctx.duid,
        .dra_ca_pem = c->auth_info.dra_ca ? c->auth_info.dra_ca : s_dra_ca,
        .timeout_ms = 15000,
    };
    status = iotc_dra_run(&dra);
    if (IOTCL_SUCCESS != status)
    {
        IOTCL_ERROR(status, "IOTC: discovery/identity failed");
        return status;
    }

    IotclMqttConfig *mc = iotcl_mqtt_get_config();
    IOTCL_INFO("IOTC: broker %s, client id %s", mc->host ? mc->host : "?",
               mc->client_id ? mc->client_id : "?");
    return IOTCL_SUCCESS;
}

static void prv_status_forward(iotc_mqtt_evt_t evt)
{
    if (!s_ctx.cfg.status_cb)
    {
        return;
    }
    switch (evt)
    {
        case IOTC_MQTT_EVT_CONNECTED:
            s_ctx.cfg.status_cb(IOTC_CS_MQTT_CONNECTED);
            break;
        case IOTC_MQTT_EVT_DISCONNECTED:
            s_ctx.cfg.status_cb(IOTC_CS_MQTT_DISCONNECTED);
            break;
        case IOTC_MQTT_EVT_DELIVERED:
            s_ctx.cfg.status_cb(IOTC_CS_MQTT_DELIVERED);
            break;
        case IOTC_MQTT_EVT_SEND_FAILED:
        default:
            s_ctx.cfg.status_cb(IOTC_CS_MQTT_SEND_FAILED);
            break;
    }
}

int iotconnect_sdk_connect(void)
{
    IotclMqttConfig *mc = iotcl_mqtt_get_config();
    if (!mc || !mc->host)
    {
        return -1;
    }
    iotc_mqtt_config_t cfg = {
        .host = mc->host,
        .port = 8883,
        .client_id = mc->client_id,
        .username = mc->username,
        .sub_c2d = mc->sub_c2d,
        .ca_cert_pem = s_ctx.cfg.auth_info.ca_cert ? s_ctx.cfg.auth_info.ca_cert : s_broker_ca,
        .qos = s_ctx.cfg.qos,
        .status_cb = prv_status_forward,
    };
    return iotc_mqtt_client_connect(&cfg);
}

bool iotconnect_sdk_is_connected(void)
{
    return iotc_mqtt_client_is_connected();
}

int iotconnect_sdk_send_telemetry_number(const char *path, double value)
{
    IotclMessageHandle msg = iotcl_telemetry_create();
    if (!msg)
    {
        return IOTCL_ERR_OUT_OF_MEMORY;
    }
    int status = iotcl_telemetry_set_number(msg, path, value);
    if (IOTCL_SUCCESS == status)
    {
        status = iotcl_mqtt_send_telemetry(msg, false);
    }
    iotcl_telemetry_destroy(msg);
    return status;
}

int iotconnect_sdk_send_telemetry_string(const char *path, const char *value)
{
    IotclMessageHandle msg = iotcl_telemetry_create();
    if (!msg)
    {
        return IOTCL_ERR_OUT_OF_MEMORY;
    }
    int status = iotcl_telemetry_set_string(msg, path, value);
    if (IOTCL_SUCCESS == status)
    {
        status = iotcl_mqtt_send_telemetry(msg, false);
    }
    iotcl_telemetry_destroy(msg);
    return status;
}

void iotconnect_sdk_disconnect(void)
{
    iotc_mqtt_client_disconnect();
}

void iotconnect_sdk_deinit(void)
{
    iotconnect_sdk_disconnect();
    if (s_ctx.lib_inited)
    {
        iotcl_deinit();
        s_ctx.lib_inited = false;
    }
    free(s_ctx.env);
    free(s_ctx.cpid);
    free(s_ctx.duid);
    s_ctx.env = s_ctx.cpid = s_ctx.duid = NULL;
}
