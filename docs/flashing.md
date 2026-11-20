# Flashing the firmware

The prebuilt image is [`firmware/lightlink-d1mini-4M3M-v1.0.0.bin`](../firmware/), built for a
**Wemos D1 mini (4 MB flash)**. It also runs on a NodeMCU / ESP-12E / ESP-12F with 4 MB of flash.
You only need **esptool**; no compiler is required.

## Before you start

- **Disconnect the wire from the sign's 5 V supply.** The board runs on USB while you flash it,
  and USB powers the 74HCT245s through the D1 mini's 5V pin. Never connect USB and the sign's 5 V
  at the same time.
- If the 74HCT245s are wired in but unpowered, they hold the ESP's boot pins low and flashing
  fails. Powering from USB as above avoids this.
- Use a USB **data** cable. Some cables only carry power.

## 1. Install esptool

**macOS (Intel or Apple Silicon)**
```
brew install esptool
```
No Homebrew? Install it from [brew.sh](https://brew.sh), or use `pipx install esptool`.

**Linux**
```
pipx install esptool            # or: pip install --user esptool
sudo usermod -aG dialout $USER  # serial port access; log out and back in
```

**Windows**
```
py -m pip install esptool
```

Recent esptool versions (v5) install the command as `esptool` and use dashed sub-commands
(`write-flash`). Older installs call it `esptool.py` and use underscores (`write_flash`).
Both spellings work on v5.

## 2. Find the serial port

Plug in the D1 mini, then:

| OS | Command | Looks like |
|---|---|---|
| macOS | `ls /dev/cu.*` | `/dev/cu.usbserial-1410` or `/dev/cu.wchusbserial1410` |
| Linux | `ls /dev/ttyUSB*` | `/dev/ttyUSB0` |
| Windows | Device Manager → Ports | `COM5` |

Most D1 minis use a **CH340** USB chip. macOS and Linux include a driver for it. On Windows, if
no COM port appears, install the WCH CH340 driver. Boards with a **CP2102** chip use the
Silicon Labs CP210x driver instead.

## 3. Erase and flash

From the folder containing the `.bin` (swap in your port):

```
esptool --port /dev/cu.usbserial-1410 erase-flash
esptool --port /dev/cu.usbserial-1410 --baud 460800 write-flash 0x0 lightlink-d1mini-4M3M-v1.0.0.bin
```

- `erase-flash` wipes old WiFi settings and files. Use it for the first install; leave it out for
  updates if you want to keep your settings, playlist and sequences.
- You don't need to press any buttons: the D1 mini enters flash mode automatically.
- If you get "Failed to connect", try `--baud 115200`, a different cable, or a different USB port.

To check the download first, compare its SHA-256 with [`firmware/SHA256SUMS`](../firmware/SHA256SUMS):

```
shasum -a 256 lightlink-d1mini-4M3M-v1.0.0.bin      # macOS / Linux
certutil -hashfile lightlink-d1mini-4M3M-v1.0.0.bin SHA256   # Windows
```

## 4. Watch it boot (optional)

```
screen /dev/cu.usbserial-1410 115200       # macOS / Linux; quit with Ctrl-A then K
```
Press the D1 mini's reset button. You'll see the version, then either `Connected, IP ...` or the
name of the setup access point.

Next: [first-time setup](setup.md).

## Updating later (no USB needed)

Once the sign is on your network, use any of these:

- **Web UI:** *Settings → Firmware update*, then upload the new `.bin`. Settings and files are kept.
- **PlatformIO OTA:** `pio run -t upload --upload-port <hostname>.local`
- **Arduino IDE:** the sign appears as a network port.

If you set a web password, the update page and OTA both require it.
