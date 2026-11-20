#!/usr/bin/env python3
"""Generate a small V2-uncompressed .fseq test animation for the 200x16 sign.

  python3 make_test_fseq.py test.fseq            # 5 s, 20 fps
  python3 make_test_fseq.py test.fseq --frames 60 --step 50

Pixel order matches the firmware default: row-major, top-left origin, not
serpentine, RGB per pixel (red -> red LEDs, green -> green LEDs).
"""
import argparse
import struct

W, H = 200, 16


def frame(n):
    buf = bytearray(W * H * 3)
    for y in range(H):
        for x in range(W):
            band = ((x + n * 4) // 10 + y // 4) % 4  # moving diagonal stripes
            r, g = [(255, 0), (0, 255), (255, 255), (0, 0)][band]
            if abs(x - (n * 3) % W) < 2:  # sweeping orange cursor
                r, g = 255, 255
            i = (y * W + x) * 3
            buf[i], buf[i + 1] = r, g
    return bytes(buf)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("out")
    ap.add_argument("--frames", type=int, default=100)
    ap.add_argument("--step", type=int, default=50, help="ms per frame")
    a = ap.parse_args()
    channels = W * H * 3
    hdr = bytearray(32)
    hdr[0:4] = b"PSEQ"
    struct.pack_into("<H", hdr, 4, 32)  # channel data offset
    hdr[6], hdr[7] = 0, 2               # version 2.0
    struct.pack_into("<H", hdr, 8, 32)  # header length
    struct.pack_into("<I", hdr, 10, channels)
    struct.pack_into("<I", hdr, 14, a.frames)
    hdr[18] = a.step
    hdr[20] = 0                          # no compression, 0 blocks
    hdr[22] = 0                          # no sparse ranges
    with open(a.out, "wb") as f:
        f.write(hdr)
        for n in range(a.frames):
            f.write(frame(n))
    print(f"wrote {a.out}: {a.frames} frames x {channels} ch, "
          f"{32 + a.frames * channels} bytes, {a.frames * a.step / 1000:.1f} s")


if __name__ == "__main__":
    main()
