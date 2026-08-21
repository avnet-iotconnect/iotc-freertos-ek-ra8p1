# IOTCONNECT EK-RA8P1 Vision AI Demo — Project Plan

## Context

Build a new demo: on-device object detection on the Renesas EK-RA8P1 (1 GHz Cortex-M85 + Ethos-U55 NPU), connected to Avnet /IOTCONNECT over Gigabit Ethernet, on **Renesas FSP** (not Zephyr). The kit's 5 MP OV5640 MIPI camera feeds a TFLM model on the NPU; detections stream as telemetry; a cloud snapshot command uploads an annotated color JPEG to /IOTCONNECT Telemetry Files. Headline feature: **live model hot-swap via /IOTCONNECT AI Model Management** — push a Vela-compiled `.tflite` from the cloud, device stores it in 64 MB OSPI flash and swaps it in seconds, no reflash/reboot. Display (bundled 7" 1024×600 parallel LCD) shows video + overlays when connected but is optional. Kinesis Video Streams is a later phase (out of scope here).

Research established:
- **No FSP+Ethernet IoTConnect client exists anywhere** — the transport layer is new work.
- **No Renesas example loads a model at runtime** — every example compiles the model in as a C array. Runtime load of a Vela-compiled flatbuffer is architecturally sound (TFLM + Ethos-U driver take pointer+length) but we are pioneering it.
- **RA8P1 has no hardware JPEG codec** — snapshot encode is software on the M85.

## Decisions (confirmed with user)

