/*
 * Copyright (c) 2026 Avnet, Inc.
 * SPDX-License-Identifier: MIT
 *
 * Runtime device configuration, persisted on LittleFS (OSPI flash) so it
 * survives power cycles. Holds the IOTCONNECT identity: environment, CPID,
 * device unique ID, and the device X.509 certificate + private key (PEM).
 *
 * Values are written by the serial provisioning CLI (iotc_cli.c). At boot the
 * stored identity takes precedence over any identity compiled in via
 * app_secrets.h.
 */
#ifndef IOTC_CONFIG_H
#define IOTC_CONFIG_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define IOTC_CONFIG_ENV_MAX   16
#define IOTC_CONFIG_CPID_MAX  64
#define IOTC_CONFIG_DUID_MAX  64
#define IOTC_CONFIG_PEM_MAX   4096

/* A fully loaded identity; string members are heap-allocated. */
typedef struct
{
    char *env;
    char *cpid;
    char *duid;
    char *cert_pem;
    char *key_pem;
} iotc_config_identity_t;

/*
 * Load the stored identity. Returns 0 and fills *out (all members allocated)
 * only when every field is present; otherwise returns a negative errno and
 * leaves *out zeroed. Free with iotc_config_identity_free().
 */
int iotc_config_load(iotc_config_identity_t *out);
void iotc_config_identity_free(iotc_config_identity_t *ident);

/* Store one field (name: "env" | "cpid" | "duid" | "cert" | "key").
 * Returns 0 or a negative errno. */
int iotc_config_write(const char *name, const char *value, size_t len);

/* Read one field into buf (NUL-terminated). Returns length or negative errno
 * (-2 when the field is not stored). */
int iotc_config_read(const char *name, char *buf, size_t size);

/* Remove all stored fields. Returns 0 or a negative errno. */
int iotc_config_erase(void);

#ifdef __cplusplus
}
#endif

#endif /* IOTC_CONFIG_H */
