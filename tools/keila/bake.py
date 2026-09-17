import json
import math
import os
import sys

from PIL import Image, ImageDraw

Image.MAX_IMAGE_PIXELS = None

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
RAW = os.path.join(ROOT, "_RESOURCES", "keila", "raw")
CENTER_E, CENTER_N = 524102.52, 6574626.33
WORLD_HALF = 1024
PLAY_HALF = 512
GRID_STEP = 2
GRID = 2 * WORLD_HALF // GRID_STEP + 1
SHEETS = [("63721_dtm_1m.tif", 520000.0, 6575000.0), ("63723_dtm_1m.tif", 520000.0, 6580000.0)]
SKIP_TYPES = {"Vundament", "Vare"}


def to_world(e, n):
    return e - CENTER_E, -(n - CENTER_N)


def load(layer):
    with open(os.path.join(RAW, layer + ".json")) as f:
        return json.load(f)["features"]


class Dtm:
    def __init__(self):
        self.e0 = int(CENTER_E - WORLD_HALF) - 4
        self.n1 = int(CENTER_N + WORLD_HALF) + 5
        self.w = int(CENTER_E + WORLD_HALF) + 5 - self.e0
        self.h = self.n1 - (int(CENTER_N - WORLD_HALF) - 4)
        self.rows = [None] * self.h
        for name, left, top in SHEETS:
            im = Image.open(os.path.join(RAW, name))
            sw, sh = im.size
            c0 = max(0, int(self.e0 - left))
            c1 = min(sw, int(self.e0 + self.w - left))
            r0 = max(0, int(top - self.n1))
            r1 = min(sh, int(top - (self.n1 - self.h)))
            if c1 <= c0 or r1 <= r0:
                continue
            data = list(im.crop((c0, r0, c1, r1)).getdata())
            cw = c1 - c0
            for r in range(r0, r1):
                row = data[(r - r0) * cw:(r - r0 + 1) * cw]
                row += [row[-1]] * (self.w - len(row))
                self.rows[int(self.n1 - (top - r))] = row
        last = None
        for i, row in enumerate(self.rows):
            if row is None:
                continue
            prev = None
            for k, v in enumerate(row):
                if v < -1000:
                    row[k] = prev if prev is not None else (last[k] if last else 0.0)
                prev = row[k]
            last = row
        first = next(r for r in self.rows if r is not None)
        for i in range(self.h):
            if self.rows[i] is None:
                self.rows[i] = first
            else:
                first = self.rows[i]

    def sample(self, e, n):
        fx = min(max(e - self.e0 - 0.5, 0.0), self.w - 1.001)
        fy = min(max(self.n1 - n - 0.5, 0.0), self.h - 1.001)
        ix, iy = int(fx), int(fy)
        tx, ty = fx - ix, fy - iy
        a, b = self.rows[iy], self.rows[iy + 1]
        top = a[ix] + (a[ix + 1] - a[ix]) * tx
        bot = b[ix] + (b[ix + 1] - b[ix]) * tx
        return top + (bot - top) * ty

    def smooth(self, e, n):
        return 0.25 * (self.sample(e - 0.5, n - 0.5) + self.sample(e + 0.5, n - 0.5) +
                       self.sample(e - 0.5, n + 0.5) + self.sample(e + 0.5, n + 0.5))


def bake_heights(dtm):
    values = []
    for j in range(GRID):
        z = -WORLD_HALF + j * GRID_STEP
        n = CENTER_N - z
        for i in range(GRID):
            values.append(dtm.smooth(CENTER_E - WORLD_HALF + i * GRID_STEP, n))
    lo = math.floor(min(values))
    return lo, [min(65535, int(round((v - lo) * 100.0))) for v in values]


def signed_area(p):
    return 0.5 * sum(p[i][0] * p[(i + 1) % len(p)][1] - p[(i + 1) % len(p)][0] * p[i][1]
                     for i in range(len(p)))