| Decision | Choice |
|---|---|
| TCP/IP stack | **FreeRTOS+TCP** (FSP-native, matches EK-RA8P1 `ethernet` example, pairs with coreMQTT/coreHTTP/mbedTLS) |
| Snapshot format | **Color JPEG** (software encode, annotated with boxes, ~640×480) |
| Model strategy | **Face detect first** (Renesas' proven YOLO Fastest 192×192, Vela `ethos-u55-256`), then COCO-class object detector as headline + hot-swap showcase |
| IoTConnect backend | **AWS** (awsdiscovery.iotconnect.io; S3 SigV4 Telemetry Files) |
| Model compile path | **Vela + TFLM binary flatbuffer** (NOT RUHMI/MERA — its C-codegen forecloses hot-swap) |
| Ethernet driver | **`r_rmac` + `r_rmac_phy`** (GPY111 PHY, RGMII, gigabit) — *not* `r_ether` |
| Camera | **`r_mipi_csi` + `r_vin`** (OV5640 on J35 via I2C + GPT 24 MHz cam clock), frame buffers in SDRAM |
| NPU | **`rm_ethosu`** + FSP-bundled TFLM core + Ethos-U core driver (C++17) |
| Model storage | **64 MB OSPI flash** (`r_ospi_b`) with a slotted model store; execute from SDRAM staging |
| Toolchain | **FSP 6.5.x + e² studio 2026-04.x + LLVM Embedded Toolchain** (matches all current EK-RA8P1 examples) |

## Tooling / source assets

Starting points (merge, don't write from scratch):
- **`ra-fsp-examples/application_projects/r11an0995/.../image_classification_ethosu_mobilenet_v1`** — complete camera→NPU→display FreeRTOS pipeline for this exact kit (`camera_thread_entry.c`, `ai_inference_thread_entry.c`, `display_thread_entry.c`, `camera_layer/ov5640.c`, `camera_layer/camera_control.c`). **Primary skeleton.**
- `ra-fsp-examples/example_projects/ek_ra8p1/ethos_u55_face_detection/ethos_u55_face_detection_ek_ra8p1_ep` — Vela+TFLM YOLO Fastest face detect, post-processing code, arena sizing.
- `ra-fsp-examples/example_projects/ek_ra8p1/mipi_csi/mipi_csi_ek_ra8p1_ep` — camera timing/CSC/cache config reference (write-through D-cache for SDRAM frame buffers).
- `ra-fsp-examples/example_projects/ek_ra8p1/ethernet/ethernet_ek_ra8p1_ep` + `Getting_started_with_FreeRTOS_Network` — RMAC + FreeRTOS+TCP + DHCP/DNS.
- `ra-fsp-examples/example_projects/ek_ra8p1/littlefs_ospi_b`, `ospi_b` — OSPI flash access.
- **`avnet-iotconnect/iotc-c-lib` (master, protocol 2.1)** — git submodule; build `core/`, `lib/cJSON`, **and `modules/device-rest-api/`** (discovery/identity — the RA6W1 project omitted it; we won't).
- **`avnet-iotconnect/iotc-zephyr-sdk/lib/zephyr-layer/`** — the five-file transport seam to re-implement on FSP: `iotc_mqtt_client.c`, `iotc_dra_client.c`, `iotc_tls_credentials.c`, `iotc_time.c`, `iotc_file_upload.c` (+ orchestrator `lib/iotconnect.c` and public headers `include/iotconnect*.h` — API kept identical so demo app code ports over).
- **`avnet-iotconnect/iotc-zephyr-demos/demos/vision-occupancy`** — app-level blueprint: `src/model_store.c` (IOTV envelope: magic/ver/len/CRC32/name + flatbuffer), `on_model_push()` C2D handler, snapshot worker + `iotc_fu_upload()` call shape, `tools/pack_model.py`, device template + dashboard JSON.
- Local `c:\dev\renesas\ra6w1b\iotc-freertos-ek-ra6w1/src/iotconnect.c` — telemetry-build + `mqtt_send_cb` pattern (portable); its FSP config/`*_w` modules are RA6W1-only and will NOT be reused. Do not copy its checked-in-credentials pattern.
- JPEG encoder: small portable encoder (e.g. **JPEGENC (bitbank2)** or `toojpeg`) — RGB565→JPEG with boxes pre-drawn into the frame copy.

## New repo

`c:\dev\renesas\ra8p1\iotc-freertos-ek-ra8p1` (name mirrors `iotc-freertos-ek-ra6w1`; final GitHub name can change at publish time). e² studio project at repo root (like the RA6W1 repo), `iotc-c-lib` as submodule.

Planned layout:
```
configuration.xml, ra_gen/, ra_cfg/, ra/        # FSP smart-configurator output
iotc-c-lib/                                     # submodule (master, protocol 2.1)
src/
  iotc/               # FSP transport seam (port of zephyr-layer)
    iotconnect.c/.h        # orchestrator: DRA → creds → MQTT connect → pump
    iotc_mqtt_client.c     # coreMQTT + mbedTLS transport (mutual TLS)
    iotc_dra_client.c      # HTTPS GET (discovery/identity/model download) over coreHTTP or raw sockets+mbedTLS
    iotc_tls_credentials.c # cert/key provisioning (LittleFS or data flash; runtime-provisioned via CLI, never in git)
    iotc_time.c            # SNTP (coreSNTP or FreeRTOS+TCP helper)
    iotc_file_upload.c     # Telemetry Files: mTLS creds fetch → SigV4 S3 PUT → fu-topic announce
  app/
    main_app_thread.c      # state machine, telemetry loop, command dispatch
    commands.c             # snapshot, set-threshold, set-interval, model-info, revert, reboot…
    telemetry.c            # vision.* / model.* / sys.* fields (mirror vision-occupancy template)
  vision/
    camera_thread.c        # r_vin/r_mipi_csi capture → SDRAM ring
    inference_thread.c     # TFLM + rm_ethosu; detection post-proc (YOLO decode/NMS)
    annotate.c             # draw boxes/labels into snapshot frame copy
    jpeg_encode.c          # software JPEG (vendored encoder)
  model_store/
    model_store.c          # IOTV envelope, OSPI slot management, CRC, SDRAM staging, hot-swap lock
    model_builtin.c        # compiled-in fallback model (face detect)
  display/
    display_thread.c       # optional GLCDC path; runs only if panel detected/configured
tools/
  pack_model.py            # wrap Vela-compiled .tflite in IOTV (+zip) for AI Model upload
  vela/                    # model compile scripts (ethos-u55-256)
templates/                 # IoTConnect device template JSON
dashboard/                 # dashboard export JSON
docs/ (README, QUICKSTART, DEMO)
```

## Implementation phases

Each phase ends demonstrably working on hardware.

**Phase 0 — Workspace & skeleton.** Clone `ra-fsp-examples` (sparse: ek_ra8p1 + r11an0995). Verify installed e² studio/FSP versions (need FSP ≥6.5.0 with RA8P1). Create repo + git init, add iotc-c-lib submodule. Import & build `image_classification_ethosu_mobilenet_v1` unmodified; run on board — proves toolchain, camera, NPU, display.

**Phase 1 — Base project: camera + NPU + Ethernet coexisting.** New FSP project (or fork of r11an0995 project) adding `r_rmac`/`r_rmac_phy` + FreeRTOS+TCP (DHCP, DNS, SNTP) alongside camera/NPU threads. Deliverable: pings + live inference simultaneously, detections on serial console. Watch: pin conflicts (camera vs RGMII vs OSPI vs LCD), SDRAM cache policy, heap layout.

**Phase 2 — Face detection pipeline.** Swap MobileNet classification for YOLO Fastest 192×192 face detect (from `ethos_u55_face_detection_ep`): capture → resize/CSC to model input → NPU infer → decode boxes + NMS. Console prints boxes + inference ms + fps. Display overlay if panel attached.

**Phase 3 — IoTConnect connectivity.** Port the transport seam: DRA discovery/identity (`iotcl_dra_*` over our HTTPS client), mutual-TLS coreMQTT connect, SNTP time, telemetry publish loop (`vision.*`, `model.*`, `sys.*` fields per vision-occupancy template), C2D command dispatch via `iotcl_c2d_process_event` + acks. Create device template + test device in IoTConnect. Deliverable: live detections on dashboard.

**Phase 4 — Snapshot to Telemetry Files.** Annotate frame copy (boxes/labels) → software JPEG encode (~640×480) → port `iotc_file_upload.c`: mTLS GET to `fs.url` credentials provider (`x-amzn-iot-thingname` header) → SigV4-signed S3 PUT → announce on `fu` topic with `cf` classification JSON. mbedTLS provides SHA-256/HMAC primitives. Deliverable: "Take Snapshot" command on dashboard renders annotated JPEG in Telemetry Files panel.

**Phase 5 — Model store + hot-swap (headline).** `model_store.c`: IOTV-enveloped slots in OSPI flash (LittleFS or raw slotted region), CRC32-validated, staged into SDRAM (16-byte aligned) for execution. Inference thread holds a model lock; swap = pause → repoint TFLM interpreter at new flatbuffer → resume (worst-case tensor arena; superset op resolver so pushed models only use linked-in ops — document the op list). C2D `ct:2` module command handler (`iotcl_c2d_get_ota_url_*`) → HTTPS GET of signed URL streamed to flash → validate → swap → ack. Commands: `model-info`, `model-revert` (builtin fallback). Register models in IoTConnect **AI Models**, push from cloud. Deliverable: push model, swap in seconds, no reboot; `model.ver/src` telemetry updates.

**Phase 6 — Object detection model + demo polish.** Vela-compile a COCO-class object detector (YOLO Fastest COCO or SSD MobileNet person/object variant) with `tools/vela/`; validate accuracy/latency; make it the demo's pushed model (face detect = builtin fallback, and the A/B pair proves hot-swap). Dashboard build + export, device template finalized, README/QUICKSTART/DEMO docs, credential provisioning flow documented (certs via CLI/LittleFS, never committed).

**Phase 7 (later, out of scope) — Kinesis Video Streams** (protocol already reserves `ct:112/113` KVS start/stop).

## Key risks / verify-early items

1. **NPU reading hot-loaded models** — validate in Phase 5 *step one*: hand TFLM a flatbuffer copied to SDRAM at runtime (before any cloud plumbing). Cache maintenance (clean/invalidate) around NPU-visible buffers.
2. **Pin/peripheral conflicts** (RGMII + MIPI CSI + OSPI + GLCDC + SDRAM concurrently) — resolve in FSP pin configurator in Phase 1; the quickstart proves camera+LCD+OSPI coexist, Ethernet is the addition.
3. **SigV4 upload** is the heaviest new code — Phase 4 is timeboxed with the Zephyr `iotc_file_upload.c` as line-by-line reference.
4. **Display board interface ambiguity** (marketing says both MIPI and parallel) — treat display as optional; follow whatever the r11an0995/quickstart display path uses for the bundled panel.
5. **Memory budget** is comfortable (2 MB SRAM + 64 MB SDRAM + 64 MB OSPI) but arena + frame buffers + TLS + JPEG must be laid out deliberately in Phase 1.

## Verification

- **Per phase:** build in e² studio (headless build via `eclipsec` where scriptable), flash via J-Link, verify on serial console (115200).
- **End-to-end demo script:** boot → DHCP → discovery/identity → MQTT connected → dashboard shows live `vision.*` telemetry; send Take Snapshot → annotated JPEG appears in Telemetry Files; push object-detection model from AI Models → `model.ver` bumps within seconds, detections change class set, uptime proves no reboot; power-cycle → device boots the flash-stored model; `model-revert` → builtin face detect.
- **Regression:** telemetry cadence maintained during model download (MQTT stays up on separate TLS session).
