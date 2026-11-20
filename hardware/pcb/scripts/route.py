#!/usr/bin/env python3
"""Small 2-layer grid maze router for the LightLink interface board.

Routes every net except GND (GND goes to copper pours on both layers), then adds the pours,
fills them and saves. KiCad's own DRC is the authority on the result (see check.py).
"""
import heapq
import math
import sys
import numpy as np
import pcbnew
from pcbnew import FromMM, ToMM, VECTOR2I

SRC, DST = sys.argv[1], sys.argv[2]
W_BOARD, H_BOARD = 100.0, 64.0
G = 0.2                       # grid pitch, mm
CLR = 0.2                     # copper clearance, mm
MARGIN = 0.06                 # extra safety for grid quantisation
EDGE_CLR = 0.45
TW = {"default": 0.4, "+5V": 0.6, "VIN": 0.6}
VIA_D, VIA_DRILL = 0.9, 0.45
WMAX_HALF = 0.3               # half of the widest track, used for conservative painting
NX, NY = int(W_BOARD / G) + 1, int(H_BOARD / G) + 1
KEEPOUT = None                # filled from the board's rule area

b = pcbnew.LoadBoard(SRC)
FREE, BLOCK = -1, -2
def paint_disk(layers, cx, cy, r, owner):
    i0, i1 = max(0, int((cx - r) / G) - 1), min(NX - 1, int((cx + r) / G) + 1)
    j0, j1 = max(0, int((cy - r) / G) - 1), min(NY - 1, int((cy + r) / G) + 1)
    xx, yy = np.meshgrid(XS[i0:i1 + 1], YS[j0:j1 + 1])
    m = (xx - cx) ** 2 + (yy - cy) ** 2 <= r * r
    for l in layers:
        sub = grid[l][j0:j1 + 1, i0:i1 + 1]
        free = (sub == FREE) & m
        clash = m & (sub != FREE) & (sub != owner)
        sub[free] = owner
        sub[clash] = BLOCK

def paint_rect(layers, x0, y0, x1, y1, r, owner):
    """Rectangle x0..x1, y0..y1 grown by r (rounded corners)."""
    i0, i1 = max(0, int((x0 - r) / G) - 1), min(NX - 1, int((x1 + r) / G) + 1)
    j0, j1 = max(0, int((y0 - r) / G) - 1), min(NY - 1, int((y1 + r) / G) + 1)
    xx, yy = np.meshgrid(XS[i0:i1 + 1], YS[j0:j1 + 1])
    dx = np.maximum(np.maximum(x0 - xx, xx - x1), 0)
    dy = np.maximum(np.maximum(y0 - yy, yy - y1), 0)
    m = dx * dx + dy * dy <= r * r
    for l in layers:
        sub = grid[l][j0:j1 + 1, i0:i1 + 1]
        free = (sub == FREE) & m
        clash = m & (sub != FREE) & (sub != owner)
        sub[free] = owner
        sub[clash] = BLOCK

def paint_seg(layer, x1, y1, x2, y2, r, owner):
    i0, i1 = max(0, int((min(x1, x2) - r) / G) - 1), min(NX - 1, int((max(x1, x2) + r) / G) + 1)
    j0, j1 = max(0, int((min(y1, y2) - r) / G) - 1), min(NY - 1, int((max(y1, y2) + r) / G) + 1)
    xx, yy = np.meshgrid(XS[i0:i1 + 1], YS[j0:j1 + 1])
    dx, dy = x2 - x1, y2 - y1
    L2 = dx * dx + dy * dy
    t = np.clip(((xx - x1) * dx + (yy - y1) * dy) / L2, 0, 1) if L2 > 0 else 0
    px, py = x1 + t * dx, y1 + t * dy
    m = (xx - px) ** 2 + (yy - py) ** 2 <= r * r
    sub = grid[layer][j0:j1 + 1, i0:i1 + 1]
    free = (sub == FREE) & m
    clash = m & (sub != FREE) & (sub != owner)
    sub[free] = owner
    sub[clash] = BLOCK