def clean_ring(ring):
    pts = [to_world(c[0], c[1]) for c in ring]
    if pts[0] == pts[-1]:
        pts.pop()
    out = []
    for p in pts:
        if not out or math.hypot(p[0] - out[-1][0], p[1] - out[-1][1]) > 0.15:
            out.append(p)
    if len(out) > 1 and math.hypot(out[0][0] - out[-1][0], out[0][1] - out[-1][1]) <= 0.15:
        out.pop()
    changed = True
    while changed and len(out) > 3:
        changed = False
        for i in range(len(out)):
            a, b, c = out[i - 1], out[i], out[(i + 1) % len(out)]
            cross = (b[0] - a[0]) * (c[1] - b[1]) - (b[1] - a[1]) * (c[0] - b[0])
            if abs(cross) < 0.02:
                out.pop(i)
                changed = True
                break
    if len(out) < 3:
        return None
    if signed_area(out) < 0:
        out.reverse()
    return out


def inside_tri(p, a, b, c):
    d1 = (p[0] - b[0]) * (a[1] - b[1]) - (a[0] - b[0]) * (p[1] - b[1])
    d2 = (p[0] - c[0]) * (b[1] - c[1]) - (b[0] - c[0]) * (p[1] - c[1])
    d3 = (p[0] - a[0]) * (c[1] - a[1]) - (c[0] - a[0]) * (p[1] - a[1])
    neg = d1 < 0 or d2 < 0 or d3 < 0
    pos = d1 > 0 or d2 > 0 or d3 > 0
    return not (neg and pos)


def triangulate(p):
    idx = list(range(len(p)))
    tris = []
    guard = 0
    while len(idx) > 3 and guard < 10000:
        guard += 1
        clipped = False
        for k in range(len(idx)):
            ia, ib, ic = idx[k - 1], idx[k], idx[(k + 1) % len(idx)]
            a, b, c = p[ia], p[ib], p[ic]
            if (b[0] - a[0]) * (c[1] - a[1]) - (b[1] - a[1]) * (c[0] - a[0]) <= 1e-9:
                continue
            if any(inside_tri(p[m], a, b, c) for m in idx if m not in (ia, ib, ic)):
                continue
            tris.append((ia, ib, ic))
            idx.pop(k)
            clipped = True
            break
        if not clipped:
            idx.pop(0)
    if len(idx) == 3:
        tris.append(tuple(idx))
    return tris


def bake_buildings():
    out = []
    for f in load("e_401_hoone_ka"):
        pr = f["properties"]
        if pr["tyyp_tekst"] in SKIP_TYPES:
            continue
        ring = clean_ring(f["geometry"]["coordinates"][0])
        if not ring or signed_area(ring) < 6.0:
            continue
        if all(abs(x) > WORLD_HALF or abs(z) > WORLD_HALF for x, z in ring):
            continue
        main = pr["tyyp_tekst"] == "Elu- või ühiskondlik hoone"
        height = pr["korgus_m"]
        if height is None or height < 2.5:
            height = 7.0 if main else 3.5
        out.append({"ring": ring, "height": float(height), "kind": 0 if main else 1,
                    "tris": triangulate(ring), "address": pr["ads_lahiaadress"] or ""})
    out.sort(key=lambda b: (round(b["ring"][0][1], 2), round(b["ring"][0][0], 2)))
    return out


def point_in_ring(x, z, ring):
    hit = False
    for i in range(len(ring)):
        ax, az = ring[i]
        bx, bz = ring[(i + 1) % len(ring)]
        if (az > z) != (bz > z) and x < (bx - ax) * (z - az) / (bz - az) + ax:
            hit = not hit
    return hit


def wall_distance(x, z, ring):
    best = 1e9
    for i in range(len(ring)):
        ax, az = ring[i]
        bx, bz = ring[(i + 1) % len(ring)]
        ex, ez = bx - ax, bz - az
        t = min(1.0, max(0.0, ((x - ax) * ex + (z - az) * ez) / (ex * ex + ez * ez)))
        best = min(best, math.hypot(x - ax - ex * t, z - az - ez * t))
    return best


