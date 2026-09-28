# Workshop: Push AI Models to a Microcontroller from the Cloud

A hands-on workshop for 30 attendees, each with a Renesas EK-RA8P1 kit and a wired Ethernet
connection. Attendees push three different AI models to their board from /IOTCONNECT AI Model
Management and watch it change task in seconds, with no reflash and no reboot.

Two formats. The **full workshop** (2 h 30 min) has attendees provision their own board over
the serial console and adds live WebRTC video. The **one-hour format** moves provisioning into
facilitator prep, in a dedicated /IOTCONNECT instance with one entity and one user per seat, so
the whole hour is spent pushing models.

| Document | For |
|---|---|
| [ATTENDEE-GUIDE.md](ATTENDEE-GUIDE.md) | Full workshop, attendee flow: six labs with checkpoints, from an unboxed board to a cloud-deployed model that survives a power cycle |
| [FACILITATOR-GUIDE.md](FACILITATOR-GUIDE.md) | Full workshop, runbook: prep timeline, room and network requirements, account staging, flashing 30 boards, seat cards, run of show, contingencies, closing down |
| [ONE-HOUR-ATTENDEE-GUIDE.md](ONE-HOUR-ATTENDEE-GUIDE.md) | One-hour format, attendee flow: three labs on a pre-provisioned board |
| [ONE-HOUR-FACILITATOR-GUIDE.md](ONE-HOUR-FACILITATOR-GUIDE.md) | One-hour format, runbook: the dedicated instance with one entity per seat, what moves into prep, the 60-minute run of show, contingencies |
| [EK-RA8P1-workshop-deck.pptx](EK-RA8P1-workshop-deck.pptx) | Full workshop slide deck, 25 slides with speaker notes, on the Avnet PowerPoint template |
| [EK-RA8P1-workshop-deck-1-hour.pptx](EK-RA8P1-workshop-deck-1-hour.pptx) | One-hour slide deck, 15 slides with speaker notes, on the Avnet PowerPoint template |
| [PRESENTATION.md](PRESENTATION.md) | Both decks' content and speaker notes, slide by slide, in text |
| [`tools/make_workshop_images.py`](../../tools/make_workshop_images.py) | Makes one firmware image per seat with a unique MAC address. Required for either format: the prebuilt image ships with a single fixed MAC, and 30 boards on one switch would collide |

## Agenda at a glance

### Full workshop, 2 h 30 min

| Clock | Block | Minutes |
|---|---|---|
| 0:00 | Welcome, seat check | 5 |
| 0:05 | Talk 1: edge AI on a microcontroller, the board, the cloud side | 15 |
| 0:20 | Lab 1: Meet your board | 10 |
| 0:30 | Lab 2: Claim your device in /IOTCONNECT | 10 |
| 0:40 | Lab 3: Provision the board | 15 |
| 0:55 | Lab 4: Dashboard, first model push, first snapshot | 20 |
| 1:15 | Break | 10 |
| 1:25 | Talk 2: anatomy of a model push, the model library | 10 |
| 1:35 | Lab 5: Re-task the device from the cloud | 30 |
| 2:05 | Lab 6: Live video, and a push mid-stream | 10 |
| 2:15 | Talk 3: bring your own model, where to go next | 10 |
| 2:25 | Close | 5 |

### One-hour format

| Clock | Block | Minutes |
|---|---|---|
| 0:00 | Welcome; attendees plug in and the boards connect | 3 |
| 0:03 | Talk 1: what you will do, the board, what happens when you click Deploy | 10 |
| 0:13 | Lab A: Find your board in the cloud | 5 |
| 0:18 | Lab B: Your first push, and a snapshot | 15 |
| 0:33 | Lab C: Re-task the device, then power-cycle it | 15 |
| 0:48 | Talk 2: three numbers that prove it, bring your own model | 7 |
| 0:55 | Close | 5 |

## Assumptions

- Full format: one /IOTCONNECT company on the AWS backend, staged by the facilitator, where
  each attendee creates their own device. One-hour format: a dedicated /IOTCONNECT instance
  created for the event, with one entity per seat (`user-1` to `user-33`) holding one
  pre-created device and one user login, so each attendee sees only their own board. Either
  way the subscription must allow at least 34 devices.
- Boards are pre-flashed by the facilitator with per-seat images. In the one-hour format they
  are also pre-provisioned, and attendees need nothing but a browser.
- Attendees bring their own laptop (with a serial terminal in the full format, a browser only
  in the one-hour format); the room provides wired Ethernet for the boards and Wi-Fi for the
  laptops.
- The LCD is attached but optional; every lab works headless through the dashboard.

Two-hour and three-hour variants of the full format are in the
[facilitator guide](FACILITATOR-GUIDE.md#13-shorter-and-longer-variants).
