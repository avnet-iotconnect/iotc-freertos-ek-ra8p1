# Facilitator Guide, one-hour format

How to run the [one-hour attendee flow](ONE-HOUR-ATTENDEE-GUIDE.md) for 30 people. This guide
covers only what differs from the full workshop; everything else (room and network requirements,
flashing boards with unique MAC addresses, the triage table) is in the
[full facilitator guide](FACILITATOR-GUIDE.md), and you should read that first.

## Contents

- [1. The trade: prep time for room time](#1-the-trade-prep-time-for-room-time)
- [2. The account model: one instance, one entity per seat](#2-the-account-model-one-instance-one-entity-per-seat)
- [3. What moves into preparation](#3-what-moves-into-preparation)
- [4. Prep timeline](#4-prep-timeline)
- [5. Seat cards and attendee pre-work](#5-seat-cards-and-attendee-pre-work)
- [6. Run of show, 60 minutes](#6-run-of-show-60-minutes)
- [7. Contingencies specific to one hour](#7-contingencies-specific-to-one-hour)
- [8. Closing down](#8-closing-down)

## 1. The trade: prep time for room time

The full workshop spends 55 minutes getting each attendee from an unboxed board to a connected
device: serial console, device creation, certificate paste, dashboard import. None of that is
the point of the session. The one-hour format moves all of it into preparation, so the hour is
spent on the thing attendees came for: pushing models and watching a microcontroller change task
without a reboot.

| | Full workshop | One hour |
|---|---|---|
| Cloud account | An existing /IOTCONNECT company; all attendees share one entity | A dedicated /IOTCONNECT instance for the event; one entity and one user per seat |
| Attendee provisions the board over serial | Yes (Lab 3, 15 min) | No: boards arrive provisioned |
| Attendee creates the device in /IOTCONNECT | Yes (Lab 2, 10 min) | No: one device pre-created in each seat's entity |
| Attendee imports a dashboard | Yes (Lab 4a, 5 min) | No: one dashboard pre-imported per seat |
| What an attendee can see | Every device in the company | Their own board and dashboard, nothing else |
| Serial terminal on the attendee laptop | Required | Optional |
| Live video lab | Yes (Lab 6) | Presenter demo only, if time |
| Model pushes per attendee | 4 to 6 | 3 to 5 |
| Talks | 35 min | 17 min |
| Facilitator prep on top of the full-format prep | 0 | about 4 person-hours |

The cost is prep: building the instance and provisioning 33 boards takes two people an
afternoon, and it must be done against the real workshop instance. The benefit is that the
attendee's first click is a model deploy, the deploy dialog offers them exactly one device, and
the most failure-prone step (pasting PEM files at 230400 baud) never happens in the room.

## 2. The account model: one instance, one entity per seat

The one-hour format runs in a **/IOTCONNECT instance created for the event** (a new
subscription on the AWS backend, used for nothing else), structured like this:

```
Workshop instance (root entity)
├── RA8P1 Vision AI template          imported once
├── AI Models: the five workshop models  registered once
├── Dashboard artwork                 uploaded once to the image bucket
├── entity user-1
│     device ek-ra8p1-01, user login user-1, dashboard "Seat 01 - RA8P1 Vision AI"
├── entity user-2
│     device ek-ra8p1-02, user login user-2, dashboard "Seat 02 - RA8P1 Vision AI"
├── ...
├── entity user-30
├── entity user-31, user-32, user-33   the three spare boards
└── presenter device, at the root
```

Each seat is its own **entity** named `user-N`, holding exactly one device and one user login,
also named `user-N`. Because a user's view is scoped to their entity, an attendee signed in as
`user-7` sees one device, one dashboard, and a deploy dialog with one board in it. They cannot
see, push to, or command anyone else's board. The template and the AI Models live at the root of
the instance, so every seat deploys the same five registered models.

Why a separate instance rather than entities inside an existing company: nothing from the
event touches production data, the trial or subscription is sized for the event, 33 user logins
with facilitator-known passwords never exist next to real users, and closing down is deleting
the instance.

## 3. What moves into preparation

Everything in the full guide's §2 (unique MAC images) and §6 (flashing and labelling) still
applies. The account and board preparation is this, in order:

1. **Create the workshop instance.** A new /IOTCONNECT subscription on the AWS backend, at
   [subscription.iotconnect.io](https://subscription.iotconnect.io/subscribe?cloud=aws) (30-day
   trial) or through the AWS Marketplace (60 days). It must allow at least **34 devices**
   (30 seats, 3 spares, the presenter) and **34 users**; if the trial caps either lower than
   that, ask for the cap to be raised or use a paid tier. Do this three weeks out; everything
   below waits on it.
2. **Import the template** [`templates/ra8p1-vision-ai-template.json`](../../templates/ra8p1-vision-ai-template.json)
   at the root. It arrives as **RA8P1 Vision AI** with file support and video streaming
   enabled.
3. **Register the five models** under **AI Models**, Model Type `AI Model`, Variant `Renesas`,
   with the names and codes in the full guide's §5 table, uploading the zips from
   [`tools/models/`](../../tools/models/). Names are what attendees see in the deploy list.
4. **Upload the dashboard artwork** from [`dashboard/images/`](../../dashboard/images/) to the
   instance's image bucket under `images/renesas/ek-ra8p1/`, keys case-sensitive.
5. **Create the entities** `user-1` to `user-33` under the root entity.
6. **Create one device per entity**: `ek-ra8p1-NN` in entity `user-N`, template RA8P1 Vision
   AI, certificate auto-generated. Download each certificate package into a folder per seat.
7. **Create one user per entity**: login `user-N`, assigned to entity `user-N`, with a role
   that can view devices and dashboards, send commands, and deploy AI models, but not create
   devices, edit templates, or manage users. Each user needs an email address: use addresses
   you control, one alias per seat on the facilitator's mailbox (for example
   `workshop+user-7@yourcompany.com`, if your mail system delivers plus-addressed mail), so the
   invitation and temporary password arrive with you. Complete any forced first-login password
   change yourself during prep and record the final password for the seat card.
8. **Provision every board** over its serial console with its seat's certificate, exactly as
   the full attendee guide's Lab 3 describes (`set env`, `set cpid`, `set duid`, `set cert`,
   `set key`, `apply`). About four minutes per board once you have a rhythm; confirm
   `IOTC: connected` on each. ENV and CPID come from the new instance's Key Vault. The identity
   is stored in the board's flash, so this can be done any day before the event.
9. **Import one dashboard per seat while signed in as that seat's user**, so it lives where
   the attendee will look: file `ra8p1-vision-ai-dashboard.json`, device `ek-ra8p1-NN`, name
   `Seat NN - RA8P1 Vision AI`. About two minutes per seat.
10. **Verify the isolation from a seat login.** Sign in as `user-7` and confirm: one device
    listed (`ek-ra8p1-07`, Connected), one dashboard, the five models visible under AI Models,
    and the deploy dialog offering only `ek-ra8p1-07`. If the models or the deploy action are
    not visible from a seat login, adjust the role's AI Models permission. This check decides
    whether the workshop works; do it before provisioning the other 32 boards.
11. **Leave every board with no model loaded.** If you pushed a model to a board while testing,
    send it **Model Revert** and confirm inference time returns to zero. The attendee's first
    push should be the first model that board has ever run.
12. **Keep one table**: seat, entity, login, password, device ID, dashboard name, MAC, image
    file. `workshop-images/manifest.csv` from the image tool gives you seat, MAC, image and
    device ID; add the rest. This table is the seat-card mail merge and the helpers' lookup
    sheet.

Seats 31 to 33 are the spares, built exactly like the others. In a one-hour session a board
that does not connect is swapped, not debugged, and because a spare lives in its own entity the
attendee takes the spare's seat card (login and dashboard) along with the board.

## 4. Prep timeline

Do the full guide's §4 timeline for the venue, network, kits and images, plus:

### Three weeks before

- [ ] Create the workshop instance and confirm its device and user limits (§3 step 1).

### Two weeks before

- [ ] Template, models, artwork, entities, devices and users (§3 steps 2 to 7).
- [ ] Provision one board, import its dashboard as its user, and run the isolation check
      (§3 steps 8 to 10) on that single seat before doing the rest.
- [ ] Send the attendee pre-work note (§5).

### One week before

- [ ] Provision all remaining boards and confirm each connects. Two people, one afternoon.
- [ ] Import the remaining dashboards, each signed in as its seat's user.
- [ ] Decide on board power. USB-C from the attendee's laptop is the default and gives the
      optional console. If you prefer boards not to depend on laptops, test one board on a
      USB-C wall adapter during prep and, if it boots and connects, bring 33 adapters.
- [ ] Print the seat cards from the table (§5).

### The day before

- [ ] Power every board once, confirm it connects and that its dashboard shows inference at
      zero, then power it off. Any board used for a test push gets Model Revert.
- [ ] Sign in as three random seat users and confirm each sees only its own board.
- [ ] Present the deck once against the clock. Sixty minutes is unforgiving; know which slide
      you will drop first (§6).

### Day of, 30 minutes before

- [ ] Boards at seats, unpowered, Ethernet already plugged into the board. Attendees plug in
      only USB.
- [ ] Wi-Fi details and `console.iotconnect.io` on the whiteboard.
- [ ] Presenter board powered and its dashboard on the big screen, with no model loaded, so
      your demo push in Lab B is a real first push.

## 5. Seat cards and attendee pre-work

The one-hour seat card carries the login and the names the attendee will see. No ENV, no CPID,
no MAC (keep the MAC in the helpers' table).

```
┌──────────────────────────────────────────────────────┐
│  SEAT 07                                             │
│                                                      │
│  /IOTCONNECT  console.iotconnect.io                  │
│  Login        user-7                                 │
│  Password     (on the back)                          │
│                                                      │
│  Your board   ek-ra8p1-07  (the only device you see) │
│  Dashboard    Seat 07 - RA8P1 Vision AI              │
│                                                      │
│  Guide        <short URL to ONE-HOUR-ATTENDEE-GUIDE> │
│                                                      │
│  Plug in Ethernet, then USB-C to DEBUG1.             │
│  Power-cycle, never RESET.                           │
└──────────────────────────────────────────────────────┘
```

Print the password on the back, and collect or destroy the cards at the end.

Attendee pre-work is one line: bring a laptop with a browser and a free USB port (USB-A or
USB-C). No driver, no terminal, no editor. Mention that anyone who *wants* to see the board's
log can install a serial terminal and the SEGGER J-Link software for its USB driver, but that it
is optional.

## 6. Run of show, 60 minutes

Slides refer to the one-hour deck. "Gate" means do not start the next block until roughly
80 percent of the room is at the checkpoint; helpers swap boards for the rest.

| Clock | Block | Min | Slides | Notes for the lead |
|---|---|---|---|---|
| 0:00 | Welcome. "Plug in USB now." | 3 | 1 | Boards connect while you talk. Seat number on the card matches the sticker |
| 0:03 | Talk 1: what you will do, what is on your desk, why edge AI on an MCU, the board, what happens when you click Deploy | 10 | 2 – 7 | Slides 4 and 5 are the ones to compress if you are late. Slide 7 is the mechanism; they will see it happen in seven minutes |
| 0:13 | Lab A: sign in, open the one dashboard, confirm Connected | 5 | 8 | Gate: dashboard shows Connected. A board not connected after two minutes is swapped, with its spare seat card, no debugging |
| 0:18 | Lab B: first push, snapshot | 15 | 9 | Demo the deploy on the big screen first (two minutes), then let them go. Gate: a snapshot with a box on it |
| 0:33 | Lab C: re-task, model-info, power-cycle, face again | 15 | 10 – 11 | Push the classifier on your own board and hold up a mug. Call the power cycle for the whole room together: that is the applause moment |
| 0:48 | Talk 2: three numbers that prove it, bring your own model, where to go next | 7 | 12 – 13 | If you are late, slide 12 alone is enough |
| 0:55 | Close: erase or keep, questions | 5 | 14 – 15 | Say which closing card applies before anyone stands up. Collect seat cards |

Things the lead says at specific moments:

- **At 0:00:** "Ethernet is already in. Plug the USB-C into DEBUG1 now, and leave it. By the
  time I stop talking your board will be online."
- **Before Lab A:** "Your login is on your card. It sees exactly one board and one dashboard:
  yours. If you see more than one of anything, tell a helper."
- **Before Lab B:** "The deploy dialog will show you one device. Pick it and dispatch."
- **Before Lab C step 4:** "Everybody together: unplug the USB, count to three, plug it back in.
  Now watch Model Source on your dashboard change to flash."

If you have a live-video-capable network and are running ahead of time, demonstrate the Video
Streaming tab from the presenter board during Talk 2 and push a model mid-stream. Do not make it
a lab; it needs 15 seconds of setup per seat that the hour does not have.

## 7. Contingencies specific to one hour

| Situation | What to do |
|---|---|
| A board is not connected at the Lab A gate | Swap it for a spare immediately, together with the spare's seat card: the spare is in its own entity with its own login and dashboard. Debug the original after the session |
| A login is rejected | The helper's table has the password. If the account is locked or demands a password change, hand over a spare seat card and its board rather than fixing it live |
| A seat login sees more than one device | An entity assignment is wrong. It does no harm during the hour (they can still find their own board by its ID on the card); fix it afterwards |
| More than three boards fail at the gate | The network, not the boards. Check the switch and the DHCP pool with a laptop on a board's port. Move to the backup uplink (full guide §3). While that happens, keep the room on slides 4 to 7 |
| The console is slow with 30 logins | Odd seats deploy first, even seats one minute later. Boards are unaffected |
| Running five minutes late at 0:33 | Lab C becomes steps 1, 2 and 4 (classifier, person, power-cycle). Talk 2 becomes slide 12 only |
| Running ten minutes late at 0:33 | Lab C becomes steps 1 and 4. Skip Talk 2; close from slide 12 |
| The presenter board fails during the demo | Switch the big screen to an attendee's dashboard (sign in as that seat's user on the presenter laptop) and continue |

## 8. Closing down

Attendees never typed anything into the board, so **the facilitator erases the identities**:
for every returned board, open the console and type `erase` then `reboot`. Reflashing with the
seat image does not clear the stored identity; only `erase` does.

Then retire the instance. Delete the 33 devices, the 33 users and the 33 entities, or simply let
the subscription lapse if it was a trial created for the event, and delete the folder of
certificate packages. Thirty-three provisioned boards pointing at a live instance with
facilitator-known passwords is the thing to avoid; a week is a reasonable deadline.

If boards go home with attendees, leave them provisioned for the evening, tell attendees the
instance will be retired, and point them at the [Quickstart](../QUICKSTART.md) for their own
account and re-provisioning.
