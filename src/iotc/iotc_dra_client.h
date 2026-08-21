/*
 * Copyright (c) 2020-2026 Avnet, Inc.
 * SPDX-License-Identifier: MIT
 *
 * Device REST API (DRA) HTTPS client seam, FSP/FreeRTOS port. Performs the
 * two-step Discovery -> Identity GET sequence over a TLS socket
 * (FreeRTOS+TCP + mbedTLS), handing raw response bodies to the iotc-c-lib
 * DRA parsers (modules/device-rest-api) which populate the library's MQTT
 * config. After a successful run the orchestrator reads connection params via
 * iotcl_mqtt_get_config().
 */
#ifndef IOTC_DRA_CLIENT_H
#define IOTC_DRA_CLIENT_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Platform string for the discovery URL builder. */
typedef enum {
    IOTC_DRA_PF_AWS = 0,
    IOTC_DRA_PF_AZURE,
} iotc_dra_platform_t;

/**
 * Parameters for the DRA bootstrap. cpid/env/duid identify the device;
 * dra_ca_pem verifies the HTTPS endpoint (GoDaddy/Starfield G2 chain).
 */
typedef struct {
    iotc_dra_platform_t platform;
    const char *discovery_host; /* e.g. "awsdiscovery.iotconnect.io" */
    const char *cpid;
    const char *env;
    const char *duid;
    const char *dra_ca_pem;
    int timeout_ms;
} iotc_dra_config_t;

/**
 * Set the fallback root CA (PEM) used when a call passes ca_pem == NULL.
 */
void iotc_dra_set_default_ca(const char *ca_pem);

/**
 * Run discovery + identity and populate the iotc-c-lib MQTT config in place.
 * On success, iotcl_mqtt_get_config() returns the resolved host/client_id/
 * username/topics. The library MUST already be initialized in
 * IOTCL_DCT_CUSTOM mode before calling this. Returns 0 (IOTCL_SUCCESS) or an
 * error code (HTTP failure or IOTCL_ERR_*).
 */
int iotc_dra_run(const iotc_dra_config_t *cfg);

/**
 * General-purpose single HTTPS GET reusing the DRA transport: fetch
 * https://<hostname><resource> verifying the peer with ca_pem (NULL = accept
 * the AWS/GoDaddy roots baked into the transport), into buf. Intended for
 * small downloads delivered by C2D URLs; the body must fit in buf or
 * -ENOMEM is returned. Returns 0 or a negative errno.
 */
int iotc_https_download(const char *hostname,
                        const char *resource,
                        const char *ca_pem,
                        int timeout_ms,
                        uint8_t *buf,
                        size_t buf_size,
                        size_t *out_len);

/**
 * Large download: the caller's buffer serves as the coreHTTP work buffer
 * (headers + body), so multi-hundred-KB payloads (AI models) avoid a second
 * copy. On success the body is moved to the start of workbuf.
 */
int iotc_https_download_large(const char *hostname, const char *resource,
                              const char *ca_pem, int timeout_ms,
                              uint8_t *workbuf, size_t workbuf_size, size_t *out_len);

/**
 * Streaming variant for large payloads (AI model blobs): the body is
 * delivered in chunks to sink() as it arrives, so multi-MB models never need
 * a contiguous RAM buffer and can be written straight to OSPI flash.
 * Returning nonzero from sink() aborts the transfer. Returns 0 or -errno.
 */
typedef int (*iotc_https_sink_t)(const uint8_t *chunk, size_t len, void *user);
int iotc_https_download_stream(const char *hostname,
                               const char *resource,
                               const char *ca_pem,
                               int timeout_ms,
                               iotc_https_sink_t sink,
                               void *user,
                               size_t *out_total);

#ifdef __cplusplus
}
#endif

#endif /* IOTC_DRA_CLIENT_H */
