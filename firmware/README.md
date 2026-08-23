# Prebuilt firmware

`iotc-vision-ai-ek-ra8p1-local-demo.hex` — the **local demo** image for the
[Quickstart](../docs/QUICKSTART.md): camera → Ethos-U55 face detection → LCD overlay +
serial output, built from this source tree with cloud connectivity compiled out
(`IOTC_CFG_ENABLED 0`, no credentials embedded).

Flash with J-Flash Lite or J-Link Commander, device `R7KA8P1KF_CPU0`, SWD @ 4000 kHz.

For /IOTCONNECT connectivity (telemetry, snapshots, model push) build from source with your
device credentials — see the [Developer Guide](../docs/DEVELOPER-GUIDE.md).
