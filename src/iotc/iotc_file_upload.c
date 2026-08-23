/*
 * Copyright (c) 2026 Avnet, Inc.
 * SPDX-License-Identifier: MIT
 *
 * IOTCONNECT Telemetry Files upload, FSP/FreeRTOS port. Flow (see header):
 *
 *   1. GET the AWS IoT credentials-provider URL from the identity (fs.url)
 *      with MUTUAL TLS (device cert) + x-amzn-iot-thingname -> temp STS
 *      credentials.
 *   2. When the upload bucket is customer-owned (identity fs.buckets[].ca
 *      with a role ARN in .rarn), those credentials cannot touch the bucket
 *      directly: STS AssumeRole(rarn) is called with them, yielding a SECOND
 *      set of temp credentials.
 *   3. The file is PUT to s3://<bucket>/device-uploads/<client_id>/<path>
 *      with an on-device AWS SigV4 signature (service "s3").
 *   4. A telemetry-shaped {"url": ...,"cf": ...} record on the identity's
 *      fu topic makes the file appear in the platform's Telemetry Files.
 *
 * Ported from the Avnet IOTCONNECT reference implementation onto this
 * project's stack: coreHTTP for the HTTP client, mbedTLS one-shots for
 * hash/HMAC, FreeRTOS heap for allocation.
 */
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "FreeRTOS.h"
#include "core_http_client.h"
#include "transport_mbedtls_pkcs11.h"
#include "core_pkcs11_config.h"

#include "mbedtls/sha256.h"
#include "mbedtls/md.h"

#include "cJSON.h"

#include "iotcl.h"
#include "iotcl_util.h"
#include "iotcl_log.h"
#include "iotc_mqtt_client.h"
#include "iotc_time.h"
#include "iotc_file_upload.h"

#define FU_RESP_BUF_SIZE 6144
#define FU_TIMEOUT_MS 20000

#define SHA256_HEX_LEN 65 /* 64 hex chars + NUL */
#define SIGNED_HEADERS "host;x-amz-content-sha256;x-amz-date;x-amz-security-token"

/* Captured from the identity response by iotc_fu_identity_hook(). */
static char *fu_topic;
static char *fu_bucket;
static char *fu_role_arn; /* NULL unless the bucket is customer-owned */

/* Root CA for the credentials provider / STS / S3 (all *.amazonaws.com:
 * Amazon Root CA 1). Set by the orchestrator at init. */
static const char *s_fu_ca;

/* Response scratch (headers + body). Uploads run on the net thread only. */
static uint8_t s_fu_buf[FU_RESP_BUF_SIZE];

struct NetworkContext
{
    TlsTransportParams_t *pParams;
};

void iotc_fu_set_ca(const char *ca_pem)
{
    s_fu_ca = ca_pem;
}

/* --------------------------------------------------------------------------
 * Identity capture
 * -------------------------------------------------------------------------- */

static char *dup_or_null(const char *s)
{
    if (s == NULL)
    {
        return NULL;
    }
    size_t n = strlen(s) + 1;
    char *d = pvPortMalloc(n);

    if (d != NULL)
    {
        memcpy(d, s, n);
    }
    return d;
}

static void free_null(char **p)
{
    if (*p != NULL)
    {
        vPortFree(*p);
        *p = NULL;
    }
}

