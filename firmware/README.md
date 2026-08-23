# Prebuilt firmware

`iotc-vision-ai-ek-ra8p1-demo.hex` — the demo image for the
[Quickstart](../docs/QUICKSTART.md). Runs the full application: camera → Ethos-U55 face
detection → LCD overlay (LCD optional) with serial output, and connects to /IOTCONNECT
once credentials are provisioned through the serial CLI (stored on OSPI flash, persistent
across power cycles). **No credentials are embedded in this image.**

Flash with J-Flash Lite or J-Link Commander, device `R7KA8P1KF_CPU0`, SWD @ 4000 kHz, then
follow the Quickstart to provision and connect.
