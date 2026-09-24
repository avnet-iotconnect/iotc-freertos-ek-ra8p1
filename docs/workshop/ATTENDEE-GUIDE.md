# Attendee Guide: Push AI Models to a Microcontroller from the Cloud

**Workshop:** Vision AI on the Renesas EK-RA8P1 with /IOTCONNECT AI Model Management
**Length:** 2 hours 30 minutes · six labs · one board per attendee

You will connect a Renesas EK-RA8P1 to /IOTCONNECT over Ethernet, then push three different
AI models to it from the cloud and watch the board change task in seconds, with no reflash and
no reboot. By the end, your board runs a model you chose, stored in its own flash, and it comes
back with that model after a power cycle.

Work through the labs in order. Each lab ends with a **Checkpoint**: do not move on until you
have seen it. Raise a hand (or your seat card) when you are stuck; a helper will come to you.

## Contents

- [Your seat card](#your-seat-card)
- [Lab 1: Meet your board (10 min)](#lab-1-meet-your-board-10-min)
- [Lab 2: Claim your device in /IOTCONNECT (10 min)](#lab-2-claim-your-device-in-iotconnect-10-min)
- [Lab 3: Provision the board (15 min)](#lab-3-provision-the-board-15-min)
- [Lab 4: Dashboard, first model push, first snapshot (20 min)](#lab-4-dashboard-first-model-push-first-snapshot-20-min)
- [Lab 5: Re-task the device from the cloud (30 min)](#lab-5-re-task-the-device-from-the-cloud-30-min)
- [Lab 6: Live video, and a push mid-stream (10 min)](#lab-6-live-video-and-a-push-mid-stream-10-min)
- [Before you leave](#before-you-leave)
- [Troubleshooting](#troubleshooting)
- [Going further](#going-further)

## Your seat card

Everything specific to you is on the card at your seat. You will type these values in Lab 3.

| Field | What it is | Used in |
|---|---|---|
| **Seat** | Your seat number, `01` to `30` | Everywhere |
| **Board MAC** | The Ethernet address flashed into your board, e.g. `02:8a:9b:57:53:07` | Lab 1 (to confirm you have the right board) |
| **Device ID** | The Unique ID for your device, e.g. `ek-ra8p1-07` | Labs 2 and 3 |
| **Login** | Your /IOTCONNECT sign-in for the workshop account | Lab 2 |
| **ENV** and **CPID** | The workshop account's environment and Company ID | Lab 3 |

Two rules for the whole session:

1. **Power-cycle, never RESET.** If you need to restart the board, unplug the USB-C cable and
   plug it back in. The LCD's timing controller only initialises from a cold start, so after the
   RESET button the panel stays white until the next power cycle. The rest of the board keeps
   running either way.
2. **Paste one PEM block per command.** Your certificate and private key are pasted separately
   in Lab 3. Pasting both at once is rejected.

## Lab 1: Meet your board (10 min)

The board is already flashed with the workshop firmware. In this lab you bring it up and confirm
it is on the network.

1. **Check the hardware.** The OV5640 camera board sits on connector **J35** with its flex cable
   latched at both ends. If the 7-inch LCD is attached, both of its flat cables are seated. The
   LCD is optional; the workshop runs headless too.
2. **Plug in Ethernet** from the wall or switch port at your seat to the board's RJ45 jack.
3. **Plug in USB-C** from your laptop to the board's **DEBUG1** port. This one cable powers the
   board, and carries the serial console. A new COM port (Windows) or `/dev/tty` device
   (macOS, Linux) appears on your laptop.
4. **Open a serial terminal** on that port:

   | Setting | Value |
   |---|---|
   | Speed | `230400` (not 115200) |
   | Data / parity / stop | 8 / none / 1 |
   | Flow control | none |
   | Line ending (transmit) | **CR** or **CR+LF** |

   Tera Term on Windows: *Setup → Serial port* for the speed, *Setup → Terminal → New-line
   Transmit: CR*. macOS or Linux: `screen /dev/tty.usbmodem* 230400`.

5. **Press Enter, then type** `help`. The board answers with its provisioning commands.
6. **Find the network lines** in the scrolling output and compare the MAC with your seat card:

   ```
   Network up (DHCP):
     MAC     : 02:8a:9b:57:53:07
     IPv4    : 192.168.10.57
   ```

**Checkpoint.** The console repeats a processing report every few seconds, the MAC matches your
seat card, and an IPv4 address was assigned:

```
FD: no built-in model in this build and no stored model - push one from IOTCONNECT AI Models
Processing time:
  Camera image capture vsync period :   18 ms,   55 fps
  AI inference time (Ethos-U55)     :    0 us,    0 fps
IOTC: no credentials provisioned - use the serial CLI (type 'help') ...
```

All of this is expected. The camera runs at 55 fps. Inference reads zero because this image
carries no compiled-in model: you will push one from the cloud in Lab 4. The cloud connection
comes in Lab 3. If the LCD is attached it shows the live camera view.

> If the MAC on your console does not match your seat card, tell a helper before going on. Two
> boards with the same MAC on one network will fight each other.

## Lab 2: Claim your device in /IOTCONNECT (10 min)

In this lab you create the cloud identity your board will use.

1. **Sign in** at [console.iotconnect.io](https://console.iotconnect.io) with the login on your
   seat card.
2. **Create the device.** Go to *Devices → Create Device* and fill in:

   | Field | Value |
   |---|---|
   | Unique ID | The **Device ID** on your seat card, e.g. `ek-ra8p1-07` |
   | Entity | The workshop entity (there is only one) |
   | Template | `RA8P1 Vision AI (ra8p1vis)` |
   | Device Certificate | `Auto-generated` |

   Click **Save & View**.

   > Use the `RA8P1 Vision AI` template exactly. The template enables video streaming, and the
   > platform provisions the video channel at the moment the device is created. A device made
   > from any other template will never stream video in Lab 6.

3. **Download the certificate package** from the device page and unzip it. It contains two
   files: the device certificate and the private key, both PEM text files.
4. **Open both files in a plain-text editor** (Notepad, TextEdit in plain-text mode, VS Code).
   Do not use Word: it changes the quotes and line endings and the board will reject the paste.

**Checkpoint.** Your device is in the device list with status *Disconnected*, and you have the
certificate and key open in a text editor.

## Lab 3: Provision the board (15 min)

In this lab you store the cloud identity on the board. It is written to the board's OSPI flash,
so it survives power cycles: this is a one-time step.

1. In the serial terminal, **enter the three identity values**, pressing Enter after each. Take
   `ENV` and `CPID` from your seat card and use your own Device ID:

   ```
   set env <ENV>
   set cpid <CPID>
   set duid ek-ra8p1-07
   ```

   The board replies `ok` to each.

2. **Type** `set cert` and press Enter, then **paste the entire certificate**, from the
   `-----BEGIN CERTIFICATE-----` line to the `-----END CERTIFICATE-----` line inclusive. In Tera
   Term paste with *Edit → Paste* or a right-click. Capture ends automatically at the END line
   and the board replies `certificate stored`.
3. **Type** `set key` and press Enter, then **paste the private key** the same way. The board
   replies `private key stored`.
4. **Type** `show` to review what is stored (the key is redacted), then **type** `apply`.

**Checkpoint.** Within about 30 seconds the console shows, in this order:

```
IOTC: starting (env=<ENV> duid=ek-ra8p1-07, credentials: stored)
IOTC: time synced (...)
FU: file upload ready (bucket ...)
IOTC: connected
FU: selftest creds fetch -> 0 (OK)
```

In the browser, your device's status changes to **Connected**. Open the device and select the
**Live Data** tab: telemetry arrives every 10 seconds.

From now on the board connects by itself at every boot. You will not repeat this lab.

## Lab 4: Dashboard, first model push, first snapshot (20 min)

In this lab you bring up your dashboard, push your first model, and take a photo from the cloud.

### 4a. Import the dashboard (5 min)

1. Get `ra8p1-vision-ai-dashboard.json` from the location your facilitator gave you (it is also
   in this repository under [`dashboard/`](../../dashboard/)).
2. In /IOTCONNECT select **Create Dashboard** at the top of the page, then **Import Dashboard**.
3. Choose the file, select template **RA8P1 Vision AI** and your device (`ek-ra8p1-07`), and
   name it `Seat 07 - RA8P1 Vision AI`. Complete the import, then click **Save** in the upper
   right to leave edit mode.

**Checkpoint.** The dashboard renders with live numbers: Camera FPS reads 55 and Uptime counts
up. Active Model and the inference figures are empty or zero because no model is loaded yet.

### 4b. Push your first model (10 min)

The models are already registered in the workshop account under **AI Models**.

1. Go to **AI Models**, open **RA8P1 Face Detect**, and choose **Deploy**.
2. Select your device (`ek-ra8p1-07`) and dispatch the deployment. Deploy to your own device
   only.
3. **Watch the serial console.** The download, validation, and hot-swap take a few seconds:

   ```
   MQTT: C2D message (... bytes)
   IOTC: model push from https://...
   IOTC: model downloaded (441248 bytes)
   FD: hot-swapping to model "face-v3" v3 (441088 bytes)
   FD: model "face-v3" v3 (cloud, 441088 bytes) loaded: face detector, ethos-u: yes
   FD: model persisted to OSPI flash store
   FD: 1 face(s): [25,63 49x58 90%]
   ```

4. **Step in front of the camera.** On the dashboard the Detection State card switches to
   FACE DETECTED and the Faces gauge moves. On the LCD, green boxes track your face.

**Checkpoint.** The Model Source card shows a cloud-delivered model, Active Model reads `face-v3`,
and NPU Inference Time is about 5,800 µs. The board never rebooted: Uptime kept counting.

### 4c. Take a snapshot from the cloud (5 min)

1. On the dashboard press **Take Snapshot** (or send the `snapshot` command from the device's
   Commands panel).
2. Within about 10 seconds the **Latest Snapshot** widget shows a colour photo of what the camera
   saw, with the detection boxes drawn on it and the detection results and timing figures at the
   moment of capture.

The board annotated the frame, PNG-encoded it, and uploaded it to S3 with an AWS SigV4 signature
computed on the microcontroller. There is no gateway in the path. On a headless installation,
this is the viewfinder.

**Checkpoint.** A snapshot with a green box around your face is on your dashboard.

**Break.** Leave the board powered and connected.

## Lab 5: Re-task the device from the cloud (30 min)

This is the main event. The same board becomes a 1000-class image classifier, then an occupancy
sensor, then a face detector again, purely by pushing models. Keep the serial console and the
dashboard both visible.

### 5a. Push the big classifier

1. **AI Models → RA8P1 ImageNet Classifier v2 → Deploy** to your device.
2. Watch the console: this file is 3.1 MB, about seven times the face detector.

   ```
   IOTC: model downloaded (3152538 bytes)
   FD: hot-swapping to model "mobilenet-v2" v1 (3152368 bytes)
   FD: model "mobilenet-v2" v1 (cloud, 3152368 bytes) loaded: classifier, ethos-u: yes
   FD: model persisted to OSPI flash store
   ```

3. **Hold up a single object**, centred and close to the camera: a coffee mug, a banana, a water
   bottle, a keyboard, your phone. The Detection / Class tile shows the top ImageNet label and
   the state card switches to CLASSIFYING.

**What to watch.**

- The **NPU Inference Time** chart steps from about 5,800 µs to about 40,000 µs. A roughly
  seven-times larger workload was absorbed mid-flight.
- The **Uptime** tile keeps counting. Nothing rebooted.
- Confidence of 30 to 60 percent is normal for a softmax over 1,000 classes. The label being
  right is the point.

### 5b. Push the occupancy sensor

1. **AI Models → RA8P1 Person Detect → Deploy** to your device.
2. Walk out of the frame and back in.

**Checkpoint.** The state card flips between PERSON PRESENT and NO PERSON, and inference time
drops to about 1,500 µs. Three tasks, one board, three pushes.

### 5c. Ask the device what it is running

On the dashboard's **Send Command** widget choose **Model Info** and execute it. The
acknowledgment in the command history lists the active model's name, version, source, and size.

### 5d. Prove the model survives power loss

1. **Unplug the USB-C cable**, wait three seconds, and plug it back in.
2. Watch the boot log. With no reprovisioning, the board loads the model it was running from its
   own flash and reconnects:

   ```
   FD: model "person-detect" v1 (flash, 240272 bytes) loaded: classifier, ethos-u: yes
   IOTC: starting (env=<ENV> duid=ek-ra8p1-07, credentials: stored)
   IOTC: connected
   ```

**Checkpoint.** The word `flash` appears where `cloud` was before, and the dashboard's Model
Source card changes to show the model came from flash. Model rollout that survives a power cut,
on a microcontroller.

### 5e. Finish on the face detector

Push **RA8P1 Face Detect** once more. The green boxes return.

Optional, if you have time: push **RA8P1 ImageNet Classifier 0.25** and **0.5** and compare the
inference time chart across all five models. The library spans 1.5 ms to 40 ms.

> `Model Revert` on the dashboard clears the stored model rather than restoring a built-in one.
> This image has no compiled-in model, so after a revert inference idles until your next push.
> That is fine; it just means one more push to get boxes back.

## Lab 6: Live video, and a push mid-stream (10 min)

This lab needs UDP traffic to leave the room network. If your facilitator says video is not
available on this network, skip to [Before you leave](#before-you-leave).

1. Open your device in /IOTCONNECT and select the **Video Streaming** tab.
2. Click **Start Video** and allow about 15 seconds. The camera's live view appears in the
   browser: 320×240 at roughly 8 to 10 frames per second. If the first attempt after a boot stays
   black, click **Stop**, then **Start** again.
3. Wave at the camera. A second or two of latency through the relay is normal.
4. **With the video still playing, push a different model** from AI Models. The download,
   validation, and hot-swap happen while the stream continues, and the device changes task on
   screen.
5. Click **Stop Video**.

There is no video hardware on this chip. H.264 is encoded in software on the Cortex-M85 while
the NPU keeps running inference on the same frames, telemetry keeps flowing, and a second TLS
session downloads the model. One viewer per device is supported.

**Checkpoint.** You saw the live stream, the console printed `[KVSMedia] streaming ON`, and the
model changed while the video played.

## Before you leave

Ask your facilitator which applies to you.

- **The board stays with the workshop.** Type `erase` in the serial console, then `reboot`. The
  board forgets its identity and returns to the unprovisioned state. Your pushed model stays in
  flash; that is harmless.
- **The board goes home with you.** Leave it provisioned for now. At home, follow the
  [Quickstart](../QUICKSTART.md) to create your own /IOTCONNECT trial account, then `erase`,
  `reboot`, and provision the board against your own account. The workshop account will be
  closed after the event.

## Troubleshooting

| Symptom | Fix |
|---|---|
| No serial output | Pick the J-Link CDC UART port, not another COM port, and check the speed is `230400` |
| Typed characters are ignored | Set the terminal to transmit CR or CR+LF line endings |
| The MAC does not match the seat card | Tell a helper; you may have the wrong board |
| No IPv4 line, or `dhcp=0` keeps printing | Re-seat the Ethernet cable, then power-cycle. Still nothing: tell a helper (port or network problem) |
| A PEM paste is rejected as too large | You pasted both files at once. Paste the certificate under `set cert` and the key under `set key`, separately |
| `certificate stored` never appears | The paste stopped before the END line, or an editor mangled the text. Re-open the file in a plain-text editor and paste again |
| `IOTC: connected` never appears | Run `show` and compare ENV, CPID, and DUID with your seat card, character by character. Then `apply` again |
| Device shows *Connected* but the dashboard shows `null` | The dashboard is bound to the wrong device, or the template was hand-made. Re-import with your device selected |
| Dashboard state cards are blank | Refresh the page. Still blank: tell a helper (artwork missing from the image bucket) |
| The model push never arrives | Check the deployment was dispatched to *your* device in AI Models. The console prints `MQTT: C2D message` the moment one arrives |
| `Inference time` stays at `0 us` | No model is loaded. Push one from AI Models |
| The LCD is white | Expected after a warm reset. Power-cycle the board |
| No camera image | Re-seat the camera board on J35 and check the flex cable at both ends |
| Classifier labels look wrong | Hold one object, centred, close to the lens. ImageNet knows 1,000 specific objects, not scenes |
| Snapshot ack says upload failed | Send it again; transient. The boot log should have printed `FU: file upload ready` |
| Video tab stays black | Stop, then Start, and wait 15 seconds. Still black: the room network may block UDP; skip Lab 6 |

## Going further

- [Quickstart](../QUICKSTART.md): repeat today's flow at home on your own account, from
  unboxing, with the prebuilt image.
- [Developer Guide](../DEVELOPER-GUIDE.md): build from source, the architecture, and how to add
  your own Vela-compiled model with `tools/pack_model.py`.
- [Demo Guide](../DEMO-GUIDE.md): a ten-minute presenter's script for showing this to others.
- [EK-RA8P1 Evaluation Kit](https://www.renesas.com/en/design-resources/boards-kits/ek-ra8p1)
  and the [/IOTCONNECT free trial](https://subscription.iotconnect.io/subscribe?cloud=aws).