def init_grids():
    global grid, no_via, XS, YS, netpads
    FREE, BLOCK = -1, -2
    grid = [np.full((NY, NX), FREE, dtype=np.int32) for _ in range(2)]   # 0 = F.Cu, 1 = B.Cu
    no_via = np.zeros((NY, NX), dtype=bool)   # cells where a via drill would crowd a pad hole
    XS = np.arange(NX) * G
    YS = np.arange(NY) * G

    # ---------------- collect pads ----------------
    netpads = {}      # netname -> list of (x, y, radius_for_target, pad)
    nc_owner = 100000
    pad_inf = CLR + WMAX_HALF + MARGIN
    for fp in b.GetFootprints():
        for p in fp.Pads():
            pos = p.GetPosition()
            x, y = ToMM(pos.x), ToMM(pos.y)
            name = p.GetNetname()
            rv = ToMM(p.GetDrillSize().x) / 2 + VIA_DRILL / 2 + 0.3 + MARGIN + 0.5   # hole-to-hole plus annulus
            ii0, ii1 = max(0, int((x - rv) / G) - 1), min(NX - 1, int((x + rv) / G) + 1)
            jj0, jj1 = max(0, int((y - rv) / G) - 1), min(NY - 1, int((y + rv) / G) + 1)
            xx_, yy_ = np.meshgrid(XS[ii0:ii1 + 1], YS[jj0:jj1 + 1])
            no_via[jj0:jj1 + 1, ii0:ii1 + 1] |= (xx_ - x) ** 2 + (yy_ - y) ** 2 <= rv * rv
            if p.GetAttribute() == pcbnew.PAD_ATTRIB_NPTH:
                r = ToMM(p.GetDrillSize().x) / 2
                paint_disk((0, 1), x, y, r + 0.25 + WMAX_HALF + MARGIN, BLOCK)
                continue
            code = p.GetNetCode() if name else nc_owner
            if not name:
                nc_owner += 1
            bb = p.GetBoundingBox()
            bx0, by0 = ToMM(bb.GetX()), ToMM(bb.GetY())
            bx1, by1 = bx0 + ToMM(bb.GetWidth()), by0 + ToMM(bb.GetHeight())
            if p.GetShape() == pcbnew.PAD_SHAPE_CIRCLE:
                rr = ToMM(p.GetSize().x) / 2
                paint_disk((0, 1), x, y, rr + pad_inf, code)
                tr = rr
            else:
                paint_rect((0, 1), bx0, by0, bx1, by1, pad_inf, code)
                tr = min(bx1 - bx0, by1 - by0) / 2
            if name:
                netpads.setdefault(name, []).append((x, y, tr, p))

    # board edge and antenna keepout
    for l in (0, 1):
        e = EDGE_CLR + WMAX_HALF
        ei, ej = int(math.ceil(e / G)), int(math.ceil(e / G))
        grid[l][:ej, :] = BLOCK; grid[l][-ej:, :] = BLOCK
        grid[l][:, :ei] = BLOCK; grid[l][:, -ei:] = BLOCK
    for z in b.Zones():
        if z.GetIsRuleArea():
            bb = z.GetBoundingBox()
            x0, y0 = ToMM(bb.GetX()), ToMM(bb.GetY())
            paint_rect((0, 1), x0, y0, x0 + ToMM(bb.GetWidth()), y0 + ToMM(bb.GetHeight()),
                       WMAX_HALF + MARGIN, BLOCK)


init_grids()
net_by_name = {n: b.FindNet(n) for n in netpads}

# ---------------- A* ----------------
DIRS = [(1, 0, 1.0), (-1, 0, 1.0), (0, 1, 1.0), (0, -1, 1.0),
        (1, 1, 1.414), (1, -1, 1.414), (-1, 1, 1.414), (-1, -1, 1.414)]
