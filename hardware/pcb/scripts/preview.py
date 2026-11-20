#!/usr/bin/env python3
"""Composite fab-style previews (green mask, gold pads, white silk) from KiCad layer exports."""
import subprocess, sys, io
import cairosvg
from PIL import Image, ImageOps, ImageChops

PCB, OUT = sys.argv[1], sys.argv[2]
WIDTH = 2000

def layer(name, drill=0, mirror=False):
    svg = f"/tmp/prev_{name.replace('.', '_')}.svg"
    cmd = ["kicad-cli", "pcb", "export", "svg", "--layers", f"{name},Edge.Cuts", "--black-and-white",
           "--page-size-mode", "2", "--exclude-drawing-sheet", "--drill-shape-opt", str(drill), "-o", svg, PCB]
    if mirror:
        cmd.insert(4, "--mirror")
    subprocess.run(cmd, check=True, capture_output=True)
    png = cairosvg.svg2png(url=svg, output_width=WIDTH, background_color="white")
    im = Image.open(io.BytesIO(png)).convert("L")
    return ImageOps.invert(im)          # copper/silk = white (255), empty = black

def side(prefix, mirror):
    cu = layer(f"{prefix}.Cu", drill=2, mirror=mirror)
    mask = layer(f"{prefix}.Mask", drill=2, mirror=mirror)
    silk = layer(f"{prefix}.SilkS", mirror=mirror)
    edge = layer("Edge.Cuts", mirror=mirror)
    w, h = cu.size
    # board area = inside the edge outline: flood from the corners
    board = Image.new("L", (w, h), 255)
    e = edge.point(lambda v: 255 if v > 60 else 0)
    from PIL import ImageDraw
    filled = e.convert("L").copy()
    ImageDraw.floodfill(filled, (1, 1), 128)
    board = filled.point(lambda v: 0 if v == 128 else 255)
    img = Image.new("RGB", (w, h), (255, 255, 255))
    img.paste((20, 90, 45), mask=board)                                   # bare laminate under mask
    img.paste((40, 125, 60), mask=ImageChops.multiply(cu, board))         # copper under mask
    img.paste((205, 165, 70), mask=ImageChops.multiply(mask, board))      # exposed pads (HASL)
    holes = layer(f"{prefix}.Cu", drill=2, mirror=mirror)
    img.paste((235, 235, 235), mask=silk.point(lambda v: 255 if v > 90 else 0))
    return img

top = side("F", False)
bot = side("B", True)
top.save(OUT + "-top.png")
bot.save(OUT + "-bottom.png")
print("ok")
