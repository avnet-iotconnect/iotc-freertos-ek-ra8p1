/*
 * Copyright (c) 2020-2026 Avnet, Inc.
 * SPDX-License-Identifier: MIT
 *
 * Device REST API (DRA) HTTPS client seam, FSP/FreeRTOS port.
 * coreHTTP over the FSP MbedTLS/PKCS11 TLS transport (server auth only for
 * discovery/identity; the DRA endpoints do not require mutual TLS).
 * Hands raw response bodies to the iotc-c-lib DRA parsers which populate
 * the library MQTT config.
 */

#include "iotc_dra_client.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "FreeRTOS.h"
#include "core_http_client.h"
#include "transport_mbedtls_pkcs11.h"

#include "iotcl.h"
#include "iotcl_log.h"
#include "iotcl_dra_url.h"
#include "iotcl_dra_discovery.h"
#include "iotcl_dra_identity.h"

/* One shared scratch for HTTP headers+body; DRA responses are a few KB. */
#define IOTC_DRA_HTTP_BUF_SIZE (8 * 1024)
static uint8_t s_http_buf[IOTC_DRA_HTTP_BUF_SIZE];

struct NetworkContext
{
    TlsTransportParams_t *pParams;
};

/* Root CAs trusted for DRA + model-download GETs: Starfield G2 (AWS side)
 * and GoDaddy G2 chains cover *.iotconnect.io and S3 signed URLs. Supplied
 * by iotconnect.c from iotcl_certs.h at init. */
static const char *s_default_ca;

void iotc_dra_set_default_ca(const char *ca_pem)
{
    s_default_ca = ca_pem;
}

/*
 * Single HTTPS GET: connect, request, deliver body via callback or buffer.
 * With sink == NULL the body lands in buf/out_len.
 */
static int https_get_ex(const char *host,
                        const char *resource,
                        const char *ca_pem,
                        int timeout_ms,
                        uint8_t *resp_buf,   /* coreHTTP work buffer (headers+body) */
                        size_t resp_size,
                        uint8_t *buf,
                        size_t buf_size,
                        size_t *out_len,
                        iotc_https_sink_t sink,
                        void *sink_user,
                        size_t *sink_total)
{
    TlsTransportParams_t params;
    NetworkContext_t net = {0};
    memset(&params, 0, sizeof(params));
    net.pParams = &params;

    NetworkCredentials_t creds;
    memset(&creds, 0, sizeof(creds));
    const char *ca = ca_pem ? ca_pem : s_default_ca;
    creds.pRootCa = (const unsigned char *) ca;
    creds.rootCaSize = ca ? strlen(ca) + 1 : 0;
    /* no client cert labels: server-auth only */

    if (timeout_ms <= 0)
    {
        timeout_ms = 10000;
    }

    TlsTransportStatus_t ts = TLS_FreeRTOS_Connect(&net, host, 443, &creds,
                                                   (uint32_t) timeout_ms,
                                                   (uint32_t) timeout_ms);
    if (TLS_TRANSPORT_SUCCESS != ts)
    {
        IOTCL_ERROR(ts, "DRA: TLS connect failed");
        return -1;
    }

    int rc = -1;
    TransportInterface_t xport = {0};
    xport.pNetworkContext = &net;
    xport.send = TLS_FreeRTOS_send;
    xport.recv = TLS_FreeRTOS_recv;

    HTTPRequestInfo_t req = {0};
    HTTPRequestHeaders_t hdrs = {0};
    HTTPResponse_t resp = {0};

    req.pMethod = HTTP_METHOD_GET;
    req.methodLen = strlen(HTTP_METHOD_GET);
    req.pHost = host;
    req.hostLen = strlen(host);
    req.pPath = resource;
    req.pathLen = strlen(resource);
    req.reqFlags = 0;

    hdrs.pBuffer = resp_buf;
    hdrs.bufferLen = resp_size;

    if (HTTPSuccess == HTTPClient_InitializeRequestHeaders(&hdrs, &req))
    {
        resp.pBuffer = resp_buf;
        resp.bufferLen = resp_size;

        HTTPStatus_t hs = HTTPClient_Send(&xport, &hdrs, NULL, 0, &resp, 0);
        if ((HTTPSuccess == hs) && (resp.statusCode >= 200) && (resp.statusCode < 300))
        {
            if (sink)
            {
                /* Buffered variant of streaming: coreHTTP delivered the whole
                 * body into s_http_buf; forward it in one chunk. Models beyond
                 * IOTC_DRA_HTTP_BUF_SIZE need the raw-socket streaming path
                 * (planned in the model download rework if needed). */
                rc = sink(resp.pBody, resp.bodyLen, sink_user);
                if (sink_total)
                {
                    *sink_total = resp.bodyLen;
                }
            }
            else if (buf)
            {
                if (resp.bodyLen > buf_size)
                {
                    rc = -12; /* -ENOMEM */
                }
                else
                {
                    /* memmove: DRA callers pass s_http_buf itself as buf. */
                    memmove(buf, resp.pBody, resp.bodyLen);
                    if (out_len)
                    {
                        *out_len = resp.bodyLen;
                    }
                    rc = 0;
                }
            }
            else
            {
                rc = 0;
            }
        }
        else
        {
            IOTCL_ERROR((int) resp.statusCode, "DRA: HTTP GET failed");
        }
    }

    TLS_FreeRTOS_Disconnect(&net);
    return rc;
}

