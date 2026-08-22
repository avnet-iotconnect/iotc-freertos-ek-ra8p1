/*
 * Copyright (c) 2026 Avnet, Inc.
 * SPDX-License-Identifier: MIT
 *
 * Minimal PNG encoder (stored deflate blocks, no compression) for 8-bit
 * grayscale (channels = 1) or RGB truecolor (channels = 3) images. `pixels`
 * is row-major, w*channels bytes per row. Returns the encoded length or a
 * negative error. Worst-case output size is roughly
 * (w*channels+1)*h + (w*channels+1)*h/65535*5 + 80 bytes.
 */
#ifndef PNG_GRAY_H
#define PNG_GRAY_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

int png_encode(const uint8_t *pixels, uint16_t w, uint16_t h,
               uint8_t channels, uint8_t *out, size_t out_size);

#ifdef __cplusplus
}
#endif

#endif /* PNG_GRAY_H */
