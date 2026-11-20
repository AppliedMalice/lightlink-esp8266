# First-time setup

This assumes the firmware is flashed ([flashing.md](flashing.md)) and the interface is wired
([hardware.md](hardware.md)).

## 1. Join the setup network

On first boot (or whenever it can't reach your WiFi) the controller starts its own access point:

| | |
|---|---|
| Network | **LightLink-XXXX** (XXXX = the last 4 hex digits of the chip ID) |
| Password | `lightlink` |
| Address | **http://192.168.4.1** |

Join it from a phone or laptop. A setup page may open by itself (captive portal); if not, browse to
the address above. With the panels connected, the sign also shows the network name and address.

## 2. Enter your settings

Open the **Settings** tab:

| Field | What to enter |
|---|---|
| WiFi SSID / password | Your network. **2.4 GHz only**: the ESP8266 can't see 5 GHz networks. |
| Hostname | A unique name such as `lightlink-shop`. Letters, digits and hyphens only; no spaces or underscores. The controller sends it to your router (DHCP) and answers to `<hostname>.local`. |
| Setup AP password | Password for the LightLink-XXXX network (8+ characters). |
| Web UI login | Optional user / password for the web UI, the API and OTA updates. Blank = open. |
| POSIX TZ | Your time zone, for `{time}` and `{date}`. See the table below. |
| NTP server | Default `pool.ntp.org` is fine. |

Click **Save settings**. The controller reboots and joins your network. If it can't connect within
about 20 seconds (a typo in the password, say), the LightLink-XXXX network comes back so you can fix it.

Common time zones:

| Zone | POSIX TZ string |
|---|---|
| US Eastern | `EST5EDT,M3.2.0,M11.1.0` |
| US Central | `CST6CDT,M3.2.0,M11.1.0` |
| US Mountain | `MST7MDT,M3.2.0,M11.1.0` |
| Arizona | `MST7` |
| US Pacific | `PST8PDT,M3.2.0,M11.1.0` |
| UK | `GMT0BST,M3.5.0/1,M10.5.0` |
| Central Europe | `CET-1CEST,M3.5.0,M10.5.0/3` |
| UTC | `UTC0` |

> The web login is plain HTTP basic auth: it keeps casual users out but isn't encrypted.
> Keep the sign on a trusted network or VLAN.

## 3. Find the sign on your network

Any of these work:

- **By name:** `http://<hostname>.local`, e.g. `http://lightlink-shop.local`. Works on macOS, iOS,
  Linux with Avahi, and Windows 10/11.
- **On the sign:** every boot scrolls `<hostname>.local  <IP address>` once in orange, just like the
  original controller did. With an empty playlist it keeps showing it.
- **Your router's** DHCP client list, under the hostname you chose. Consider a DHCP reservation so the address never changes.
- **Serial console** at 115200 baud: it prints `Connected, IP x.x.x.x` at boot.

## 4. Bring-up tests

Open the web UI and go to the **Setup** tab. Keep brightness low for the first run (the slider on the **Sign** tab).

### ident: orientation
Click **ident**. You should see:

- green `T1>` `T2>` … `T5>` across the **top** band, one per panel board
- red `B1>` … `B5>` across the **bottom** band
- an orange bar down the far **left** edge

| If you see | Fix |
|---|---|
| Mirrored text, `T5` on the left | tick **Reverse X** |
| Upside-down characters within each band | tick **Flip rows** |
| `T` labels on the bottom, `B` on the top | tick **Swap bands** |

Click **Apply & save**. The settings take effect immediately, with no reflash.

### The other patterns

| Pattern | Checks |
|---|---|
| red / green / orange | every LED in each color. A band missing one color = a broken data line. |
| rows | one row at a time, top to bottom across all 16. Two lit at once or a skipped row = a row-address wire. |
| cols | a column sweeping left to right across all 200. Garbage on the far panels = shift clock too fast. |
| checker | adjacent pixels and alternating colors |
| panels | alternating red / green blocks, one per panel board |

Click **Stop test** to return to normal operation. If anything looks wrong, see
[troubleshooting.md](troubleshooting.md).

### Brightness and heat
The default brightness is 60%. Run for an hour or so and check that the panels and the power
supply are only warm before turning it up. Brightness is saved and survives a reboot.

Next: [usage.md](usage.md).