VIA_COST = 12.0
PREF = 1.6   # penalty factor for non-preferred direction (F.Cu horizontal, B.Cu vertical)

def cell(x, y):
    return int(round(x / G)), int(round(y / G))

def via_ok(code, j, i):
    if no_via[j, i]:
        return False
    for l in (0, 1):
        sub = grid[l][max(0, j - 2):j + 3, max(0, i - 2):i + 3]
        if np.any((sub != FREE) & (sub != code)):
            return False
    return True

def astar(code, sources, target, tx, ty, box):
    """sources: set of (l,j,i); target: set of (l,j,i). Returns path list of (l,j,i)."""
    i0, i1, j0, j1 = box
    INF = 1e18
    best = {}
    parent = {}
    heap = []
    ti, tj = tx / G, ty / G
    def h(j, i):
        dx, dy = abs(i - ti), abs(j - tj)
        return (max(dx, dy) - min(dx, dy)) + 1.414 * min(dx, dy)
    for s in sources:
        best[s] = 0.0
        heapq.heappush(heap, (h(s[1], s[2]), 0.0, s))
        parent[s] = None
    while heap:
        f, gcost, s = heapq.heappop(heap)
        if gcost > best.get(s, INF):
            continue
        if s in target:
            path = []
            while s is not None:
                path.append(s); s = parent[s]
            return path[::-1]
        l, j, i = s
        for di, dj, c in DIRS:
            ni, nj = i + di, j + dj
            if ni < i0 or ni > i1 or nj < j0 or nj > j1:
                continue
            v = grid[l][nj, ni]
            if v != FREE and v != code:
                continue
            if di != 0 and dj != 0:   # no corner cutting
                a, bb = grid[l][j, ni], grid[l][nj, i]
                if (a != FREE and a != code) or (bb != FREE and bb != code):
                    continue
            cc = c
            if (l == 0 and dj != 0 and di == 0) or (l == 1 and di != 0 and dj == 0):
                cc *= PREF
            ng = gcost + cc
            ns = (l, nj, ni)
            if ng < best.get(ns, INF):
                best[ns] = ng; parent[ns] = s
                heapq.heappush(heap, (ng + h(nj, ni), ng, ns))
        ol = 1 - l
        v = grid[ol][j, i]
        if (v == FREE or v == code) and via_ok(code, j, i):
            ng = gcost + VIA_COST
            ns = (ol, j, i)
            if ng < best.get(ns, INF):
                best[ns] = ng; parent[ns] = s
                heapq.heappush(heap, (ng + h(j, i), ng, ns))
    return None

def pad_cells(x, y, r):
    ci, cj = cell(x, y)
    rr = max(1, int(r * 0.6 / G))
    out = set()
    for dj in range(-rr, rr + 1):
        for di in range(-rr, rr + 1):
            if (di * G) ** 2 + (dj * G) ** 2 <= (r * 0.6) ** 2 + 1e-9:
                for l in (0, 1):
                    out.add((l, cj + dj, ci + di))
    return out

# ---------------- route ----------------
results = {}   # net -> list of (layer, [(x,y),...]) polylines and via list

def simplify(path):
    """Turn a cell path into polylines per layer plus via positions."""
    polys, vias = [], []
    cur_l, pts = path[0][0], [path[0]]
    for k in range(1, len(path)):
        l, j, i = path[k]
        if l != cur_l:
            polys.append((cur_l, pts)); vias.append((j, i))
            cur_l, pts = l, [path[k]]
        else:
            pts.append(path[k])
    polys.append((cur_l, pts))
    out = []
    for l, cells in polys:
        if len(cells) < 2:
            out.append((l, [(cells[0][2] * G, cells[0][1] * G)]))
            continue
        keep = [cells[0]]
        for k in range(1, len(cells) - 1):
            a, m, c = keep[-1], cells[k], cells[k + 1]
            d1 = (np.sign(m[1] - cells[k - 1][1]), np.sign(m[2] - cells[k - 1][2]))
            d2 = (np.sign(c[1] - m[1]), np.sign(c[2] - m[2]))
            if d1 != d2:
                keep.append(m)
        keep.append(cells[-1])
        out.append((l, [(c[2] * G, c[1] * G) for c in keep]))
    return out, [(i * G, j * G) for j, i in vias]