static int https_get(const char *host, const char *resource, const char *ca_pem,
                     int timeout_ms, uint8_t *buf, size_t buf_size, size_t *out_len,
                     iotc_https_sink_t sink, void *sink_user, size_t *sink_total)
{
    return https_get_ex(host, resource, ca_pem, timeout_ms,
                        s_http_buf, sizeof(s_http_buf),
                        buf, buf_size, out_len, sink, sink_user, sink_total);
}

int iotc_https_download(const char *hostname, const char *resource, const char *ca_pem,
                        int timeout_ms, uint8_t *buf, size_t buf_size, size_t *out_len)
{
    return https_get(hostname, resource, ca_pem, timeout_ms, buf, buf_size, out_len,
                     NULL, NULL, NULL);
}

/*
 * Large download: the caller's buffer serves as the coreHTTP work buffer
 * (headers + body), so multi-hundred-KB payloads (AI models) never need a
 * second copy. On success the body is moved to the start of workbuf.
 */
int iotc_https_download_large(const char *hostname, const char *resource,
                              const char *ca_pem, int timeout_ms,
                              uint8_t *workbuf, size_t workbuf_size, size_t *out_len)
{
    return https_get_ex(hostname, resource, ca_pem, timeout_ms,
                        workbuf, workbuf_size,
                        workbuf, workbuf_size, out_len, NULL, NULL, NULL);
}

int iotc_https_download_stream(const char *hostname, const char *resource, const char *ca_pem,
                               int timeout_ms, iotc_https_sink_t sink, void *user,
                               size_t *out_total)
{
    return https_get(hostname, resource, ca_pem, timeout_ms, NULL, 0, NULL,
                     sink, user, out_total);
}

int iotc_dra_run(const iotc_dra_config_t *cfg)
{
    int status;
    IotclDraUrlContext discovery_url = {0};
    IotclDraUrlContext base_url = {0};

    if (cfg->platform == IOTC_DRA_PF_AZURE)
    {
        status = iotcl_dra_discovery_init_url_azure(&discovery_url, cfg->cpid, cfg->env);
    }
    else
    {
        status = iotcl_dra_discovery_init_url_aws(&discovery_url, cfg->cpid, cfg->env);
    }
    if (IOTCL_SUCCESS != status)
    {
        return status;
    }

    size_t len = 0;
    status = https_get(iotcl_dra_url_get_hostname(&discovery_url),
                       iotcl_dra_url_get_resource(&discovery_url),
                       cfg->dra_ca_pem, cfg->timeout_ms,
                       s_http_buf, sizeof(s_http_buf) - 1, &len, NULL, NULL, NULL);
    /* NOTE: body shares s_http_buf with headers; https_get memcpy'd body to start. */
    if (0 != status)
    {
        iotcl_dra_url_deinit(&discovery_url);
        return status;
    }
    s_http_buf[len] = 0;

    status = iotcl_dra_discovery_parse(&base_url, 0, (const char *) s_http_buf);
    iotcl_dra_url_deinit(&discovery_url);
    if (IOTCL_SUCCESS != status)
    {
        return status;
    }

    status = iotcl_dra_identity_build_url(&base_url, cfg->duid);
    if (IOTCL_SUCCESS == status)
    {
        len = 0;
        status = https_get(iotcl_dra_url_get_hostname(&base_url),
                           iotcl_dra_url_get_resource(&base_url),
                           cfg->dra_ca_pem, cfg->timeout_ms,
                           s_http_buf, sizeof(s_http_buf) - 1, &len, NULL, NULL, NULL);
        if (0 == status)
        {
            s_http_buf[len] = 0;
            status = iotcl_dra_identity_configure_library_mqtt((const char *) s_http_buf);
        }
    }

    iotcl_dra_url_deinit(&base_url);
    return status;
}
