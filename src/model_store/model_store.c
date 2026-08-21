/*
 * Copyright (c) 2026 Avnet, Inc.
 * SPDX-License-Identifier: MIT
 *
 * IOTV model envelope + persistence. See model_store.h.
 *
 * Storage: a raw slot in the 64 MB OSPI flash at +56 MB (top 8 MB),
 * clear of the 32 MB LittleFS credential partition and the factory asset
 * area. Written with the r_ospi_b spi-flash API; read back through the
 * XIP window (with D-cache invalidation - the OSPI programs the array
 * behind the cache's back).
 */

#include "model_store.h"

#include <string.h>
#include <stdio.h>

#include "hal_data.h"
#include "iotc/iotc_fs.h"

/* OSPI CS1 XIP window base and the model slot within the flash. */
#define OSPI_XIP_BASE      (0x90000000u)
#define MODEL_SLOT_OFFSET  (56u * 1024u * 1024u)
#define MODEL_SLOT_ADDR    (OSPI_XIP_BASE + MODEL_SLOT_OFFSET)
#define OSPI_BLOCK_ERASE   (65536u)
#define OSPI_PROG_CHUNK    (64u)

extern const spi_flash_instance_t g_ospi0;

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

static int prv_wait_idle(void)
{
    spi_flash_status_t st = {0};
    for (uint32_t i = 0; i < 4000000u; i++)
    {
        if (FSP_SUCCESS != g_ospi0.p_api->statusGet(g_ospi0.p_ctrl, &st))
        {
            return -5;
        }
        if (!st.write_in_progress)
        {
            return 0;
        }
    }
    return -110; /* timeout */
}

int model_store_save(const uint8_t *iotv_blob, size_t len)
{
    if (0 != iotc_fs_init()) /* opens the OSPI (littlefs mount may fail; ok) */
    {
        /* iotc_fs_init reports failure when littlefs is unusable, but the
         * OSPI itself is opened first; proceed if the flash API works. */
    }
    if (NULL != iotv_validate(iotv_blob, len))
    {
        return -22;
    }

    iotc_fs_lock();
    int rc = 0;

    /* Erase whole 64KB blocks covering the blob. */
    size_t erase_len = (len + OSPI_BLOCK_ERASE - 1) & ~(OSPI_BLOCK_ERASE - 1);
    for (size_t off = 0; (0 == rc) && (off < erase_len); off += OSPI_BLOCK_ERASE)
    {
        if (FSP_SUCCESS != g_ospi0.p_api->erase(g_ospi0.p_ctrl,
                                                (uint8_t *) (MODEL_SLOT_ADDR + off),
                                                OSPI_BLOCK_ERASE))
        {
            rc = -5;
            break;
        }
        rc = prv_wait_idle();
    }

    /* Program in small chunks (combination-buffer sized). */
    for (size_t off = 0; (0 == rc) && (off < len); off += OSPI_PROG_CHUNK)
    {
        size_t n = len - off;
        if (n > OSPI_PROG_CHUNK)
        {
            n = OSPI_PROG_CHUNK;
        }
        if (FSP_SUCCESS != g_ospi0.p_api->write(g_ospi0.p_ctrl,
                                                (uint8_t *) (iotv_blob + off),
                                                (uint8_t *) (MODEL_SLOT_ADDR + off),
                                                (uint32_t) n))
        {
            rc = -5;
            break;
        }
        rc = prv_wait_idle();
    }

    if (0 == rc)
    {
        /* Verify through the XIP window. */
        SCB_InvalidateDCache_by_Addr((void *) MODEL_SLOT_ADDR, (int32_t) len);
        if (0 != memcmp((const void *) MODEL_SLOT_ADDR, iotv_blob, len))
        {
            rc = -84; /* verify failed */
        }
    }
    iotc_fs_unlock();
    return rc;
}

int model_store_load(uint8_t *buf, size_t buf_size, size_t *out_len)
{
    (void) iotc_fs_init();

    iotc_fs_lock();
    SCB_InvalidateDCache_by_Addr((void *) MODEL_SLOT_ADDR, IOTV_HDR_LEN);
    const struct iotv_hdr *h = (const struct iotv_hdr *) MODEL_SLOT_ADDR;
    if (0 != memcmp(h->magic, IOTV_MAGIC, 4))
    {
        iotc_fs_unlock();
        return -2; /* empty */
    }
    size_t total = (size_t) h->model_len + IOTV_HDR_LEN;
    if ((h->model_len > MODEL_MAX_BLOB - IOTV_HDR_LEN) || (total > buf_size))
    {
        iotc_fs_unlock();
        return -12;
    }
    SCB_InvalidateDCache_by_Addr((void *) MODEL_SLOT_ADDR, (int32_t) total);
    memcpy(buf, (const void *) MODEL_SLOT_ADDR, total);
    iotc_fs_unlock();

    if (NULL != iotv_validate(buf, total))
    {
        return -2; /* treat corrupt as absent */
    }
    if (out_len)
    {
        *out_len = total;
    }
    return 0;
}

int model_store_erase(void)
{
    (void) iotc_fs_init();
    iotc_fs_lock();
    int rc = 0;
    if (FSP_SUCCESS != g_ospi0.p_api->erase(g_ospi0.p_ctrl,
                                            (uint8_t *) MODEL_SLOT_ADDR, 4096))
    {
        rc = -5;
    }
    else
    {
        rc = prv_wait_idle();
    }
    iotc_fs_unlock();
    return rc;
}
