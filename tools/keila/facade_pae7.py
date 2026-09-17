import os
import random

from PIL import Image, ImageDraw, ImageFilter

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
CELL = 512
BAY_M = 3.0
FLOOR_M = 3.2
PLINTH_M = 1.3
WALL = (236, 232, 221)
BOARD = (240, 238, 230)
ORANGE = (192, 96, 58)
FRAME = (58, 60, 64)
CONCRETE = (150, 148, 141)
rng = random.Random(7)


def speckle(img, amount):
    px = img.load()
    for y in range(img.size[1]):
        for x in range(img.size[0]):
            n = rng.randint(-amount, amount)
            r, g, b = px[x, y]
            px[x, y] = (max(0, min(255, r + n)), max(0, min(255, g + n)), max(0, min(255, b + n)))


def cell(base, height_m):
    img = Image.new("RGB", (CELL, CELL), base)
    return img, ImageDraw.Draw(img), CELL / BAY_M, CELL / height_m


def glass(d, x0, y0, x1, y1):
    d.rectangle((x0, y0, x1, y1), fill=FRAME)
    for a, b in ((x0 + 6, x0 + (x1 - x0) * 0.38), (x0 + (x1 - x0) * 0.38 + 6, x1 - 6)):
        top = (74, 92, 110)
        for row in range(int(y0 + 6), int(y1 - 6)):
            t = (row - y0) / (y1 - y0)
            d.line((a, row, b, row), fill=tuple(int(c * (1.0 - 0.35 * t)) for c in top))
        if rng.random() < 0.7:
            w = (b - a) * rng.uniform(0.25, 0.5)
            side = a if rng.random() < 0.5 else b - w
            d.rectangle((side, y0 + 8, side + w, y1 - 8), fill=(196, 194, 184))


def window_strip(d, sx, sy, with_panel):
    y0, y1 = CELL - 2.45 * sy, CELL - 0.95 * sy
    glass(d, 0.15 * sx, y0, 1.65 * sx, y1)
    if with_panel:
        d.rectangle((1.65 * sx, y0, 2.98 * sx, y1), fill=ORANGE)
        for k in range(1, 9):
            y = y0 + (y1 - y0) * k / 9
            d.line((1.65 * sx, y, 2.98 * sx, y), fill=(150, 70, 40), width=2)
    d.rectangle((0.10 * sx, y1, (2.98 if with_panel else 1.70) * sx, y1 + 5), fill=(40, 40, 42))


def boards(d, step_px):
    y = 0
    while y < CELL:
        d.line((0, y, CELL, y), fill=(198, 196, 188), width=2)
        d.line((0, y + 2, CELL, y + 2), fill=(250, 249, 244), width=1)
        y += step_px


def build():
    atlas = Image.new("RGB", (CELL * 4, CELL * 2), WALL)
    cells = []

    img, d, sx, sy = cell(WALL, FLOOR_M)
    window_strip(d, sx, sy, True)
    cells.append(img)

    img, d, sx, sy = cell(WALL, FLOOR_M)
    window_strip(d, sx, sy, False)
    cells.append(img)

    img, d, sx, sy = cell(BOARD, FLOOR_M)
    boards(d, 0.22 * sy)
    cells.append(img)

    img, d, sx, sy = cell(WALL, FLOOR_M)
    cells.append(img)

    img, d, sx, sy = cell(CONCRETE, PLINTH_M)
    d.line((CELL // 2, 0, CELL // 2, CELL), fill=(120, 118, 112), width=3)
    d.rectangle((0, 0, CELL, 10), fill=(176, 174, 166))
    cells.append(img)

    img, d, sx, sy = cell(CONCRETE, PLINTH_M)
    d.rectangle((0, 0, CELL, 10), fill=(176, 174, 166))
    d.rectangle((0.9 * sx, CELL - 0.95 * sy, 2.1 * sx, CELL - 0.45 * sy), fill=FRAME)
    d.rectangle((0.9 * sx + 8, CELL - 0.95 * sy + 8, 2.1 * sx - 8, CELL - 0.45 * sy - 8), fill=(60, 74, 86))
    cells.append(img)

    img, d, sx, sy = cell(WALL, FLOOR_M)
    d.rectangle((0.7 * sx, CELL - 2.55 * sy, 2.3 * sx, CELL), fill=(222, 196, 140))
    d.rectangle((0.5 * sx, CELL - 2.75 * sy, 2.5 * sx, CELL - 2.55 * sy), fill=(44, 44, 46))
    d.rectangle((1.0 * sx, CELL - 2.2 * sy, 2.0 * sx, CELL), fill=FRAME)
    d.rectangle((1.12 * sx, CELL - 2.05 * sy, 1.88 * sx, CELL - 0.9 * sy), fill=(66, 80, 94))
    cells.append(img)

    img, d, sx, sy = cell(WALL, FLOOR_M)
    d.rectangle((1.1 * sx, 0, 1.9 * sx, CELL), fill=FRAME)
    for k in range(3):
        y0 = k * CELL / 3 + 6
        d.rectangle((1.1 * sx + 6, y0, 1.9 * sx - 6, y0 + CELL / 3 - 12), fill=(70, 88, 106))
    cells.append(img)

    for i, c in enumerate(cells):
        speckle(c, 5)
        c = c.filter(ImageFilter.GaussianBlur(0.6))
        atlas.paste(c, ((i % 4) * CELL, (i // 4) * CELL))
    atlas.save(os.path.join(ROOT, "textures", "keila_pae7.png"), optimize=True)


if __name__ == "__main__":
    build()
