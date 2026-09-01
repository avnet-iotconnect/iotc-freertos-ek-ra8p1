# Getting Started: Renesas EK-RA8P1 Vision AI with /IOTCONNECT

Purchase the kit: [EK-RA8P1 Evaluation Kit for RA8P1 MCU Group](https://www.renesas.com/en/design-resources/boards-kits/ek-ra8p1)

<img src="images/ek-ra8p1-board.webp" alt="Renesas EK-RA8P1 board" width="420"/>

## Contents

- [1. Introduction](#1-introduction)
- [2. Prerequisites](#2-prerequisites)
- [3. Hardware Setup](#3-hardware-setup)
- [4. Flash the Firmware](#4-flash-the-firmware)
- [5. Create /IOTCONNECT Account](#5-create-iotconnect-account)
- [6. Acquire Account Information](#6-acquire-account-information)
- [7. Device Template Setup](#7-device-template-setup)
- [8. Create a Device](#8-create-a-device)
- [9. Configure the Board](#9-configure-the-board)
- [10. Verify Data](#10-verify-data)
- [11. Import a Dashboard](#11-import-a-dashboard)
- [12. Deploy an AI Model](#12-deploy-an-ai-model)
- [13. Using the Demo](#13-using-the-demo)
- [14. Troubleshooting and Known Issues](#14-troubleshooting-and-known-issues)
- [Resources](#resources)

## 1. Introduction

This guide takes the [Renesas EK-RA8P1](https://www.renesas.com/en/design-resources/boards-kits/ek-ra8p1)
from unboxing to a connected device with a live /IOTCONNECT dashboard, using a **prebuilt
binary** — no toolchain and no source build required.

The board runs camera vision inference on its Ethos-U55 NPU and connects to /IOTCONNECT over
Gigabit Ethernet. Once connected you will deploy an AI model from the cloud, capture an
annotated snapshot, and open a live WebRTC video stream — all on a microcontroller.

To build the same application from source, see the [Developer Guide](DEVELOPER-GUIDE.md).

> [!NOTE]
> This guide has been written and tested with the hardware and software listed below, but may
> work with other environments with some modifications.

## 2. Prerequisites

### Hardware

* [EK-RA8P1 Evaluation Kit](https://www.renesas.com/en/design-resources/boards-kits/ek-ra8p1),
  which includes the board, the OV5640 camera expansion board, the 7-inch LCD, and the cables
* A USB-C cable to the board's **DEBUG1** port (included in the kit)
* An Ethernet cable to a network with DHCP and internet access (included in the kit)
* PC with Windows 10/11

> [!NOTE]
> The 7-inch LCD is **optional**. With it attached you get live video and detection overlays on
> the board. Without it the device runs headless and the /IOTCONNECT dashboard becomes the
> interface — telemetry as the data feed and cloud-triggered snapshots as the viewfinder.

### Software

* [SEGGER J-Link Software](https://www.segger.com/downloads/jlink/) — **V9.38 or later**
* A serial terminal application such as [Tera Term](https://sourceforge.net/projects/tera-term/)
  (Recommended) or a browser-based version such as
  [Google Chrome Labs Serial Terminal](https://googlechromelabs.github.io/serial-terminal/)
* This repository, cloned or downloaded — you will need
  `firmware/iotc-vision-ai-ek-ra8p1-demo.hex`
* An /IOTCONNECT account with an **AWS** backend (see [Step 5](#5-create-iotconnect-account))

> [!IMPORTANT]
> J-Link software earlier than V9.38 does not recognize the RA8P1 and will fail to connect.

## 3. Hardware Setup

1. **Seat** the OV5640 camera expansion board on connector **J35** and latch the flex cable at
   both ends.
2. **Connect** the Ethernet cable from the board to your network.
3. **Connect** the USB-C cable from your PC to the board's **DEBUG1** port. This single port is
   both the on-board J-Link debug probe and the serial console.

The board powers up from the USB-C connection. Once enumerated, a J-Link CDC UART COM port
appears on your PC — note which COM port it is.

Serial terminal settings:

* Port: (Select the COM port with the device)
* Speed: `230400`
* Data: `8 bits`
* Parity: `none`
* Stop Bits: `1`
* Flow Control: `none`

> [!IMPORTANT]
> The console runs at `230400` baud, not the more common `115200`. Set your terminal to send
> **CR** or **CR+LF** line endings, or the board will not accept typed commands.

## 4. Flash the Firmware

We will program the prebuilt demo image into the board's MRAM using J-Flash Lite, which is
installed with the J-Link software. This guide was tested with J-Link software v9.38.

1. **Start** J-Flash Lite.
2. **Set** Device to `R7KA8P1KF_CPU0`, Interface to `SWD`, and Speed to `4000 kHz`.

   > [!CAUTION]
   > Select `R7KA8P1KF_CPU0`. `CPU0` is the Cortex-M85 that runs this application — programming
   > `_CPU1` will not produce a working device.

3. **Click** the `...` button next to Data File and select
   `firmware/iotc-vision-ai-ek-ra8p1-demo.hex` from this repository.
4. **Click** the **Program Device** button. Programming takes about 15 seconds.
5. **Unplug** the USB-C cable and **plug it back in**.

> [!IMPORTANT]
> Power-cycle the board rather than pressing the RESET button, here and any time you restart the
> board. The LCD's timing controller only re-initialises from a cold start, so after a warm reset
> the panel stays white — everything else keeps running, but the display will be blank until the
> next power cycle.

<details>
<summary>Alternative: flashing from the command line with J-Link Commander</summary>

Create a file named `flash.jlink` containing:

```
r                                                  // reset the MCU
h                                                  // halt the core
loadfile firmware/iotc-vision-ai-ek-ra8p1-demo.hex // program MRAM (~10 s)
r                                                  // reset so the new image boots cleanly
g                                                  // go (release the core)
q                                                  // quit, leaving the target running
```

Then run:

```
JLink.exe -device R7KA8P1KF_CPU0 -if SWD -speed 4000 -AutoConnect 1 -CommandFile flash.jlink
```

Power-cycle the board afterwards, for the reason given above.

</details>

**Verify the board is running.** Open your serial terminal with the settings from
[Step 3](#3-hardware-setup). Within a few seconds you should see a repeating processing report:

```
FD: no built-in model in this build and no stored model - push one from IOTCONNECT AI Models
Processing time:
  Camera image capture vsync period :   18 ms,   55 fps
  AI inference time (Ethos-U55)     :    0 us,    0 fps
IOTC: no credentials provisioned - use the serial CLI (type 'help') ...
```

Both messages are expected on a freshly flashed board. The camera is running at 55 fps.
Inference reads zero because this image carries no compiled-in model — its flash budget went to
the live-video stack — and you will deploy one from the cloud in
[Step 12](#12-deploy-an-ai-model). The cloud connection is configured in
[Step 9](#9-configure-the-board).

If the LCD is attached, it now shows the live camera image.

## 5. Create /IOTCONNECT Account

An /IOTCONNECT account with an AWS backend is required.  If you need to create an account, a free trial subscription is available.
The free subscription may be obtained directly from [iotconnect.io](https://iotconnect.io) or through the AWS Marketplace.

* Option #1 **(Recommended)**
/IOTCONNECT via [AWS Marketplace](https://github.com/avnet-iotconnect/avnet-iotconnect.github.io/blob/main/documentation/iotconnect/subscription/iotconnect_aws_marketplace.md) - 60 day trial; AWS account creation required

* Option #2
/IOTCONNECT via [iotconnect.io](https://subscription.iotconnect.io/subscribe?cloud=aws) - 30 day trial; no credit card required

> [!NOTE]
> Be sure to check any SPAM folder for the temporary password after registering.

Login to the platform by navigating to [console.iotconnect.io](https://console.iotconnect.io)

## 6. Acquire Account Information

The Company ID (**CPID**) and Environment (**ENV**) variables identifying your /IOTCONNECT
account must be configured for the device. Take note of these values for later reference
located in the "Settings" -> "Key Vault" section of the platform.

<img src="https://raw.githubusercontent.com/avnet-iotconnect/avnet-iotconnect.github.io/main/documentation/iotconnect/media/key-vault.png" width="600"/>

## 7. Device Template Setup

A device template defines the telemetry attributes and commands this demo uses.

* **Download** the premade device template
  [`ra8p1-vision-ai-template.json`](../templates/ra8p1-vision-ai-template.json?raw=1)
  (**must** Right-Click the link, Save As)
* **Import** the template into your /IOTCONNECT instance following the
  [Importing a Device Template](https://github.com/avnet-iotconnect/avnet-iotconnect.github.io/blob/main/documentation/iotconnect/import_device_template.md)
  guide

The imported template is named **RA8P1 Vision AI** with the template code `ra8p1vis`.

> [!IMPORTANT]
> Import the supplied template rather than creating one by hand. The numeric attributes must be
> of type DECIMAL; a hand-built template is the most common cause of a dashboard showing `null`
> for every value.

## 8. Create a Device

* **Create** a new device following the
  [Create a New Device](https://github.com/avnet-iotconnect/avnet-iotconnect.github.io/blob/main/documentation/iotconnect/create_new_device.md)
  guide, with these values:
  * **Unique ID**: a name of your choosing, such as `ek-ra8p1-01` — you will type this into the
    board in the next step
  * **Entity**: your company's entity (for new accounts, there is only one option)
  * **Template**: `RA8P1 Vision AI (ra8p1vis)`
  * **Device Certificate**: `Auto-generated`
* **Click** `Save & View`
* **Download** the device's certificate package from the device page and unzip it. It contains
  the device certificate and private key PEM files.

> [!CAUTION]
> The template enables video streaming (WebRTC), so the platform provisions a Kinesis Video
> Streams signaling channel **at the moment the device is created**. This cannot be added to an
> existing device later — a device created from a different template will never stream video.

## 9. Configure the Board

We will store the cloud identity on the board over the serial console. The values are written to
the board's OSPI flash and survive power cycles, so this is a one-time step per board.

1. In the serial terminal, **press** Enter, then type `help` to list the provisioning commands.
2. **Enter** your account values, pressing Enter after each line and substituting your own:

   ```
   set env poc
   set cpid <your CPID>
   set duid ek-ra8p1-01
   ```

   `env` and `cpid` come from the Key Vault in [Step 6](#6-acquire-account-information); `duid`
   is the Unique ID from [Step 8](#8-create-a-device).

3. **Type** `set cert`, then **paste** the entire device certificate PEM, including the
   `-----BEGIN CERTIFICATE-----` and `-----END CERTIFICATE-----` lines. Capture ends
   automatically at the END line and the board replies `certificate stored`.
4. **Type** `set key`, then **paste** the private key PEM the same way.
5. **Type** `show` to review what is stored (the key is redacted), then **type** `apply` to
   connect.

> [!NOTE]
> Paste one PEM block per command. Pasting both at once will be rejected as too large.

## 10. Verify Data

Within about 30 seconds of `apply`, the serial console shows:

```
IOTC: starting (env=poc duid=ek-ra8p1-01, credentials: stored)
IOTC: connected
FU: selftest creds fetch -> 0 (OK)
```

Switch back to the /IOTCONNECT browser window and verify the device status is displaying as
`Connected`. Open the device and select the **Live Data** tab to watch telemetry arriving every
10 seconds.

From now on the board connects automatically at every boot — there is no need to repeat
[Step 9](#9-configure-the-board).

## 11. Import a Dashboard

* **Upload** the artwork from [`dashboard/images/`](../dashboard/images/) to your public image
  bucket under `images/renesas/ek-ra8p1/`

  > [!IMPORTANT]
  > The image keys are case-sensitive and must match exactly, or the dashboard's state cards
  > render blank.

* **Download** the premade dashboard
  [`ra8p1-vision-ai-dashboard.json`](../dashboard/ra8p1-vision-ai-dashboard.json?raw=1)
  (**must** Right-Click the link, Save As)
* **Select** `Create Dashboard` from the top of the page
* **Select** the `Import Dashboard` option and select `RA8P1 Vision AI` for **template** and
  `ek-ra8p1-01` for **device**
* **Enter** a name (such as `RA8P1 Vision AI Demo Dashboard`) and complete the import

You will now be in the dashboard edit mode. You can add/remove widgets or just click `Save` in the upper-right corner to exit the edit mode.

![Live dashboard](images/dashboard-live.png)

## 12. Deploy an AI Model

The firmware ships with no compiled-in model, so this step is required before the device will
detect anything. Deploying a model from the cloud is also the headline capability of this demo:
the device swaps models between two inferences, with no reflash and no reboot.

**Register the model** (one-time per model):

* **Navigate** to **AI Models** and **select** `Create Model`
* **Set** Model Type to `AI Model` and Variant to `Renesas`
* **Enter** a name such as `RA8P1 Face Detect` and the code `ra8p1face`

  > [!NOTE]
  > Model codes must be 3–10 characters.

* **Upload** [`tools/models/face-v3_v3.zip`](../tools/models/) from this repository

**Deploy it:**

* From **AI Models**, **deploy** `RA8P1 Face Detect` to your device

Watch the serial console. The download, validation, and hot-swap take a few seconds:

```
IOTC: model downloaded (441248 bytes)
FD: hot-swapping to model "face-v3" v3 (441088 bytes)
FD: model "face-v3" v3 (cloud, 441088 bytes) loaded: face detector, ethos-u: yes
FD: model persisted to OSPI flash store
FD: 1 face(s): [25,63 49x58 90%]
```

Step in front of the camera. The dashboard's Detection State card switches to FACE DETECTED and
the Faces gauge moves; on the LCD, green boxes track the face.

**Re-task the device.** Four more models are bundled in
[`tools/models/`](../tools/models/). Register and deploy them exactly as above to change what
the device does, without a reflash and without a reboot:

| Zip | Suggested name | Code | What the device becomes | Inference time |
|---|---|---|---|---|
| `face-v3_v3.zip` | RA8P1 Face Detect | `ra8p1face` | Face detector with boxes | ~5.8 ms |
| `person-detect_v1.zip` | RA8P1 Person Detect | `ra8p1prsn` | Occupancy sensor — walk in and out of frame | ~1.5 ms |
| `mobilenet-025_v1.zip` | RA8P1 ImageNet Classifier 0.25 | `ra8p1mn025` | 1000-class classifier, speed tier | ~4.5 ms |
| `mobilenet-050_v1.zip` | RA8P1 ImageNet Classifier 0.5 | `ra8p1mn050` | 1000-class classifier, mid tier | ~9 ms |
| `mobilenet-v2_v1.zip` | RA8P1 ImageNet Classifier v2 | `ra8p1mnv2` | 1000-class classifier, accuracy tier — hold up a coffee mug, a banana, a water bottle | ~40 ms |

Deploying `mobilenet-v2` is the most striking of these: it is a roughly 7x larger workload
than the face detector, and the device absorbs it mid-flight. Watch the Uptime tile on the
dashboard while the swap happens — it keeps counting, which is the proof that nothing
rebooted.

> [!NOTE]
> `model-revert` clears the stored model rather than falling back to a built-in one. This
> image has no compiled-in model, so after a revert inference idles until you push another
> model.

**The model survives power loss.** Deployed models are written to the board's OSPI flash.
Remove power from the board and reconnect it — without any reprovisioning, it boots straight
back into the model you pushed and reconnects to the cloud:

```
FD: model "face-v3" v3 (flash, 441088 bytes) loaded: face detector, ethos-u: yes
IOTC: starting (env=poc duid=ek-ra8p1-01, credentials: stored)
IOTC: connected
```

Note `flash` in place of `cloud` — the model came from the board's own storage this time.

For a narrated walkthrough of the full model library, see the [Demo Guide](DEMO-GUIDE.md).

## 13. Using the Demo

Commands are sent from the device's **Commands** panel or from the dashboard's command widget.

**Capture a snapshot.** Send the `snapshot` command. Within about 10 seconds the dashboard's
Latest Snapshot widget shows a color photograph of what the camera saw, with the detection
boxes drawn onto it and tagged with the detection results and performance figures at the
moment of capture. The board annotated the image, PNG-encoded it, and uploaded it to S3 with
an AWS SigV4 signature computed on the microcontroller — there is no gateway or intermediary
in the path. On a headless installation this is the viewfinder.

| Command | Effect |
|---|---|
| `snapshot` | Captures what the camera sees, draws the detection boxes on it, and uploads an annotated color PNG to Telemetry Files (about 10 seconds) |
| `set-interval <seconds>` | Telemetry period; the default is 10 seconds |
| `model-info` | Acknowledges with the active model's name, version, source, and size |
| `model-revert` | Clears the stored model; inference idles until the next model is pushed |
| `set-brightness <0\|1>` | Camera exposure target: `0` is normal, `1` brightens the image by roughly 1 EV for dim rooms |

**Live video.** Open the device and select the **Video Streaming** tab, then click
**Start Video**. Within a few seconds the browser negotiates a WebRTC session with the board and
the camera's live view appears — H.264 encoded in software on the Cortex-M85 at 320x240, roughly
8–10 frames per second. Inference and telemetry keep running while the stream is live.

> [!NOTE]
> One viewer is supported at a time. If the first session after a boot stays black, click Stop
> and Start once more and allow about 15 seconds for the connection to be established.

The same commands are also available on the serial console, along with `show`, `erase`, and
`reboot`. Type `help` to list them.

**Clearing a board.** The cloud identity is stored on the board, so a board that is passed on
to someone else keeps working as your device. To remove it, type `erase` followed by `reboot`
in the serial console. The board returns to the unprovisioned state from
[Step 4](#4-flash-the-firmware), ready to be configured for a different account.

## 14. Troubleshooting and Known Issues

| Symptom | Fix |
|---|---|
| J-Link cannot find the device | Update the J-Link software to V9.38 or later, and select `R7KA8P1KF_CPU0`, not `_CPU1` |
| No serial output | Select the J-Link CDC UART COM port, and check the speed is `230400`, not `115200` |
| Typed characters are not accepted | Set the terminal to send CR or CR+LF line endings |
| A PEM paste is rejected as too large | Paste one PEM block per command — the certificate and the key separately |
| The LCD is all white after flashing or a debugger reset | Expected. Power-cycle the board; the panel needs a cold start and a warm reset is not enough. Everything else keeps running |
| No camera image | Re-seat the OV5640 camera board on J35; the flex cable must be fully latched at both ends |
| The LCD stays blank | The LCD is optional and the demo runs headless; if it is attached, check both flat cables |
| The device connects but the dashboard shows `null` | Import the supplied template rather than creating one by hand — numeric attributes must be DECIMAL |
| Dashboard state cards are blank | The artwork is missing from the image bucket, or the key case does not match |
| Inference time stays at `0 us` | No model is loaded — deploy one as described in [Step 12](#12-deploy-an-ai-model) |
| A model push never arrives | Confirm the deployment was dispatched in AI Models; the device logs `MQTT: C2D message` the moment one arrives |
| A snapshot acknowledgment reports upload failed | Send the command again; confirm the boot log printed `FU: file upload ready` |
| Classifier labels look wrong | Hold a single recognizable object centered and close to the camera; ImageNet classifiers are trained on single objects, not scenes |
| The Video Streaming tab stays black | Click Stop, then Start again, and allow about 15 seconds. The device must have been created from the supplied template |

## Resources

* [Developer Guide](DEVELOPER-GUIDE.md) — build from source, the architecture, and adding your
  own Vela-compiled models
* [Demo Guide](DEMO-GUIDE.md) — a presenter's script for demonstrating the full model hot-swap
* [Purchase the EK-RA8P1 Evaluation Kit](https://www.renesas.com/en/design-resources/boards-kits/ek-ra8p1)
* [/IOTCONNECT Overview](https://www.iotconnect.io/)
* [/IOTCONNECT Knowledgebase](https://help.iotconnect.io/)

## Revision Info

![GitHub last commit](https://img.shields.io/github/last-commit/avnet-iotconnect/iotc-freertos-ek-ra8p1?label=Last%20Commit)
- View changes to this repository: [Commit History](https://github.com/avnet-iotconnect/iotc-freertos-ek-ra8p1/commits/main)
- View changes to this document: [QUICKSTART.md](https://github.com/avnet-iotconnect/iotc-freertos-ek-ra8p1/commits/main/docs/QUICKSTART.md)
