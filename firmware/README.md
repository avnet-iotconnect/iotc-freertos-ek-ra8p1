# Prebuilt firmware

`iotc-vision-ai-ek-ra8p1-demo.hex` — the demo image for the
[Quickstart](../docs/QUICKSTART.md). Runs the full application: camera capture, Ethos-U55
inference, LCD overlay (LCD optional), serial provisioning CLI, /IOTCONNECT telemetry,
snapshot upload, cloud model hot-swap, and live KVS WebRTC video streaming to the
Video Streaming tab. **No credentials are embedded in this image** — provision them
through the serial CLI (stored on OSPI flash, persistent across power cycles).

This image carries no compiled-in model (the video stack and the model array do not fit
the 1 MB code flash together): after connecting, push any model from
[tools/models/](../tools/models/) via /IOTCONNECT AI Models. The pushed model persists
across power cycles.

Flash with J-Flash Lite or J-Link Commander, device `R7KA8P1KF_CPU0`, SWD @ 4000 kHz, then
follow the Quickstart to provision and connect.
