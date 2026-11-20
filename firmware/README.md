# Prebuilt firmware

| File | Target |
|---|---|
| `lightlink-d1mini-4M3M-v1.0.0.bin` | Wemos D1 mini or any ESP8266 with 4 MB flash. 1 MB program / 3 MB filesystem, 160 MHz, `SHIFT_NOPS=6`. |

Flash at offset `0x0`:

```
esptool --port <port> write-flash 0x0 lightlink-d1mini-4M3M-v1.0.0.bin
```

Or upload it from the web UI under *Settings → Firmware update*. Full instructions are in
[docs/flashing.md](../docs/flashing.md). Checksums are in `SHA256SUMS`.
