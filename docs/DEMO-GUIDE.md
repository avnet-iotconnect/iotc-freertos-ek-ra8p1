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

- [The dashboard](#the-dashboard)
- [Before the demo](#before-the-demo)
- [Part 1 — Live vision inference](#part-1--live-vision-inference-2-min)
- [Part 2 — Snapshot to the cloud](#part-2--snapshot-to-the-cloud-2-min)
- [Part 3 — Cloud model deployment and hot-swap](#part-3--cloud-model-deployment-and-hot-swap-4-min)
- [Part 4 — Persistence across power cycles](#part-4--persistence-across-power-cycles-1-min)
- [Part 5 — Live video streaming](#part-5--live-video-streaming-2-min)
- [Command reference](#command-reference)
- [Troubleshooting during a demo](#troubleshooting-during-a-demo)

## The dashboard

![Live dashboard](images/dashboard-live.png)

*The demo dashboard, live: annotated snapshot with classification metadata, detection-state
card, face/confidence gauges, tiles, the model-swap inference-time chart, model-source card,
and the command panel with acknowledgment history.*

One-time setup:

1. Upload the artwork from [`dashboard/images/`](../dashboard/images/) to the public S3
   bucket under `images/renesas/ek-ra8p1/` (keys are case-sensitive): `banner_vision_ai.png`
   plus the eight `state_*.png` / `model_*.png` cards.
2. **Create Dashboard → Import Dashboard**, select template **RA8P1 Vision AI** and your
   device, and choose [`dashboard/ra8p1-vision-ai-dashboard.json`](../dashboard/ra8p1-vision-ai-dashboard.json).

The detection-state card switches artwork for face / clear / person / no-person; any other
value (an ImageNet class label) falls through to the CLASSIFYING card, with the label itself
shown in the Detection/Class tile. The model-source card tracks builtin / flash / cloud.

## Before the demo

- [ ] Board powered, Ethernet plugged, camera aimed at the demo area (LCD attached if
      using the on-screen portion; the demo also runs fully headless via the dashboard)
- [ ] Serial console open (J-Link CDC COM port, 230400) — optional but adds credibility
- [ ] The demo dashboard imported and open (see [The dashboard](#the-dashboard))
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

Start on the face detector (push `face-v3` beforehand).

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
- **Key point**: pushed models are persisted and reload at boot — model rollout that
  survives power loss, on a microcontroller.

## Part 5 — Live video streaming (2 min)

- Open the device's **Video Streaming** tab and click **Start Video**. Within a few
  seconds the camera's live view appears in the browser.
- Wave at the camera; point out the latency (a second or two through the TURN relay is
  normal).
- **Key points**:
  - There is no video hardware on this chip: H.264 is encoded in software on the
    Cortex-M85 (QVGA, ~8–10 fps) while the NPU keeps running inference on the same
    frames — telemetry continues uninterrupted during streaming.
  - Transport is production WebRTC: AWS Kinesis Video Streams signaling, ICE/TURN
    traversal, DTLS-SRTP encryption — negotiated on the microcontroller.
- Close the tab (or Stop Video) and show telemetry `video.state` returning from `live`.

## Command reference

| Command | Effect |
|---|---|
| `snapshot` | annotated color PNG → Telemetry Files (~10 s) |
| `set-interval <seconds>` | telemetry period (default 10 s) |
| `model-info` | ack with active model name/version/source/size |
| `model-revert` | clears the stored model (inference idles until the next push; this build has no built-in fallback) |
| `set-brightness <0\|1>` | camera exposure target: 0 = normal, 1 = bright (~+1 EV, for dark rooms) |

## Troubleshooting during a demo

| Symptom | Recovery |
|---|---|
| Push seems ignored | Check the deployment actually dispatched (platform side); the device logs `MQTT: C2D message` the moment one arrives. Re-push — downloads retry 3× on transient network errors |
| Snapshot ack "upload failed" | Re-send once (transient); confirm boot log said `FU: file upload ready` |
| Odd classifier labels | Expected on cluttered scenes — ImageNet knows 1,000 *specific* objects; use a single centered prop |
| Classifier confidence looks low (30–60%) | Normal for softmax over 1,000 classes; the label being *right* is the demo |
| Telemetry nulls | Template attribute types (numerics must be DECIMAL) — re-import the bundled template |
| LCD all white (typically right after a reflash) | Power-cycle the board — the panel needs a cold start. The cloud side is unaffected, so telemetry and video keep working even while the screen is white |
| Board unresponsive | Power cycle: it reboots into the stored model and reconnects in ~30 s |
| Video tab shows no stream | Give it ~15 s (TURN allocation); retry once by Stop/Start. The device must have been created from the bundled template (video streaming is fixed at device creation) |
