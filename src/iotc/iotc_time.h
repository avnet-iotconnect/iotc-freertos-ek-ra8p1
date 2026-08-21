/*
 * Copyright (c) 2020-2026 Avnet, Inc.
 * SPDX-License-Identifier: MIT
 *
 * Wall-clock seam, FSP/FreeRTOS port. Obtains UTC via SNTP (coreSNTP or a
 * minimal SNTP exchange over FreeRTOS+TCP) and keeps an epoch offset against
 * the FreeRTOS tick, which mbedTLS reads via the platform time hook during
 * certificate validity checks. Must run after the network is up and before
 * the first TLS handshake.
 */
#ifndef IOTC_TIME_H
#define IOTC_TIME_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * One-shot SNTP sync: query the server and latch the UTC offset. Returns 0
 * on success or a negative errno.
 */
int iotc_time_sync(const char *sntp_server, uint32_t timeout_ms);

/** True once iotc_time_sync() has succeeded at least once this boot. */
bool iotc_time_is_synced(void);

/** Current UTC epoch seconds (0 if never synced). */
int64_t iotc_time_now(void);

#ifdef __cplusplus
}
#endif

#endif /* IOTC_TIME_H */
