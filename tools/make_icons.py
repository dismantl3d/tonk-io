"""Builds the game's stat and superpower icons from the source art (1024x1024 PNGs on black).

    python tools/make_icons.py <folder with 1.png .. 16.png>

Writes web/icons/:
  atlas.png      17 cells of 128x128 stacked top to bottom, for the game's renderer (a texture array).
                 Stat cells (0-4, and 16 for Field of View) are masks the game colors itself: R = how much is icon, G = how much is rim,
                 the rest is the light fill; A = coverage. Superpower cells (5-15) are in full color.
  stat-<k>.png   each stat icon in its own HUD color (k = the stat's index in the game).
  pow-<k>.png    each superpower icon (k = the Power's index in the game).
The black background is cut away (with soft edges), every icon is cropped to its own outline and scaled to the
same size, and Lightning's bolt is brought down to the size of the other icons (its art came out bigger, with a thinner rim).
"""
import os
import sys
from collections import deque

from PIL import Image, ImageFilter

SRC = sys.argv[1] if len(sys.argv) > 1 else '.'
OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'web', 'icons')
CELL, PAD = 128, 4

# Source image -> stat index (Move Speed, Bullet Speed, Bullet Range, Max Health, Fire Rate, Bullet Damage, FOV).
STAT_SRC = {0: 1, 1: 4, 2: 2, 4: 5, 5: 3, 6: 17}
STAT_CELL = {0: 0, 1: 1, 2: 2, 4: 3, 5: 4, 6: 16}
STAT_COL = {0: '#29b5f5', 1: '#ff8c38', 2: '#9e66ff', 4: '#ff5d8f', 5: '#ff4d4d', 6: '#2ec4b6'}  # web/ui.js STATS
# Source image -> Power index (core/powerups.h).
POW_SRC = {1: 6, 2: 7, 3: 8, 5: 9, 4: 10, 6: 11, 7: 12, 8: 13, 9: 14, 11: 15, 12: 16}
POW_ORDER = [1, 2, 3, 4, 5, 6, 7, 8, 9, 11, 12]  # cells 5..15


def hexrgb(h):
    h = h.lstrip('#')
    return tuple(int(h[i:i + 2], 16) for i in (0, 2, 4))


def mix(a, b, k):
    return tuple(a[i] + (b[i] - a[i]) * k for i in range(3))


