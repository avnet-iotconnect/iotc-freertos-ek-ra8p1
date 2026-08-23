# /IOTCONNECT EK-RA8P1 Vision AI Demo

On-device vision AI on the Renesas **EK-RA8P1** (1 GHz Arm Cortex-M85 + Ethos-U55 NPU),
connected to Avnet [/IOTCONNECT](https://www.iotconnect.io/) over Gigabit Ethernet, built on
Renesas FSP with FreeRTOS — **no Linux, no MPU, everything on the microcontroller**.

The kit's OV5640 MIPI CSI-2 camera feeds TensorFlow Lite Micro models running on the
Ethos-U55 NPU. Results stream to /IOTCONNECT as telemetry, annotated snapshots upload to
Telemetry Files on demand, and **models are deployed live from the cloud**: push a
Vela-compiled model from /IOTCONNECT AI Model Management and the device hot-swaps it
between two inferences, with no reflash and no reboot. Pushed models persist in the 64 MB
OSPI flash and survive power cycles.

The LCD is **optional**. A key use case is fully headless operation: no display attached,
with the /IOTCONNECT dashboard as the interface — live telemetry as the data feed and
cloud-triggered snapshot capture as the viewfinder.

All capabilities below are verified on hardware.

![Live /IOTCONNECT dashboard](docs/images/dashboard-live.png)

*The bundled /IOTCONNECT dashboard, live: annotated snapshot, detection state, gauges,
model-swap history, and device commands. Import JSON and artwork are in
[dashboard/](dashboard/).*

<img src="docs/images/ek-ra8p1-board.webp" alt="Renesas EK-RA8P1 board" width="360"/>

*Renesas EK-RA8P1 evaluation board. The kit also includes the OV5640 camera expansion
board and a 7-inch parallel LCD.*

## Contents

- [Capabilities](#capabilities)
- [The model library](#the-model-library)
- [Documentation](#documentation)
- [Architecture](#architecture)
- [Repository layout](#repository-layout)
- [Requirements](#requirements)
- [Credentials](#credentials)

## Capabilities

| Capability | Detail |
|---|---|
| Live inference | Camera at 55 fps, NPU inference 1.5–40 ms depending on model, LCD overlay at 29 fps |
| Cloud telemetry | Detections, model identity, performance metrics, device vitals every 10 s (configurable) |
| Snapshot to cloud | `snapshot` command uploads a 480×480 color PNG with detection boxes to Telemetry Files (SigV4 S3 upload signed on-device) |
| **Model hot-swap** | Push from AI Model Management → download → validate → swap in seconds, uptime uninterrupted |
| Model persistence | Active model stored in a raw OSPI slot, reloaded at boot; `model-revert` returns to the built-in model |
| Multi-task | The device re-tasks itself by model shape: face detection (boxes), person/occupancy detection, or 1000-class ImageNet classification |
| Commands | `snapshot`, `set-interval <s>`, `model-info`, `model-revert` |

## The model library

Five ready-to-push models ship in [tools/models/](tools/models/) (upload the `.zip` to
/IOTCONNECT → AI Models):

| Model | Task | Input | Inference | Size |
|---|---|---|---|---|
| `face-v3` | Face detection + boxes (YOLO Fastest) | 192×192 gray | ~5.8 ms (172 fps) | 441 KB |
| `person-detect` | Person present / absent (visual wake words) | 96×96 gray | ~1.5 ms (666 fps) | 240 KB |
| `mobilenet-025` | ImageNet classifier, speed tier | 224×224 RGB | ~4.5 ms (222 fps) | 432 KB |
| `mobilenet-050` | ImageNet classifier, mid tier | 224×224 RGB | ~9 ms (110 fps) | 1.1 MB |
| `mobilenet-v2` | ImageNet classifier, accuracy tier | 224×224 RGB | ~40 ms (25 fps) | 3.1 MB |

Any model matching the input contract can be added with the workflow in the
[Developer Guide](docs/DEVELOPER-GUIDE.md#adding-models): Vela-compile → `pack_model.py` →
upload → push.

## Documentation

| Document | For |
|---|---|
| [docs/QUICKSTART.md](docs/QUICKSTART.md) | Flash a prebuilt image, provision credentials over the serial terminal, and connect — no toolchain |
| [docs/WORKSHOP.md](docs/WORKSHOP.md) | Hands-on workshop: flash → provision → dashboard → snapshots → model hot-swap |
| [docs/DEVELOPER-GUIDE.md](docs/DEVELOPER-GUIDE.md) | Build from source, connect to /IOTCONNECT, architecture, adding models |
| [docs/DEMO-GUIDE.md](docs/DEMO-GUIDE.md) | The presenter's script: what to show, in what order, with expected results |
| [docs/BUILD-NOTES.md](docs/BUILD-NOTES.md) | Raw engineering log: toolchain quirks, hardware gotchas, debugging recipes |

## Architecture

```
                 EK-RA8P1 (R7KA8P1, Cortex-M85 @ 1 GHz)
  ┌────────────────────────────────────────────────────────────────┐
  │  OV5640 camera ── MIPI CSI-2 ── r_vin ──► SDRAM frame ring     │
  │                                             │                  │
  │        camera_thread: crop/convert ──► 224×224 RGB staging     │
  │                                             │                  │
  │   ai_inference_thread: resample ──► TFLM + Ethos-U55 (rm_ethosu)
  │        model flatbuffer in SDRAM staging ◄── hot-swap ◄─┐      │
  │                                             │           │      │
  │   display_thread: overlay boxes ──► GLCDC ──► 7" LCD    │      │
  │                                                         │      │
  │   net_thread: FreeRTOS+TCP ── r_rmac (RGMII GbE)        │      │
  │      ├─ SNTP ─ DRA discovery/identity (coreHTTP+mbedTLS)│      │
  │      ├─ coreMQTT mutual-TLS ──► /IOTCONNECT (AWS)       │      │
  │      ├─ telemetry / commands / OTA ct:2 ────────────────┘      │
  │      └─ snapshot: PNG encode ─ SigV4 S3 PUT ─ fu announce      │
  │                                                                │
  │   model_store: IOTV envelope ──► raw OSPI slot (+56 MB)        │
  │   PKCS#11 credentials ──► LittleFS on OSPI (+32 MB)            │
  └────────────────────────────────────────────────────────────────┘
```

## Repository layout

```
configuration.xml, ra_gen/, ra_cfg/, ra/   FSP Smart Configurator project (vendor code + in-repo patches)
iotc-c-lib/                                /IOTCONNECT protocol library, submodule (+ nested cJSON submodule)
firmware/                                  Prebuilt local-demo image for the Quickstart
src/iotc/                                  /IOTCONNECT transport + app layer (MQTT/TLS, DRA, SNTP, file upload, snapshot)
src/ai_application/                        TFLM glue, model lifecycle + hot-swap, YOLO post-processing, labels
src/model_store/                           IOTV envelope validation + raw OSPI model slot
src/camera_layer/, src/display_layer/      OV5640/VIN capture, GLCDC output, detection overlay
tools/pack_model.py                        Wrap a Vela .tflite as a pushable .iotv (+ STORED zip)
tools/models/                              The five ready-to-push model zips
templates/ra8p1-vision-ai-template.json    /IOTCONNECT device template (import this)
dashboard/                                 /IOTCONNECT dashboard import JSON + widget artwork
docs/                                      Quickstart, workshop, developer guide, demo guide, build notes
```

## Requirements

| Item | Version used |
|---|---|
| Board | Renesas EK-RA8P1 kit with bundled OV5640 camera (LCD optional) |
| e² studio | 2025-10 (25.10.0) |
| FSP | 6.3.1 packs |
| Toolchain | LLVM Embedded Toolchain for Arm (ATfE) 21.1.1 |
| Debug probe | On-board J-Link OB (SEGGER J-Link software V9.38+ — earlier versions lack RA8P1) |
| Cloud | /IOTCONNECT account on the **AWS** backend |
| Model tooling | Python 3.10+ with `ethos-u-vela` (only needed to add models) |

## Credentials

The /IOTCONNECT identity (environment, CPID, device ID, X.509 certificate and key) is
**provisioned at runtime** through a serial CLI and stored on the OSPI flash, where it
survives power cycles — the prebuilt image connects to your account without any source
build (see the [Quickstart](docs/QUICKSTART.md)). Networking is wired Ethernet with DHCP,
so there is no network configuration.

For development, an identity can alternatively be compiled in via the gitignored
`src/iotc/app_secrets.h` (see the
[Developer Guide](docs/DEVELOPER-GUIDE.md#3-iotconnect-setup)); stored runtime credentials
always take precedence. Credentials are never committed to the repository.