void iotc_fu_identity_hook(const char *identity_json)
{
    cJSON *root = cJSON_Parse(identity_json);

    if (root == NULL)
    {
        return;
    }

    free_null(&fu_topic);
    free_null(&fu_bucket);
    free_null(&fu_role_arn);

    cJSON *d = cJSON_GetObjectItem(root, "d");
    cJSON *p = cJSON_GetObjectItem(d, "p");
    cJSON *topics = cJSON_GetObjectItem(p, "topics");
    cJSON *fu = cJSON_GetObjectItem(topics, "fu");

    if (cJSON_IsString(fu))
    {
        fu_topic = dup_or_null(cJSON_GetStringValue(fu));
    }

    cJSON *fs = cJSON_GetObjectItem(p, "fs");
    cJSON *buckets = cJSON_GetObjectItem(fs, "buckets");

    /* Prefer a platform-owned bucket (no AssumeRole hop); fall back to
     * the first customer-owned one and remember its role ARN. */
    if (cJSON_IsArray(buckets))
    {
        cJSON *chosen = NULL;
        cJSON *it;

        cJSON_ArrayForEach(it, buckets)
        {
            if (!cJSON_IsString(cJSON_GetObjectItem(it, "bn")))
            {
                continue;
            }
            if (chosen == NULL)
            {
                chosen = it;
            }
            if (!cJSON_IsTrue(cJSON_GetObjectItem(it, "ca")))
            {
                chosen = it;
                break;
            }
        }
        if (chosen != NULL)
        {
            fu_bucket = dup_or_null(cJSON_GetStringValue(
                cJSON_GetObjectItem(chosen, "bn")));
            if (cJSON_IsTrue(cJSON_GetObjectItem(chosen, "ca")))
            {
                fu_role_arn = dup_or_null(cJSON_GetStringValue(
                    cJSON_GetObjectItem(chosen, "rarn")));
            }
        }
    }

    if (fu_topic != NULL && fu_bucket != NULL)
    {
        IOTCL_INFO("FU: file upload ready (bucket %s%s)", fu_bucket,
                   fu_role_arn != NULL ? ", via AssumeRole" : "");
    }
    cJSON_Delete(root);
}

bool iotc_fu_available(void)
{
    IotclMqttConfig *mc = iotcl_mqtt_get_config();

    return mc != NULL && mc->aws.fs_creds_url != NULL &&
           mc->client_id != NULL && fu_topic != NULL && fu_bucket != NULL;
}

/* --------------------------------------------------------------------------
 * Minimal HTTPS request (GET/PUT/POST) with extra headers over the FSP
 * MbedTLS/PKCS11 transport. The device cert labels are always supplied; the
 * client certificate is only sent when the server requests it (the
 * credentials provider does; STS and S3 do not).
 * -------------------------------------------------------------------------- */

struct fu_header
{
    const char *name;
    const char *value;
};

