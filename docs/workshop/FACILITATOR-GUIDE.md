# Facilitator Guide: EK-RA8P1 Vision AI Workshop

The runbook for delivering the [Attendee Guide](ATTENDEE-GUIDE.md) to a room of 30 people, each
with an EK-RA8P1 board and a wired Ethernet drop. It covers what to prepare and when, the room
and network requirements, the run of show with timings, what to do when a seat gets stuck, and
how to close the event down.

Read the whole guide once, then use the checklists.

## Contents

- [1. Shape of the workshop](#1-shape-of-the-workshop)
- [2. The one blocker: every board needs its own MAC address](#2-the-one-blocker-every-board-needs-its-own-mac-address)
- [3. Room and network requirements](#3-room-and-network-requirements)
- [4. Prep timeline](#4-prep-timeline)
- [5. Staging the /IOTCONNECT account](#5-staging-the-iotconnect-account)
- [6. Flashing and labelling 30 boards](#6-flashing-and-labelling-30-boards)
- [7. Seat cards](#7-seat-cards)
- [8. Attendee pre-work email](#8-attendee-pre-work-email)
- [9. Run of show](#9-run-of-show)
- [10. Contingencies](#10-contingencies)
- [11. Triage table for helpers](#11-triage-table-for-helpers)
- [12. Closing down](#12-closing-down)
- [13. Shorter and longer variants](#13-shorter-and-longer-variants)

## 1. Shape of the workshop

| | |
|---|---|
| Attendees | 30, embedded or IoT engineers; no Renesas or /IOTCONNECT experience assumed |
| Length | 2 h 30 min: three short talks, six labs, one break |
| Staff | 1 lead presenter + 2 helpers (one per 15 seats). Helpers sweep the room during every lab |
| Per seat | EK-RA8P1 kit (board, OV5640 camera fitted on J35, 7-inch LCD attached, USB-C cable), a wired Ethernet drop, the attendee's own laptop, a printed seat card |
| Cloud | One /IOTCONNECT company on the **AWS** backend, staged by you. Each attendee creates one device in it |
| Firmware | The prebuilt image, flashed by you before the event, one uniquely-addressed image per seat (see §2) |
| Presentation | The workshop deck; its content and speaker notes are in [PRESENTATION.md](PRESENTATION.md) |

Design decisions worth knowing, so you can defend or change them:

- **Boards are pre-flashed.** Flashing is not what the workshop is about, it needs J-Link
  software on every laptop, and it is the step where a wrong core selection bricks the
  session. You flash all 30 boards the day before (about 20 minutes, §6).
- **Attendees provision their own board.** Pasting the certificate and key over the serial
  console is the most error-prone lab, but it is the moment the board becomes *theirs*, and it
  teaches the identity model. Three pre-provisioned spare boards absorb anyone who cannot get
  through it (§10).
- **The first model push happens before the break.** Lab 4 ends with a face detector pushed
  and a snapshot with boxes. If the schedule slips after that, the core experience has already
  been delivered.
- **Models are registered once, by you.** AI Models are account-level. Attendees deploy, they
  do not upload.
- **One dashboard per attendee.** The bundled dashboard is bound to one device at import, so
  each attendee imports their own, named by seat. Thirty dashboards in one account is fine.

## 2. The one blocker: every board needs its own MAC address

The prebuilt image carries **one fixed Ethernet MAC address**, `02:8A:9B:71:04:D2`, compiled
into the FSP driver tables and the FreeRTOS+TCP configuration (`configuration.xml`,
`ra_gen/common_data.c`, `src/net_thread_entry.c`). Thirty boards with the same MAC on one
switch will not work: the switch's forwarding table flaps between ports, the DHCP server keys
its leases on the MAC and hands every board the same address, and no board holds a TLS session
for long. Typically one board works at a time and the rest stall at DHCP or reconnect forever.

The fix is a per-seat image. `tools/make_workshop_images.py` rewrites the address in the hex
file, once per seat, and writes a manifest:

```
python tools/make_workshop_images.py --count 30 --out-dir workshop-images
```

```
workshop-images/
  board-01.hex ... board-30.hex   one image per seat
  manifest.csv                    seat, image, MAC, MAC as printed at boot, suggested device ID
```

Seat *N* gets `02:8A:9B:57:53:NN` (locally administered; `57 53` is ASCII "WS", so a board still
showing the shipped `71:04:D2` address is one you missed). The firmware only copies the address
at run time, so the patched image is equivalent to a rebuild with a different MAC. The tool
refuses to run unless it finds exactly the four expected copies in the image, so it cannot
silently patch a different firmware build. Need more seats, or different addresses? See
`--count`, `--first-seat`, `--base-mac`, `--duid-prefix`.

Spot-check after flashing: the boot log prints the address as
`  MAC     : 02:8a:9b:57:53:07`, and attendees compare it with their seat card in Lab 1.

> The long-term fix belongs in firmware: derive the MAC from the RA8P1's unique device ID
> (`R_BSP_UniqueIdGet`) at start-up instead of a compiled constant. Until that lands, use the
> tool.

## 3. Room and network requirements

### Network

| Requirement | Detail |
|---|---|
| 30 wired drops | One RJ45 per seat, plus 3 for spares and 1 at the presenter's desk. A pair of 24-port unmanaged gigabit switches is enough. The board negotiates 1 Gb/s; 100 Mb/s ports also work |
| DHCP | A pool of at least 80 addresses (30 boards, 30 laptops, spares, staff). Lease time of 4 hours or more, so nothing renews mid-lab |
| No captive portal, no 802.1X on the wired ports | The boards cannot click through a portal or present an enterprise login. Test this with a real board on the real ports the day before, not with a laptop |
| Outbound from the boards | TCP 443 (discovery, model download from S3, snapshot upload, video signaling), TCP 8883 (MQTT over TLS), UDP 123 (SNTP to `pool.ntp.org`), UDP 53 (DNS). For Lab 6 also UDP 3478 and ephemeral UDP for STUN/TURN |
| Outbound from the laptops | HTTPS to `console.iotconnect.io`. Wi-Fi is fine for laptops; only the boards need wire |
| Client isolation | Irrelevant. Boards talk only to the cloud, never to the laptops |
| Bandwidth | Trivial. Thirty attendees downloading the 3.1 MB classifier at once is under 100 MB total; thirty video streams at 500 kbit/s is 15 Mbit/s |

Bring a **backup uplink** you control (an LTE or 5G router with an Ethernet WAN feeding the
switches) in case the venue's wired network fails the captive-portal or DHCP test on the day.

### Power and desks

- The board and LCD are powered from the laptop's USB-C port. Ask attendees to keep laptops on
  mains power; a laptop on battery may throttle its USB ports.
- Each seat needs about 60 cm of desk for board, LCD, and laptop. Cameras point at the attendee,
  so face-to-face seating across a table is ideal for the face detector and the person detector.
- Bring props for the classifier lab: coffee mugs, bananas, water bottles, a keyboard. ImageNet
  recognises single objects, and the lab is much better with something to hold up.
- A big screen or projector for the deck and for your own dashboard, with your own board and
  console visible during the talks.

## 4. Prep timeline

### Two weeks before

- [ ] Confirm the /IOTCONNECT company (AWS backend) and that its subscription allows at least
      **34 devices** (30 seats, 3 spares, 1 presenter). A trial that caps devices will stop
      Lab 2 for everyone past the cap.
- [ ] Order or collect 33 EK-RA8P1 kits (30 + 3 spares) and check each box has the camera board
      and the USB-C cable.
- [ ] Confirm the venue's wired network against §3, in writing. Ask specifically about captive
      portal, 802.1X, DHCP pool size, and outbound port filtering.
- [ ] Send the attendee pre-work email (§8).
- [ ] Decide whether boards go home with attendees. It changes the closing script (§12).

### One week before

- [ ] Stage the /IOTCONNECT account (§5) and run the entire attendee flow yourself on a board,
      from Lab 1 to Lab 6, against that account.
- [ ] Generate the 30 per-seat images and the manifest (§2).
- [ ] Print the seat cards (§7) and seat-number stickers for the boards.
- [ ] Put `ra8p1-vision-ai-dashboard.json` somewhere attendees can fetch it in one click: a
      short URL to the raw file in this repository, or the file on USB sticks.

### The day before

- [ ] Flash, label, and box all 33 boards (§6). Fit the camera on J35 and attach the LCD on
      each. Spot-check three boards on a console for the right MAC.
- [ ] Provision the 3 spare boards against spare devices `ek-ra8p1-31` to `-33`, push the face
      detector to each, and put them in a box marked SPARES.
- [ ] Take one board to the venue and test it on the real wired port: DHCP lease, `IOTC:
      connected`, a model push, a snapshot, and a video stream. This test is the single most
      valuable hour of preparation.
- [ ] Load the deck on the presenter laptop and check it works offline (export a PDF as backup).

### Day of, one hour before

- [ ] One board, one LCD, one USB-C cable, one Ethernet cable, and one seat card at every seat,
      seat numbers matching the board sticker.
- [ ] Presenter board connected and on the big screen with its dashboard open.
- [ ] Helpers have this guide's §11 open and a spare board each.
- [ ] Write the Wi-Fi details, the `console.iotconnect.io` URL, and the dashboard-file URL on
      the whiteboard.

## 5. Staging the /IOTCONNECT account

All of this is one-time, in the workshop company, and takes about an hour.

1. **Template.** Import [`templates/ra8p1-vision-ai-template.json`](../../templates/ra8p1-vision-ai-template.json)
   (*Devices → Templates → Import*). It arrives as **RA8P1 Vision AI**, code `ra8p1vis`, with
   file support and video streaming enabled. Do not build one by hand: numeric attributes must be
   DECIMAL or every dashboard shows `null`.
2. **Models.** Under **AI Models → Create Model**, register all five, Model Type `AI Model`,
   Variant `Renesas`, uploading the zips from [`tools/models/`](../../tools/models/). The names
   are what attendees will see in the deploy list, so use these exactly:

   | Zip | Name | Code |
   |---|---|---|
   | `face-v3_v3.zip` | RA8P1 Face Detect | `ra8p1face` |
   | `person-detect_v1.zip` | RA8P1 Person Detect | `ra8p1prsn` |
   | `mobilenet-025_v1.zip` | RA8P1 ImageNet Classifier 0.25 | `ra8p1mn025` |
   | `mobilenet-050_v1.zip` | RA8P1 ImageNet Classifier 0.5 | `ra8p1mn050` |
   | `mobilenet-v2_v1.zip` | RA8P1 ImageNet Classifier v2 | `ra8p1mnv2` |

3. **Dashboard artwork.** Upload the eleven images in [`dashboard/images/`](../../dashboard/images/)
   to the account's public image bucket under `images/renesas/ek-ra8p1/`. Keys are
   case-sensitive; a wrong key shows as blank state cards on every attendee's dashboard.
4. **Logins.** Create one user per attendee (*Settings → Users*) with a role that can create
   devices, import dashboards, and deploy AI models, and put each login on that seat's card. If
   per-attendee users are impractical, create a handful of shared workshop logins and spread
   the seats across them; a single login shared by 30 browsers is the last resort.
5. **Key Vault.** Note the **CPID** and **ENV** from *Settings → Key Vault*. They go on every
   seat card.
6. **Your own device.** Create `ek-ra8p1-00` for the presenter board, provision it, import the
   dashboard, and leave it running on the big screen.
7. **Rehearse.** Run the attendee guide end to end as an attendee would, with an attendee login,
   on a freshly flashed board. Time yourself; if Lab 3 takes you more than eight minutes, it will
   take some attendees twenty.

## 6. Flashing and labelling 30 boards

Per board this is about 40 seconds of work. Two people with two laptops finish 33 boards in
20 minutes.

Requirements: SEGGER J-Link software **V9.38 or later** (earlier versions do not know the
RA8P1) and the `workshop-images/` folder from §2.

For each seat *NN*:

1. Connect USB-C to **DEBUG1**.
2. Flash `board-NN.hex` to `R7KA8P1KF_CPU0` (**CPU0**, the Cortex-M85; `_CPU1` is the wrong
   core and produces a dead board). Either in **J-Flash Lite** (Device `R7KA8P1KF_CPU0`, SWD,
   4000 kHz, Data File `board-NN.hex`, *Program Device*), or from the command line with a file
   `flash-NN.jlink` containing:

   ```
   r
   h
   loadfile workshop-images/board-NN.hex
   r
   g
   q
   ```

   ```
   JLink.exe -device R7KA8P1KF_CPU0 -if SWD -speed 4000 -AutoConnect 1 -CommandFile flash-NN.jlink
   ```

   Confirm the `Flash download:` line appeared; a J-Link session conflict occasionally exits
   early and leaves stale firmware.
3. Unplug and replug USB (a cold start, so the LCD initialises).
4. Stick the seat-number label on the board next to the RJ45 jack, where it is visible with the
   LCD attached.
5. Optional spot check on every fifth board: open the console at 230400 and confirm
   `MAC     : 02:8a:9b:57:53:nn` matches the sticker.

Seats 31 to 33 are the spares. Provision them the day before against `ek-ra8p1-31` to `-33`
and push the face detector, so a stuck attendee can swap boards and continue at Lab 4.

## 7. Seat cards

One printed card per seat, generated from `workshop-images/manifest.csv` with a mail merge.
Everything an attendee types comes from this card, so print large and proofread against the
manifest.

```
┌──────────────────────────────────────────────────────────┐
│  SEAT 07                                                 │
│                                                          │
│  Board MAC   02:8a:9b:57:53:07   (check it in Lab 1)     │
│  Device ID   ek-ra8p1-07         (type it in Labs 2, 3)  │
│                                                          │
│  /IOTCONNECT console.iotconnect.io                       │
│  Login       seat07@workshop.example                     │
│  Password    (on the back)                               │
│                                                          │
│  ENV         poc                                         │
│  CPID        ABCDEF0123456789ABCDEF0123456789            │
│                                                          │
│  Guide       <short URL to ATTENDEE-GUIDE.md>            │
│  Dashboard   <short URL to ra8p1-vision-ai-dashboard.json>│
│                                                          │
│  Serial: 230400 8N1, line ending CR. Power-cycle, never  │
│  RESET. Paste the certificate and the key separately.    │
└──────────────────────────────────────────────────────────┘
```

Print the login password on the back, and collect or destroy the cards at the end.

## 8. Attendee pre-work email

Send two weeks out and again two days out. The workshop cannot absorb 30 driver installs in the
first ten minutes.

> **Subject: Your laptop for the EK-RA8P1 workshop (5 minutes of setup)**
>
> Please bring a laptop with a free USB port (USB-A or USB-C; bring an adapter if you only have
> USB-C and the venue cable is USB-A) and permission to install a driver. Before you arrive:
>
> 1. Install the SEGGER J-Link software, **V9.38 or later**, from
>    https://www.segger.com/downloads/jlink/. This provides the USB serial driver for the
>    board. You will not need to flash anything.
> 2. Install a serial terminal. Windows: Tera Term (https://sourceforge.net/projects/tera-term/).
>    macOS and Linux: the built-in `screen` command is enough.
> 3. Have a plain-text editor (Notepad, TextEdit in plain-text mode, VS Code). You will paste
>    two certificate files from it; Word will break them.
>
> The board, camera, display, cables, and a wired network connection are provided. Your laptop
> only needs Wi-Fi for the cloud console. See you there.

## 9. Run of show

Total 2 h 30 min. Slide numbers refer to [PRESENTATION.md](PRESENTATION.md). "Gate" means do not
start the next block until roughly 80 percent of the room is at the checkpoint; helpers keep
sweeping for the rest and hand out spares where needed.

| Clock | Block | Min | Slides | Notes for the lead |
|---|---|---|---|---|
| 0:00 | Welcome, seat check | 5 | 1 – 3 | Everyone finds their seat card, confirms the sticker matches. Wi-Fi and URLs on the whiteboard |
| 0:05 | Talk 1: why edge AI on an MCU, the board, the cloud side | 15 | 4 – 9 | End on slide 9 (the lab flow) and leave it up |
| 0:20 | Lab 1: Meet your board | 10 | 10 | Gate: console open, MAC matches, IPv4 assigned. This gate catches every driver, cable, and network problem early |
| 0:30 | Lab 2: Claim your device | 10 | 11 | Gate: device created, two PEM files open in a text editor |
| 0:40 | Lab 3: Provision the board | 15 | 12 | The slow lab. Helpers watch for Word paste, both PEMs pasted at once, wrong line endings. Gate: `IOTC: connected` |
| 0:55 | Lab 4: Dashboard, first push, first snapshot | 20 | 13 – 14 | Demonstrate the deploy on the big screen first, then let them go. Gate: a snapshot with a box on it |
| 1:15 | Break | 10 | 15 | Boards stay powered. Helpers use the break to clear stragglers |
| 1:25 | Talk 2: anatomy of a model push, the model library | 10 | 16 – 18 | The "how" now lands, because they just did it |
| 1:35 | Lab 5: Re-task the device | 30 | 19 – 20 | Push the classifier on your own board first and hold up a mug on camera. Then let them go. The power-cycle step (5d) is the applause moment; call it out for the room to do together |
| 2:05 | Lab 6: Live video and a push mid-stream | 10 | 21 | Only if the day-before network test passed. Otherwise demonstrate from the presenter board and move on |
| 2:15 | Talk 3: bring your own model, where to go next | 10 | 22 – 23 | Vela, `pack_model.py`, the input contracts, the repository |
| 2:25 | Close | 5 | 24 | Erase or keep (§12). Collect seat cards |

Things the lead says at specific moments:

- **Before Lab 3:** "Certificate first, then the key. Two separate pastes. If the board says the
  paste is too large, that is the one mistake everyone makes once."
- **Before Lab 4b:** "Deploy to your own device only. Look at the device ID in the deploy dialog
  before you click."
- **Before Lab 5d:** "Everybody together: unplug the cable, count to three, plug it back in. Now
  watch the word `flash` show up in the boot log."

## 10. Contingencies

| Situation | What to do |
|---|---|
| Venue wired network fails the day-before test (captive portal, 802.1X, no DHCP, ports blocked) | Switch to the backup LTE router feeding your own switches. Test again with a board. If video (UDP) is blocked but the rest works, drop Lab 6 to a presenter demo |
| A seat cannot get through Lab 3 after two tries | Give them a pre-provisioned spare (seats 31 to 33). They rejoin at Lab 4 with the spare's device ID and dashboard; the helper tells them which. Fix the original board later |
| A seat's laptop cannot see the serial port at all | Pair them with a neighbour for the console (one laptop, two boards: the neighbour opens a second terminal on the second port), or hand them a spare board with the console on a helper's laptop for Lab 3 only. After Lab 3 the board works without a console |
| The /IOTCONNECT console is slow with 30 sessions | Stagger: odd seats deploy first, even seats one minute later. The device-side work is unaffected |
| A model push does not arrive at a seat | Confirm the deployment was dispatched to that device (not another seat's). Re-push. The console prints `MQTT: C2D message` when anything arrives; if it never does, the MQTT session dropped: power-cycle |
| Two boards on the console show the same MAC | A board was flashed with the wrong image. Swap one for a spare and reflash the duplicate after the session |
| A board's LCD is white | Expected after a warm reset (the `reboot` command, RESET button, or a debugger). Power-cycle. Everything else was running the whole time |
| Presenter board misbehaves during a talk | Say so, switch to an attendee's dashboard on the big screen, keep going. The room already has the experience in front of them |
| Running 20 minutes late by the break | Cut Lab 5 to 5a, 5b, and 5d (classifier, person, power-cycle). Drop Lab 6 to a demo. Talk 3 becomes two slides |

## 11. Triage table for helpers

Order of questions when someone raises a hand. Most problems are in the first four rows.

| Ask or look for | Then |
|---|---|
| Terminal speed 230400? Line ending CR? | Nine out of ten "nothing works" cases. Fix and retry |
| Did the MAC on the console match the seat card? | If not, wrong board. Swap for a spare and note the seat |
| Is there an `IPv4` line? Does `dhcp=1` print? | If not: cable, port, or the network. Move the board to a known-good port |
| PEMs pasted separately, from a plain-text editor? | Re-paste. `show` tells you what the board holds |
| `show` values match the seat card exactly? | Typos in CPID and DUID are common. `set` again, then `apply` |
| `IOTC: connected` but no data on the dashboard? | Dashboard bound to another seat's device. Re-import with the right device |
| Push dispatched to the right device? | Check the device ID in the deployment. Re-push |
| Snapshot fails | Re-send once. Confirm boot printed `FU: file upload ready` |
| Video black | Stop, Start, wait 15 s. Then skip; it is UDP through the venue firewall |
| Anything else | Power-cycle the board. It reconnects and reloads its model in about 30 seconds |

## 12. Closing down

**If the boards stay with the workshop:** attendees type `erase` then `reboot` in the console
before they leave (the attendee guide tells them). Collect the boards by seat number, so the
manifest stays valid for the next run. Boards keep their pushed model in flash; that is
harmless, and the next attendee's push replaces it.

**If the boards go home with attendees:** leave them provisioned so they still work that
evening, but tell attendees the workshop account will be closed and point them to the
[Quickstart](../QUICKSTART.md) for creating their own account and re-provisioning. Close the
account, or delete the workshop devices, within a week: 30 boards connected to an orphaned
account is 30 sets of live credentials you no longer control.

Either way, on the day after:

- [ ] Delete or disable the attendee logins and rotate any shared password.
- [ ] Delete the 30 dashboards and, if boards were returned, the 34 devices.
- [ ] Reflash any board that showed a duplicate MAC.
- [ ] Note what slipped and by how much, and adjust §9 before the next run.

## 13. Shorter and longer variants

**Two hours.** Drop Lab 6 (demonstrate video from the presenter board in 3 minutes during Talk
3), shorten Lab 5 to 5a, 5b, and 5d, and cut Talk 1 to ten minutes. Keep the break at ten
minutes; it is where helpers catch up.

**Three hours.** Add a seventh lab after Lab 6: bring your own model. Attendees with Python
run `pip install ethos-u-vela`, compile a quantized `.tflite` you provide with Vela for
`ethos-u55-256`, wrap it with `tools/pack_model.py`, upload it under AI Models with their seat
number in the code (`ws07mdl`), and push it to their own board. Budget 30 minutes and expect
the Vela install to be the slow part; pre-download the wheel. The input contracts and the
640 KiB arena limit are in the [Developer Guide](../DEVELOPER-GUIDE.md#9-adding-models).

**Half day with source builds.** Precede everything with an e² studio build lab from the
[Developer Guide](../DEVELOPER-GUIDE.md). Requires e² studio, FSP 6.3.1, and the LLVM
Embedded Toolchain on every laptop, which is a pre-work email of a different order.
