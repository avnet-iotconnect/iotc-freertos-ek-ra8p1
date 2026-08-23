/*
 * Copyright (c) 2020-2026 Avnet, Inc.
 * SPDX-License-Identifier: MIT
 *
 * Public API for the IOTCONNECT FSP/FreeRTOS SDK layer (EK-RA8P1).
 *
 * This header mirrors the IOTCONNECT generic-SDK family contract
 * (IotConnectClientConfig + the init_config/init/connect/is_connected/
 * disconnect/deinit lifecycle and the OTA/command/status callbacks) so
 * applications move across the SDK family with minimal change. The
 * underlying iotc-c-lib telemetry builder (iotcl_telemetry_*) remains
 * available for rich payloads.
 *
 * Ownership: the SDK is a global singleton. iotconnect_sdk_init() deep-copies
 * the caller's config strings, so the caller need not keep the struct alive
 * after init. iotconnect_sdk_deinit() frees them.
 */
#ifndef IOTCONNECT_H
#define IOTCONNECT_H

#include <stdbool.h>
#include <stddef.h>

/* iotc-c-lib C2D types (IotclC2dEventData + the command/OTA callback typedefs). */
#include "iotcl_c2d.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Cloud back end; selects DRA discovery builder + username handling. */
typedef enum {
    IOTC_CT_UNDEFINED = 0,
    IOTC_CT_AWS = 1,
    IOTC_CT_AZURE = 2,
} IotConnectConnectionType;

/** Device authentication scheme. This port supports X509 only. */
typedef enum {
    IOTC_AT_UNDEFINED = 0,
    IOTC_AT_X509 = 1,
} IotConnectAuthType;

/** Connection status reported to the application status callback. */
typedef enum {
    IOTC_CS_UNDEFINED = 0,
    IOTC_CS_MQTT_CONNECTED = 1,
    IOTC_CS_MQTT_DISCONNECTED = 2,
    IOTC_CS_MQTT_DELIVERED = 3,
    IOTC_CS_MQTT_SEND_FAILED = 4,
} IotConnectMqttStatus;

/* Command and OTA callbacks come straight from iotc-c-lib so app code shares
 * the exact iotcl_c2d_get_* accessors across the SDK family:
 *   typedef void (*IotclCommandCallback)(IotclC2dEventData data);
 *   typedef void (*IotclOtaCallback)(IotclC2dEventData data);
 */
typedef void (*IotConnectStatusCallback)(IotConnectMqttStatus status);

/**
 * TLS / auth material.
 *
 * The char* fields carry in-memory PEM blobs (NUL-terminated). On this port
 * they are handed to mbedTLS parse functions directly; there is no
 * sec-tag/keystore indirection. ca_cert is the cloud (broker) root CA; for
 * AWS this is Amazon Root CA 1 / Starfield G2. dra_ca verifies the
 * discovery/identity HTTPS endpoint (GoDaddy/Starfield G2 chain).
 */
typedef struct {
    IotConnectAuthType type;

    const char *ca_cert; /* broker root CA, PEM */
    size_t ca_cert_len;  /* including NUL */

    const char *dra_ca;  /* discovery/identity HTTPS root CA, PEM */
    size_t dra_ca_len;

    struct {
        const char *device_cert; /* PEM */
        size_t device_cert_len;
        const char *device_key;  /* PEM */
        size_t device_key_len;
    } cert_info;
} IotConnectAuthInfo;

/**
 * Top-level application config. Mirror of the family IotConnectClientConfig.
 * Fields left NULL/0 fall back to their IOTC_CFG_* defaults in app_config.h.
 */
typedef struct {
    IotConnectConnectionType connection_type;
    const char *env;
    const char *cpid;
    const char *duid;
    int qos; /* default 1 */

    IotConnectAuthInfo auth_info;

    /* iotc-c-lib callbacks: void(*)(IotclC2dEventData). NULL = ignore. */
    IotclOtaCallback ota_cb;
    IotclCommandCallback cmd_cb;
    IotConnectStatusCallback status_cb;

    bool verbose;
} IotConnectClientConfig;

/** Zero the config and apply defaults (memset 0, qos = 1). Call FIRST. */
void iotconnect_sdk_init_config(IotConnectClientConfig *c);

/**
 * Validate + deep-copy config, init iotc-c-lib (iotcl_init), and run
 * discovery/identity to resolve the broker/topics. Requires the network up
 * and time synced (iotc_time_sync). Does NOT connect. Returns 0
 * (IOTCL_SUCCESS) or an IOTCL_ERR_* code.
 */
int iotconnect_sdk_init(IotConnectClientConfig *c);

/**
 * Open the MQTT/TLS connection, subscribe to the C2D topic, and start the
 * message-pump task. Returns 0 on success.
 */
int iotconnect_sdk_connect(void);

/** Report MQTT transport connection state. */
bool iotconnect_sdk_is_connected(void);

/**
 * Convenience telemetry send. Builds a single-value telemetry message and
 * publishes it via iotcl_mqtt_send_telemetry(). For multi-field/nested
 * payloads use the iotc-c-lib builder directly. Returns 0 or IOTCL_ERR_*.
 */
int iotconnect_sdk_send_telemetry_number(const char *path, double value);
int iotconnect_sdk_send_telemetry_string(const char *path, const char *value);

/** Graceful MQTT disconnect; stops the message pump. */
void iotconnect_sdk_disconnect(void);

/** Tear down iotc-c-lib and free copied config strings. Safe if not inited. */
void iotconnect_sdk_deinit(void);

#ifdef __cplusplus
}
#endif

#endif /* IOTCONNECT_H */
