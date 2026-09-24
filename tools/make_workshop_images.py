#!/usr/bin/env python3
"""Make one prebuilt firmware image per workshop board, each with a unique MAC.

The prebuilt image (firmware/iotc-vision-ai-ek-ra8p1-demo.hex) carries one
fixed, locally-administered Ethernet MAC address (02:8A:9B:71:04:D2) in its
.data initializers: the FSP-generated r_rmac / layer3_switch tables and the
FreeRTOS+TCP address handed to FreeRTOS_IPInit(). Two boards with the same
MAC on one Ethernet segment collide - the switch's forwarding table flaps,
DHCP hands both the same lease, and neither keeps a TLS session up. For a
room full of boards on a shared network every board needs its own MAC.

This tool rewrites those initializers in the Intel HEX image, once per seat,
and writes:

  <out-dir>/board-01.hex ... board-NN.hex   one image per seat
  <out-dir>/manifest.csv                     seat, image, MAC, suggested device ID

The firmware only ever copies the address at run time (no checksum or
derived value depends on it), so patching the initializer bytes is equivalent
to rebuilding with a different MAC. The tool refuses to run unless the image
contains exactly the expected number of copies of the base MAC, so a
different firmware build cannot be patched blindly.

Example (30 seats):

  python tools/make_workshop_images.py --count 30

The MAC printed at boot ("  MAC     : 02:8a:9b:57:53:07") lets each attendee
confirm they are holding the board that matches their seat card.
"""

import argparse
import csv
import pathlib
import re
import sys

DEFAULT_IMAGE = pathlib.Path(__file__).resolve().parent.parent / "firmware" / "iotc-vision-ai-ek-ra8p1-demo.hex"
# The MAC compiled into the shipped image (configuration.xml, ra_gen/common_data.c,
# src/net_thread_entry.c).
SHIPPED_MAC = "02:8A:9B:71:04:D2"
# Three FSP driver tables + the FreeRTOS+TCP copy.
EXPECTED_COPIES = 4
# Locally-administered block for workshop boards: 02:8A:9B:57:53:<seat>
# ("57 53" is ASCII "WS"). Boards still showing the shipped MAC are unpatched.
DEFAULT_BASE_MAC = "02:8A:9B:57:53:00"


def parse_mac(text: str) -> bytes:
    parts = text.strip().split(":")
    if len(parts) != 6:
        sys.exit(f"error: MAC '{text}' must be six colon-separated hex bytes")
    try:
        mac = bytes(int(p, 16) for p in parts)
    except ValueError:
        sys.exit(f"error: MAC '{text}' is not hexadecimal")
    if not mac[0] & 0x02 or mac[0] & 0x01:
        sys.exit(f"error: MAC '{text}' must be unicast and locally administered "
                 "(first byte x2, x6, xA or xE)")
    return mac


def fmt_mac(mac: bytes, upper: bool = True) -> str:
    s = ":".join(f"{b:02x}" for b in mac)
    return s.upper() if upper else s


class HexRecord:
    __slots__ = ("length", "address", "kind", "data")

    def __init__(self, line: str, lineno: int):
        if not line.startswith(":"):
            sys.exit(f"error: line {lineno}: not an Intel HEX record")
        try:
            raw = bytes.fromhex(line[1:])
        except ValueError:
            sys.exit(f"error: line {lineno}: bad hex digits")
        if len(raw) < 5 or len(raw) != 5 + raw[0]:
            sys.exit(f"error: line {lineno}: record length mismatch")
        if sum(raw) & 0xFF:
            sys.exit(f"error: line {lineno}: checksum mismatch")
        self.length = raw[0]
        self.address = int.from_bytes(raw[1:3], "big")
        self.kind = raw[3]
        self.data = bytearray(raw[4:4 + self.length])

    def render(self) -> str:
        body = bytes([self.length]) + self.address.to_bytes(2, "big") + bytes([self.kind]) + bytes(self.data)
        checksum = (-sum(body)) & 0xFF
        return ":" + (body + bytes([checksum])).hex().upper()


def load_hex(path: pathlib.Path):
    """Return (records, placements): placements maps absolute address -> (record index, offset)."""
    records = []
    placements = {}
    upper = 0
    for lineno, line in enumerate(path.read_text().splitlines(), 1):
        line = line.strip()
        if not line:
            continue
        rec = HexRecord(line, lineno)
        records.append(rec)
        idx = len(records) - 1
        if rec.kind == 0x00:
            for off in range(rec.length):
                placements[upper + rec.address + off] = (idx, off)
        elif rec.kind == 0x04:
            upper = int.from_bytes(rec.data, "big") << 16
        elif rec.kind == 0x02:
            upper = int.from_bytes(rec.data, "big") << 4
        elif rec.kind in (0x01, 0x03, 0x05):
            pass
        else:
            sys.exit(f"error: line {lineno}: unsupported record type {rec.kind:02x}")
    if not records or records[-1].kind != 0x01:
        sys.exit("error: image does not end with an EOF record")
    return records, placements