def bake_spawns(buildings):
    near = [b for b in buildings if any(abs(x) < PLAY_HALF + 20 and abs(z) < PLAY_HALF + 20
                                        for x, z in b["ring"])]
    cands = []
    for f in load("e_501_tee_j"):
        if f["properties"]["tyyp_tekst"] not in ("Tänav", "Kergliiklustee", "Rada"):
            continue
        geom = f["geometry"]
        lines = geom["coordinates"] if geom["type"] == "MultiLineString" else [geom["coordinates"]]
        for line in lines:
            for c in line:
                x, z = to_world(c[0], c[1])
                if abs(x) < PLAY_HALF - 40 and abs(z) < PLAY_HALF - 40:
                    cands.append((x, z))
    cands = [c for c in cands if not any(point_in_ring(c[0], c[1], b["ring"]) or
                                         wall_distance(c[0], c[1], b["ring"]) < 2.0 for b in near)]
    cands.sort(key=lambda c: c[0] * c[0] + c[1] * c[1])
    picked = []
    for c in cands:
        if all(math.hypot(c[0] - p[0], c[1] - p[1]) > 70.0 for p in picked):
            picked.append(c)
        if len(picked) == 40:
            break
    return picked


def lines_of(feature):
    g = feature["geometry"]
    if g["type"] == "LineString":
        return [g["coordinates"]]
    if g["type"] == "MultiLineString":
        return g["coordinates"]
    return []


def polys_of(feature):
    g = feature["geometry"]
    if g["type"] == "Polygon":
        return [g["coordinates"]]
    if g["type"] == "MultiPolygon":
        return g["coordinates"]
    return []


class Canvas:
    def __init__(self, half, size, scale, flip):
        self.half, self.size, self.ss, self.flip = half, size, scale, flip
        self.px = size * scale / (2.0 * half)

    def new(self, mode, fill):
        side = self.size * self.ss
        return Image.new(mode, (side, side), fill)

    def pt(self, e, n):
        x, z = to_world(e, n)
        u = (x + self.half) * self.px
        v = (self.half - z) * self.px if self.flip else (z + self.half) * self.px
        return u, v

    def line(self, draw, coords, width, fill):
        pts = [self.pt(c[0], c[1]) for c in coords]
        w = max(1, int(round(width * self.px)))
        draw.line(pts, fill=fill, width=w, joint="curve")
        r = w * 0.5
        for u, v in (pts[0], pts[-1]):
            draw.ellipse((u - r, v - r, u + r, v + r), fill=fill)

    def poly(self, draw, rings, fill, hole):
        draw.polygon([self.pt(c[0], c[1]) for c in rings[0]], fill=fill)
        for ring in rings[1:]:
            draw.polygon([self.pt(c[0], c[1]) for c in ring], fill=hole)

    def finish(self, img):
        return img.resize((self.size, self.size), Image.LANCZOS) if self.ss > 1 else img


def road_width(props):
    w = props.get("laius")
    if w:
        return float(w)
    return {"Rada": 1.5, "Kergliiklustee": 2.5}.get(props["tyyp_tekst"], 5.0)


def bake_surface():
    cv = Canvas(WORLD_HALF, 2048, 2, True)
    paved, loose, water = cv.new("L", 0), cv.new("L", 0), cv.new("L", 0)
    dp, dl, dw = ImageDraw.Draw(paved), ImageDraw.Draw(loose), ImageDraw.Draw(water)
    for f in load("e_501_tee_a"):
        for rings in polys_of(f):
            cv.poly(dp, rings, 255, 0)
    for f in load("e_501_tee_j"):
        hard = f["properties"]["teekate_tekst"] in ("Püsikate", "Kivikate")
        for line in lines_of(f):
            cv.line(dp if hard else dl, line, road_width(f["properties"]), 255)
    for f in load("e_502_roobastee_j"):
        for line in lines_of(f):
            cv.line(dl, line, 4.0, 255)
    for layer in ("e_203_vooluveekogu_a", "e_202_seisuveekogu_a"):
        for f in load(layer):
            for rings in polys_of(f):
                cv.poly(dw, rings, 255, 0)
    for f in load("e_203_vooluveekogu_j"):
        for line in lines_of(f):
            cv.line(dw, line, 1.5, 255)
    img = Image.merge("RGB", (cv.finish(paved), cv.finish(loose), cv.finish(water)))
    img.save(os.path.join(ROOT, "textures", "keila_surface.png"), optimize=True)


