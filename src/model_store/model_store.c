/*
 * Copyright (c) 2026 Avnet, Inc.
 * SPDX-License-Identifier: MIT
 *
 * IOTV model envelope + LittleFS persistence. See model_store.h.
 */

#include "model_store.h"

#include <string.h>
#include <stdio.h>

#include "lfs.h"
#include "iotc/iotc_fs.h"

#define MODEL_FILE "model.iotv"

uint32_t iotv_crc32(const uint8_t *data, size_t len)
{
    uint32_t crc = 0xFFFFFFFFu;
    for (size_t i = 0; i < len; i++)
    {
        crc ^= data[i];
        for (int b = 0; b < 8; b++)
        {
            crc = (crc >> 1) ^ (0xEDB88320u & (uint32_t) - (int32_t) (crc & 1u));
        }
    }
    return ~crc;
}

const char *iotv_validate(const uint8_t *buf, size_t len)
{
    if (len < IOTV_HDR_LEN)
    {
        return "short blob";
    }
    const struct iotv_hdr *h = (const struct iotv_hdr *) buf;
    if (0 != memcmp(h->magic, IOTV_MAGIC, 4))
    {
        return "bad magic";
    }
    if (IOTV_FMT_VER != h->fmt_ver)
    {
        return "format version";
    }
    if ((h->model_len > MODEL_MAX_BLOB - IOTV_HDR_LEN) ||
        (h->model_len + IOTV_HDR_LEN > len))
    {
        return "length";
    }
    const uint8_t *payload = buf + IOTV_HDR_LEN;
    if (0 != memcmp(&payload[4], "TFL3", 4))
    {
        return "not a tflite flatbuffer";
    }
    if (iotv_crc32(payload, h->model_len) != h->crc)
    {
        return "crc mismatch";
    }
    return NULL;
}

void iotv_wrap_in_place(uint8_t *buf, size_t payload_len, uint16_t model_ver,
                        const char *name)
{
    struct iotv_hdr *h = (struct iotv_hdr *) buf;
    memcpy(h->magic, IOTV_MAGIC, 4);
    h->fmt_ver = IOTV_FMT_VER;
    h->model_ver = model_ver;
    h->model_len = (uint32_t) payload_len;
    h->crc = iotv_crc32(buf + IOTV_HDR_LEN, payload_len);
    memset(h->name, 0, sizeof(h->name));
    strncpy(h->name, name, sizeof(h->name) - 1);
}

int model_store_save(const uint8_t *iotv_blob, size_t len)
{
    if (0 != iotc_fs_init())
    {
        return -5;
    }
    const char *reason = iotv_validate(iotv_blob, len);
    if (reason)
    {
        (void) reason;
        return -22;
    }

    int rc;
    lfs_file_t f;
    iotc_fs_lock();
    rc = lfs_file_open(iotc_fs_lfs(), &f, MODEL_FILE,
                       LFS_O_WRONLY | LFS_O_CREAT | LFS_O_TRUNC);
    if (0 == rc)
    {
        lfs_ssize_t n = lfs_file_write(iotc_fs_lfs(), &f, iotv_blob, (lfs_size_t) len);
        int cl = lfs_file_close(iotc_fs_lfs(), &f);
        rc = ((n == (lfs_ssize_t) len) && (0 == cl)) ? 0 : -5;
    }
    iotc_fs_unlock();
    return rc;
}

int model_store_load(uint8_t *buf, size_t buf_size, size_t *out_len)
{
    if (0 != iotc_fs_init())
    {
        return -5;
    }

    int rc;
    lfs_file_t f;
    iotc_fs_lock();
    rc = lfs_file_open(iotc_fs_lfs(), &f, MODEL_FILE, LFS_O_RDONLY);
    if (0 != rc)
    {
        iotc_fs_unlock();
        return -2; /* ENOENT */
    }
    lfs_soff_t size = lfs_file_size(iotc_fs_lfs(), &f);
    if ((size <= 0) || ((size_t) size > buf_size))
    {
        (void) lfs_file_close(iotc_fs_lfs(), &f);
        iotc_fs_unlock();
        return -12;
    }
    lfs_ssize_t n = lfs_file_read(iotc_fs_lfs(), &f, buf, (lfs_size_t) size);
    (void) lfs_file_close(iotc_fs_lfs(), &f);
    iotc_fs_unlock();

    if (n != size)
    {
        return -5;
    }
    if (NULL != iotv_validate(buf, (size_t) n))
    {
        return -2; /* treat corrupt as absent; caller falls back to builtin */
    }
    if (out_len)
    {
        *out_len = (size_t) n;
    }
    return 0;
}

int model_store_erase(void)
{
    if (0 != iotc_fs_init())
    {
        return -5;
    }
    iotc_fs_lock();
    int rc = lfs_remove(iotc_fs_lfs(), MODEL_FILE);
    iotc_fs_unlock();
    return (rc == 0 || rc == LFS_ERR_NOENT) ? 0 : -5;
}
