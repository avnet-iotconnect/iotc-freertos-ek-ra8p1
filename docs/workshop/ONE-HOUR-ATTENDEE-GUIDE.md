# Attendee Guide, one-hour format: Push AI Models to a Microcontroller from the Cloud

**Workshop:** Vision AI on the Renesas EK-RA8P1 with /IOTCONNECT AI Model Management
**Length:** 60 minutes · three labs · one board per attendee

Your board is already connected to the cloud as your own device. In the next hour you will push
three different AI models to it from /IOTCONNECT and watch it change task in seconds, with no
reflash and no reboot. You will then pull the power and watch it come back running the model
you chose.

Each lab ends with a **Checkpoint**. Do not move on until you have seen it; raise your seat card
and a helper will come to you.

## Your seat card

| Field | What it is | Used in |
|---|---|---|
| **Seat** | Your seat number, `01` to `30`. It matches the sticker on your board | Everywhere |
| **Device ID** | Your board's identity in the cloud, e.g. `ek-ra8p1-07` | Labs B and C (pick it when you deploy) |
| **Dashboard** | The name of your dashboard, e.g. `Seat 07 - RA8P1 Vision AI` | Lab A |
| **Login** | Your /IOTCONNECT sign-in for the workshop account | Lab A |

One rule for the hour: **power-cycle, never RESET.** To restart the board, unplug the USB-C cable
and plug it back in. After the RESET button the LCD stays white until the next power cycle; the
rest of the board keeps running either way.

## Lab A: Find your board in the cloud (5 min)

1. **Plug in**, if it is not already: Ethernet from the wall or switch to the board, then USB-C
   from your laptop (or the USB power adapter) to the board's **DEBUG1** port. The board boots,
   gets an address, and connects to the cloud by itself in about 30 seconds. If the LCD is
   attached it shows the live camera view.
2. **Sign in** at [console.iotconnect.io](https://console.iotconnect.io) with the login on your
   seat card.
3. **Open Dashboards** and pick the one named with your seat, for example
   `Seat 07 - RA8P1 Vision AI`.
4. **Read the values.** Camera FPS reads 55 and Uptime counts up. Active Model is empty and
   NPU Inference Time is zero: the board arrives with no model. You push the first one next.

**Optional.** If you have a serial terminal on your laptop, open it on the board's USB port at
`230400` baud, 8N1, and you will see the board's log lines as models arrive. It is not required;
the dashboard and the LCD show everything.

**Checkpoint.** Your dashboard shows the device as Connected, Camera FPS 55, and Uptime counting.

## Lab B: Your first push (15 min)

The models are already registered in the workshop account under **AI Models**.

1. Go to **AI Models**, open **RA8P1 Face Detect**, and choose **Deploy**.
2. Select **your device only** (`ek-ra8p1-NN` from your seat card) and dispatch.
3. **Watch it land.** Within seconds the dashboard's Active Model reads `face-v3`, the Model
   Source card says the model came from the cloud, and NPU Inference Time is about 5,800 µs.
4. **Step in front of the camera.** The Detection State card switches to FACE DETECTED, the
   Faces gauge moves, and on the LCD a green box tracks your face.
5. **Take a photo from the cloud.** Press **Take Snapshot** on the dashboard. About 10 seconds
   later the Latest Snapshot widget shows what the camera saw, with the box drawn on it.

If you have the serial console open, this is what arrives:

```
IOTC: model downloaded (441248 bytes)
FD: hot-swapping to model "face-v3" v3 (441088 bytes)
FD: model "face-v3" v3 (cloud, 441088 bytes) loaded: face detector, ethos-u: yes
FD: model persisted to OSPI flash store
```

The board downloaded the file over HTTPS, checked it, swapped it in between two inferences, and
wrote it to its own flash. The Uptime tile never reset: nothing rebooted.

**Checkpoint.** Active Model reads `face-v3`, and a snapshot with a box around your face is on
your dashboard.

## Lab C: Re-task the device, then prove it (15 min)

The same board becomes a 1000-class image classifier, then an occupancy sensor, purely by
pushing models. Keep the dashboard visible.

1. **Deploy RA8P1 ImageNet Classifier v2** to your device. It is 3.1 MB, about seven times the
   face detector. Hold up a single object, centred and close to the camera: a coffee mug, a
   banana, a water bottle, your phone. The Detection / Class tile shows the top label. The NPU
   Inference Time chart steps from about 5,800 µs to about 40,000 µs, and Uptime keeps counting.
   Confidence of 30 to 60 percent is normal for a softmax over 1,000 classes.
2. **Deploy RA8P1 Person Detect.** Walk out of frame and back in. The state card flips between
   PERSON PRESENT and NO PERSON, and inference time drops to about 1,500 µs.
3. **Ask the device what it is running.** On the dashboard's Send Command widget choose
   **Model Info** and execute it. The acknowledgment lists the model's name, version, source and
   size.
4. **Prove it survives power loss.** Unplug the USB-C cable, count to three, and plug it back
   in. About 30 seconds later the board is back on the dashboard, running the same model, and
   the Model Source card now says the model came from **flash**. Nobody reprovisioned anything.
5. **Finish on the face detector.** Deploy **RA8P1 Face Detect** once more. The boxes return.

Time left? Push **RA8P1 ImageNet Classifier 0.25** and **0.5** and compare the inference time
chart across all five models. The library spans 1.5 ms to 40 ms.

**Checkpoint.** The Model Source card said flash after the power cycle, the inference chart shows
a step for every push, and Uptime never went to zero during a swap.

## Before you leave

Ask your facilitator which applies to you.

- **The board stays with the workshop.** Leave it, the cables, and your seat card on the desk.
  If you opened a serial console, type `erase` then `reboot` to clear the identity; otherwise
  the facilitator will.
- **The board goes home with you.** Leave it as it is tonight. The workshop account will be
  closed after the event, so follow the [Quickstart](../QUICKSTART.md) to create your own
  /IOTCONNECT trial account, then `erase`, `reboot`, and provision the board against it.

## Troubleshooting

| Symptom | Fix |
|---|---|
| The dashboard shows the device as Disconnected | Re-seat the Ethernet cable, then power-cycle. Still disconnected after a minute: raise your seat card. A helper will swap the board for a spare so you keep going |
| You cannot find your dashboard | Dashboards, then search for your seat number. Still nothing: a helper has the list |
| The model push never arrives | Check the deployment was dispatched to *your* device in AI Models. Re-push |
| Inference time stays at zero | No model is loaded. Push one from AI Models |
| The LCD is white | Expected after a warm reset. Power-cycle the board |
| Classifier labels look wrong | Hold one object, centred, close to the lens. ImageNet knows 1,000 specific objects, not scenes |
| Snapshot says upload failed | Send it again; it is transient |
| Serial terminal shows nothing (optional step) | Speed 230400, not 115200; line ending CR |

## Going further

- [Quickstart](../QUICKSTART.md): repeat this at home on your own account, including
  provisioning the board yourself.
- [Full workshop](ATTENDEE-GUIDE.md): the two-and-a-half-hour version adds provisioning over
  the serial console and live WebRTC video from the board.
- [Developer Guide](../DEVELOPER-GUIDE.md): build from source and add your own Vela-compiled
  model with `tools/pack_model.py`.
