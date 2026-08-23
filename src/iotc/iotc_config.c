/*
 * Copyright (c) 2026 Avnet, Inc.
 * SPDX-License-Identifier: MIT
 *
 * Runtime device configuration on LittleFS (see iotc_config.h).
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "FreeRTOS.h"

#include "lfs.h"
#include "iotc_fs.h"
#include "iotc_config.h"

/* One file per field, at the filesystem root, away from the PKCS#11 store. */
static int prv_path(const char *name, char *out, size_t size)
{
    static const char *const valid[] = {"env", "cpid", "duid", "cert", "key"};
    for (size_t i = 0; i < sizeof(valid) / sizeof(valid[0]); i++)
    {
        if (0 == strcmp(name, valid[i]))
        {
            snprintf(out, size, "iotc_cfg_%s", name);
            return 0;
        }
    }
    return -22; /* -EINVAL */
}

int iotc_config_write(const char *name, const char *value, size_t len)
{
    char path[32];
    if ((0 != prv_path(name, path, sizeof(path))) || (NULL == value))
    {
        return -22;
    }
    if (0 != iotc_fs_init())
    {
        return -5; /* -EIO */
    }

    iotc_fs_lock();
    lfs_file_t f;
    int rc = lfs_file_open(iotc_fs_lfs(), &f, path,
                           LFS_O_WRONLY | LFS_O_CREAT | LFS_O_TRUNC);
    if (rc >= 0)
    {
        lfs_ssize_t n = lfs_file_write(iotc_fs_lfs(), &f, value, (lfs_size_t) len);
        int rc2 = lfs_file_close(iotc_fs_lfs(), &f);
        rc = (n < 0) ? (int) n : ((rc2 < 0) ? rc2 : 0);
    }
    iotc_fs_unlock();
    return (rc < 0) ? -5 : 0;
}

int iotc_config_read(const char *name, char *buf, size_t size)
{
    char path[32];
    if ((0 != prv_path(name, path, sizeof(path))) || (NULL == buf) || (size < 2))
    {
        return -22;
    }
    if (0 != iotc_fs_init())
    {
        return -5;
    }

    iotc_fs_lock();
    lfs_file_t f;
    int rc = lfs_file_open(iotc_fs_lfs(), &f, path, LFS_O_RDONLY);
    if (rc < 0)
    {
        iotc_fs_unlock();
        return -2; /* -ENOENT */
    }
    lfs_ssize_t n = lfs_file_read(iotc_fs_lfs(), &f, buf, (lfs_size_t) (size - 1));
    (void) lfs_file_close(iotc_fs_lfs(), &f);
    iotc_fs_unlock();

    if (n < 0)
    {
        return -5;
    }
    buf[n] = '\0';
    return (int) n;
}

int iotc_config_erase(void)
{
    static const char *const names[] = {"env", "cpid", "duid", "cert", "key"};
    if (0 != iotc_fs_init())
    {
        return -5;
    }
    iotc_fs_lock();
    for (size_t i = 0; i < sizeof(names) / sizeof(names[0]); i++)
    {
        char path[32];
        (void) prv_path(names[i], path, sizeof(path));
        (void) lfs_remove(iotc_fs_lfs(), path); /* ENOENT is fine */
    }
    iotc_fs_unlock();
    return 0;
}

static char *prv_load_field(const char *name, size_t max)
{
    char *buf = pvPortMalloc(max);
    if (NULL == buf)
    {
        return NULL;
    }
    int n = iotc_config_read(name, buf, max);
    if (n <= 0)
    {
        vPortFree(buf);
        return NULL;
    }
    return buf;
}

int iotc_config_load(iotc_config_identity_t *out)
{
    memset(out, 0, sizeof(*out));
    out->env = prv_load_field("env", IOTC_CONFIG_ENV_MAX);
    out->cpid = prv_load_field("cpid", IOTC_CONFIG_CPID_MAX);
    out->duid = prv_load_field("duid", IOTC_CONFIG_DUID_MAX);
    out->cert_pem = prv_load_field("cert", IOTC_CONFIG_PEM_MAX);
    out->key_pem = prv_load_field("key", IOTC_CONFIG_PEM_MAX);

    if (out->env && out->cpid && out->duid && out->cert_pem && out->key_pem)
    {
        return 0;
    }
    iotc_config_identity_free(out);
    return -2; /* -ENOENT: incomplete */
}

void iotc_config_identity_free(iotc_config_identity_t *ident)
{
    if (ident->env) { vPortFree(ident->env); }
    if (ident->cpid) { vPortFree(ident->cpid); }
    if (ident->duid) { vPortFree(ident->duid); }
    if (ident->cert_pem) { vPortFree(ident->cert_pem); }
    if (ident->key_pem) { vPortFree(ident->key_pem); }
    memset(ident, 0, sizeof(*ident));
}