static int fu_https_request(const char *method, const char *host,
                            const char *path, const struct fu_header *headers,
                            size_t n_headers, const char *content_type,
                            const uint8_t *payload, size_t payload_len,
                            uint8_t *workbuf, size_t work_size,
                            size_t *out_len, uint16_t *http_status)
{
    TlsTransportParams_t params;
    NetworkContext_t net = {0};
    memset(&params, 0, sizeof(params));
    net.pParams = &params;

    if (out_len != NULL)
    {
        *out_len = 0;
    }

    NetworkCredentials_t creds;
    memset(&creds, 0, sizeof(creds));
    creds.pRootCa = (const unsigned char *) s_fu_ca;
    creds.rootCaSize = s_fu_ca ? strlen(s_fu_ca) + 1 : 0;
    creds.pClientCertLabel = pkcs11configLABEL_DEVICE_CERTIFICATE_FOR_TLS;
    creds.pPrivateKeyLabel = pkcs11configLABEL_DEVICE_PRIVATE_KEY_FOR_TLS;

    TlsTransportStatus_t ts = TLS_FreeRTOS_Connect(&net, host, 443, &creds,
                                                   FU_TIMEOUT_MS, FU_TIMEOUT_MS);
    if (TLS_TRANSPORT_SUCCESS != ts)
    {
        IOTCL_ERROR(ts, "FU: TLS connect to %s failed", host);
        return -1;
    }
    /* Debug: confirm what this session actually negotiated and which client
     * certificate mbedTLS loaded from the PKCS#11 store. */
    {
        extern uint32_t iotv_crc32(const uint8_t *data, size_t len);
        const mbedtls_x509_crt *cc = &params.sslContext.clientCert;
        IOTCL_INFO("FU: tls=%s cert_len=%u cert_crc=%08lx",
                   mbedtls_ssl_get_version(&params.sslContext.context),
                   (unsigned) cc->raw.len,
                   (cc->raw.p != NULL)
                       ? (unsigned long) iotv_crc32(cc->raw.p, cc->raw.len)
                       : 0UL);
    }

    int rc = -1;
    TransportInterface_t xport = {0};
    xport.pNetworkContext = &net;
    xport.send = TLS_FreeRTOS_send;
    xport.recv = TLS_FreeRTOS_recv;

    HTTPRequestInfo_t req = {0};
    HTTPRequestHeaders_t hdrs = {0};
    HTTPResponse_t resp = {0};

    req.pMethod = method;
    req.methodLen = strlen(method);
    req.pHost = host;
    req.hostLen = strlen(host);
    req.pPath = path;
    req.pathLen = strlen(path);
    req.reqFlags = 0;

    hdrs.pBuffer = workbuf;
    hdrs.bufferLen = work_size;

    HTTPStatus_t hs = HTTPClient_InitializeRequestHeaders(&hdrs, &req);
    for (size_t i = 0; (HTTPSuccess == hs) && (i < n_headers); i++)
    {
        hs = HTTPClient_AddHeader(&hdrs,
                                  headers[i].name, strlen(headers[i].name),
                                  headers[i].value, strlen(headers[i].value));
    }
    if ((HTTPSuccess == hs) && (content_type != NULL))
    {
        hs = HTTPClient_AddHeader(&hdrs, "Content-Type",
                                  strlen("Content-Type"),
                                  content_type, strlen(content_type));
    }

    if (HTTPSuccess == hs)
    {
        resp.pBuffer = workbuf;
        resp.bufferLen = work_size;

        hs = HTTPClient_Send(&xport, &hdrs, payload, payload_len, &resp, 0);
        if (HTTPSuccess == hs)
        {
            if (http_status != NULL)
            {
                *http_status = resp.statusCode;
            }
            /* Move the body to the start of workbuf and NUL-terminate so
             * callers can parse it as a string. */
            if ((resp.pBody != NULL) && (resp.bodyLen > 0) &&
                (resp.bodyLen < work_size))
            {
                memmove(workbuf, resp.pBody, resp.bodyLen);
                workbuf[resp.bodyLen] = '\0';
                if (out_len != NULL)
                {
                    *out_len = resp.bodyLen;
                }
            }
            else if (work_size > 0)
            {
                workbuf[0] = '\0';
            }
            rc = 0;
        }
        else
        {
            IOTCL_ERROR((int) hs, "FU: HTTP %s %s failed", method, host);
        }
    }

    TLS_FreeRTOS_Disconnect(&net);
    return rc;
}

/* --------------------------------------------------------------------------
 * SigV4
 * -------------------------------------------------------------------------- */

struct aws_creds
{
    char *akid;
    char *secret;
    char *token;
};

static void creds_free(struct aws_creds *c)
{
    free_null(&c->akid);
    free_null(&c->secret);
    free_null(&c->token);
}

static void to_hex(const uint8_t *in, size_t in_len, char *out)
{
    static const char hexd[] = "0123456789abcdef";

    for (size_t i = 0; i < in_len; i++)
    {
        out[i * 2] = hexd[in[i] >> 4];
        out[i * 2 + 1] = hexd[in[i] & 0xF];
    }
    out[in_len * 2] = '\0';
}

static int sha256_hex(const uint8_t *data, size_t len, char *hex_out)
{
    uint8_t hash[32];

    if (0 != mbedtls_sha256(data, len, hash, 0))
    {
        return -5; /* -EIO */
    }
    to_hex(hash, sizeof(hash), hex_out);
    return 0;
}

static int hmac_sha256(const uint8_t *key, size_t key_len, const char *msg,
                       uint8_t out[32])
{
    const mbedtls_md_info_t *md = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);

    if ((md == NULL) ||
        (0 != mbedtls_md_hmac(md, key, key_len,
                              (const uint8_t *) msg, strlen(msg), out)))
    {
        return -5; /* -EIO */
    }
    return 0;
}

/*
 * Build the SigV4 Authorization header value and x-amz-date timestamp for
 * (method host path service region payload_hash) signed with creds. The
 * canonical request signs exactly: host, x-amz-content-sha256, x-amz-date,
 * x-amz-security-token. auth_val gets the bare header value (no name/CRLF;
 * coreHTTP formats headers), amz_ts gets "YYYYMMDDTHHMMSSZ" (>= 17 bytes).
 */
