/*
 * Copyright (c) 2026 Avnet, Inc.
 * SPDX-License-Identifier: MIT
 *
 * Minimal grayscale PNG encoder (stored deflate blocks). Returns the
 * encoded length or a negative error. Worst-case output size is roughly
 * (w+1)*h + (w+1)*h/65535*5 + 64 bytes.
 */
#ifndef PNG_GRAY_H
#define PNG_GRAY_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

int png_gray_encode(const uint8_t *gray, uint16_t w, uint16_t h,
                    uint8_t *out, size_t out_size);

#ifdef __cplusplus
}
#endif

#endif /* PNG_GRAY_H */
