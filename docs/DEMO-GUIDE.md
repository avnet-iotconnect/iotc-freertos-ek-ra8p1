# Demo Guide

The presenter's script for the /IOTCONNECT EK-RA8P1 Vision AI demo. Total runtime is about
10 minutes; every step below has been verified on hardware.

**Demo summary**: a Cortex-M85 microcontroller with an Ethos-U55 NPU runs camera vision
inference, streams results to /IOTCONNECT, and is re-tasked from the cloud in seconds. The
same device performs face detection, occupancy sensing, or 1000-class image classification
depending on which model is pushed — with no reflash and no reboot. The demo can be run
with the LCD attached (local video with overlays) or fully headless, using the dashboard
telemetry and cloud-triggered snapshots as the only interface.

## Contents

- [Before the demo](#before-the-demo)
- [Part 1 — Live vision inference](#part-1--live-vision-inference-2-min)
- [Part 2 — Snapshot to the cloud](#part-2--snapshot-to-the-cloud-2-min)
- [Part 3 — Cloud model deployment and hot-swap](#part-3--cloud-model-deployment-and-hot-swap-4-min)
- [Part 4 — Persistence across power cycles](#part-4--persistence-across-power-cycles-1-min)
- [Command reference](#command-reference)
- [Troubleshooting during a demo](#troubleshooting-during-a-demo)

## Before the demo

- [ ] Board powered, Ethernet plugged, camera aimed at the demo area (LCD attached if
      using the on-screen portion; the demo also runs fully headless via the dashboard)
- [ ] Serial console open (J-Link CDC COM port, 230400) — optional but adds credibility
- [ ] /IOTCONNECT dashboard open on the device: live telemetry view + Telemetry Files panel
- [ ] All five models registered under AI Models (see table below)
- [ ] Console shows `IOTC: connected` and `FU: selftest creds fetch -> 0 (OK)`
- [ ] A couple of recognizable props nearby: coffee mug, banana, water bottle, keyboard

Registered models (uploads from `tools/models/`):

| Zip | Suggested Name | Code |
|---|---|---|
| `face-v3_v3.zip` | RA8P1 Face Detect | `ra8p1face` |
| `person-detect_v1.zip` | RA8P1 Person Detect | `ra8p1prsn` |
| `mobilenet-025_v1.zip` | RA8P1 ImageNet Classifier 0.25 | `ra8p1mn025` |
| `mobilenet-050_v1.zip` | RA8P1 ImageNet Classifier 0.5 | `ra8p1mn050` |
| `mobilenet-v2_v1.zip` | RA8P1 ImageNet Classifier v2 | `ra8p1mnv2` |

## Part 1 — Live vision inference (2 min)

Start on the face detector (push `face-v3` beforehand, or `model-revert` to the built-in).

- Step in front of the camera: green boxes track the face on the LCD, and the info panel
  shows the live figures. (Headless: use the serial console detections and the dashboard
  telemetry instead.)
- **Key point — the performance figures**: camera 55 fps; NPU inference **5,800 µs**
  (~172 fps capability). The Ethos-U55 is roughly 30% utilized and the camera is the
  bottleneck, leaving headroom for substantially larger models — demonstrated in Part 3.
- On the dashboard: `vision.face_count`, `vision.score`, `vision.state` updating every 10 s,
  plus `perf.infer_us` / `perf.infer_fps` / `perf.cam_fps` and device vitals.

## Part 2 — Snapshot to the cloud (2 min)

- Send the **Take Snapshot** command from the device page.
- Within ~10 s a 480×480 **color PNG with the detection boxes drawn on it** appears in
  Telemetry Files, tagged with the detection results and performance metrics at capture
  time.
- **Key point**: the image is annotated, PNG-encoded, and uploaded directly to S3 with an
  AWS SigV4 signature computed on the microcontroller — no gateway or intermediary. In a
  headless deployment this is the viewfinder.

## Part 3 — Cloud model deployment and hot-swap (4 min)

1. **AI Models → push `mobilenet-v2`** (3.1 MB) to the device. The console shows the
   sequence: download over TLS, validation, hot-swap. Within seconds the LCD panel changes
   to `Model: mobilenet-v2` and begins labeling.
   - Hold up a recognizable object: `coffee mug`, `banana`, `water bottle` — the panel
     shows the top class and confidence; telemetry `vision.state` carries the label.
   - **Key point**: the device switched from face detection to 1000-class image
     classification in one push, with uptime uninterrupted. Inference is now ~40 ms — a
     roughly 7× larger workload absorbed without disturbing the video pipeline.
2. **Push `person-detect`**. Walk in and out of frame: `person` / `no person`, inference
   **1,500 µs**. Occupancy sensing — the same device, a third task.
3. **Push `face-v3`** to return to the starting model. Detection boxes return.
   - **Key point — performance tiers**: 1.5 ms → 5.8 ms → 40 ms across the library; the
     `perf.infer_us` telemetry graphs the swap history.

## Part 4 — Persistence across power cycles (1 min)

- Remove power, then repower. The device boots directly into **the last pushed model**
  (from its OSPI flash store), reconnects, and resumes telemetry.
- **Key point**: pushed models are persisted, and `model-revert` returns to the factory
  model — model rollout with rollback semantics, on a microcontroller.

## Command reference

| Command | Effect |
|---|---|
| `snapshot` | annotated color PNG → Telemetry Files (~10 s) |
| `set-interval <seconds>` | telemetry period (default 10 s) |
| `model-info` | ack with active model name/version/source/size |
| `model-revert` | back to the built-in face detector, clears the stored model |

## Troubleshooting during a demo

| Symptom | Recovery |
|---|---|
| Push seems ignored | Check the deployment actually dispatched (platform side); the device logs `MQTT: C2D message` the moment one arrives. Re-push — downloads retry 3× on transient network errors |
| Snapshot ack "upload failed" | Re-send once (transient); confirm boot log said `FU: file upload ready` |
| Odd classifier labels | Expected on cluttered scenes — ImageNet knows 1,000 *specific* objects; use a single centered prop |
| Classifier confidence looks low (30–60%) | Normal for softmax over 1,000 classes; the label being *right* is the demo |
| Telemetry nulls | Template attribute types (numerics must be DECIMAL) — re-import the bundled template |
| Board unresponsive | Power cycle: it reboots into the stored model and reconnects in ~30 s |