def bake_bare(buildings):
    cv = Canvas(PLAY_HALF, 1024, 1, False)
    img = cv.new("L", 0)
    d = ImageDraw.Draw(img)
    for f in load("e_501_tee_a"):
        for rings in polys_of(f):
            cv.poly(d, rings, 255, 0)
    for f in load("e_501_tee_j"):
        for line in lines_of(f):
            cv.line(d, line, road_width(f["properties"]) + 0.6, 255)
    for f in load("e_502_roobastee_j"):
        for line in lines_of(f):
            cv.line(d, line, 4.6, 255)
    for layer in ("e_203_vooluveekogu_a", "e_202_seisuveekogu_a"):
        for f in load(layer):
            for rings in polys_of(f):
                cv.poly(d, rings, 255, 0)
    for b in buildings:
        pts = [((x + PLAY_HALF) * cv.px, (z + PLAY_HALF) * cv.px) for x, z in b["ring"]]
        d.polygon(pts, fill=255, outline=255)
    px = img.load()
    out = []
    for row in range(1024):
        for byte in range(128):
            v = 0
            for bit in range(8):
                if px[byte * 8 + bit, row] > 96:
                    v |= 1 << bit
            out.append(str(v))
    return out


def draw_map(cv, buildings_features):
    img = cv.new("RGB", (92, 108, 74))
    d = ImageDraw.Draw(img)
    for layer, col in (("e_303_haritav_maa_a", (120, 128, 84)), ("e_304_lage_a", (104, 120, 80)),
                       ("e_301_muu_kolvik_a", (84, 112, 70)), ("e_302_ou_a", (112, 116, 92)),
                       ("e_305_puittaimestik_a", (52, 80, 50)),
                       ("e_203_vooluveekogu_a", (58, 84, 104)), ("e_202_seisuveekogu_a", (58, 84, 104))):
        for f in load(layer):
            for rings in polys_of(f):
                cv.poly(d, rings, col, (92, 108, 74))
    for f in load("e_203_vooluveekogu_j"):
        for line in lines_of(f):
            cv.line(d, line, 2.0, (58, 84, 104))
    for f in load("e_501_tee_a"):
        for rings in polys_of(f):
            cv.poly(d, rings, (150, 150, 146), (92, 108, 74))
    for f in load("e_501_tee_j"):
        hard = f["properties"]["teekate_tekst"] in ("Püsikate", "Kivikate")
        for line in lines_of(f):
            cv.line(d, line, road_width(f["properties"]) + 1.0,
                    (158, 158, 152) if hard else (150, 136, 104))
    for f in load("e_502_roobastee_j"):
        for line in lines_of(f):
            cv.line(d, line, 2.5, (60, 56, 54))
    for f in buildings_features:
        if f["properties"]["tyyp_tekst"] in SKIP_TYPES:
            continue
        main = f["properties"]["tyyp_tekst"] == "Elu- või ühiskondlik hoone"
        for rings in polys_of(f):
            cv.poly(d, rings, (196, 186, 170) if main else (150, 142, 132), (92, 108, 74))
    return cv.finish(img)


def bake_ortho():
    folder = os.path.join(RAW, "ortho")
    if not os.path.exists(os.path.join(folder, "far.jpg")):
        print("ortho missing: run tools/keila/fetch_ortho.py")
        return
    near = Image.new("RGB", (4096, 4096))
    for row in range(2):
        for col in range(2):
            tile = Image.open(os.path.join(folder, "near_%d_%d.jpg" % (col, row)))
            near.paste(tile, (col * 2048, (1 - row) * 2048))
    near.transpose(Image.FLIP_TOP_BOTTOM).save(
        os.path.join(ROOT, "textures", "keila_ortho_near.jpg"), quality=90)
    Image.open(os.path.join(folder, "far.jpg")).transpose(Image.FLIP_TOP_BOTTOM).save(
        os.path.join(ROOT, "textures", "keila_ortho_far.jpg"), quality=90)


def fmt(values, per_line):
    lines = []
    for i in range(0, len(values), per_line):
        lines.append(",".join(values[i:i + per_line]))
    return ",\n".join(lines)


