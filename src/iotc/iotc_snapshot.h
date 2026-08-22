/*
 * Copyright (c) 2026 Avnet, Inc.
 * SPDX-License-Identifier: MIT
 *
 * "Take Snapshot" command backend: grab the center 480x480 crop of the live
 * 640x480 camera frame (the region the model sees), draw the current
 * detection boxes, encode a grayscale PNG and upload it to IOTCONNECT
 * Telemetry Files. Blocking (2-3 TLS handshakes + ~230 KB PUT); call from
 * the net thread, never from the MQTT callback task.
 */
#ifndef IOTC_SNAPSHOT_H
#define IOTC_SNAPSHOT_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Returns 0 on success or a negative errno. msg (if non-NULL) receives a
 * short human-readable result for the command ack, e.g.
 * "snap-000042.png uploaded (231 KB, 1 face)".
 */
int iotc_snapshot_capture_upload(char *msg, size_t msg_size);

#ifdef __cplusplus
}
#endif

#endif /* IOTC_SNAPSHOT_H */