def passable(l, x, y, code):
    i0, j0 = int(math.floor(x / G)), int(math.floor(y / G))
    for jj in (j0, j0 + 1):
        for ii in (i0, i0 + 1):
            if ii < 0 or jj < 0 or ii >= NX or jj >= NY:
                return False
            v = grid[l][jj, ii]
            if v != FREE and v != code:
                return False
    return True

def clear(l, a, c, code):
    d = math.hypot(c[0] - a[0], c[1] - a[1])
    n = max(1, int(d / 0.05))
    for k in range(n + 1):
        t = k / n
        if not passable(l, a[0] + t * (c[0] - a[0]), a[1] + t * (c[1] - a[1]), code):
            return False
    return True

def smooth(l, pts, code):
    if len(pts) <= 2:
        return pts
    out, i = [pts[0]], 0
    while i < len(pts) - 1:
        k = len(pts) - 1
        while k > i + 1 and not clear(l, pts[i], pts[k], code):
            k -= 1
        out.append(pts[k]); i = k
    return out

def route_net(name):
    pads = netpads[name]
    code = net_by_name[name].GetNetCode()
    w = TW.get(name, TW["default"])
    tr_inf = w / 2 + CLR + WMAX_HALF + MARGIN
    tree = set(pad_cells(*pads[0][:3]))
    done = [pads[0]]
    todo = pads[1:]
    segs, vias = [], []
    while todo:
        # nearest remaining pad to anything already connected
        def dist(p):
            return min(math.hypot(p[0] - d[0], p[1] - d[1]) for d in done)
        todo.sort(key=dist)
        tgt = todo.pop(0)
        xs = [d[0] for d in done] + [tgt[0]]
        ys = [d[1] for d in done] + [tgt[1]]
        path = None
        for pad_mm in (6, 15, 200):
            box = (max(0, int((min(xs) - pad_mm) / G)), min(NX - 1, int((max(xs) + pad_mm) / G)),
                   max(0, int((min(ys) - pad_mm) / G)), min(NY - 1, int((max(ys) + pad_mm) / G)))
            path = astar(code, tree, pad_cells(*tgt[:3]), tgt[0], tgt[1], box)
            if path:
                break
        if not path:
            return None
        polys, vs = simplify(path)
        # snap ends onto pad centres
        first_l, first_pts = polys[0]
        last_l, last_pts = polys[-1]
        src_pad = None
        for d in done:
            if (path[0][1], path[0][2]) in {(c[1], c[2]) for c in pad_cells(*d[:3])}:
                src_pad = d
        if src_pad:
            first_pts.insert(0, (src_pad[0], src_pad[1]))
        last_pts.append((tgt[0], tgt[1]))
        polys = [(l, smooth(l, pts, code)) for l, pts in polys]
        for l, pts in polys:
            for a, c in zip(pts, pts[1:]):
                if a != c:
                    segs.append((l, a, c, w))
                    paint_seg(l, a[0], a[1], c[0], c[1], tr_inf, code)
            for (l2, j, i) in [(l, round(p[1] / G), round(p[0] / G)) for p in pts]:
                tree.add((l2, j, i))
        for vx, vy in vs:
            vias.append((vx, vy))
            paint_disk((0, 1), vx, vy, VIA_D / 2 + CLR + WMAX_HALF + MARGIN, code)
        tree |= set(path)
        tree |= pad_cells(*tgt[:3])
        done.append(tgt)
    return segs, vias