def landmark_records(buildings):
    with open(os.path.join(ROOT, "tools", "keila", "landmarks.json")) as f:
        config = json.load(f)
    recs = []
    for lm in config:
        matches = [i for i, b in enumerate(buildings) if b["address"] == lm["address"]]
        if not matches:
            print("landmark not found", lm["address"])
            continue
        index = max(matches, key=lambda i: signed_area(buildings[i]["ring"]))
        walls = list(lm["walls"])
        if len(walls) != len(buildings[index]["ring"]):
            print("landmark", lm["address"], "needs", len(buildings[index]["ring"]), "wall entries")
            walls = (walls + ["P"] * 64)[:len(buildings[index]["ring"])]
        walls = "|".join(walls)
        buildings[index]["kind"] = 2
        recs.append('{%d,%d,%.2ff,%.2ff,"%s","%s"}' % (index, lm["floors"], lm["plinth"],
                                                       lm["parapet"], lm["texture"], walls))
    return recs


def write_cpp(lo, heights, buildings, spawns, bare):
    landmarks = landmark_records(buildings)
    xz, tris, recs = [], [], []
    for b in buildings:
        first, tri_first = len(xz) // 2, len(tris)
        for x, z in b["ring"]:
            xz += ["%.2ff" % x, "%.2ff" % z]
        for t in b["tris"]:
            tris += [str(t[0]), str(t[1]), str(t[2])]
        recs.append("{%d,%d,%d,%d,%.1ff,%d}" % (first, len(b["ring"]), tri_first,
                                                len(b["tris"]) * 3, b["height"], b["kind"]))
    with open(os.path.join(ROOT, "src", "keila_data.cpp"), "w") as f:
        f.write('#include "keila.h"\n\n')
        f.write("const float KEILA_HEIGHT_BASE = %.1ff;\n" % lo)
        f.write("const uint16_t KEILA_HEIGHT_CM[KEILA_GRID * KEILA_GRID] = {\n%s\n};\n\n"
                % fmt([str(v) for v in heights], 64))
        f.write("const int KEILA_BUILDING_COUNT = %d;\n" % len(recs))
        f.write("const KeilaBuilding KEILA_BUILDINGS[] = {\n%s\n};\n\n" % fmt(recs, 6))
        f.write("const float KEILA_BUILDING_XZ[] = {\n%s\n};\n\n" % fmt(xz, 16))
        f.write("const uint16_t KEILA_ROOF_INDEX[] = {\n%s\n};\n\n" % fmt(tris, 48))
        f.write("const int KEILA_LANDMARK_COUNT = %d;\n" % len(landmarks))
        f.write("const KeilaLandmark KEILA_LANDMARKS[] = {\n%s\n};\n\n" % fmt(landmarks, 1))
        f.write("const uint8_t KEILA_BARE[KEILA_BARE_SIDE * KEILA_BARE_SIDE / 8] = {\n%s\n};\n\n"
                % fmt(bare, 64))
        f.write("const int KEILA_SPAWN_COUNT = %d;\n" % len(spawns))
        f.write("const float KEILA_SPAWNS[][2] = {\n%s\n};\n"
                % fmt(["{%.1ff,%.1ff}" % s for s in spawns], 8))


def main():
    print("dtm")
    lo, heights = bake_heights(Dtm())
    print("height base", lo, "range cm", min(heights), max(heights))
    buildings = bake_buildings()
    print("buildings", len(buildings), "walls", sum(len(b["ring"]) for b in buildings))
    spawns = bake_spawns(buildings)
    print("spawns", len(spawns))
    write_cpp(lo, heights, buildings, spawns, bake_bare(buildings))
    bake_surface()
    bake_ortho()
    features = load("e_401_hoone_ka")
    draw_map(Canvas(PLAY_HALF, 1024, 2, True), features).save(
        os.path.join(ROOT, "textures", "map_keila.png"), optimize=True)
    draw_map(Canvas(WORLD_HALF, 2048, 1, True), features).transpose(Image.FLIP_TOP_BOTTOM).save(
        os.path.join(ROOT, "_RESOURCES", "keila", "preview_north_up.png"))
    return 0


if __name__ == "__main__":
    sys.exit(main())
