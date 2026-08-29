# Developer Guide

Everything needed to build this project from source, connect a device to /IOTCONNECT, and
extend it — including adding your own pushable models.

For the raw engineering log (every toolchain quirk and hardware gotcha found during bring-up,
with the reasoning), see [BUILD-NOTES.md](BUILD-NOTES.md). This guide is the distilled path.

## Contents

- [1. Prerequisites](#1-prerequisites)
- [2. Clone](#2-clone)
- [3. /IOTCONNECT setup](#3-iotconnect-setup)
- [4. Build](#4-build)
- [5. Flash and console](#5-flash-and-console)
- [6. Architecture](#6-architecture)
- [7. Memory budget](#7-memory-budget)
- [8. Vendor patches](#8-vendor-patches-survive-with-care)
- [9. Adding models](#9-adding-models)
- [10. Live video (KVS WebRTC)](#10-live-video-kvs-webrtc)
- [11. Troubleshooting](#11-troubleshooting)

---

## 1. Prerequisites

| Tool | Version used | Notes |
|---|---|---|
| e² studio | 2025-10 (25.10.0) | includes the FSP Smart Configurator (DDSC) |
| FSP packs | 6.3.1 | install into the e² studio tree |
| LLVM Embedded Toolchain for Arm (ATfE) | 21.1.1 | GNU ARM toolchains are not used |
| SEGGER J-Link | V9.38+ | earlier versions lack RA8P1 flash support |
| Python | 3.10+ | for `tools/pack_model.py` and Vela |
| ethos-u-vela | 5.1.0 | `pip install ethos-u-vela` (only for adding models) |

## 2. Clone

The protocol library is a submodule with its own nested submodule (cJSON) — clone
recursively:

```
git clone --recurse-submodules <repo-url>
```

(Already cloned? `git submodule update --init --recursive`.)

**Windows**: run `git config core.longpaths true` and keep the checkout at a short path
(e.g. `c:\dev\`) — parts of the vendor tree have deep paths and GNU make cannot stat
>260-character paths.

## 3. /IOTCONNECT setup

1. In /IOTCONNECT (AWS backend): **Device → Templates → Import**, and import
   [`templates/ra8p1-vision-ai-template.json`](../templates/ra8p1-vision-ai-template.json).
   The template defines the telemetry attributes (all numerics DECIMAL — types matter, a
   mismatch shows as `null` on the dashboard), the four commands, and enables
   **File Support** (required for snapshot upload).
2. **Device → Create Device** on that template, auth type X.509 ("Auto-generated" is
   easiest). Download the device's certificate/key zip and the `iotcDeviceConfig.json`.
3. Provide the identity to the device — two options:
   - **Runtime provisioning (default, no rebuild)**: the serial CLI stores env, CPID,
     DUID, and the certificate/key PEMs on LittleFS (OSPI flash); they survive power
     cycles and take precedence over anything compiled in. The walkthrough is in the
     [Quickstart §5](QUICKSTART.md#5-provision-credentials-over-the-serial-terminal); the
     implementation is `src/iotc/iotc_cli.c` + `src/iotc/iotc_config.c`.
   - **Compile-time (development convenience)**: copy `src/iotc/app_secrets.h.example` →
     `src/iotc/app_secrets.h` (gitignored) and fill in `IOTC_CFG_ENV`/`CPID`/`DUID` and
     the PEMs, one `"...\n"` string literal per line, with `IOTC_CFG_ENABLED 1`. Used only
     when no runtime configuration is stored.
4. To register the bundled models for pushing: **AI Models → Create Model**, Model Type
   "AI Model", Variant "Renesas", and upload a zip from `tools/models/`. Codes must be
   3–10 characters. Name/Code are platform bookkeeping only — the name the device displays
   comes from inside the file.

## 4. Build

### e² studio (IDE)

Import the project (**File → Import → Existing Projects**), let the Smart Configurator
generate (`ra_gen/`, `ra_cfg/`), and build the **Debug** configuration. First build takes a
few minutes; the image links at ~1,022 KB of the 1,024 KB MRAM (see §7 before adding code).

### Headless (CI / command line)

```
e2studioc.exe -nosplash --launcher.suppressErrors ^
  -application org.eclipse.cdt.managedbuilder.core.headlessbuild ^
  -data <workspace-dir> -import <project-dir> -build "iotc_freertos_ek_ra8p1/Debug"
```

Quirks that matter (details in BUILD-NOTES):
- The ATfE `bin` directory must be on `PATH` or clang is not found.
- `-import` is needed once per workspace; afterwards `-build` alone suffices.
- The DDSC generator runs automatically and re-emits `ra_gen/`/`ra_cfg/` from
  `configuration.xml` — which is why config changes are made in `configuration.xml`
  (see §8 on vendor patches).

## 5. Flash and console

```
# commands.jlink:  r / h / loadfile Debug/iotc_freertos_ek_ra8p1.elf / r / g / q
JLink.exe -device R7KA8P1KF_CPU0 -if SWD -speed 4000 -AutoConnect 1 -CommandFile commands.jlink
```

- MRAM programs at ~130 KB/s (full image ≈ 10 s). **Verify the "Flash download:" line
  appeared** — a J-Link session conflict occasionally exits early and leaves stale firmware.
- Serial console = the J-Link OB CDC UART at **230400 8N1**.

A healthy cloud boot prints, in order: DHCP lease →
`IOTC: starting (…, credentials: stored|compiled)` → `IOTC: time synced` → identity
provisioned → `FU: file upload ready (bucket …)` → `IOTC: connected` →
`FU: selftest creds fetch -> 0 (OK)` → telemetry every 10 s. With no stored or compiled
identity the device runs the vision pipeline and prints a provisioning hint instead.

## 6. Architecture

| Thread | Source | Role |
|---|---|---|
| camera_thread | `src/camera_thread_entry.c` | VIN capture ring, RGB565→RGB888 224×224 staging for the NPU |
| ai_inference_thread | `src/ai_inference_thread_entry.c` + `src/ai_application/object_detection/FaceDetection.cc` | model lifecycle, hot-swap, inference, post-processing |
| display_thread | `src/display_thread_entry.c`, `src/display_layer/` | GLCDC output, detection overlay, info panel |
| net_thread | `src/net_thread_entry.c`, `src/iotc/` | DHCP, SNTP, DRA, MQTT, telemetry, commands, snapshot upload, model download |

Key modules in `src/iotc/`:

| File | Purpose |
|---|---|
| `iotconnect.c` | orchestrator: filesystem → PKCS#11 provisioning → DRA → MQTT |
| `iotc_dra_client.c` | discovery/identity HTTPS + generic HTTPS download (model pull) |
| `iotc_mqtt_client.c` | coreMQTT over mutual TLS; **the API mutex is recursive** because C2D callbacks publish acks from inside `MQTT_ProcessLoop` |
| `iotc_file_upload.c` | Telemetry Files: AWS credentials provider (mTLS) → optional STS AssumeRole → SigV4 S3 PUT → `fu` announce |
| `iotc_snapshot.c` | frame grab → RGB888 → boxes → color PNG (`png_gray.c`) → upload |
| `iotc_app.c` | net-thread state machine, telemetry build, command + OTA (ct:2) handlers |

Model lifecycle (`FaceDetection.cc` + `src/model_store/`): pushed blobs are IOTV-enveloped
(32-byte header: magic/version/length/CRC32/name). Download lands in an SDRAM pending
buffer; the AI thread validates, swaps between two inferences (destructor + placement-new of
the TFLM wrapper over a shared arena), then persists the envelope to a raw OSPI slot that is
reloaded at boot. The model **family is auto-detected from the input tensor shape** — see
§9.

## 7. Memory budget

| Region | Size | Notes |
|---|---|---|
| MRAM (code flash) | 1 MB | image is ~21 KB from full — check `llvm-size` after every feature; the 441 KB built-in model array is the big lever if space is needed |
| SRAM | 2 MB | 640 KB tensor arena, 256 KB FreeRTOS heap (mbedTLS allocates here — two concurrent TLS sessions need ≥100 KB free), 64 KB libc heap |
| SDRAM | 64 MB | frame buffers, 4 MB model staging + 4 MB pending, snapshot buffers |
| OSPI flash | 64 MB | lower 32 MB factory-protected; LittleFS (PKCS#11 store) at +32 MB/16 MB; model slot at +56 MB/8 MB |

## 8. Vendor patches (survive with care)

`ra/` is FSP-generated vendor code, but several files carry required in-repo patches. A
Smart Configurator regeneration can overwrite them — re-check after FSP version changes:

| File | Patch |
|---|---|
| `ra/.../r_layer3_switch.h` | C++-clean (clang DR2229: no volatile anonymous bitfields) |
| `ra/.../rm_littlefs_spi_flash.c` | D-cache invalidate before XIP reads |
| `ra/.../NetworkInterface.c` | NO_DATA no longer treated as a received frame (spin fix) |
| `ra/.../FreeRTOS_DHCP.c` | MSG_PEEK loop breaker (spin fix) |
| `ra/.../transport_mbedtls_pkcs11.c` | TLS capped at 1.2 (AWS credentials provider mishandles client certs over 1.3) |

Config that must stay set in `configuration.xml` (regen-safe): mbedTLS **SNI enabled**
(`mbedtls_ssl_server_name_indication` — without it every Telemetry Files request is rejected
403), FreeRTOS heap 0x40000, OFS2 block for `rm_ethosu`.

## 9. Adding models

The hot-swap accepts two model families, decided by input shape at load:

| Family | Input contract | Output contract |
|---|---|---|
| Face detector | 192×192×1 (int8/uint8) | 2 output tensors (YOLO Fastest heads) |
| Classifier | 32..224 × 32..224, 1 or 3 channels, int8/uint8 | 1 output tensor; 2-class = person/no-person labels, 1000/1001-class = ImageNet labels (background offset handled) |

Constraints: enveloped blob ≤ **4 MB**; Vela-reported "Total SRAM used" ≤ **640 KiB**; only
ops linked in `YoloFastestModel.cc` are available — both shipped families are 100%
NPU-resident (single ethos-u op), and MRAM is too full for a wide CPU-kernel fallback set.

Workflow:

```
pip install ethos-u-vela

vela --accelerator-config=ethos-u55-256 --optimise Performance \
     --config <default_vela.ini> --memory-mode=Shared_Sram \
     --system-config=Ethos_U55_High_End_Embedded  model_quant.tflite

# If "Total SRAM used" > 640 KiB, recompile with --optimise Size
# (trades speed for a much smaller arena; MobileNet v2: 1474 -> 353 KiB).

python tools/pack_model.py model_quant_vela.tflite --version 1 --name my-model
```

`pack_model.py` emits `my-model_v1.iotv` and `my-model_v1.zip` (STORED zip — the firmware
unwraps STORED zips only; a normally-compressed zip is rejected on-device). Upload the zip
to AI Models and push. A model with an unsupported shape is rejected at load and the
previous model restored.

Note on quantized classifier outputs: models emitting post-softmax probabilities (v1-style,
output scale 1/256) and models emitting logits (v2-style) are both handled — the firmware
softmaxes automatically when the dequantized top value exceeds 1.

## 10. Live video (KVS WebRTC)

The device streams live camera video to the /IOTCONNECT Video Streaming tab as a WebRTC
master over an AWS Kinesis Video Streams signaling channel.

**Provisioning.** The platform creates the KVS channel when a device is created from a
template with `videoStreamResource: "1"` / `videoStreamType: "2"` (the bundled template
has these). At runtime, the identity response carries a `d.p.vs` block:
`carn` (the signaling channel ARN — region and channel name are parsed out of it) and
`url` (the IoT credentials-provider role-alias URL used to fetch temporary AWS
credentials with the device's X.509 certificate). Parsing lives in
`src/kvs_app/kvs_webrtc_task.c` (`iotc_kvs_identity_hook`), called from the identity
flow next to the file-upload hook.

**Stack.** `src/kvs/` vendors the AWS modular KVS WebRTC components (signaling, ICE,
STUN, SDP, RTP/RTCP), libsrtp, and the wslay websocket library; DTLS-SRTP runs on the
FSP mbedTLS 3.6. `src/kvs_port/lwip_shim/` provides the BSD-socket API the stack expects
on top of FreeRTOS+TCP (fd table, `select()` mapped to `FreeRTOS_select` with per-task
socket sets, `getaddrinfo` over FreeRTOS DNS).

**Media.** `src/kvs_app/port/ra8p1_media_port.c` converts the shared camera frame
(640×480 RGB565) to I420 QVGA and encodes H.264 with minih264 (`src/video/minih264e.h`)
— all in software: ~90 ms/frame with the encoder state in SRAM, so ~8–10 fps at around
500 kbit/s. Encoding runs only while a viewer is connected; the vision pipeline is
unaffected either way.

**Port notes (hard-won, do not regress):**

- libsrtp uses its **native software AES-ICM/HMAC ciphers** — the FSP hardware-AES
  alternate implementation fails the AES-ICM known-answer self-test (wrapped-key CTR),
  which leaves the srtp crypto kernel refusing all sessions.
- `MBEDTLS_SSL_KEEP_PEER_CERTIFICATE` must stay enabled (FSP property): the DTLS
  handshake verifies the peer certificate against the SDP fingerprint.
- The shim's `select()` must keep one FreeRTOS socket set **per task**: the websocket
  receive loop and the ICE socket listener block in `select()` concurrently.
- The build carries no built-in model (`IOTC_CFG_NO_BUILTIN_MODEL=1`, `-Os`): the KVS
  stack and the 441 KB model array do not fit the 1 MB code flash together.

**Known limitations:** one viewer at a time (`AWS_MAX_VIEWER_NUM 1`); TURN-over-TLS
(`turns:`) handshakes to the KVS TURN fleet currently fail (plain-UDP TURN and direct
paths carry the session); role-alias credentials are refreshed by the signaling
controller before expiry.

## 11. Troubleshooting

| Symptom | Cause / fix |
|---|---|
| Dashboard telemetry values `null` | Template attribute type mismatch — numerics must be DECIMAL. Re-import the bundled template |
| `DUPLICATE_CLIENTID` in platform log after reflash | Benign: the new session kicks the old one |
| Snapshot/upload fails 403 "Certificate is invalid on this endpoint" | SNI disabled in mbedTLS config — see §8 |
| Second TLS connection fails intermittently (`HANDSHAKE_FAILED`) | FreeRTOS heap exhaustion — keep ≥100 KB free; the heap is 484 KB (`0x79000`) for this reason |
| LCD is all white after flashing or after any warm reset | The panel's timing controller only re-initialises from a cold start. Toggling `DISP_RESET` (10 µs, 20 ms, and a full blank-plus-200 ms sequence were all measured) does not clear it, and no panel power-enable pin is brought out — only `DISP_BLEN` and `DISP_RESET` exist in `bsp_pin_cfg.h`. Power-cycle the board after flashing. The MCU side is unaffected: telemetry, video streaming, snapshots and model push all continue to run |
| Model download `TLS connect failed` | Signed model URLs are S3 → must be verified against Amazon Root CA 1 (already wired); transient DNS/TLS errors retry 3× |
| OSPI returns `FSP_ERR_DEVICE_BUSY` after reset | The MX25 flash stays in octal DDR mode across MCU resets — `iotc_fs_init` pulses P106 RESET#; keep that ordering |
| Pushed model rejected "unsupported shape" | Input outside the contracts in §9 |
| Boot says `FU: selftest creds fetch -> -13` | File Support not enabled on the template, or the device's certificate is not the one registered |