def flat_image(records, placements):
    addresses = sorted(placements)
    base = addresses[0]
    image = bytearray(b"\xFF") * (addresses[-1] - base + 1)
    for addr, (idx, off) in placements.items():
        image[addr - base] = records[idx].data[off]
    return image, base


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--image", type=pathlib.Path, default=DEFAULT_IMAGE,
                    help=f"prebuilt Intel HEX image (default: {DEFAULT_IMAGE.name})")
    ap.add_argument("--count", type=int, default=30, metavar="N",
                    help="number of seats / images to produce (default 30)")
    ap.add_argument("--first-seat", type=int, default=1, metavar="N",
                    help="seat number of the first image (default 1)")
    ap.add_argument("--base-mac", default=DEFAULT_BASE_MAC,
                    help=f"MAC for seat 0; the seat number is added to the last byte "
                         f"(default {DEFAULT_BASE_MAC})")
    ap.add_argument("--shipped-mac", default=SHIPPED_MAC,
                    help=f"MAC compiled into the input image (default {SHIPPED_MAC})")
    ap.add_argument("--expect", type=int, default=EXPECTED_COPIES, metavar="N",
                    help=f"number of copies of the shipped MAC the image must contain "
                         f"(default {EXPECTED_COPIES})")
    ap.add_argument("--duid-prefix", default="ek-ra8p1-",
                    help="prefix for the suggested device unique ID in the manifest "
                         "(default ek-ra8p1-; the seat number is appended as two digits)")
    ap.add_argument("--out-dir", type=pathlib.Path, default=pathlib.Path("workshop-images"),
                    help="output directory (default ./workshop-images)")
    args = ap.parse_args()

    if args.count < 1 or args.first_seat < 0:
        sys.exit("error: --count must be >= 1 and --first-seat >= 0")
    base_mac = parse_mac(args.base_mac)
    shipped = parse_mac(args.shipped_mac)
    last_seat = args.first_seat + args.count - 1
    if base_mac[5] + last_seat > 0xFF:
        sys.exit(f"error: seat {last_seat} overflows the last MAC byte; choose a lower --base-mac")

    if not args.image.is_file():
        sys.exit(f"error: image not found: {args.image}")
    records, placements = load_hex(args.image)
    image, base = flat_image(records, placements)

    hits = [m.start() + base for m in re.finditer(re.escape(bytes(shipped)), bytes(image))]
    if len(hits) != args.expect:
        sys.exit(f"error: found {len(hits)} copies of {fmt_mac(shipped)} in {args.image.name}, "
                 f"expected {args.expect}. This is not the image this tool was written for; "
                 f"check the firmware build or pass --expect / --shipped-mac explicitly.")
    for addr in hits:
        for off in range(6):
            if (addr + off) not in placements:
                sys.exit(f"error: MAC copy at {addr:08x} is not fully covered by data records")

    args.out_dir.mkdir(parents=True, exist_ok=True)
    manifest_path = args.out_dir / "manifest.csv"
    rows = []
    for seat in range(args.first_seat, last_seat + 1):
        mac = bytes(base_mac[:5]) + bytes([base_mac[5] + seat])
        for addr in hits:
            for off, value in enumerate(mac):
                idx, roff = placements[addr + off]
                records[idx].data[roff] = value
        name = f"board-{seat:02d}.hex"
        out = args.out_dir / name
        out.write_text("\n".join(r.render() for r in records) + "\n")
        rows.append({
            "seat": seat,
            "image": name,
            "mac": fmt_mac(mac),
            "mac_as_printed_at_boot": fmt_mac(mac, upper=False),
            "suggested_duid": f"{args.duid_prefix}{seat:02d}",
        })

    with manifest_path.open("w", newline="") as f:
        w = csv.DictWriter(f, fieldnames=list(rows[0].keys()))
        w.writeheader()
        w.writerows(rows)

    print(f"input    : {args.image} ({len(records)} records, {len(placements)} data bytes)")
    print(f"patched  : {len(hits)} copies of {fmt_mac(shipped)} at "
          + ", ".join(f"{a:08x}" for a in hits))
    print(f"wrote    : {args.count} images in {args.out_dir}/ "
          f"(seat {args.first_seat:02d} {rows[0]['mac']} .. seat {last_seat:02d} {rows[-1]['mac']})")
    print(f"manifest : {manifest_path}")


if __name__ == "__main__":
    main()