static int sigv4_build(const char *method, const char *host, const char *path,
                       const char *service, const char *region,
                       const char *payload_hash, const struct aws_creds *c,
                       char *auth_val, size_t auth_size, char *amz_ts)
{
    char date[9];
    char canon_hash[SHA256_HEX_LEN];
    /* iotc_time_now(), not time(): picolibc's time() links against
     * gettimeofday, which this project does not provide. */
    time_t now = (time_t) iotc_time_now();
    struct tm tm;
    int ret;

    gmtime_r(&now, &tm);
    snprintf(date, sizeof(date), "%04d%02d%02d", tm.tm_year + 1900,
             tm.tm_mon + 1, tm.tm_mday);
    snprintf(amz_ts, 17, "%sT%02d%02d%02dZ", date, tm.tm_hour,
             tm.tm_min, tm.tm_sec);

    size_t canon_size = 512 + strlen(c->token);
    char *canon = pvPortMalloc(canon_size);

    if (canon == NULL)
    {
        return -12; /* -ENOMEM */
    }
    snprintf(canon, canon_size,
             "%s\n%s\n\n"
             "host:%s\n"
             "x-amz-content-sha256:%s\n"
             "x-amz-date:%s\n"
             "x-amz-security-token:%s\n"
             "\n" SIGNED_HEADERS "\n%s",
             method, path, host, payload_hash, amz_ts, c->token,
             payload_hash);
    ret = sha256_hex((const uint8_t *) canon, strlen(canon), canon_hash);
    vPortFree(canon);
    if (ret != 0)
    {
        return ret;
    }

    char scope[80], sts_str[256];

    snprintf(scope, sizeof(scope), "%s/%s/%s/aws4_request", date, region,
             service);
    snprintf(sts_str, sizeof(sts_str), "AWS4-HMAC-SHA256\n%s\n%s\n%s",
             amz_ts, scope, canon_hash);

    uint8_t k1[32], k2[32], sig[32];
    char kseed[64], sig_hex[SHA256_HEX_LEN];

    snprintf(kseed, sizeof(kseed), "AWS4%s", c->secret);
    if (hmac_sha256((uint8_t *) kseed, strlen(kseed), date, k1) != 0 ||
        hmac_sha256(k1, 32, region, k2) != 0 ||
        hmac_sha256(k2, 32, service, k1) != 0 ||
        hmac_sha256(k1, 32, "aws4_request", k2) != 0 ||
        hmac_sha256(k2, 32, sts_str, sig) != 0)
    {
        return -5; /* -EIO */
    }
    to_hex(sig, 32, sig_hex);

    snprintf(auth_val, auth_size,
             "AWS4-HMAC-SHA256 Credential=%s/%s, "
             "SignedHeaders=" SIGNED_HEADERS ", Signature=%s",
             c->akid, scope, sig_hex);
    return 0;
}

/* --------------------------------------------------------------------------
 * Small parsing helpers
 * -------------------------------------------------------------------------- */

