# Changelog

## Interface PCB rev 1.0 (2026-11-20)

- Two-layer through-hole board: NodeMCU v2/v3 dual footprint, 2x 74HCT245, box headers for the original ribbons, 5 V screw terminal, VIN jumper
- KiCad project, PCBWay Gerber zip, BOM and assembly guide (`docs/pcb.md`)

## 1.0.0 (2026-11-20)

First release.

- Panel driver for the Inova 630019 boards: two bands, 200 x 16, red / green / orange, timer-driven 1:8 scan, brightness control
- Web UI: live preview, quick messages, playlist editor, file manager, settings, test patterns
- Text in 5 fonts, static / scroll / blink, color cycling, `{time}` `{time12}` `{date}` `{ip}` `{host}` tokens
- FSEQ V1 and V2-uncompressed playback from LittleFS, including sparse ranges
- Live DDP and E1.31 (unicast) input for Falcon Player and xLights
- Setup access point with captive portal, mDNS, ArduinoOTA and web OTA, optional basic auth
- REST API
- Verified on a CFM16.200S-IP
