# Presentation: Push AI Models to a Microcontroller from the Cloud

The workshop deck, slide by slide, with speaker notes. This is the version-controlled source of
the deck's content; the deck itself is a 24-slide, 16:9 presentation built from it. Slide
numbers here match the run of show in the [Facilitator Guide](FACILITATOR-GUIDE.md#9-run-of-show).

Three talks, interleaved with the labs:

| Talk | Slides | When | Minutes |
|---|---|---|---|
| 1. Why edge AI on an MCU, the board, the cloud side | 1 – 9 | Opening | 15 |
| 2. Anatomy of a model push, the model library | 16 – 18 | After the break | 10 |
| 3. Bring your own model, where to go next | 22 – 24 | Closing | 10 |

Lab slides (10 – 13, 19, 21) stay on screen while the room works. Slides 14, 15 and 20 are
transitions.

Look and feel: dark navy `#111C2E` and warm off-white `#F5F6F3` backgrounds, teal `#2BB3A0`
and amber `#E9A23B` accents, IBM Plex Sans for text and JetBrains Mono for console output.
Images: the board photo and the live dashboard screenshot from [`docs/images/`](../images/).

---

## 1. Cover

**Push an AI model to a microcontroller. From the cloud. In seconds.**
Vision AI on the Renesas EK-RA8P1 with /IOTCONNECT AI Model Management. Hands-on workshop,
2 hours 30 minutes. Board photo.

> Notes. While people settle: everyone should have a board, an LCD, a USB-C cable, an Ethernet
> cable and a printed seat card. The seat number on the card must match the sticker on the
> board. Wi-Fi details and the two URLs are on the whiteboard. Today is hands-on: about 40
> minutes of talking, the rest is you and your board.

## 2. What you will do today

Three cards: **Connect** (provision a board over its serial console and watch it appear in
/IOTCONNECT), **Push** (deploy three different AI models from the cloud), **Prove** (uptime keeps
counting through every swap; pull the power and the model comes back from flash). A strip
showing the day: Talk 15 min, Labs 1 to 4 55 min, Break, Talk, Labs 5 and 6 40 min, Talk.

> Notes. Three outcomes, in order. The first push happens before the break, so by the time we
> get coffee everyone has already done the headline thing.

## 3. What is on your desk

Four cards: the board (camera on J35, LCD attached, already flashed, seat sticker), two cables
(USB-C to DEBUG1 is power and console; Ethernet to the drop), the seat card (seat, MAC, Device
ID, login, ENV, CPID), your laptop (serial terminal at 230400, browser, plain-text editor).
Banner: **power-cycle, never RESET**; **paste the certificate and the key separately**.

> Notes. The USB-C cable does double duty through the on-board J-Link. The seat card is the
> source of truth for everything typed today, and the MAC on it exists because every board was
> flashed with its own address. The LCD's timing controller only initialises from a cold start,
> so a warm reset leaves the screen white while everything else keeps running.

## 4. Why run vision AI on a microcontroller

Three columns: **No Linux, no MPU** (boots in milliseconds, deterministic, no OS to patch);
**Milliwatts and dollars** (always-on sensing at a BOM and power budget an application processor
cannot reach; face detection in under 6 ms); **But models change** (highlighted: the detector
you ship today is retrained next quarter; updating it must not mean a truck roll, a debugger, or
a firmware release). Closing line: how do you ship a new model to a fleet of these, and know it
took?

> Notes. The third card is why this workshop exists. A model is not firmware: it changes on a
> different cadence, owned by a different team, and it is a data file. If updating it means a
> firmware release, the model never gets updated.

## 5. The board: Renesas EK-RA8P1

Board photo and a spec table: RA8P1 with Cortex-M85 at 1 GHz; Ethos-U55 with 256 MACs per
cycle; 1 MB MRAM, 2 MB SRAM, 64 MB SDRAM; 64 MB OSPI flash for credentials and the model slot;
Gigabit Ethernet; OV5640 over MIPI CSI-2 at 55 fps; 7-inch LCD, optional; on-board J-Link, with
DEBUG1 as the console.

> Notes. One megabyte of code flash is why this image has no built-in model (the budget went to
> the video stack) and the first model you run comes from the cloud. Sixty-four megabytes of
> external flash is where credentials and the pushed model live, and why both survive a power
> cycle.

## 6. What the firmware does

Top row, the vision pipeline: Camera (55 fps into SDRAM) → Preprocess (to the model's input
shape) → NPU inference (TensorFlow Lite Micro on the Ethos-U55; hot-swappable) → Display
(boxes over the live view at 29 fps). Middle row, the network thread: Telemetry every 10 s over
MQTT with mutual TLS; Commands; **Model push** (highlighted: HTTPS download, validate, hot-swap,
persist); Snapshot (annotated PNG, SigV4-signed upload to S3); Video (software H.264 over
WebRTC). Bottom line: OSPI flash holds a LittleFS volume for the identity and a raw 8 MB slot
for the active model, both read at boot.

> Notes. Four FreeRTOS threads. The vision pipeline runs whether or not the cloud is
> reachable. The highlighted card is today's subject.

## 7. /IOTCONNECT in one slide

Six cards: **Template** (attributes, commands, file upload, video; ours is RA8P1 Vision AI),
**Device** (Unique ID plus X.509 certificate and key; mutual TLS, no passwords), **Telemetry and
commands** (MQTT over TLS on 8883, each command acknowledged), **Dashboard** (widgets bound to
one device), **AI Models** (highlighted: register once, version, deploy to any device on a
compatible template), **Telemetry Files** (device uploads such as the snapshot; the board signs
the S3 upload itself).

> Notes. Vocabulary for the labs. Template is already imported. Device is Lab 2, and its
> certificate is what you paste in Lab 3. AI Models is the point of the day: the five models are
> registered; you deploy them. The template turns on video streaming, provisioned when the device
> is created, so use exactly this template.

## 8. The headline capability (statement slide, teal)

**Click Deploy in AI Models. The board downloads the model, validates it, swaps it in between
two inferences, and saves it to flash.** No reflash. No reboot. Uptime uninterrupted. It comes
back after a power cut.

> Notes. Say it once, plainly, then do it for real in Lab 4. Checkable on three widgets: Uptime,
> Model Source, and the inference-time chart.

## 9. The lab flow

Table: Lab, name, what you do, checkpoint, minutes.

| Lab | Name | What you do | Checkpoint | Min |
|---|---|---|---|---|
| 1 | Meet your board | Console at 230400, confirm MAC and IPv4 | Camera at 55 fps, network up | 10 |
| 2 | Claim your device | Create a device from the RA8P1 Vision AI template | Certificate and key downloaded | 10 |
| 3 | Provision the board | set env, cpid, duid; paste cert and key; apply | `IOTC: connected` | 15 |
| 4 | Dashboard, first push, snapshot | Import the dashboard, deploy Face Detect, Take Snapshot | A snapshot with a box on it | 20 |
| 5 | Re-task the device | Classifier, person detector, model-info, power-cycle | Model reloads from flash | 30 |
| 6 | Live video | Start Video, then push a model mid-stream | Stream stays up through the swap | 10 |

> Notes. Leave this slide up while people start Lab 1. The gates are real: Lab 1 catches every
> driver, cable and network problem early. Helpers: the checkpoint column is what you look for on
> each screen as you sweep.

## 10. Lab 1: Meet your board (10 min)

Steps: camera on J35 and LCD cables seated; Ethernet and USB-C to DEBUG1; terminal at 230400,
8N1, line ending CR; Enter, then `help`; find the MAC line and compare with the seat card.
Console panel:

```
Network up (DHCP):
  MAC     : 02:8a:9b:57:53:07
  IPv4    : 192.168.10.57
Processing time:
  Camera image capture vsync period :   18 ms,   55 fps
  AI inference time (Ethos-U55)     :    0 us,    0 fps
IOTC: no credentials provisioned - use the serial CLI (type 'help') ...
FD: no built-in model in this build and no stored model - push one from IOTCONNECT AI Models
```

Checkpoint: MAC matches, IPv4 assigned, camera at 55 fps; inference at zero is expected.

> Notes. 230400 not 115200, and the terminal must transmit CR. Tera Term: Setup, Terminal,
> New-line Transmit: CR. Mac and Linux: `screen` on the usbmodem device. The MAC check matters:
> two boards with the same MAC on one switch fight each other; if console and card disagree,
> get a helper.

## 11. Lab 2: Claim your device in /IOTCONNECT (10 min)

Steps: sign in with the seat-card login; Devices, Create Device; Save & View; download the
certificate package and unzip it; open both PEM files in a plain-text editor. Field table:
Unique ID = your Device ID; Entity = the workshop entity; Template = RA8P1 Vision AI
(ra8p1vis); Device Certificate = Auto-generated. Checkpoint: device listed as Disconnected,
certificate and key open in a text editor. Not Word.

> Notes. The template must be exactly RA8P1 Vision AI, because the video channel is provisioned
> at creation. Word and rich-text editors break the PEM paste.

## 12. Lab 3: Provision the board (15 min)

Left panel, what you type:

```
set env <ENV>
set cpid <CPID>
set duid ek-ra8p1-07
set cert      (then paste the certificate)
set key       (then paste the private key)
show
apply
```

Right panel, what the board prints within 30 seconds:

```
IOTC: starting (env=... duid=ek-ra8p1-07, credentials: stored)
IOTC: time synced
FU: file upload ready
IOTC: connected
FU: selftest creds fetch -> 0 (OK)
```

Checkpoint: `IOTC: connected`, the device shows Connected in the browser, Live Data updates
every 10 seconds.

> Notes. Certificate first, then the key, two separate pastes, BEGIN to END inclusive. The board
> says `ok`, `certificate stored`, `private key stored`. If a paste is rejected as too large,
> both files went in at once. After `apply` the board syncs time over SNTP, discovers its
> endpoint, fetches file-upload credentials, and connects over MQTT with mutual TLS. From now on
> it connects at every boot without a console.

## 13. Lab 4: Dashboard, first model push, first snapshot (20 min)

Three cards: **4a Import the dashboard** (Create Dashboard, Import Dashboard,
`ra8p1-vision-ai-dashboard.json`, template RA8P1 Vision AI, your device, name it Seat NN);
**4b Push Face Detect** (highlighted: AI Models, RA8P1 Face Detect, Deploy, your device only;
watch the console; step in front of the camera); **4c Take Snapshot** (press it on the
dashboard; about 10 seconds later the photo appears with the box drawn on it). Console panel:

```
IOTC: model downloaded (441248 bytes)
FD: hot-swapping to model "face-v3" v3 (441088 bytes)
FD: model "face-v3" v3 (cloud, 441088 bytes) loaded: face detector, ethos-u: yes
FD: model persisted to OSPI flash store
```

Checkpoint: Active Model reads face-v3, inference about 5,800 µs, Uptime never reset, a
snapshot with a box around your face.

> Notes. Do 4b on the big screen first. Deploy to your own device only. The snapshot is worth a
> sentence: the board draws the boxes, encodes the PNG and signs the S3 upload with SigV4
> computed on the microcontroller. When most of the room has a box on a face, call the break.

## 14. Your dashboard once the face detector is loaded

Full-width screenshot of the live dashboard. Three callouts: **Detection State card** (FACE,
CLEAR, PERSON, NO PERSON, CLASSIFYING), **Model Source card** (cloud after a push, flash after a
power cycle), **NPU Inference Time chart** (the model-swap history; every push is a step).

> Notes. The screenshot is from a build with a built-in model, so its source card says BUILT-IN;
> yours will say cloud. The Uptime tile is the fourth widget: it must never reset today.

## 15. Break (10 minutes)

Leave your board powered and connected. When we return: what actually happened when you clicked
Deploy.

> Notes. Helpers clear anyone short of the Lab 4 checkpoint, handing out a spare board where
> needed.

## 16. Anatomy of a model push

Five numbered cards with arrows: **1 Deploy** (AI Models sends a command over MQTT with a
signed download URL); **2 Download** (HTTPS from S3 into a 4 MB pending buffer in SDRAM, three
retries); **3 Validate** (envelope header: magic, length, CRC32; then a trial load checks the
input shape); **4 Hot-swap** (highlighted: between two inferences, tear down the interpreter,
rebuild it over the same arena); **5 Persist** (envelope written to a raw OSPI slot, reloaded at
every boot). Two facts: a failure at step 3 or 4 keeps the previous model running; a push during
a live video stream is deferred until the download can share the heap, then proceeds with the
stream playing.

> Notes. The deploy is an ordinary cloud-to-device command on the MQTT session the board
> already holds. A second TLS session downloads into SDRAM, not into the running model's memory.
> The AI thread destroys the TensorFlow Lite Micro interpreter and constructs a new one over the
> same tensor arena. The safety property: any failure leaves the previous model running.

## 17. What is inside the file you push

Left, the envelope diagram: a STORED zip (no compression) containing a 32-byte IOTV header
(magic, format version, model version, payload length, CRC32, display name) followed by the
Vela-compiled `.tflite` flatbuffer, the whole network as one ethos-u custom op carrying the
NPU command stream. Made by `tools/pack_model.py`. Right, the family table: 192×192×1 is a face
detector with two YOLO output heads; 96×96×1 is a two-class person / no-person classifier;
224×224×3 is a 1000-class ImageNet classifier. Limits: 4 MB enveloped, 640 KiB tensor arena,
only the ops linked into this build; anything else is rejected at load and the previous model
restored.

> Notes. STORED zip because the board walks it in place with no inflate code. Vela rewrites
> everything the Ethos-U55 can run into a single custom op, which is why both shipped families
> run entirely on the NPU. The board looks at the input tensor shape to decide what kind of
> model it received, so the same firmware runs three tasks with no configuration.

## 18. The model library, already registered for you

| In AI Models | What the board becomes | Input | Inference | Size |
|---|---|---|---|---|
| RA8P1 Face Detect | Face detector with boxes (YOLO Fastest) | 192×192 gray | 5.8 ms | 441 KB |
| RA8P1 Person Detect | Occupancy sensor: person present or absent | 96×96 gray | 1.5 ms | 240 KB |
| RA8P1 ImageNet Classifier 0.25 | 1000-class classifier, speed tier | 224×224 RGB | 4.5 ms | 432 KB |
| RA8P1 ImageNet Classifier 0.5 | 1000-class classifier, mid tier | 224×224 RGB | 9 ms | 1.1 MB |
| RA8P1 ImageNet Classifier v2 | 1000-class classifier, accuracy tier | 224×224 RGB | 40 ms | 3.1 MB |

Two facts: the camera delivers a frame every 18 ms and every model here is faster than that;
Classifier v2 is roughly seven times the face detector's size and workload, the one to push
while you watch the chart.

> Notes. The names are exactly what appears in the deploy list. The face detector at 5.8 ms
> uses about a third of the NPU's time budget at 55 fps, which is why the larger classifier can
> be absorbed without disturbing the camera or the display.

## 19. Lab 5: Re-task the device from the cloud (30 min, the main event)

Steps: **5a** deploy ImageNet Classifier v2 (3.1 MB); hold up a mug, a banana, a bottle; watch
the chart step to about 40 ms and Uptime keep counting. **5b** deploy Person Detect; walk out
of frame and back; about 1.5 ms. **5c** send Model Info from the dashboard. **5d**
(highlighted) unplug USB, count to three, plug it back in; the boot log says the model loaded
from flash and the board reconnects. **5e** deploy Face Detect again; time left, push the 0.25
and 0.5 classifiers and compare the chart. Watch panel:

```
FD: model "mobilenet-v2" v1 (cloud, 3152368 bytes) loaded: classifier, ethos-u: yes

FD: model "person-detect" v1 (flash, 240272 bytes) loaded: classifier, ethos-u: yes
IOTC: starting (env=... duid=ek-ra8p1-07, credentials: stored)
IOTC: connected
```

The word `flash` in place of `cloud` after the power cycle, the Model Source card changing to
match, and an Uptime tile that never went to zero during the swaps.

> Notes. Push the classifier on your own board first and hold up a mug on the big screen.
> Confidence between 30 and 60 percent is a softmax over a thousand classes; the label being
> right is the point. Do 5d together as a room: that is the applause moment. Model Revert clears
> the stored model rather than restoring a built-in one; after a revert inference idles until the
> next push.

## 20. Three numbers that prove it (dark)

**7×** larger model, absorbed live (5,800 µs to 40,000 µs, swapped while camera and display kept
running). **0** reboots across every swap (the Uptime tile counts through the whole lab). **3**
tasks, one firmware image (the family is chosen by the model's input shape, not a build flag).
And after the power cycle: the same model, from the board's own flash, in about 30 seconds, with
no console attached.

> Notes. Close Lab 5 with this once most of the room has done the power cycle. Each number is
> something they saw on their own dashboard. If anyone asks about fleet scale: every board in
> this room did this at the same time from the same AI Models entry.

## 21. Lab 6: Live video, and a push mid-stream (10 min, if the network allows UDP out)

Steps: open the device's Video Streaming tab; Start Video, allow 15 seconds, Stop and Start once
more if black; wave; with the video playing, deploy a different model; watch the stream stay up
while the task changes; Stop Video. Three facts: no video hardware on this chip (software
H.264 on the Cortex-M85 at 320×240, 8 to 10 fps); real WebRTC (Kinesis Video Streams
signaling, ICE and TURN, DTLS-SRTP, one viewer per device); everything at once (NPU inference
on the same frames, telemetry flowing, a second TLS session downloading the model). Checkpoint:
live view seen, console printed `[KVSMedia] streaming ON`, model changed while the video played.

> Notes. Only run as a lab if the day-before venue test streamed. The first session after a boot
> can time out during TURN allocation. Step four is the closer: two TLS sessions, a WebRTC
> session, the NPU and the camera, concurrently, on one microcontroller.

## 22. Bring your own model

Four steps: **1 Quantize** (int8 TensorFlow Lite, input matching one of the three contracts);
**2 Compile with Vela** (target ethos-u55-256; recompile for Size if Total SRAM used exceeds
640 KiB); **3 Pack** (`tools/pack_model.py` wraps it in the IOTV envelope and writes the STORED
zip); **4 Upload and push** (AI Models, Create Model, Variant Renesas, upload the zip, deploy to
one device, then to the fleet). Command panel:

```
pip install ethos-u-vela
vela --accelerator-config=ethos-u55-256 --optimise Performance --config <default_vela.ini> \
     --memory-mode=Shared_Sram --system-config=Ethos_U55_High_End_Embedded model_quant.tflite
python tools/pack_model.py model_quant_vela.tflite --version 1 --name my-model
```

Limits: 4 MB enveloped, 640 KiB arena, only the ops linked into this build.

> Notes. Vela is Arm's compiler for the Ethos-U; the accelerator config is the 256-MAC U55 in
> the RA8P1. MobileNet v2 went from 1474 KiB to 353 KiB of arena by recompiling for size.
> `pack_model.py` is thirty lines of Python in the repository. The slide omits the `--config`
> argument for space; the developer guide has the full command.

## 23. Where to go next

Six cards: **Quickstart** (repeat today at home on your own trial account, from unboxing, no
toolchain); **Developer Guide** (build from source, the thread architecture, the memory budget,
adding models); **Demo Guide** (a ten-minute presenter's script); **The repository**
(github.com/avnet-iotconnect/iotc-freertos-ek-ra8p1); **The kit** (EK-RA8P1 Evaluation Kit on
renesas.com); **/IOTCONNECT trial** (subscription.iotconnect.io, or 60 days through the AWS
Marketplace).

> Notes. Everything here is in the repository README. Point at the URL on the whiteboard rather
> than reading it out.

## 24. Before you leave (dark)

Two cards: **The board stays here** (type `erase`, then `reboot`; leave the board, cables and
seat card on the desk). **The board goes home with you** (leave it provisioned tonight; the
account closes after the event, so follow the Quickstart to make your own, then erase, reboot
and provision again). Closing line: thank you; you pushed three models to a microcontroller
today and it never rebooted.

> Notes. Say which card applies before anyone stands up. A provisioned board is a live identity
> in the workshop account. Collect seat cards either way; they carry a login.