def cut_out(path):
    """Loads an icon and finds the background: everything dark that is connected to the image's edge.
    Returns the RGB image, its alpha (0-255, soft at the outline) and the outline's color."""
    im = Image.open(path).convert('RGB')
    w, h = im.size
    px = im.load()
    bright = lambda p: max(p)
    seen = bytearray(w * h)
    q = deque()
    for x in range(w):
        q.append((x, 0)); q.append((x, h - 1))
    for y in range(h):
        q.append((0, y)); q.append((w - 1, y))
    bg = []
    while q:
        x, y = q.popleft()
        i = y * w + x
        if seen[i]:
            continue
        seen[i] = 1
        if bright(px[x, y]) > 110:
            continue
        bg.append((x, y))
        if x > 0: q.append((x - 1, y))
        if x < w - 1: q.append((x + 1, y))
        if y > 0: q.append((x, y - 1))
        if y < h - 1: q.append((x, y + 1))
    alpha = Image.new('L', (w, h), 255)
    ap = alpha.load()
    isbg = bytearray(w * h)
    for x, y in bg:
        isbg[y * w + x] = 1
    # The outline's color: the brightest-sided pixels just inside the background.
    edge = []
    for x, y in bg[::97]:
        for dx, dy in ((4, 0), (-4, 0), (0, 4), (0, -4)):
            xx, yy = x + dx * 3, y + dy * 3
            if 0 <= xx < w and 0 <= yy < h and not isbg[yy * w + xx]:
                edge.append(px[xx, yy])
    edge.sort(key=bright)
    rim = edge[len(edge) // 2]
    top = max(bright(rim), 1)
    for x, y in bg:
        ap[x, y] = max(0, min(255, int((bright(px[x, y]) - 14) * 255 / (top - 14))))
        px[x, y] = rim  # transparent pixels take the rim's color (no dark fringe when scaled down)
    return im, alpha, rim


def crop_square(im, alpha):
    box = alpha.point(lambda v: 255 if v > 40 else 0).getbbox()
    x0, y0, x1, y1 = box
    s = max(x1 - x0, y1 - y0)
    cx, cy = (x0 + x1) / 2, (y0 + y1) / 2
    b = (int(cx - s / 2), int(cy - s / 2), int(cx - s / 2) + s, int(cy - s / 2) + s)
    return im.crop(b), alpha.crop(b)


def stat_layers(path):
    """A stat icon as three weights: icon (dark), rim, fill (light); plus coverage. Returns an RGBA mask image."""
    im, alpha, rim = cut_out(path)
    im, alpha = crop_square(im, alpha)
    g = im.convert('L')
    w, h = g.size
    gp, ap = g.load(), alpha.load()
    cx, cy, R = w / 2, h / 2, w / 2
    # Where the rim ends inside: walk in from the edge until the light fill starts (median of 16 directions).
    import math
    ins = []
    for k in range(16):
        a = k * math.pi / 8
        r = R - 3
        while r > R * 0.5 and gp[int(cx + math.cos(a) * r), int(cy + math.sin(a) * r)] < 200:
            r -= 1
        ins.append(r)
    ins.sort()
    rin = ins[8]
    FILL, RIM, ICON = 232.0, 140.0, 113.0
    out = Image.new('RGBA', (w, h))
    op = out.load()
    for y in range(h):
        for x in range(w):
            L = gp[x, y]
            d = math.hypot(x + 0.5 - cx, y + 0.5 - cy)
            if d > rin - 6:  # the rim (and its soft inner edge)
                rw = max(0.0, min(1.0, (FILL - L) / (FILL - RIM)))
                iw = 0.0
            else:
                rw = 0.0
                iw = max(0.0, min(1.0, (FILL - L) / (FILL - ICON)))
            if ap[x, y] < 255:  # the outer edge: all rim
                rw, iw = 1.0, 0.0
            op[x, y] = (int(iw * 255 + 0.5), int(rw * 255 + 0.5), 0, ap[x, y])
    return out


def power_rgba(path, k):
    im, alpha, rim = cut_out(path)
    im, alpha = crop_square(im, alpha)
    if k == 8:
        im, alpha = fix_lightning(im, alpha, rim)
    out = im.convert('RGBA')
    out.putalpha(alpha)
    return out


def fix_lightning(im, alpha, rim):
    """Lightning's art came out bigger than the others: a thinner rim and a bigger bolt once it's scaled to the same
    outline. Thicken the rim to match the others (about 7.4% of the size) and shrink the bolt to 90%."""
    w, h = im.size
    fill = (249, 197, 193)
    px = im.load()
    # The bolt alone (how red each pixel inside is), shrunk around the middle.
    def erode(frac):  # the shape shrunk by frac of its size (padded first: the filter copies the image's edge outward)
        q = w // 4
        small = Image.new('L', (q + 8, q + 8), 0)
        small.paste(alpha.resize((q, q), Image.BILINEAR), (4, 4))
        for _ in range(int(q * frac)):
            small = small.filter(ImageFilter.MinFilter(3))
        return small.crop((4, 4, q + 4, q + 4)).resize((w, h), Image.BILINEAR)
    inner = erode(0.074).point(lambda v: 255 if v > 128 else 0).filter(ImageFilter.GaussianBlur(1.2))
    bolt = Image.new('L', (w, h), 0)
    bp = bolt.load()
    old_inner = erode(0.06).load()
    for y in range(h):
        for x in range(w):
            if old_inner[x, y] > 200:
                g = px[x, y][1]
                bp[x, y] = max(0, min(255, int((197 - g) * 255 / (197 - 50))))
    s = 0.9
    bw = int(w * s)
    shrunk = bolt.resize((bw, bw), Image.LANCZOS)
    bolt2 = Image.new('L', (w, h), 0)
    bolt2.paste(shrunk, ((w - bw) // 2, (h - bw) // 2))
    out = Image.new('RGB', (w, h), rim)
    op = out.load()
    ip, b2 = inner.load(), bolt2.load()
    for y in range(h):
        for x in range(w):
            t = ip[x, y] / 255.0
            c = mix(rim, fill, t)                 # rim outside the inner square, fill inside
            c = mix(c, rim, b2[x, y] / 255.0 * t)  # the bolt, in the rim's red
            op[x, y] = tuple(int(v + 0.5) for v in c)
    return out, alpha


def fit(img, size, pad):
    inner = size - 2 * pad
    sm = img.resize((inner, inner), Image.LANCZOS)
    cell = Image.new('RGBA', (size, size), (0, 0, 0, 0))
    # Padding takes the edge's color (fully transparent) so scaling down never pulls in black.
    edge = sm.getpixel((inner // 2, 0))
    cell.paste(Image.new('RGBA', (size, size), edge[:3] + (0,)), (0, 0))
    cell.paste(sm, (pad, pad))
    return cell


def colored(mask, col):
    """A stat mask in a stat's colors: rim in its color, light fill, icon in a deeper shade (as the HUD draws)."""
    c = hexrgb(col)
    fill, dark = mix(c, (255, 255, 255), 0.8), mix(c, (0, 0, 0), 0.25)
    out = Image.new('RGBA', mask.size)
    mp, op = mask.load(), out.load()
    for y in range(mask.size[1]):
        for x in range(mask.size[0]):
            iw, rw, _, a = mp[x, y]
            v = mix(mix(fill, c, rw / 255.0), dark, iw / 255.0)
            op[x, y] = tuple(int(t + 0.5) for t in v) + (a,)
    return out


def main():
    os.makedirs(OUT, exist_ok=True)
    atlas = Image.new('RGBA', (CELL, CELL * 17), (0, 0, 0, 0))
    for k, src in STAT_SRC.items():
        m = stat_layers(os.path.join(SRC, f'{src}.png'))
        atlas.paste(fit(m, CELL, PAD), (0, CELL * STAT_CELL[k]))
        colored(m.resize((CELL, CELL), Image.LANCZOS), STAT_COL[k]).save(os.path.join(OUT, f'stat-{k}.png'), optimize=True)
        print('stat', k)
    for i, k in enumerate(POW_ORDER):
        p = power_rgba(os.path.join(SRC, f'{POW_SRC[k]}.png'), k)
        atlas.paste(fit(p, CELL, PAD), (0, CELL * (5 + i)))
        p.resize((CELL, CELL), Image.LANCZOS).save(os.path.join(OUT, f'pow-{k}.png'), optimize=True)
        print('power', k)
    atlas.save(os.path.join(OUT, 'atlas.png'), optimize=True)


if __name__ == '__main__':
    main()
