/*
 * Copyright (c) 2026 Avnet, Inc.
 * SPDX-License-Identifier: MIT
 *
 * KVS WebRTC live video streaming for /IOTCONNECT (EK-RA8P1).
 *
 * The platform provisions a Kinesis Video Streams signaling channel per
 * device (identity d.p.vs block). This module parses that block, connects
 * the device as the WebRTC master, and streams H.264 video to viewers
 * opened from the dashboard's Video Streaming tab. Streaming to a peer
 * starts when the viewer connects through the signaling channel; the
 * dashboard's Start/Stop Video buttons (ct:112/113) only need acks.
 *
 * All functions are safe to call from the net thread.
 */
#ifndef KVS_WEBRTC_TASK_H
#define KVS_WEBRTC_TASK_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Parse the raw /IOTCONNECT identity response for the d.p.vs video-streaming
 * block (url = IoT credentials-provider role-alias URL, carn = KVS signaling
 * channel ARN, as = autostart). No-op if the block is absent. Requires the
 * iotc-c-lib MQTT config to be set (client_id doubles as the thing name).
 */
void iotc_kvs_identity_hook(const char *identity_json);

/**
 * Provide the device X.509 credentials (PEM, NUL-terminated) used for the
 * IoT role-alias credentials fetch. Copies the strings.
 */
void iotc_kvs_set_credentials(const char *cert_pem, const char *key_pem);

/** True once the vs block has been parsed successfully. */
bool iotc_kvs_config_ready(void);

/**
 * Create the KVS WebRTC task (idempotent). The task waits until config and
 * credentials are present, then brings up the media pipeline and signaling.
 */
void iotc_kvs_start_task(void);

/** Short state string for telemetry/console ("off"/"wait"/"ready"/"live"). */
const char *iotc_kvs_state(void);

#ifdef __cplusplus
}
#endif

#endif /* KVS_WEBRTC_TASK_H */
