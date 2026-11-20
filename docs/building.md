# Building from source

You only need this if you want to change the firmware. To install the prebuilt image, skip to
[flashing.md](flashing.md).

## PlatformIO (recommended)

### Install
- **macOS:** `brew install platformio`
- **Linux / Windows / macOS without Homebrew:** `pip install platformio` (Python 3.8 or newer),
  or install the PlatformIO extension in VS Code.

### Build and upload
From the repository root:

```
pio run                         # build for the Wemos D1 mini
pio run -e nodemcuv2            # build for a NodeMCU instead
pio run -t upload               # build and flash over USB
pio device monitor              # serial console, 115200 baud
```

PlatformIO downloads the ESP8266 toolchain and the libraries (Adafruit GFX, ArduinoJson) the first
time you build. The output is `.pio/build/d1_mini/firmware.bin`.

To update over WiFi instead of USB (after the first flash):

```
pio run -t upload --upload-port lightlink.local
```

Use your sign's hostname. If you set a web password, PlatformIO asks for it; it's also the OTA password.

You don't need a filesystem upload step. The firmware formats its flash filesystem on first boot.

## Arduino IDE

1. Install the **ESP8266 board package** 3.1.x: add
   `https://arduino.esp8266.com/stable/package_esp8266com_index.json` under
   *Settings → Additional boards manager URLs*, then install *esp8266* from the Boards Manager.
2. Install the libraries **Adafruit GFX Library** and **ArduinoJson** (version 7) from the Library Manager.
3. Make a folder called `lightlink`, copy everything from `src/` into it, and rename `main.cpp` to
   `lightlink.ino`.
4. Select:
   - Board: **LOLIN(WEMOS) D1 R2 & mini**
   - CPU Frequency: **160 MHz**
   - Flash Size: **4MB (FS:3MB OTA:~512KB)**
5. Upload.

## Build options

| Setting | Where | Default | Notes |
|---|---|---|---|
| `SHIFT_NOPS` | `platformio.ini` `build_flags` | 6 | Delay per shift-clock edge. Raise it (12, 20, ...) if the far panels show random pixels. |
| Pin assignments | `src/config.h` | see [hardware.md](hardware.md#wiring) | Data, SRCK and RCK must be GPIO 0-15. |
| Sign size | `src/config.h` | 200 x 16 | `SIGN_W`, `SIGN_H`. Each band is 8 rows. |
| Version string | `src/config.h` | `1.0.0` | Shown in the UI and on the boot screen. |

## Code map

| File | Purpose |
|---|---|
| `src/main.cpp` | Startup, WiFi / setup access point, mDNS, OTA, main loop |
| `src/display.*` | Timer-interrupt panel scan, brightness, Adafruit GFX canvas |
| `src/runner.*` | Playlist, text rendering and effects, messages, test patterns |
| `src/fseq.*` | FSEQ V1 / V2-uncompressed reader, including sparse ranges |
| `src/live.*` | DDP and E1.31 receivers |
| `src/pixelmap.h` | Maps RGB channel data onto red / green pixels |
| `src/settings.*` | Settings stored in `/config.json` |
| `src/web.*` | HTTP server and REST API |
| `src/webui.h` | The single-page web UI |

Notes:
- Timer1 is reserved for the display, so `analogWrite`, `tone` and `Servo` can't be used.
- RAM is tight on the ESP8266 (about 45 KB of 80 KB is used statically). Watch the heap figure on the web UI's status line when adding features.

## Continuous integration

`.github/workflows/build.yml` builds both board targets on every push and pull request, and
attaches the `.bin` files as workflow artifacts.
