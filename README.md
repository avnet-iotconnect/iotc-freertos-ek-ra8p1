# /IOTCONNECT EK-RA8P1 Vision AI Demo

On-device object detection on the Renesas EK-RA8P1 (1 GHz Arm Cortex-M85 + Ethos-U55 NPU),
connected to Avnet [/IOTCONNECT](https://www.iotconnect.io/) over Gigabit Ethernet, built on
Renesas FSP with FreeRTOS.

- The kit's 5 MP OV5640 MIPI CSI-2 camera feeds a TensorFlow Lite Micro model running on the
  Ethos-U55 NPU; detections stream to /IOTCONNECT as telemetry.
- A cloud **Take Snapshot** command uploads an annotated color JPEG to /IOTCONNECT
  **Telemetry Files** — no display needed, the dashboard is the viewfinder.
- **Live model deployment via /IOTCONNECT AI Model Management**: push a new Vela-compiled
  model from the cloud and the device hot-swaps it in seconds — no reflash, no reboot.
  Models are stored in the board's 64 MB OSPI flash.
- The bundled 7" LCD (optional) shows live video with detection overlays when connected.

## Status

Early development. See [docs/PLAN.md](docs/PLAN.md) for the phased implementation plan.

## Requirements

| Item | Version |
|---|---|
| Board | Renesas EK-RA8P1 (RTK7EKA8P1S01001BE) with bundled OV5640 camera board |
| e² studio | 2025-10 or later |
| FSP | 6.3.1 or later (RA8P1 support requires ≥ 6.0.0) |
| Toolchain | LLVM Embedded Toolchain for Arm (ATfE 21.x) |
| Cloud | /IOTCONNECT account on the AWS backend |

## Repository layout

```
configuration.xml, ra_gen/, ra_cfg/, ra/   FSP Smart Configurator project
iotc-c-lib/                                IoTConnect protocol library (submodule)
src/iotc/                                  IoTConnect FSP transport layer (MQTT/TLS, HTTPS, SNTP, file upload)
src/app/                                   Application: telemetry, commands, state machine
src/vision/                                Camera capture, NPU inference, annotation, JPEG encode
src/model_store/                           OSPI model slots, IOTV envelope, hot-swap
src/display/                               Optional GLCDC display path
tools/                                     Model packing (pack_model.py) and Vela compile scripts
templates/                                 /IOTCONNECT device template
dashboard/                                 /IOTCONNECT dashboard export
docs/                                      Plan, quickstart, demo script
```

## Credentials

Device certificate and private key are **never committed**. They are provisioned onto the
device at setup time (see docs/QUICKSTART.md once available).
