# Facilitator Guide, one-hour format

How to run the [one-hour attendee flow](ONE-HOUR-ATTENDEE-GUIDE.md) for 30 people. This guide
covers only what differs from the full workshop; everything else (room and network requirements,
staging the account, flashing boards with unique MAC addresses, seat cards, triage) is in the
[full facilitator guide](FACILITATOR-GUIDE.md), and you should read that first.

## Contents

- [1. The trade: prep time for room time](#1-the-trade-prep-time-for-room-time)
- [2. What moves into preparation](#2-what-moves-into-preparation)
- [3. Prep timeline additions](#3-prep-timeline-additions)
- [4. Seat cards and attendee pre-work](#4-seat-cards-and-attendee-pre-work)
- [5. Run of show, 60 minutes](#5-run-of-show-60-minutes)
- [6. Contingencies specific to one hour](#6-contingencies-specific-to-one-hour)
- [7. Closing down](#7-closing-down)

## 1. The trade: prep time for room time

The full workshop spends 55 minutes getting each attendee from an unboxed board to a connected
device: serial console, device creation, certificate paste, dashboard import. None of that is
the point of the session. The one-hour format moves all of it into preparation, so the hour is
spent on the thing attendees came for: pushing models and watching a microcontroller change task
without a reboot.

| | Full workshop | One hour |
|---|---|---|
| Attendee provisions the board over serial | Yes (Lab 3, 15 min) | No: boards arrive provisioned |
| Attendee creates the device in /IOTCONNECT | Yes (Lab 2, 10 min) | No: 33 devices pre-created |
| Attendee imports a dashboard | Yes (Lab 4a, 5 min) | No: 33 dashboards pre-imported, named by seat |
| Serial terminal on the attendee laptop | Required | Optional |
| Live video lab | Yes (Lab 6) | Presenter demo only, if time |
| Model pushes per attendee | 4 to 6 | 3 to 5 |
| Talks | 35 min | 17 min |
| Facilitator prep on top of the full-format prep | 0 | about 3 person-hours |

The cost is prep: provisioning and importing a dashboard for 33 boards takes two people about
90 minutes, and it must be done against the real workshop account. The benefit is that the
attendee's first click is a model deploy, and the most failure-prone step (pasting PEM files at
230400 baud) never happens in the room.

## 2. What moves into preparation

Everything in the full guide's §5 (staging the account) still applies: template, five models,
dashboard artwork, logins, presenter device. On top of that:

1. **Create 33 devices** (`ek-ra8p1-01` to `-33`) from the RA8P1 Vision AI template with
   auto-generated certificates, and download each certificate package. The subscription must
   allow at least 34 devices including the presenter's.
2. **Provision every board** over its serial console, exactly as the full attendee guide's Lab 3
   describes: `set env`, `set cpid`, `set duid`, `set cert`, `set key`, `apply`. About four
   minutes per board once you have a rhythm. Confirm `IOTC: connected` on each before moving on.
   The identity is stored in the board's flash, so this can be done days ahead.
3. **Import 33 dashboards**, one per device, named `Seat NN - RA8P1 Vision AI`. The name is
   what attendees search for in Lab A, so keep it exact and print it on the seat card.
4. **Leave every board with no model loaded.** If you pushed a model to a board while testing,
   send it **Model Revert** and confirm the dashboard's inference time returns to zero. The
   attendee's first push should be the first model that board has ever run; that is the
   moment the workshop is built around.
5. **Keep the seat, device, dashboard and MAC in one table.** `workshop-images/manifest.csv`
   from the image tool already has seat, MAC and suggested device ID; add the dashboard name
   and the login. This table is the seat-card mail merge and the helpers' lookup sheet.

Seats 31 to 33 are the spares, provisioned and dashboarded like the others. In a one-hour
session a board that does not connect is swapped, not debugged.

## 3. Prep timeline additions

Do the full guide's §4 timeline, plus:

### One week before

- [ ] Create the 33 devices and download the certificate packages into a folder per seat.
- [ ] Provision all 33 boards and confirm each connects. Two people, one afternoon.
- [ ] Import the 33 dashboards. Spot-check three by opening them and watching telemetry arrive.
- [ ] Decide on board power. USB-C from the attendee's laptop is the default and gives the
      optional console. If you prefer boards not to depend on laptops at all, test one board on
      a USB-C wall adapter during prep and, if it boots and connects, bring 33 adapters.

### The day before

- [ ] Power every board once, confirm it connects and that its dashboard shows inference at
      zero, then power it off. Any board that was used for a test push gets Model Revert.
- [ ] Present the deck once against the clock. Sixty minutes is unforgiving; know which slide
      you will drop first (§5).

### Day of, 30 minutes before

- [ ] Boards at seats, unpowered, Ethernet already plugged into the board. Attendees plug in
      only USB.
- [ ] Wi-Fi details, `console.iotconnect.io`, and the words "Dashboards, search your seat
      number" on the whiteboard.
- [ ] Presenter board powered and its dashboard on the big screen, with no model loaded, so
      your demo push in Lab B is a real first push.

## 4. Seat cards and attendee pre-work

The one-hour seat card is shorter than the full one: no ENV or CPID, no MAC needed by the
attendee (keep it in the helpers' table).

```
┌──────────────────────────────────────────────────────┐
│  SEAT 07                                             │
│                                                      │
│  Device ID    ek-ra8p1-07                            │
│  Dashboard    Seat 07 - RA8P1 Vision AI              │
│                                                      │
│  /IOTCONNECT  console.iotconnect.io                  │
│  Login        seat07@workshop.example                │
│  Password     (on the back)                          │
│                                                      │
│  Guide        <short URL to ONE-HOUR-ATTENDEE-GUIDE> │
│                                                      │
│  Plug in Ethernet, then USB-C to DEBUG1.             │
│  Power-cycle, never RESET.                           │
│  Deploy to your own device only.                     │
└──────────────────────────────────────────────────────┘
```

Attendee pre-work shrinks to one line: bring a laptop with a browser and a free USB port (USB-A
or USB-C). No driver, no terminal, no editor. Mention that anyone who *wants* to see the board's
log can install a serial terminal and the SEGGER J-Link software for its USB driver, but that it
is optional.

## 5. Run of show, 60 minutes

Slides refer to the one-hour deck. "Gate" means do not start the next block until roughly
80 percent of the room is at the checkpoint; helpers swap boards for the rest.

| Clock | Block | Min | Slides | Notes for the lead |
|---|---|---|---|---|
| 0:00 | Welcome. "Plug in USB now." | 3 | 1 | Boards connect while you talk. Seat number on the card matches the sticker |
| 0:03 | Talk 1: what you will do, what is on your desk, why edge AI on an MCU, the board, what happens when you click Deploy | 10 | 2 – 7 | Slides 4 and 5 are the ones to compress if you are late. Slide 7 is the mechanism; they will see it happen in seven minutes |
| 0:13 | Lab A: find your board in the cloud | 5 | 8 | Gate: dashboard shows Connected. A board not connected after two minutes is swapped, no debugging |
| 0:18 | Lab B: first push, snapshot | 15 | 9 | Demo the deploy on the big screen first (two minutes), then let them go. Gate: a snapshot with a box on it |
| 0:33 | Lab C: re-task, model-info, power-cycle, face again | 15 | 10 – 11 | Push the classifier on your own board and hold up a mug. Call the power cycle for the whole room together: that is the applause moment |
| 0:48 | Talk 2: three numbers that prove it, bring your own model, where to go next | 7 | 12 – 13 | If you are late, slide 12 alone is enough |
| 0:55 | Close: erase or keep, questions | 5 | 14 – 15 | Say which closing card applies before anyone stands up. Collect seat cards |

Things the lead says at specific moments:

- **At 0:00:** "Ethernet is already in. Plug the USB-C into DEBUG1 now, and leave it. By the
  time I stop talking your board will be online."
- **Before Lab B:** "Deploy to your own device only. Look at the device ID in the deploy dialog
  before you click. Your device ID is on your card."
- **Before Lab C step 4:** "Everybody together: unplug the USB, count to three, plug it back in.
  Now watch Model Source on your dashboard change to flash."

If you have a live-video-capable network and are running ahead of time, demonstrate the Video
Streaming tab from the presenter board during Talk 2 and push a model mid-stream. Do not make it
a lab; it needs 15 seconds of setup per seat that the hour does not have.

## 6. Contingencies specific to one hour

| Situation | What to do |
|---|---|
| A board is not connected at the Lab A gate | Swap it for a spare immediately. The helper tells the attendee the spare's seat number, device ID and dashboard name from the table. Debug the original after the session |
| More than three boards fail at the gate | The network, not the boards. Check the switch and the DHCP pool with a laptop on a board's port. Move to the backup uplink (full guide §3). While that happens, keep the room on slides 4 to 7 |
| The console is slow with 30 logins | Odd seats deploy first, even seats one minute later. Boards are unaffected |
| An attendee deploys to someone else's device | Both boards now run that model; nothing is broken. The other attendee pushes their own next model and carries on. Remind the room about the device ID |
| Running five minutes late at 0:33 | Lab C becomes steps 1, 2 and 4 (classifier, person, power-cycle). Talk 2 becomes slide 12 only |
| Running ten minutes late at 0:33 | Lab C becomes steps 1 and 4. Skip Talk 2; close from slide 12 |
| The presenter board fails during the demo | Switch the big screen to an attendee's dashboard and continue. Every seat has the same experience in front of them |

## 7. Closing down

Same as the full guide's §12, with one difference: attendees never typed anything into the
board, so **the facilitator erases the identities**. After the session, for every returned board,
open the console and type `erase` then `reboot`, or reflash it with its seat image (which does
not clear the stored identity; only `erase` does). Then delete the 33 devices, the 33
dashboards and the attendee logins.

If boards go home with attendees, leave them provisioned for the evening, close the workshop
account within the week, and point everyone at the [Quickstart](../QUICKSTART.md) for their own
account.
