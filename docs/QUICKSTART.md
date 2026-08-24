# Quickstart — flash a prebuilt image and connect to /IOTCONNECT

Get the EK-RA8P1 running the vision pipeline in about five minutes, then connect it to your
/IOTCONNECT account by typing credentials into a serial terminal — **no toolchain and no
source build required**. Credentials are stored in the board's OSPI flash and survive power
cycles.

## Contents

- [1. What you need](#1-what-you-need)
- [2. Flash the prebuilt image](#2-flash-the-prebuilt-image)
- [3. Verify the vision pipeline](#3-verify-the-vision-pipeline)
- [4. Create the device in /IOTCONNECT](#4-create-the-device-in-iotconnect)
- [5. Provision credentials over the serial terminal](#5-provision-credentials-over-the-serial-terminal)
- [6. Watch the live video stream](#6-watch-the-live-video-stream)
- [7. Next steps](#7-next-steps)
- [Troubleshooting](#troubleshooting)

## 1. What you need

<img src="images/ek-ra8p1-board.webp" alt="Renesas EK-RA8P1 board" width="320"/>

*The EK-RA8P1 board.*

- EK-RA8P1 kit with the OV5640 camera board fitted (J35 / MIPI connector)
- Ethernet cable to a network with DHCP and internet access (the board is wired Gigabit
  Ethernet — there are no network credentials to configure)
- USB-C cable to the **DEBUG1** port (this is the on-board J-Link debug probe and the
  serial console)
- [SEGGER J-Link Software](https://www.segger.com/downloads/jlink/) **V9.38 or later**
  (earlier versions do not support the RA8P1)
- A serial terminal program (Tera Term, PuTTY, or similar)
- A /IOTCONNECT account on the **AWS** backend
- The bundled 7" LCD, **optional**: with it attached you see live video and overlays;
  without it the device runs headless, with the /IOTCONNECT dashboard as the interface —
  telemetry as the data feed and cloud-triggered snapshot capture as the viewfinder

## 2. Flash the prebuilt image

### Option A — J-Flash Lite (GUI, easiest)

1. Start **J-Flash Lite** (installed with the J-Link software).
2. Device: **R7KA8P1KF_CPU0** · Interface: **SWD** · Speed: 4000 kHz.
3. Data File: `firmware/iotc-vision-ai-ek-ra8p1-demo.hex`
4. **Program Device**. When it finishes, press the board's RESET button.

### Option B — J-Link Commander (CLI)

Create `flash.jlink` with the following J-Link Commander script (one command per line):

```
r                                                  // reset the MCU
h                                                  // halt the core
loadfile firmware/iotc-vision-ai-ek-ra8p1-demo.hex // program MRAM (~10 s)
r                                                  // reset again so the new image boots cleanly
g                                                  // go (release the core)
q                                                  // quit, leaving the target running
```

then:

```
JLink.exe -device R7KA8P1KF_CPU0 -if SWD -speed 4000 -AutoConnect 1 -CommandFile flash.jlink
```

## 3. Verify the vision pipeline

Open the serial terminal on the J-Link CDC COM port at **230400 baud**, 8N1. Within a few
seconds of reset you should see face-detection output and the periodic processing report:

```
FD: model "builtin" v1 (builtin, 441088 bytes) loaded: face detector, ethos-u: yes
FD: 1 face(s): [25,63 49x58 90%]
Processing time:
  Camera image capture vsync period :   18 ms,   55 fps
  AI inference time (Ethos-U55)     : 5800 us,  172 fps
```

If the LCD is attached it shows the live camera image with detection boxes and the
performance panel. Until credentials are provisioned, the console also prints:

```
IOTC: no credentials provisioned - use the serial CLI (type 'help') ...
```

## 4. Create the device in /IOTCONNECT

In your /IOTCONNECT account (AWS backend):

1. **Device → Templates → Import** and import
   [`templates/ra8p1-vision-ai-template.json`](../templates/ra8p1-vision-ai-template.json)
   from this repository.
2. **Device → Create Device** using that template, auth type **X.509**
   ("Auto-generated certificate" is easiest). Note the **Unique ID** you choose.
   The template enables **video streaming (WebRTC)**, so the platform provisions a
   Kinesis Video Streams signaling channel for the device at creation time — this
   cannot be added to an existing device later.
3. Download the device's certificate package (contains the certificate and private key PEM
   files).
4. Find your **CPID** and **Environment** under **Settings → Key Vault** (they are also in
   the downloadable `iotcDeviceConfig.json`).

## 5. Provision credentials over the serial terminal

In the serial terminal, press Enter, then type `help` to see the provisioning CLI. Enter
your values (press Enter after each command):

```
set env poc
set cpid <your CPID>
set duid <your device Unique ID>
```

Now store the certificate. Type:

```
set cert
```

then **paste the entire device certificate PEM** (from `-----BEGIN CERTIFICATE-----` to
`-----END CERTIFICATE-----`) into the terminal. Capture ends automatically at the END line
and the device replies `certificate stored`. Repeat for the key:

```
set key
```

and paste the private key PEM. Check the result and connect:

```
show
apply
```

Within roughly 30 seconds the console shows:

```
IOTC: starting (env=poc duid=<your id>, credentials: stored)
IOTC: connected
FU: selftest creds fetch -> 0 (OK)
```

and the device appears **connected** on the /IOTCONNECT dashboard with telemetry arriving
every 10 seconds. The stored credentials persist across power cycles — from now on the
device connects automatically at boot. Useful maintenance commands: `show` (review, key
redacted), `erase` (remove stored credentials), `reboot`.

## 6. Watch the live video stream

Once the device is connected, open the device in /IOTCONNECT and select the
**Video Streaming** tab. Click **Start Video**: within a few seconds the browser
negotiates a WebRTC session with the board and the camera's live view appears —
H.264 encoded in software on the Cortex-M85 at QVGA (320×240), roughly 8–10 frames
per second.

On the serial console a session start looks like:

```
[KVS] connecting to signaling channel '<your device id>' (us-east-1)...
[KVSMedia] streaming ON
```

Notes:

- The video stream and the vision pipeline share the camera; inference and telemetry
  continue while streaming.
- One viewer at a time is supported.
- The stream stops when the tab is closed or **Stop Video** is clicked; the device
  stays connected to the signaling channel and the next viewer can connect at any time.

## 7. Next steps

- Run the full demonstration — snapshots to Telemetry Files and over-the-air model
  hot-swap with the five bundled models: [Demo Guide](DEMO-GUIDE.md).
- Build from source, explore the architecture, or add your own models:
  [Developer Guide](DEVELOPER-GUIDE.md).

## Troubleshooting

| Symptom | Fix |
|---|---|
| J-Link cannot find the device | Update J-Link software to V9.38+; select `R7KA8P1KF_CPU0` (CPU0 = the Cortex-M85), not `_CPU1` |
| No serial output | Pick the J-Link CDC UART COM port; the baud rate is **230400**, not 115200 |
| Typed characters not accepted | Ensure the terminal sends CR or CR+LF line endings |
| PEM paste rejected as too large | Paste only one PEM block per command (certificate and key separately) |
| `apply` connects but dashboard shows nulls | Template attribute types — import the bundled template rather than creating one manually |
| No camera image | Re-seat the OV5640 camera board on J35; the flex cable must be fully latched |
| Blank LCD | The LCD is optional; if attached, check both flat cables |
