/*
 * Copyright (c) 2026 Avnet, Inc.
 * SPDX-License-Identifier: MIT
 *
 * IOTV model envelope + persistence for cloud-pushed vision models
 * (ported from the Avnet Zephyr vision-occupancy demo, storage moved to
 * LittleFS on the EK-RA8P1's 64 MB OSPI flash).
 *
 * A model travels as an "IOTV" blob: 32-byte header (magic, format ver,
 * model ver, payload length, CRC32, display name) followed by the raw
 * Vela-compiled .tflite flatbuffer. tools/pack_model.py builds one.
 */
#ifndef MODEL_STORE_H
#define MODEL_STORE_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define IOTV_MAGIC   "IOTV"
#define IOTV_FMT_VER 1
#define IOTV_HDR_LEN 32

/* Ceiling for one enveloped model (header + .tflite). Must match the
 * SDRAM staging buffer and tools/pack_model.py. */
#define MODEL_MAX_BLOB (1024 * 1024)

#if defined(__GNUC__) || defined(__clang__)
#define IOTV_PACKED __attribute__((packed))
#else
#define IOTV_PACKED
#endif

struct IOTV_PACKED iotv_hdr {
    char magic[4];      /* "IOTV" */
    uint16_t fmt_ver;   /* IOTV_FMT_VER */
    uint16_t model_ver; /* monotonic, chosen at pack time */
    uint32_t model_len; /* payload (.tflite) length in bytes */
    uint32_t crc;       /* IEEE CRC32 over the payload */
    char name[16];      /* NUL-padded display name */
};

/* NULL when buf holds a well-formed IOTV blob, else a short reason. */
const char *iotv_validate(const uint8_t *buf, size_t len);

/* Wrap a raw .tflite (already at buf + IOTV_HDR_LEN, payload_len bytes)
 * with an IOTV header in place. */
void iotv_wrap_in_place(uint8_t *buf, size_t payload_len, uint16_t model_ver,
                        const char *name);

/* IEEE CRC32 (reflected, poly 0xEDB88320), as used by zlib/pack_model.py. */
uint32_t iotv_crc32(const uint8_t *data, size_t len);

/* Persist a validated IOTV blob. Returns 0 or negative error. */
int model_store_save(const uint8_t *iotv_blob, size_t len);

/* Load the persisted IOTV blob into buf (buf_size >= blob size).
 * Returns 0 and sets *out_len when a valid blob was restored; -2 (ENOENT)
 * when the store is empty/invalid; other negatives on flash errors. */
int model_store_load(uint8_t *buf, size_t buf_size, size_t *out_len);

/* Invalidate the store. Returns 0 or negative error. */
int model_store_erase(void);

#ifdef __cplusplus
}
#endif

#endif /* MODEL_STORE_H */