static bool key_char_ok(char ch)
{
    return (ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z') ||
           (ch >= '0' && ch <= '9') || ch == '-' || ch == '_' ||
           ch == '.' || ch == '~' || ch == '/';
}

static bool key_is_clean(const char *s)
{
    for (; *s != '\0'; s++)
    {
        if (!key_char_ok(*s))
        {
            return false;
        }
    }
    return true;
}

/* Extract "<region>" from "<id>.credentials.iot.<region>.amazonaws.com". */
static int region_from_creds_host(const char *host, char *out, size_t out_size)
{
    const char *marker = ".iot.";
    const char *start = strstr(host, marker);

    if (start == NULL)
    {
        return -22; /* -EINVAL */
    }
    start += strlen(marker);
    const char *end = strchr(start, '.');

    if (end == NULL || (size_t) (end - start) >= out_size)
    {
        return -22;
    }
    memcpy(out, start, (size_t) (end - start));
    out[end - start] = '\0';
    return 0;
}

static int split_url(const char *url, char *host, size_t host_size,
                     const char **path)
{
    const char *p = strstr(url, "://");

    p = (p != NULL) ? p + 3 : url;
    const char *slash = strchr(p, '/');
    size_t hlen = (slash != NULL) ? (size_t) (slash - p) : strlen(p);

    if (hlen == 0 || hlen >= host_size)
    {
        return -22; /* -EINVAL */
    }
    memcpy(host, p, hlen);
    host[hlen] = '\0';
    *path = (slash != NULL) ? slash : "/";
    return 0;
}

/* Copy the text between <tag> and </tag> into a fresh malloc'd string. */
static char *xml_field_dup(const char *xml, const char *tag)
{
    char open[48];

    snprintf(open, sizeof(open), "<%s>", tag);
    const char *start = strstr(xml, open);

    if (start == NULL)
    {
        return NULL;
    }
    start += strlen(open);
    snprintf(open, sizeof(open), "</%s>", tag);
    const char *end = strstr(start, open);

    if (end == NULL)
    {
        return NULL;
    }

    size_t n = (size_t) (end - start);
    char *out = pvPortMalloc(n + 1);

    if (out != NULL)
    {
        memcpy(out, start, n);
        out[n] = '\0';
    }
    return out;
}

/* --------------------------------------------------------------------------
 * Step 1: temporary credentials from the AWS IoT credentials provider
 * -------------------------------------------------------------------------- */

static int fetch_credentials(const char *creds_url, const char *client_id,
                             struct aws_creds *out)
{
    char host[128];
    const char *path;
    size_t body_len = 0;
    uint16_t status = 0;
    int ret;

    ret = split_url(creds_url, host, sizeof(host), &path);
    if (ret != 0)
    {
        return ret;
    }

    const struct fu_header headers[] = {
        {"x-amzn-iot-thingname", client_id},
    };

    ret = fu_https_request(HTTP_METHOD_GET, host, path, headers, 1, NULL,
                           NULL, 0, s_fu_buf, sizeof(s_fu_buf),
                           &body_len, &status);
    if (ret != 0 || status != 200)
    {
        IOTCL_ERROR(ret, "FU: credentials GET failed (HTTP %u)", status);
        IOTCL_ERROR(0, "FU: host=%s path=%s thing=%s", host, path, client_id);
        if (body_len > 0)
        {
            IOTCL_ERROR(0, "FU: server says: %.300s", (char *) s_fu_buf);
        }
        return (ret != 0) ? ret : -13; /* -EACCES */
    }

    cJSON *root = cJSON_Parse((const char *) s_fu_buf);
    cJSON *creds = cJSON_GetObjectItem(root, "credentials");

    out->akid = dup_or_null(cJSON_GetStringValue(
        cJSON_GetObjectItem(creds, "accessKeyId")));
    out->secret = dup_or_null(cJSON_GetStringValue(
        cJSON_GetObjectItem(creds, "secretAccessKey")));
    out->token = dup_or_null(cJSON_GetStringValue(
        cJSON_GetObjectItem(creds, "sessionToken")));
    cJSON_Delete(root);

    if (out->akid == NULL || out->secret == NULL || out->token == NULL)
    {
        IOTCL_ERROR(0, "FU: credentials response missing fields");
        creds_free(out);
        return -74; /* -EBADMSG */
    }
    return 0;
}

/* One-shot diagnostic: exercise only the credentials fetch (step 1). */
int iotc_fu_selftest(void)
{
    IotclMqttConfig *mc = iotcl_mqtt_get_config();
    struct aws_creds c = {0};

    if (!iotc_fu_available())
    {
        IOTCL_INFO("FU: selftest skipped (upload not available)");
        return -95;
    }
    int ret = fetch_credentials(mc->aws.fs_creds_url, mc->client_id, &c);
    IOTCL_INFO("FU: selftest creds fetch -> %d%s", ret,
               (0 == ret) ? " (OK)" : "");
    creds_free(&c);
    return ret;
}

/* --------------------------------------------------------------------------
 * Step 2: STS AssumeRole (customer-owned buckets)
 * -------------------------------------------------------------------------- */

/* %-encode ':' and '/' (the only reserved chars in a role ARN). */
static void urlencode_arn(const char *arn, char *out, size_t out_size)
{
    size_t o = 0;

    for (; *arn != '\0' && o + 4 < out_size; arn++)
    {
        if (*arn == ':')
        {
            memcpy(&out[o], "%3A", 3);
            o += 3;
        }
        else if (*arn == '/')
        {
            memcpy(&out[o], "%2F", 3);
            o += 3;
        }
        else
        {
            out[o++] = *arn;
        }
    }
    out[o] = '\0';
}

static int sts_assume_role(const struct aws_creds *in, const char *region,
                           const char *role_arn, const char *session_name,
                           struct aws_creds *out)
{
    char sts_host[48];
    char arn_enc[256];
    char body[512];
    char payload_hash[SHA256_HEX_LEN];
    char auth_val[256], amz_ts[17];
    size_t resp_len = 0;
    uint16_t status = 0;
    int ret;

    snprintf(sts_host, sizeof(sts_host), "sts.%s.amazonaws.com", region);
    urlencode_arn(role_arn, arn_enc, sizeof(arn_enc));
    snprintf(body, sizeof(body),
             "Action=AssumeRole&Version=2011-06-15&RoleArn=%s"
             "&RoleSessionName=%s",
             arn_enc, session_name);

    ret = sha256_hex((const uint8_t *) body, strlen(body), payload_hash);
    if (ret != 0)
    {
        return ret;
    }
    ret = sigv4_build("POST", sts_host, "/", "sts", region, payload_hash,
                      in, auth_val, sizeof(auth_val), amz_ts);
    if (ret != 0)
    {
        return ret;
    }

    const struct fu_header headers[] = {
        {"Authorization", auth_val},
        {"x-amz-content-sha256", payload_hash},
        {"x-amz-date", amz_ts},
        {"x-amz-security-token", in->token},
    };

    ret = fu_https_request(HTTP_METHOD_POST, sts_host, "/", headers, 4,
                           "application/x-www-form-urlencoded",
                           (const uint8_t *) body, strlen(body),
                           s_fu_buf, sizeof(s_fu_buf), &resp_len, &status);
    if (ret != 0 || status != 200)
    {
        IOTCL_ERROR(ret, "FU: STS AssumeRole failed (HTTP %u)", status);
        if (resp_len > 0)
        {
            IOTCL_ERROR(0, "FU: STS says: %.200s", (char *) s_fu_buf);
        }
        return (ret != 0) ? ret : -13; /* -EACCES */
    }

    out->akid = xml_field_dup((const char *) s_fu_buf, "AccessKeyId");
    out->secret = xml_field_dup((const char *) s_fu_buf, "SecretAccessKey");
    out->token = xml_field_dup((const char *) s_fu_buf, "SessionToken");

    if (out->akid == NULL || out->secret == NULL || out->token == NULL)
    {
        IOTCL_ERROR(0, "FU: AssumeRole response missing fields");
        creds_free(out);
        return -74; /* -EBADMSG */
    }
    IOTCL_INFO("FU: AssumeRole OK");
    return 0;
}

/* --------------------------------------------------------------------------
 * Steps 3+4: S3 PUT + fu announce
 * -------------------------------------------------------------------------- */

int iotc_fu_upload(const char *rel_path, const uint8_t *data, size_t len,
                   const char *content_type, const char *cf_json)
{
    IotclMqttConfig *mc = iotcl_mqtt_get_config();
    struct aws_creds provider_creds = {0};
    struct aws_creds role_creds = {0};
    const struct aws_creds *s3_creds;
    char creds_host[128], region[24];
    const char *creds_path;
    char payload_hash[SHA256_HEX_LEN];
    char s3_host[192], object_key[192];
    char auth_val[256], amz_ts[17];
    int ret;

    if (!iotc_fu_available())
    {
        IOTCL_ERROR(0, "FU: not available (enable File Support on the "
                       "template and re-run discovery)");
        return -95; /* -ENOTSUP */
    }
    if (rel_path == NULL || data == NULL || len == 0 ||
        !key_is_clean(rel_path) || rel_path[0] == '/')
    {
        return -22; /* -EINVAL */
    }
    if (content_type == NULL)
    {
        content_type = "application/octet-stream";
    }

    ret = split_url(mc->aws.fs_creds_url, creds_host, sizeof(creds_host),
                    &creds_path);
    if (ret == 0)
    {
        ret = region_from_creds_host(creds_host, region, sizeof(region));
    }
    if (ret != 0)
    {
        IOTCL_WARN(0, "FU: cannot parse region; assuming us-east-1");
        strcpy(region, "us-east-1");
    }

    ret = fetch_credentials(mc->aws.fs_creds_url, mc->client_id,
                            &provider_creds);
    if (ret != 0)
    {
        return ret;
    }
    s3_creds = &provider_creds;

    if (fu_role_arn != NULL)
    {
        ret = sts_assume_role(&provider_creds, region, fu_role_arn,
                              mc->client_id, &role_creds);
        if (ret != 0)
        {
            goto out;
        }
        s3_creds = &role_creds;
    }

    snprintf(s3_host, sizeof(s3_host), "%s.s3.%s.amazonaws.com",
             fu_bucket, region);
    ret = snprintf(object_key, sizeof(object_key),
                   "/device-uploads/%s/%s", mc->client_id, rel_path);
    if (ret < 0 || (size_t) ret >= sizeof(object_key) ||
        !key_is_clean(object_key))
    {
        ret = -22; /* -EINVAL */
        goto out;
    }

    ret = sha256_hex(data, len, payload_hash);
    if (ret != 0)
    {
        goto out;
    }
    ret = sigv4_build("PUT", s3_host, object_key, "s3", region,
                      payload_hash, s3_creds, auth_val, sizeof(auth_val),
                      amz_ts);
    if (ret != 0)
    {
        goto out;
    }

    const struct fu_header headers[] = {
        {"Authorization", auth_val},
        {"x-amz-content-sha256", payload_hash},
        {"x-amz-date", amz_ts},
        {"x-amz-security-token", s3_creds->token},
    };
    size_t err_len = 0;
    uint16_t http_status = 0;

    IOTCL_INFO("FU: PUT https://%s%s (%u B)", s3_host, object_key,
               (unsigned int) len);
    ret = fu_https_request(HTTP_METHOD_PUT, s3_host, object_key, headers, 4,
                           content_type, data, len,
                           s_fu_buf, sizeof(s_fu_buf), &err_len, &http_status);
    if (ret != 0 || http_status != 200)
    {
        IOTCL_ERROR(ret, "FU: S3 PUT failed (HTTP %u)", http_status);
        if (err_len > 0)
        {
            IOTCL_ERROR(0, "FU: S3 says: %.240s", (char *) s_fu_buf);
        }
        if (ret == 0)
        {
            ret = -13; /* -EACCES */
        }
        goto out;
    }

    /* ---- announce on the fu topic ------------------------------------- */
    {
        cJSON *root = cJSON_CreateObject();
        cJSON *arr = cJSON_AddArrayToObject(root, "d");
        cJSON *item = cJSON_CreateObject();
        cJSON *drec = cJSON_CreateObject();
        char ts_iso[32];

        if (iotcl_iso_timestamp_now(ts_iso, sizeof(ts_iso)) == 0)
        {
            cJSON_AddStringToObject(item, "dt", ts_iso);
        }
        if (cf_json != NULL)
        {
            cJSON *cf = cJSON_Parse(cf_json);

            if (cf != NULL)
            {
                cJSON_AddItemToObject(drec, "cf", cf);
            }
            else
            {
                cJSON_AddStringToObject(drec, "cf", cf_json);
            }
        }
        cJSON_AddStringToObject(drec, "url", rel_path);
        cJSON_AddItemToObject(item, "d", drec);
        cJSON_AddItemToArray(arr, item);

        char *msg = cJSON_PrintUnformatted(root);

        cJSON_Delete(root);
        if (msg == NULL)
        {
            ret = -12; /* -ENOMEM */
            goto out;
        }
        iotc_mqtt_client_publish(fu_topic, msg);
        cJSON_free(msg);
        IOTCL_INFO("FU: file \"%s\" uploaded + announced", rel_path);
        ret = 0;
    }

out:
    creds_free(&provider_creds);
    creds_free(&role_creds);
    return ret;
}