order = ["+5V", "VIN", "D5", "D8", "D0", "D3", "D4", "D7", "D6", "D1", "D2"]
rest = sorted([n for n in netpads if n not in order and n != "GND" and len(netpads[n]) > 1],
              key=lambda n: -max(math.hypot(a[0] - c[0], a[1] - c[1]) for a in netpads[n] for c in netpads[n]))
order += rest + ["GND"]
for attempt in range(12):
    if attempt:
        init_grids()
    results.clear()
    failed = []
    for n in order:
        if n not in netpads or len(netpads[n]) < 2:
            continue
        r = route_net(n)
        if r is None:
            failed.append(n)
            continue
        results[n] = r
    print(f"attempt {attempt}: failed {failed}")
    if not failed:
        break
    order = failed + [n for n in order if n not in failed]

# ---------------- write tracks, vias, pours ----------------
for n, (segs, vias) in results.items():
    net = net_by_name[n]
    for l, a, c, w in segs:
        t = pcbnew.PCB_TRACK(b)
        t.SetStart(VECTOR2I(FromMM(a[0]), FromMM(a[1])))
        t.SetEnd(VECTOR2I(FromMM(c[0]), FromMM(c[1])))
        t.SetWidth(FromMM(w)); t.SetLayer(pcbnew.F_Cu if l == 0 else pcbnew.B_Cu)
        t.SetNet(net)
        b.Add(t)
    for vx, vy in vias:
        v = pcbnew.PCB_VIA(b)
        v.SetPosition(VECTOR2I(FromMM(vx), FromMM(vy)))
        v.SetWidth(FromMM(VIA_D)); v.SetDrill(FromMM(VIA_DRILL))
        v.SetNet(net)
        b.Add(v)

gnd = b.FindNet("GND")
for layer in (pcbnew.F_Cu, pcbnew.B_Cu):
    z = pcbnew.ZONE(b)
    z.SetLayer(layer); z.SetNet(gnd)
    z.SetLocalClearance(FromMM(0.3))
    z.SetMinThickness(FromMM(0.25))
    z.SetThermalReliefGap(FromMM(0.4))
    z.SetThermalReliefSpokeWidth(FromMM(0.5))
    z.SetPadConnection(pcbnew.ZONE_CONNECTION_THERMAL)
    z.SetIslandRemovalMode(pcbnew.ISLAND_REMOVAL_MODE_ALWAYS)
    z.SetAssignedPriority(0)
    ol = z.Outline(); ol.NewOutline()
    for x, y in [(0.3, 0.3), (W_BOARD - 0.3, 0.3), (W_BOARD - 0.3, H_BOARD - 0.3), (0.3, H_BOARD - 0.3)]:
        ol.Append(FromMM(x), FromMM(y))
    z.SetZoneName("GND_" + ("F" if layer == pcbnew.F_Cu else "B"))
    b.Add(z)

# stitching vias between the two GND pours, on free grid spots
stitch = 0
for y in np.arange(4.0, H_BOARD - 3.0, 6.0):
    for x in np.arange(4.0, W_BOARD - 3.0, 6.0):
        i, j = cell(x, y)
        if all(grid[l][j - 3:j + 4, i - 3:i + 4].max() == FREE and grid[l][j - 3:j + 4, i - 3:i + 4].min() == FREE
               for l in (0, 1)):
            v = pcbnew.PCB_VIA(b)
            v.SetPosition(VECTOR2I(FromMM(i * G), FromMM(j * G)))
            v.SetWidth(FromMM(VIA_D)); v.SetDrill(FromMM(VIA_DRILL)); v.SetNet(gnd)
            b.Add(v)
            paint_disk((0, 1), i * G, j * G, VIA_D / 2 + CLR + WMAX_HALF + MARGIN, gnd.GetNetCode())
            stitch += 1
print("stitching vias:", stitch)

filler = pcbnew.ZONE_FILLER(b)
filler.Fill(b.Zones())
b.Save(DST)
print("failed:", failed)
