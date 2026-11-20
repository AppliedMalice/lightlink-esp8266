# Usage

Everything is done from the web UI at `http://<hostname>.local` or over the REST API.

- [Web UI tour](#web-ui-tour)
- [Text: fonts, colors, effects, tokens](#text-fonts-colors-effects-tokens)
- [Quick messages](#quick-messages)
- [Playlist](#playlist)
- [Sequences (.fseq)](#sequences-fseq)
- [Live from Falcon Player or xLights](#live-from-falcon-player-or-xlights)
- [REST API](#rest-api)
- [Home Assistant](#home-assistant)
- [What has priority](#what-has-priority)

## Web UI tour

| Tab | What's there |
|---|---|
| **Sign** | Live preview of the sign, quick-message box, brightness slider. The header shows hostname, IP, current mode, refresh rate and free memory. |
| **Playlist** | The items the sign cycles through: text and sequences. Edit, reorder, then **Save & play**. |
| **Files** | Upload, list and delete `.fseq` sequence files, with used and free space. |
| **Setup** | Test patterns and orientation toggles (see [setup.md](setup.md#4-bring-up-tests)). |
| **Settings** | WiFi, hostname, login, time zone, FPP / xLights inputs, pixel mapping, refresh rate, reboot, firmware update. |

## Text: fonts, colors, effects, tokens

| Font | # | Notes |
|---|---|---|
| Small 5x7 (two lines) | 0 | Two 8-pixel lines. Separate the lines with `|` or a new line. |
| Block 10x14 | 1 | Chunky, full height |
| Sans 9pt | 2 | Default, proportional, full height |
| Sans Bold 9pt | 3 | Most readable from a distance |
| Mono Bold 9pt | 4 | Fixed width |

| Color | # | | Effect | # |
|---|---|---|---|---|
| Red | 0 | | Static | 0 |
| Green | 1 | | Scroll (right to left) | 1 |
| Orange | 2 | | Blink | 2 |
| Cycle (red / orange / green per character, animated) | 3 | | | |

- **Align** (static and blink): left 0, center 1, right 2.
- **Speed**: scroll speed in pixels per second (default 40).
- **Duration**: for scroll, the number of passes; for static and blink, seconds. `0` = forever / until cleared.
- Text that is too wide for the sign scrolls automatically, even when the effect is static.

**Tokens**, filled in live:

| Token | Example |
|---|---|
| `{time}` | `14:05` |
| `{time12}` | `2:05 PM` |
| `{date}` | `Fri Nov 20` |
| `{ip}` | `192.168.1.50` |
| `{host}` | `lightlink-shop.local` |

Time needs WiFi and NTP; it shows `--:--` until the clock is set. A static `{time}` item updates
every second. A clock that stays up: text `{time12}`, font 3, static, center, duration 0.

## Quick messages

Type in the **Sign** tab and click **Show now**. The message interrupts whatever is playing, runs
for its duration, then the playlist picks up where it left off. **Clear** ends it early; **Skip
item** jumps to the next playlist entry.

## Playlist

The **Playlist** tab holds up to 40 items. They play in order and loop.

- **+ Text** adds a text item with all the options above.
- **+ Sequence** adds an uploaded `.fseq` file with a loop count (0 = loop forever).
- Use ↑ ↓ to reorder, then **Save & play**. The playlist is stored on the controller and survives reboots.
- With an empty playlist, the sign shows its hostname and IP address.

## Sequences (.fseq)

Sequences are frame-by-frame animations made in [xLights](https://xlights.org) and played straight
from the controller's flash.

### xLights model
Create a **Matrix** model:

| Setting | Value |
|---|---|
| Direction | Horizontal |
| # Strings | 16 |
| Nodes / String | 200 |
| Strands / String | 1 |
| Starting Location | Top Left |
| String Type | RGB Nodes |

The sign has red and green LEDs only. A channel value at or above the **threshold** (default 64)
turns that color on, so:

| Effect color | Shows as |
|---|---|
| Red | red |
| Green | green |
| Yellow, orange, white | orange (red + green) |
| Blue | off |

If every other row comes out reversed, tick **Serpentine rows** in Settings. If the image is upside
down, tick **Origin bottom-left**.

### Export and upload
1. In xLights, set the sequence's FSEQ format to **V2 Uncompressed** (V1 also works). Compressed
   files (zstd / zlib) are rejected with a message saying so.
2. On the **Files** tab, upload the `.fseq`. Names are cut to 21 characters plus `.fseq`.
3. Add it to the playlist with **+ Sequence**.

If the sign's model isn't at channel 1 in the sequence, set **FSEQ start channel** in Settings.

### Storage
Each frame is 9,600 bytes and the filesystem holds about 3 MB, so roughly **300 frames** fit:
about 15 seconds at 20 fps, or 30 seconds at 10 fps. Longer shows should stream live from FPP (next section).

To test playback without xLights:
```
python3 tools/make_test_fseq.py test.fseq --frames 100 --step 50
```

## Live from Falcon Player or xLights

The controller accepts pixel data in real time. While data is arriving it takes over the sign. About
2.5 seconds after the last packet (adjustable: *Settings → Live timeout ms*), the playlist resumes.

The channel layout is the same as for sequences: 3,200 RGB pixels, 9,600 channels, using the same xLights model.

### DDP (recommended)
- **Falcon Player:** *Input/Output Setup → Channel Outputs*, then add a **DDP** output on the
  E1.31 / ArtNet / DDP tab. IP = the sign, start channel = where the sign sits in your channel map,
  channel count **9600**.
- **xLights:** add an Ethernet controller with protocol **DDP**, the sign's IP and 9,600 channels,
  then assign the matrix model to it. Turn on **Output to Lights** to drive it live while sequencing.
- DDP offset 0 is the sign's first channel.

### E1.31 / sACN
- **Unicast** to the sign's IP. Multicast isn't supported.
- Start universe from Settings (default **1**), **510 channels per universe**, so **19 universes**.

Both protocols can be turned off in Settings.

## REST API

All endpoints are relative to `http://<hostname>.local`. If a web password is set, use HTTP basic
auth (`curl -u admin:password ...`).

| Method and path | Action |
|---|---|
| `GET /api/status` | Mode, now playing, IP, RSSI, free heap, refresh rate, storage |
| `GET or POST /api/message` | Show a temporary message (query string or JSON body, see below) |
| `POST /api/clear` | End the current message |
| `POST /api/skip` | Next playlist item |
| `GET /api/playlist` | Playlist as JSON (`{"items":[...],"fonts":[...]}`) |
| `POST /api/playlist` | Replace the playlist; body `{"items":[...]}` |
| `GET /api/files` | Sequence files with frame counts |
| `POST /api/upload` | Upload a `.fseq` (multipart form, field `file`) |
| `DELETE /api/files?name=x.fseq` | Delete a file |
| `GET /api/config` | Settings (passwords masked) |
| `POST /api/config` | Change settings (JSON body, any subset of fields). WiFi or hostname changes reboot. |
| `POST /api/brightness?value=5-100` | Set brightness |
| `GET /api/test?pattern=ident` | Test pattern: `ident` `red` `green` `orange` `rows` `cols` `checker` `panels`. Empty = stop. |
| `GET /api/frame` | Current frame: 16 lines of 200 digits (0 off, 1 red, 2 green, 3 orange) |
| `POST /api/reboot` | Reboot |
| `/update` | Firmware upload page |

### Messages

Query string (names or numbers):
```
curl -X POST 'http://lightlink.local/api/message?text=Build%20passed&color=green&effect=blink&duration=10'
```
Parameters: `text` (required), `color` (`red` `green` `orange` `cycle`), `effect` (`static` `scroll`
`blink`), `align` (`left` `center` `right`), `font` (0-4), `speed`, `duration`.
Defaults: orange, scroll, 2 passes.

JSON body (numbers only, see the tables above; JSON defaults are green, scroll, 1 pass):
```
curl -X POST http://lightlink.local/api/message \
  -H 'Content-Type: application/json' \
  -d '{"text":"Doorbell!","font":3,"color":0,"effect":2,"align":1,"duration":8}'
```

### Playlist

```json
{"items":[
  {"type":"text","text":"{time12}","font":3,"color":2,"effect":0,"align":1,"duration":10},
  {"type":"text","text":"Welcome to the lab","font":2,"color":3,"effect":1,"speed":40,"duration":2},
  {"type":"fseq","file":"fireworks.fseq","duration":3}
]}
```
```
curl -X POST http://lightlink.local/api/playlist -H 'Content-Type: application/json' -d @playlist.json
```
`curl http://lightlink.local/api/playlist > playlist.json` makes a backup.

### Upload a sequence
```
curl -F 'file=@fireworks.fseq' http://lightlink.local/api/upload
```

## Home Assistant

`configuration.yaml`:
```yaml
rest_command:
  lightlink_message:
    url: "http://lightlink.local/api/message"
    method: POST
    content_type: "application/json"
    payload: >-
      {"text": {{ text | to_json }}, "color": {{ color | default(2) }},
       "effect": {{ effect | default(1) }}, "font": 3, "duration": {{ duration | default(2) }}}
```

An automation action:
```yaml
action: rest_command.lightlink_message
data:
  text: "Garage door open"
  color: 0
  effect: 2
  duration: 15
```

If you set a web password, add `username:` and `password:` to the `rest_command`.

## What has priority

From highest to lowest:

1. **Test pattern** (until you click *Stop test*)
2. **Live data** from FPP or xLights (until the live timeout passes)
3. **Quick message** (until its duration ends or you click *Clear*)
4. **Playlist**
5. **Idle**: hostname and IP, when the playlist is empty
