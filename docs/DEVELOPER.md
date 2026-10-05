# EK-RA8P1 Vision AI with /IOTCONNECT — Developer Guide

Everything needed to build this project from source, understand how it works, and extend it,
including adding your own pushable AI models. To run the demo with the prebuilt image instead,
follow the [Quickstart](../README.md).

## Contents

1. [Prerequisites](#1-prerequisites)
2. [Clone](#2-clone)
3. [Build](#3-build)
4. [Flash and Console](#4-flash-and-console)
5. [Provisioning the Device Identity](#5-provisioning-the-device-identity)
6. [Architecture](#6-architecture)
7. [Telemetry and Commands](#7-telemetry-and-commands)
8. [Memory Budget](#8-memory-budget)
9. [AI Models](#9-ai-models)
10. [Live Video (KVS WebRTC)](#10-live-video-kvs-webrtc)
11. [Networking](#11-networking)
12. [OSPI Flash Storage](#12-ospi-flash-storage)
13. [Vendor Patches and Required Configuration](#13-vendor-patches-and-required-configuration)
14. [Dashboard](#14-dashboard)
15. [Troubleshooting](#15-troubleshooting)
16. [Resources](#16-resources)

## 1. Prerequisites

| Tool | Version used | Notes |
|---|---|---|
| e² studio | 2025-10 (Windows), 2025-12 (Linux) | includes the FSP Smart Configurator |
| FSP packs | 6.3.1 | installed into the e² studio tree |
| LLVM Embedded Toolchain for Arm (ATfE) | 21.1.1 | bundled with the RA platform installer; GNU Arm toolchains are not used |
| SEGGER J-Link software | V9.38 | earlier versions do not support the RA8P1; V9.82 cannot reset the RA8P1 and fails to program it |
| Python | 3.10+ | for `tools/pack_model.py` and Vela |
| ethos-u-vela | 4.1.0 or later | `pip install ethos-u-vela` (only needed to add models) |
| Board | EK-RA8P1 kit | with the bundled OV5640 camera; the LCD is optional |
| Cloud | /IOTCONNECT account | AWS backend |

### Linux host setup (Ubuntu 24.04)

1. Download the RA platform installer `setup_fsp_v6_3_0_e2s_v2025-12.xz.run` from the
   [FSP v6.3.0 release](https://github.com/renesas/fsp/releases/tag/v6.3.0), make it executable
   (`chmod +x`), and run it. Choose Custom Install and untick SEGGER J-Link, so the installer
   does not replace J-Link V9.38. It installs to `~/renesas/ra/e2studio_v2025-12_fsp_v6.3.0` and
   bundles ATfE 21.1.1 under `toolchains/llvm_arm/`. The `libpython2.7` and `libncurses5`
   packages listed in Renesas' Linux quick start guide are not needed for building and are not
   available on Ubuntu 24.04.
2. Add the 6.3.1 packs: download `FSP_Packs_v6.3.1.zip` from the
   [FSP v6.3.1 release](https://github.com/renesas/fsp/releases/tag/v6.3.1) and unzip it into the
   install root, which it matches (`internal/projectgen/ra/packs/`):
   `unzip -n FSP_Packs_v6.3.1.zip -d ~/renesas/ra/e2studio_v2025-12_fsp_v6.3.0`

## 2. Clone

The /IOTCONNECT protocol library is a submodule with its own nested submodule (cJSON), so clone
recursively:

```
git clone --recurse-submodules https://github.com/avnet-iotconnect/iotc-freertos-ek-ra8p1.git
```

Already cloned? Run `git submodule update --init --recursive`.

On Windows, run `git config core.longpaths true` and keep the checkout at a short path (for
example `C:\dev\`). Parts of the vendor tree have deep paths, and the build's make cannot handle
paths over 260 characters ("No rule to make target").

## 3. Build

### e² studio (IDE)

Import the project (File → Import → Existing Projects), let the Smart Configurator generate
`ra_gen/` and `ra_cfg/`, and build the Debug configuration. The first build takes a few minutes.

### Headless (command line)

Windows (PowerShell):

```
$env:PATH = 'C:\Renesas\e2_studio\toolchains\llvm_arm\ATfE-21.1.1-Windows-x86_64\bin;' + $env:PATH
e2studioc.exe -nosplash --launcher.suppressErrors `
  -application org.eclipse.cdt.managedbuilder.core.headlessbuild `
  -data <workspace-dir> -import <project-dir> -build "iotc_freertos_ek_ra8p1/Debug"
```

Linux:

```
E2=~/renesas/ra/e2studio_v2025-12_fsp_v6.3.0
export PATH=$E2/toolchains/llvm_arm/ATfE-21.1.1-Linux-x86_64/bin:$PATH
$E2/eclipse/e2studio-cli -nosplash \
  -application org.eclipse.cdt.managedbuilder.core.headlessbuild \
  -data <workspace-dir> -import <project-dir> -build "iotc_freertos_ek_ra8p1/Debug"
```

The build produces `Debug/iotc_freertos_ek_ra8p1.elf` and `.srec`. To make a `.hex` like the
prebuilt image: `llvm-objcopy -O ihex Debug/iotc_freertos_ek_ra8p1.elf <name>.hex`.

Notes:

- The ATfE `bin` directory must be on `PATH`; e² studio does not export its own toolchain
  directory in headless builds, so clang is otherwise not found.
- `-import` is needed once per workspace; afterwards `-build` alone is enough.
- The Linux `e2studio-cli` does not accept `--launcher.suppressErrors`.
- Every build prints "Managed Build system manifest file error: Unable to resolve the category
  identifier ...includes.includeFileDir.cpp". It is harmless.
- The Smart Configurator regenerates `ra_gen/` and `ra_cfg/` from `configuration.xml` on every
  build, so configuration changes belong in `configuration.xml`
  (see [section 13](#13-vendor-patches-and-required-configuration)).
- After deleting or moving source files outside the IDE, run `-cleanBuild` instead of `-build`;
  otherwise the generated makefiles still reference the old files.
- Keep each FreeRTOS thread's entry file (for example `src/net_thread_entry.c`) at the top of
  `src/`. If the Smart Configurator finds no `<thread>_entry.*` there, it generates an empty stub.
- `src/kvs/wslay/lib/includes/wslay/wslayver.h` is normally generated by wslay's own build
  system, which e² studio does not run. It is committed here (generated from `wslayver.h.in`,
  wslay 1.1.1).

### Updating the FSP version

The project pins its FSP version. To move it to a newer installed FSP:

1. In `configuration.xml`, update the `+fsp.<version>` references, the `version="..."`
   attributes, and `<option key="#FSPVersion#" value="..."/>`.
2. In `.cproject`, `<option id="toolchain.version" .../>` must match the installed ATfE version;
   otherwise the headless build fails with "Toolchain … not currently available".
3. Headless generation does not add new FSP defaults to an existing configuration; opening the
   project in the e² studio Smart Configurator does. For example, `rm_ethosu` requires the OFS2
   option-setting block (`config.bsp.ra8p1.linker`: `option_setting.ofs2 = enabled`,
   `ofs2.dcdc = enabled`, `ofs2.cvm_reset = disabled`, `ofs2.npusa = secure`,
   `ofs2.npupa = unprivilege`).

## 4. Flash and Console

The board's on-board debugger enumerates as "J-Link OB-RA4M2". The device name in the SEGGER
database is `R7KA8P1KF_CPU0` (the Cortex-M85; `_CPU1` is the Cortex-M33). Use J-Link software
V9.38.

```
# commands.jlink:  r / h / loadfile Debug/iotc_freertos_ek_ra8p1.elf / r / g / q
JLink.exe -device R7KA8P1KF_CPU0 -if SWD -speed 4000 -AutoConnect 1 -CommandFile commands.jlink
```

On Linux the executable is `JLinkExe`, with the same arguments. If more than one probe is
connected, add `-USB <serial-number>`.

- MRAM programs at about 130 KB/s, so a full image takes about 10 seconds. Check that the
  "Flash download:" line appears; a J-Link session conflict occasionally exits early and leaves
  the old firmware in place. The option bytes are handled by the flash loader.
- After flashing, power-cycle the board instead of relying on a debugger reset. The LCD panel
  only initialises from a cold start (see [section 15](#15-troubleshooting)).
- The serial console is the J-Link OB CDC UART at 230400 baud, 8N1 (not 115200). Any serial
  terminal works, including the browser-based
  [Chrome Labs Serial Terminal](https://googlechromelabs.github.io/serial-terminal/) (Web
  Serial, so Chrome or Edge only).

A healthy boot with a stored identity prints, in order: the DHCP lease →
`IOTC: starting (…, credentials: stored|compiled)` → `IOTC: time synced` → identity provisioned
→ `FU: file upload ready (bucket …)` → `IOTC: connected` → `FU: selftest creds fetch -> 0 (OK)`
→ telemetry every 10 seconds. With no identity the device still runs the vision pipeline and
prints a provisioning hint instead.

Console output that repeats (the processing report every 5 seconds, and per-detection `FD:` lines
at most every 2 seconds) goes through `console_report_due()` in `src/console_output/`, which holds
it off while someone is typing and for 20 seconds after the last character
(`iotc_cli_is_interactive()`), so it does not interleave with typed commands or pasted PEMs. The
`quiet` command silences it entirely.

## 5. Provisioning the Device Identity

1. In /IOTCONNECT (AWS backend), import the device template
   [`templates/ra8p1-vision-ai-template.json`](../templates/ra8p1-vision-ai-template.json). It
   defines the telemetry attributes (numeric attributes must be DECIMAL — a type mismatch shows
   as `null` on the dashboard), the commands, File Support (required for snapshot upload), and
   video streaming.
2. Create a device from that template with an X.509 certificate ("Auto-generated" is easiest),
   and download its certificate and key.
3. Give the identity to the device, in one of two ways:
   - **Runtime provisioning (default, no rebuild):** the serial CLI stores env, CPID, device ID,
     certificate and private key in LittleFS on the OSPI flash. They survive power cycles and
     take precedence over a compiled-in identity. The walkthrough is
     [Quickstart step 9](../README.md#9-configure-the-board); the implementation is
     `src/iotc/iotc_cli.c` and `src/iotc/iotc_config.c`.
   - **Compile-time (development convenience):** copy `src/iotc/app_secrets.h.example` to
     `src/iotc/app_secrets.h` (gitignored), fill in `IOTC_CFG_ENV`, `IOTC_CFG_CPID`,
     `IOTC_CFG_DUID` and the PEMs (one `"...\n"` string literal per line), and set
     `IOTC_CFG_ENABLED 1`. It is used only when no runtime configuration is stored.

The firmware discovers its /IOTCONNECT endpoints through `awsdiscovery.iotconnect.io`
(`IOTC_CFG_DISCOVERY_HOST` in `src/iotc/app_config.h`).

The serial CLI's provisioning commands: `help`, `show`, `set env|cpid|duid <value>`, `set cert`,
`set key`, `apply`, `erase`, `reboot`. PEM capture ignores anything before the `-----BEGIN` line,
finishes as soon as a complete `-----END …-----` line arrives (no trailing Enter needed), rejects
a key pasted at `set cert` and the reverse, and accepts `cancel` or an empty line to abort. CR,
LF and CR+LF line endings are all accepted.

## 6. Architecture

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
  │   KVS WebRTC video: camera ─► RGB565→I420 ─► minih264 (sw)     │
  │      ─► RTP/SRTP ─► viewer;  signaling wss + ICE/TURN + DTLS   │
  │      channel + role-alias creds from the identity d.p.vs block │
  │                                                                │
  │   model_store: IOTV envelope ──► raw OSPI slot (+56 MB)        │
  │   PKCS#11 credentials ──► LittleFS on OSPI (+32 MB)            │
  └────────────────────────────────────────────────────────────────┘
```

| Thread | Source | Role |
|---|---|---|
| camera_thread | `src/camera_thread_entry.c`, `src/camera_layer/` | VIN capture ring; RGB565 → 224×224 RGB888 staging for the NPU |
| ai_inference_thread | `src/ai_inference_thread_entry.c`, `src/ai_application/object_detection/FaceDetection.cc` | model lifecycle, hot-swap, inference, post-processing |
| display_thread | `src/display_thread_entry.c`, `src/display_layer/` | GLCDC output, detection overlay, info panel |
| net_thread | `src/net_thread_entry.c`, `src/iotc/` | DHCP, SNTP, discovery, MQTT connection, telemetry, commands |
| iotc_mqtt | `src/iotc/iotc_mqtt_client.c` | MQTT process loop and keep-alive; runs command callbacks |
| iotc_xfer | `src/iotc/iotc_app.c` | long transfers, one at a time: model downloads and snapshot uploads (32 KB stack in SDRAM) |

Key modules in `src/iotc/`:

| File | Purpose |
|---|---|
| `iotconnect.c` | orchestrator: filesystem → PKCS#11 provisioning → discovery → MQTT |
| `iotc_dra_client.c` | discovery/identity HTTPS, and the large-download path used for model pulls |
| `iotc_mqtt_client.c` | coreMQTT over mutual TLS. The API mutex is recursive because command callbacks publish acknowledgements from inside `MQTT_ProcessLoop` |
| `iotc_file_upload.c` | Telemetry Files: AWS credentials provider (mutual TLS) → optional STS AssumeRole → SigV4 S3 PUT → `fu` announce |
| `iotc_snapshot.c` | frame grab → RGB888 → detection boxes → color PNG (`png_gray.c`) → upload |
| `iotc_cli.c` | serial provisioning CLI and the typing-activity gate for console output |
| `iotc_app.c` | net-thread state machine, telemetry, command and model-push (OTA `ct:2`) handlers |

Model lifecycle (`FaceDetection.cc` and `src/model_store/`): pushed models are IOTV-enveloped
(32-byte header: magic, version, length, CRC32, name). The download lands in an SDRAM pending
buffer; the AI thread validates it, swaps it in between two inferences (tearing down the TFLM
interpreter and rebuilding it over the same tensor arena), then saves the envelope to a raw OSPI
slot that is reloaded at boot. The model family is detected from the input tensor shape (see
[section 9](#9-ai-models)). If a download or swap fails, the previous model keeps running.

Model downloads and snapshot uploads run on the `iotc_xfer` task, so telemetry and command
handling continue while a transfer is in flight (a 3 MB model or a snapshot can take a minute
or more). Downloads retry three times on transient network errors, and wait for free heap when a
video session is using it. If the connection drops during a transfer, the device reconnects once
the transfer has finished.

When any TLS connection cannot be opened, the console prints the reason in one line: link state,
a fresh DNS lookup of the host, free and lowest-ever network buffers, and free, lowest-ever and
largest-block heap, for example
`MQTT: TLS connect to <host> failed: link=up dns=<ip|FAILED> netbufs=<free> (min <lowest>) heap=<free> (min <lowest>, largest block <bytes>)`.
TLS status 6 means the TCP connection itself failed (DNS, socket or no answer), 2 means out of
memory, and 4 a failed handshake.

## 7. Telemetry and Commands

Telemetry is published every 10 seconds by default (`set-interval` changes it):

| Attribute | Meaning |
|---|---|
| `vision.face_count` | faces detected in the current frame (face model) |
| `vision.score` | confidence of the best detection or class, % |
| `vision.state` | `face`, `clear`, `person`, `no person`, or the top ImageNet class label |
| `model.name`, `model.ver` | the running model's name and version (from its IOTV header) |
| `model.src` | `cloud` (pushed this session), `flash` (reloaded at boot), `builtin`, or `none` |
| `model.size_b` | model size in bytes |
| `perf.infer_us`, `perf.infer_fps` | NPU inference time and the rate it would allow |
| `perf.cam_fps` | camera frame rate |
| `sys.uptime_s`, `sys.free_heap`, `sys.msgs_sent` | device vitals |
| `video.state` | live-video state (`live` while a viewer is connected) |

Cloud commands (defined in the template):

| Command | Effect |
|---|---|
| `snapshot` | captures a 480×480 color PNG with detection boxes and uploads it to Telemetry Files, tagged with the detection results and performance figures |
| `set-interval <seconds>` | telemetry period (default 10) |
| `model-info` | acknowledges with the active model's name, version, source and size |
| `model-revert` | erases the stored model; this build has no compiled-in model, so inference idles until the next push |
| `set-brightness <0\|1>` | camera exposure target: 0 normal, 1 about +1 EV for dim rooms |
| `led-auto [0\|1]` | the green user LED (P303) follows face or person detections; off by default, no argument toggles |
| `reboot` | warm restart; reconnects and reloads the stored model in about 30 seconds (the LCD stays white until a power cycle) |

The same commands are available on the serial console, alongside the provisioning commands in
[section 5](#5-provisioning-the-device-identity) and `quiet [0|1]`.

## 8. Memory Budget

| Region | Size | Use |
|---|---|---|
| MRAM (code flash) | 1 MB | the image is about 917 KB. Check `llvm-size` after adding code. The build carries no compiled-in model (`IOTC_CFG_NO_BUILTIN_MODEL=1` in `src/iotc/app_config.h`): the live-video stack and the 441 KB face model do not fit in MRAM together |
| SRAM | 2 MB | 640 KB tensor arena; 484 KB FreeRTOS heap (`0x79000` — mbedTLS allocates here, and two concurrent TLS sessions need at least 100 KB free); 64 KB libc heap |
| SDRAM | 64 MB | camera frame buffers, 4 MB model staging plus 4 MB pending buffer, snapshot buffers |
| OSPI flash | 64 MB | lower 32 MB factory-protected; LittleFS (credential store) at +32 MB, 16 MB; model slot at +56 MB, 8 MB |

## 9. AI Models

### Model library

Five ready-to-push models ship in [`tools/models/`](../tools/models/). Upload the `.zip` in
/IOTCONNECT AI Models (Model Type "AI Model", Variant "Renesas"). The model's code must be 3–10
characters; the name and code are platform bookkeeping only — the name the device displays comes
from inside the file.

| Model | Task | Input | Inference | Size |
|---|---|---|---|---|
| `face-v3` | face detection with boxes (YOLO Fastest) | 192×192 gray | ~5.8 ms | 441 KB |
| `person-detect` | person present / absent | 96×96 gray | ~1.5 ms | 270 KB |
| `mobilenet-025` | ImageNet classifier, speed tier | 224×224 RGB | ~4.5 ms | 432 KB |
| `mobilenet-050` | ImageNet classifier, mid tier | 224×224 RGB | ~9 ms | 1.1 MB |
| `mobilenet-v2` | ImageNet classifier, accuracy tier | 224×224 RGB | ~40 ms | 3.1 MB |

### Adding a model

The hot-swap accepts two model families, chosen by input shape when the model loads:

| Family | Input | Output |
|---|---|---|
| Face detector | 192×192×1, int8/uint8 | 2 output tensors (YOLO Fastest heads) |
| Classifier | 32–224 × 32–224, 1 or 3 channels, int8/uint8 | 1 output tensor: 2 classes = person / no person; 1000 or 1001 classes = ImageNet labels (a background class at index 0 is handled) |

Constraints: the enveloped model must be at most 4 MB, and Vela's reported "Total SRAM used" at
most 640 KiB. Only the operators linked in `YoloFastestModel.cc` are available, so the model
must compile to a single Ethos-U operator (100% NPU-resident); MRAM has no room for a wide set of
CPU fallback kernels.

```
pip install ethos-u-vela

vela --accelerator-config=ethos-u55-256 --optimise Performance \
     --config Arm/vela.ini --memory-mode=Shared_Sram \
     --system-config=Ethos_U55_High_End_Embedded  model_quant.tflite

# If "Total SRAM used" is over 640 KiB, recompile with --optimise Size
# (trades speed for a much smaller arena; MobileNet v2: 1474 -> 353 KiB).

python tools/pack_model.py output/model_quant_vela.tflite --version 1 --name my-model
```

`pack_model.py` writes `my-model_v1.iotv` and `my-model_v1.zip`. The zip is STORED
(uncompressed) because the firmware only unpacks stored zips; a normally compressed zip is
rejected on the device. Upload the zip to AI Models and push it. A model with an unsupported
shape is rejected at load and the previous model is restored. The display name in the IOTV header
is up to 15 characters; the LCD shows the first 12.

Quantized classifier outputs can be post-softmax probabilities (output scale 1/256) or logits;
both are handled — the firmware applies softmax when the dequantized top value exceeds 1.

## 10. Live Video (KVS WebRTC)

The device streams live camera video to the /IOTCONNECT Video Streaming tab as a WebRTC master
over an AWS Kinesis Video Streams signaling channel.

**Provisioning.** The platform creates the signaling channel when a device is created from a
template with `videoStreamResource: "2"` and `videoStreamType: "3"`, as the bundled template has.
It cannot be added to an existing device. At runtime the identity response carries a `d.p.vs`
block: `carn` (the signaling channel ARN, from which the region and channel name are parsed) and
`url` (the IoT credentials-provider role-alias URL used to fetch temporary AWS credentials with
the device's X.509 certificate). Parsing is in `src/kvs_app/kvs_webrtc_task.c`
(`iotc_kvs_identity_hook`), called from the identity flow next to the file-upload hook. After
changing the device identity, power-cycle the board so video starts against the new channel.

**Stack.** `src/kvs/` contains the AWS modular KVS WebRTC components (signaling, ICE, STUN, SDP,
RTP/RTCP), libsrtp, and the wslay websocket library; DTLS-SRTP runs on the FSP's mbedTLS 3.6.
`src/kvs_port/lwip_shim/` provides the BSD-socket API the stack expects on top of FreeRTOS+TCP
(file-descriptor table, `select()` mapped to `FreeRTOS_select` with per-task socket sets,
`getaddrinfo` over FreeRTOS DNS).

**Media.** `src/kvs_app/port/ra8p1_media_port.c` converts the shared camera frame (640×480
RGB565) to I420 QVGA and encodes H.264 with minih264 (`src/video/minih264e.h`), entirely in
software: about 90 ms per frame with the encoder state in SRAM, giving roughly 8–10 fps at about
500 kbit/s. Encoding runs only while a viewer is connected; the vision pipeline is unaffected.

**Implementation requirements:**

- libsrtp uses its native software AES-ICM/HMAC ciphers. The FSP hardware-AES alternate
  implementation fails libsrtp's AES-ICM known-answer self-test, after which the SRTP crypto
  kernel refuses every session.
- `MBEDTLS_SSL_KEEP_PEER_CERTIFICATE` must stay enabled (an FSP property): the DTLS handshake
  checks the peer certificate against the SDP fingerprint.
- The socket shim's `select()` must keep one FreeRTOS socket set per task: the websocket receive
  loop and the ICE socket listener block in `select()` at the same time.
- `Http_Send` (`src/kvs/examples/networking/corehttp_helper/core_http_helper.c`) only
  disconnects a TLS session it established. A failed connect has already freed its TLS context,
  and freeing it again asserts in the FSP's `mbedtls_ctr_drbg_free`.

**Limitations:** one viewer at a time (`AWS_MAX_VIEWER_NUM 1` in
`src/kvs_app/port/demo_config.h`): while a session is open, an SDP offer from another viewer is
refused (`AppCommon_GetPeerConnectionSession`), so the dashboard's video widget and the device
page's Video Streaming tab cannot play at the same time. TURN over TLS (`turns:`) to the KVS
TURN servers does not connect, so sessions use plain-UDP TURN or direct paths. Role-alias
credentials are refreshed by the signaling controller before they expire.

## 11. Networking

The board uses wired Gigabit Ethernet (RGMII, GPY111 PHY) with DHCP. These settings are all
required; any one of them missing stops networking:

- **Clock tree:** ESWCLK and ESWPHYCLK must be enabled (from PLL1P), and ETHPHYCLK must be exactly
  25 MHz. It is taken from PLL1R (400 MHz) ÷ 16, which no other peripheral uses, so the camera,
  display and SDRAM clocks are unaffected. Without these the Ethernet switch is unclocked: MDIO
  reads zeros and the link fails with error 4001.
- **P708 (ETHERNET_RST)** is a GPIO output driven high; otherwise the PHY stays in reset (its ID
  reads 0000; a healthy PHY reads d565:a401).
- **The D-cache is disabled** (`config.bsp.fsp.dcache`). The FSP `r_rmac` and `r_layer3_switch`
  drivers do no cache maintenance on their DMA descriptors, and Renesas' EK-RA8P1 Ethernet
  examples also run with the cache off. The cost to the vision pipeline is pre-processing time
  (about 10 ms instead of 2 ms); camera, NPU and LCD rates are unchanged.
- **`xApplicationGetRandomNumber`** is implemented in `src/net_thread_entry.c`. FreeRTOS+TCP's
  weak default returns failure, which makes DHCP abort without an error message.
- Two FreeRTOS+TCP vendor files carry fixes (see [section 13](#13-vendor-patches-and-required-configuration)).
  Without them the console stops about 11 seconds after boot, as soon as real network traffic
  arrives.

**MAC address.** Each board derives its own locally administered MAC address: `02:8a:9b`
followed by three bytes hashed (FNV-1a) from the MCU's 128-bit unique ID (`R_BSP_UniqueIdGet()`).
It is applied in `prv_mac_from_unique_id()` before `FreeRTOS_IPInit`, which is the call that opens
the driver and latches the address. The generated MAC arrays (`g_ether0_mac_address`,
`g_layer3_switch0_mac_address_port0/1`) are writable and overwritten there, so `ra_gen/` needs no
patch. The address stays the same across reboots and reflashes, and the boot log prints it.

Many home routers isolate Wi-Fi clients from wired ones. When testing connectivity, test from the
board side (for example the gateway ping counter in the console's `Ethernet: … pong=` line), not
from a PC on Wi-Fi.

## 12. OSPI Flash Storage

- **The MX25LW51245G flash stays in octal DDR mode across MCU resets** once any firmware switches
  it, for example the factory demo. Only a power cycle or a RESET# pulse restores single-SPI mode.
  If it is not reset, every `r_ospi_b` operation returns `FSP_ERR_DEVICE_BUSY` and memory-mapped
  reads return 0xFF. `iotc_fs_init()` therefore pulses P106 (OM_0_RESET) before opening the OSPI.
- `rm_littlefs_spi_flash.c` invalidates the D-cache before memory-mapped reads, because the
  LittleFS port copies from the cacheable memory-mapped window while the OSPI driver programs the
  array behind the cache.
- Writes go through `g_ospi0.p_api` in chunks of at most 64 bytes (the combination-buffer size),
  polling status between chunks.
- The device identity is stored in LittleFS at +32 MB. The model store does not use LittleFS: it
  writes the IOTV envelope to a raw slot at +56 MB.

## 13. Vendor Patches and Required Configuration

`ra/` is FSP-generated vendor code, but several files carry required patches. A Smart
Configurator regeneration can overwrite them, so re-check them after changing the FSP version:

| File | Patch |
|---|---|
| `ra/fsp/inc/instances/r_layer3_switch.h` | made C++-clean: clang rejects `volatile` anonymous bit-fields and tagged structs inside anonymous unions (C++ DR2229); the reserved padding loses its qualifiers and the unused tags are removed |
| `ra/fsp/src/rm_littlefs_spi_flash/rm_littlefs_spi_flash.c` | D-cache invalidate before memory-mapped reads |
| `ra/fsp/src/rm_freertos_plus_tcp/NetworkInterface.c` | a NO_DATA return is no longer treated as a received frame (it caused an endless stream of zero-byte events) |
| `ra/aws/FreeRTOS/FreeRTOS-Plus/Source/FreeRTOS-Plus-TCP/source/FreeRTOS_DHCP.c` | `vDHCPProcess` no longer re-peeks an unconsumed message forever |
| `ra/aws/FreeRTOS/FreeRTOS-Plus/Source/Application-Protocols/network_transport/transport_mbedtls_pkcs11.c` | TLS capped at 1.2 (the AWS credentials provider mishandles client certificates over TLS 1.3) |

Configuration that must stay set in `configuration.xml`:

- mbedTLS Server Name Indication enabled (`mbedtls_ssl_server_name_indication`); without it every
  Telemetry Files request is rejected with 403.
- FreeRTOS heap `0x79000` (484 KB).
- The OFS2 option-setting block required by `rm_ethosu` (see [section 3](#updating-the-fsp-version)).
- The D-cache disabled, and the Ethernet clocks and pins described in
  [section 11](#11-networking).

The linker needs `-Wl,-z,norelro`: picolibc's thread-local `errno` makes lld create a `.got`
section that conflicts with the memory layout. The e² studio managed build drops the linker's
"User defined options" field, so the flag is part of the linker tool command in `.cproject`
(`<tool command="clang++ --target=arm-none-eabi -Wl,-z,norelro" …>`).

## 14. Dashboard

[`dashboard/ra8p1-vision-ai-dashboard.json`](../dashboard/ra8p1-vision-ai-dashboard.json) is the
importable dashboard. Its artwork (banner, detection-state and model-source cards) is served from
Avnet's public bucket at
`https://avnetpublicaccess.s3.us-east-1.amazonaws.com/images/renesas/ek-ra8p1/`, so an imported
dashboard needs no setup. The source images are in [`dashboard/images/`](../dashboard/images/). To
host them elsewhere, upload them keeping the exact (case-sensitive) file names, and replace the
bucket URL throughout the dashboard JSON.

The Detection State card maps `vision.state`: face, clear, person and no person each have their
own artwork, and any other value (an ImageNet label) shows the CLASSIFYING card. The Model Source
card maps `model.src` to builtin, flash or cloud.

The dashboard sends two commands through single-command buttons: Take Snapshot (`snapshot`) and
Model Revert (`model-revert`). It has no general command widget, so the other commands in
[section 7](#7-telemetry-and-commands) are sent from the Command tab on the device's page.

## 15. Troubleshooting

| Symptom | Cause / fix |
|---|---|
| J-Link cannot connect or programming fails | Use J-Link software V9.38 (newer and older versions fail on this board), and select the device `R7KA8P1KF_CPU0`, not `_CPU1` |
| No serial output | Select the J-Link CDC UART COM port (USB Serial Device if the J-Link software is not installed), and check the speed is `230400`, not `115200` |
| Typed characters are not accepted | Set the terminal to send CR or CR+LF line endings |
| Console output scrolls too fast to read what you type | The status report pauses by itself while you type and resumes about 20 seconds after you stop. To silence it for good, type `quiet` |
| A PEM paste is rejected as too large | Paste one PEM block per command — the certificate and the key separately |
| `error: that is not a certificate PEM` (or `private key PEM`) | The key was pasted at `set cert`, or the certificate at `set key`. Nothing was stored; repeat with the right file |
| Nothing happens when pasting a PEM in PuTTY | Ctrl+V does not paste in PuTTY. See the tip in [Quickstart step 9](../README.md#9-configure-the-board) |
| The Chrome Labs Serial Terminal lists no port, or will not connect | Use Chrome or Edge (Firefox and Safari have no Web Serial). Close any other program that has the port open, since only one can hold it. On Linux, add yourself to the `dialout` group. After a power cycle, click Connect again |
| The LCD is all white after flashing or any warm reset | The panel's timing controller only initialises from a cold start, and the board brings out only `DISP_BLEN` and `DISP_RESET` (no panel power control), so firmware cannot recover it. Power-cycle the board. Telemetry, video, snapshots and model pushes are unaffected |
| The LCD stays white after a power cycle | The panel ribbon cable has worked loose, or the expansion board is misaligned. Re-seat the ribbon cable from the glass panel into the expansion board (open the latch, push the cable fully in, close the latch), and check that the expansion board's pin 1 lines up with pin 1 of J1 |
| The LCD stays blank | The LCD is optional and the demo runs headless; if it is attached, check both flat cables |
| No camera image | Re-seat the OV5640 camera board on J35; the flex cable must be fully latched at both ends |
| Dashboard telemetry values are `null` | Template attribute type mismatch — numerics must be DECIMAL. Import the bundled template rather than creating one by hand |
| `DUPLICATE_CLIENTID` in the platform log after a reflash | Harmless: the new session replaces the old one |
| Boot prints `FU: selftest creds fetch -> -13` | File Support is not enabled on the template, or the device's certificate is not the one registered for it |
| A snapshot acknowledgment reports upload failed | Send the command again; confirm the boot log printed `FU: file upload ready` |
| Snapshot upload fails with 403 "Certificate is invalid on this endpoint" | mbedTLS Server Name Indication is disabled — see [section 13](#13-vendor-patches-and-required-configuration) |
| Inference time stays at `0 us` | No model is loaded — deploy one as described in [Quickstart step 12](../README.md#12-deploy-an-ai-model) |
| A model push never arrives | Confirm the deployment was dispatched in AI Models; the device logs `MQTT: C2D message` the moment one arrives |
| Model download reports `TLS connect failed` | Signed model URLs are on S3 and verify against Amazon Root CA 1 (already configured); transient DNS/TLS errors are retried three times |
| A pushed model is rejected with "unsupported shape" | Its input is outside the contracts in [section 9](#9-ai-models) |
| Classifier labels look wrong, or confidence is only 30–60% | Hold a single recognizable object centered and close to the camera; ImageNet classifiers are trained on single objects, not scenes. Confidence spread over 1,000 classes is normally modest — the label being right is what matters |
| The Video Streaming tab stays black | Click Stop, then Start again, and allow about 15 seconds. The device must have been created from the supplied template |
| The board stops responding | Power-cycle it: it boots into its stored model and reconnects within about 30 seconds |
| A second TLS connection fails intermittently (`HANDSHAKE_FAILED`) | FreeRTOS heap exhaustion — keep at least 100 KB free; this is why the heap is 484 KB |
| OSPI operations return `FSP_ERR_DEVICE_BUSY` after a reset | The flash is still in octal DDR mode — see [section 12](#12-ospi-flash-storage); keep the RESET# pulse in `iotc_fs_init()` |

## 16. Resources

- [Quickstart](../README.md) — run the demo with the prebuilt image, no toolchain needed
- [Workshop slides (PDF)](EK-RA8P1-vision-ai-workshop.pdf) — the hands-on workshop built on
  this demo ([PowerPoint](EK-RA8P1-vision-ai-workshop.pptx))
- [EK-RA8P1 Evaluation Kit](https://www.renesas.com/en/design-resources/boards-kits/ek-ra8p1) —
  board documentation and the user's manual
- [Renesas Flexible Software Package](https://github.com/renesas/fsp)
- [Arm Ethos-U Vela compiler](https://pypi.org/project/ethos-u-vela/)
- [/IOTCONNECT Overview](https://www.iotconnect.io/) ·
  [/IOTCONNECT Knowledgebase](https://help.iotconnect.io/)
