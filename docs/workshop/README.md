# Workshop: Push AI Models to a Microcontroller from the Cloud

A hands-on, 2 h 30 min workshop for 30 attendees, each with a Renesas EK-RA8P1 kit and a wired
Ethernet connection. Attendees connect their board to /IOTCONNECT, then push three different
AI models to it from AI Model Management and watch the board change task in seconds, with no
reflash and no reboot.

| Document | For |
|---|---|
| [ATTENDEE-GUIDE.md](ATTENDEE-GUIDE.md) | The attendee flow: six labs with checkpoints, from an unboxed board to a cloud-deployed model that survives a power cycle |
| [FACILITATOR-GUIDE.md](FACILITATOR-GUIDE.md) | The runbook: prep timeline, room and network requirements, account staging, flashing 30 boards, seat cards, run of show, contingencies, closing down |
| [PRESENTATION.md](PRESENTATION.md) | The slide deck's content and speaker notes, slide by slide |
| [`tools/make_workshop_images.py`](../../tools/make_workshop_images.py) | Makes one firmware image per seat with a unique MAC address. Required: the prebuilt image ships with a single fixed MAC, and 30 boards on one switch would collide |

## Agenda at a glance

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

## Assumptions

- One /IOTCONNECT company on the AWS backend, staged by the facilitator; each attendee creates
  one device in it. The subscription must allow at least 34 devices.
- Boards are pre-flashed by the facilitator with per-seat images. Attendees never need J-Link
  tooling beyond its USB serial driver.
- Attendees bring their own laptop with a serial terminal; the room provides wired Ethernet for
  the boards and Wi-Fi for the laptops.
- The LCD is attached but optional; every lab works headless through the dashboard.

Two-hour and three-hour variants are in the
[facilitator guide](FACILITATOR-GUIDE.md#13-shorter-and-longer-variants).
