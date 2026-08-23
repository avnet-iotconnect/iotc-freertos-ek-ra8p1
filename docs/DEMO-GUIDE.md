# Demo Guide

The presenter's script for the /IOTCONNECT EK-RA8P1 Vision AI demo. Total runtime ~10
minutes; every step below has been verified on hardware.

**The story**: a $60-class microcontroller — not a Linux box — runs camera vision on an NPU,
streams results to the cloud, and can be *re-tasked from the cloud in seconds*. The same
device does face detection, occupancy sensing, and 1000-class image recognition, depending
on which model you push. Nothing is reflashed. Nothing reboots.

## Before the demo

- [ ] Board powered, Ethernet plugged, LCD attached, camera aimed at the demo area
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

## Act 1 — Live vision on a microcontroller (2 min)

Start on the face detector (push `face-v3` beforehand, or `model-revert` to the built-in).

- Step in front of the camera: green boxes track your face on the LCD, and the info panel
  shows the live numbers.
- **Talking point — the performance panel**: camera 55 fps; NPU inference **5,800 µs**
  (~172 fps capability). The Ethos-U55 is ~30% busy; the camera is the bottleneck. There is
  headroom for far bigger models — which Act 3 will prove.
- On the dashboard: `vision.face_count`, `vision.score`, `vision.state` updating every 10 s,
  plus `perf.infer_us` / `perf.infer_fps` / `perf.cam_fps` and device vitals.

## Act 2 — The dashboard is the viewfinder (2 min)

- Send the **Take Snapshot** command from the device page.
- Within ~10 s a 480×480 **color PNG with the detection boxes drawn on it** appears in
  Telemetry Files, tagged with the detection results and performance metrics at capture
  time.
- **Talking point**: the image is annotated, PNG-encoded, and uploaded straight to S3 with
  an AWS SigV4 signature computed *on the microcontroller* — no gateway, no middleman.

## Act 3 — Re-task the device from the cloud (4 min) — the headline

1. **AI Models → push `mobilenet-v2`** (the 3.1 MB one) to the device. Narrate the console:
   download (~3 MB over TLS), validation, hot-swap. Within seconds the LCD panel flips to
   `Model: mobilenet-v2` and starts labeling.
   - Hold up a prop: `coffee mug`, `banana`, `water bottle`… the panel shows the top class
     and confidence; telemetry `vision.state` carries the label.
   - **Talking point**: the device went from *finding faces* to *recognizing a thousand
     object types* in one push. Uptime never reset. Inference is now ~40 ms — a 7× larger
     workload the NPU absorbed without breaking the video pipeline.
2. **Push `person-detect`**. Walk in and out of frame: `person` / `no person`, inference
   **1,500 µs**. Occupancy sensing — same device, third task.
3. **Push `face-v3`** to come home. Boxes return.
   - **Talking point — the tiers**: 1.5 ms → 5.8 ms → 40 ms across the library; the
     `perf.infer_us` telemetry graphs the swap history.

## Act 4 — It survives (1 min)

- Pull power. Repower. The device boots straight into **the last pushed model** (from its
  OSPI flash store), reconnects, resumes telemetry.
- **Talking point**: pushed models are persisted; `model-revert` returns to the factory
  model. Fleet-wide model rollout with rollback, on microcontrollers.

## Command reference

| Command | Effect |
|---|---|
| `snapshot` | annotated color PNG → Telemetry Files (~10 s) |
| `set-interval <seconds>` | telemetry period (default 10 s) |
| `model-info` | ack with active model name/version/source/size |
| `model-revert` | back to the built-in face detector, clears the stored model |

## If something goes sideways

| Symptom | Recovery |
|---|---|
| Push seems ignored | Check the deployment actually dispatched (platform side); the device logs `MQTT: C2D message` the moment one arrives. Re-push — downloads retry 3× on transient network errors |
| Snapshot ack "upload failed" | Re-send once (transient); confirm boot log said `FU: file upload ready` |
| Odd classifier labels | Expected on cluttered scenes — ImageNet knows 1,000 *specific* objects; use a single centered prop |
| Classifier confidence looks low (30–60%) | Normal for softmax over 1,000 classes; the label being *right* is the demo |
| Telemetry nulls | Template attribute types (numerics must be DECIMAL) — re-import the bundled template |
| Board unresponsive | Power cycle: it reboots into the stored model and reconnects in ~30 s |
