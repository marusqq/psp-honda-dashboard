#!/usr/bin/env python3
"""Generate assets/icon0.png (144x80) for PSP OBD Dashboard game menu."""
import struct, zlib, math, os

W, H = 144, 80

def make_png(pixels):
    def chunk(name, data):
        c = struct.pack('>I', len(data)) + name + data
        c += struct.pack('>I', zlib.crc32(name + data) & 0xFFFFFFFF)
        return c
    sig  = b'\x89PNG\r\n\x1a\n'
    ihdr = chunk(b'IHDR', struct.pack('>IIBBBBB', W, H, 8, 2, 0, 0, 0))
    raw  = b''
    for row in pixels:
        raw += b'\x00'
        for r, g, b in row:
            raw += bytes([r & 0xFF, g & 0xFF, b & 0xFF])
    idat = chunk(b'IDAT', zlib.compress(raw, 9))
    iend = chunk(b'IEND', b'')
    return sig + ihdr + idat + iend

pixels = [[(12, 12, 18)] * W for _ in range(H)]

def sp(x, y, c):
    if 0 <= x < W and 0 <= y < H:
        pixels[y][x] = c

def frect(x, y, w, h, c):
    for dy in range(h):
        for dx in range(w):
            sp(x + dx, y + dy, c)

def lerp_col(a, b, t):
    return tuple(int(a[i] + (b[i] - a[i]) * t) for i in range(3))

# Dark blue-gray gradient background
for y in range(H):
    t = y / H
    c = lerp_col((10, 10, 16), (20, 20, 30), t)
    for x in range(W):
        pixels[y][x] = c

# Subtle radial vignette effect (darker corners)
for y in range(H):
    for x in range(W):
        dx = (x - W/2) / (W/2)
        dy = (y - H/2) / (H/2)
        dist = math.sqrt(dx*dx + dy*dy)
        factor = max(0.0, 1.0 - dist * 0.35)
        r, g, b = pixels[y][x]
        pixels[y][x] = (int(r * factor), int(g * factor), int(b * factor))

# Honda H logo - red, left-center
HX, HY = 16, 16
S = 5   # 5px per unit, H is 6 units wide x 7 units tall = 30x35px

red_hi  = (240, 55, 55)
red_mid = (210, 18, 18)
red_sh  = (150,  8,  8)

# Left vertical bar
frect(HX,       HY,       S*2, S*7, red_mid)
# Right vertical bar
frect(HX + S*4, HY,       S*2, S*7, red_mid)
# Centre bar (1 unit tall, full width)
frect(HX,       HY + S*3, S*6, S,   red_mid)

# Highlight top and left edges of each bar
for i in range(S*7):
    sp(HX,           HY + i, red_hi)   # left edge, left bar
    sp(HX + S*4,     HY + i, red_hi)   # left edge, right bar
for i in range(S*2):
    sp(HX + i,       HY,     red_hi)   # top, left bar
    sp(HX + S*4 + i, HY,     red_hi)   # top, right bar

# Shadow right and bottom edges
for i in range(S*7):
    sp(HX + S*2 - 1, HY + i, red_sh)  # right edge, left bar
    sp(HX + S*6 - 1, HY + i, red_sh)  # right edge, right bar
for i in range(S*6):
    sp(HX + i, HY + S*7 - 1, red_sh)  # bottom row

# Speedometer arc - right portion of icon
CX, CY = 106, 40
R = 28

a_start = math.pi * 0.75    # 135 deg
a_end   = math.pi * 2.25    # 405 deg (270-deg sweep)
STEPS   = 360

def arc_col(frac):
    if frac < 0.55:
        return (30, 180, 70)   # green
    elif frac < 0.78:
        return (200, 175, 35)  # yellow
    else:
        return (210, 38, 38)   # red

# Dark track (2px wide: R-5 to R-4)
for i in range(STEPS):
    a = a_start + (a_end - a_start) * i / STEPS
    for rr in range(R - 6, R + 2):
        x = int(CX + rr * math.cos(a))
        y = int(CY + rr * math.sin(a))
        sp(x, y, (40, 42, 52))

# Coloured fill arc (3px: R-5 to R-2)
for i in range(STEPS):
    frac = i / STEPS
    a    = a_start + (a_end - a_start) * frac
    col  = arc_col(frac)
    for rr in range(R - 5, R - 1):
        x = int(CX + rr * math.cos(a))
        y = int(CY + rr * math.sin(a))
        sp(x, y, col)

# Outer bright rim (1px at R)
for i in range(STEPS):
    a   = a_start + (a_end - a_start) * i / STEPS
    frac = i / STEPS
    col  = arc_col(frac)
    x = int(CX + R * math.cos(a))
    y = int(CY + R * math.sin(a))
    sp(x, y, tuple(min(255, v + 40) for v in col))

# Major tick marks (9 marks = 0 to 8 krpm)
for k in range(9):
    frac = k / 8.0
    a = a_start + (a_end - a_start) * frac
    for rr in range(R - 11, R - 6):
        x = int(CX + rr * math.cos(a))
        y = int(CY + rr * math.sin(a))
        sp(x, y, (210, 210, 215))

# Minor ticks (4 between each major = 40 total)
for k in range(40):
    frac = k / 40.0
    a = a_start + (a_end - a_start) * frac
    for rr in range(R - 9, R - 7):
        x = int(CX + rr * math.cos(a))
        y = int(CY + rr * math.sin(a))
        sp(x, y, (130, 130, 140))

# Needle at ~42% (cruising RPM, ~3400 in green zone)
na = a_start + (a_end - a_start) * 0.42
steps_n = R - 6
for step in range(steps_n):
    frac = step / steps_n
    nx = int(CX + math.cos(na) * (R - 7) * frac)
    ny = int(CY + math.sin(na) * (R - 7) * frac)
    sp(nx, ny, (255, 65, 35))
    # Thickness +1 perpendicular
    px = int(math.cos(na + math.pi / 2))
    py = int(math.sin(na + math.pi / 2))
    sp(nx + px, ny + py, (255, 65, 35))

# Centre hub: 5x5 silver dot
frect(CX - 2, CY - 2, 5, 5, (170, 170, 178))
frect(CX - 1, CY - 1, 3, 3, (90,  90,  100))

# Thin dividing line between H logo and speedo
for y in range(8, H - 8):
    sp(64, y, (40, 42, 55))

# "OBD2" label bottom-left in dim text (3x5 pixel font)
FONT_3X5 = {
    'O': [0b111, 0b101, 0b101, 0b101, 0b111],
    'B': [0b110, 0b101, 0b110, 0b101, 0b110],
    'D': [0b110, 0b101, 0b101, 0b101, 0b110],
    '2': [0b111, 0b001, 0b111, 0b100, 0b111],
    'v': [0b000, 0b101, 0b101, 0b101, 0b010],
    '1': [0b010, 0b110, 0b010, 0b010, 0b111],
    '.': [0b000, 0b000, 0b000, 0b000, 0b010],
}

def draw_char(x, y, ch, col):
    rows = FONT_3X5.get(ch)
    if not rows:
        return
    for row, bits in enumerate(rows):
        for col_i in range(3):
            if bits & (1 << (2 - col_i)):
                sp(x + col_i * 2, y + row * 2, col)
                sp(x + col_i * 2 + 1, y + row * 2, col)
                sp(x + col_i * 2, y + row * 2 + 1, col)

label_col = (90, 90, 105)
tx = 16
ty = H - 22
for ch in 'OBD2':
    draw_char(tx, ty, ch, label_col)
    tx += 9

# "v1.0" below
tx2 = 17
ty2 = H - 10
for ch in 'v1.0':
    if ch == '.':
        sp(tx2, ty2 + 4, (60, 60, 75))
        tx2 += 4
    else:
        draw_char(tx2, ty2, ch, (55, 55, 70))
        tx2 += 8

# Outer border (1px dark frame)
for x in range(W):
    sp(x, 0,   (5, 5, 10))
    sp(x, H-1, (5, 5, 10))
for y in range(H):
    sp(0,   y, (5, 5, 10))
    sp(W-1, y, (5, 5, 10))

os.makedirs(os.path.join(os.path.dirname(__file__), '..', 'assets'), exist_ok=True)
out = os.path.join(os.path.dirname(__file__), '..', 'assets', 'icon0.png')
with open(out, 'wb') as f:
    f.write(make_png(pixels))
print(f"Generated {os.path.abspath(out)} ({W}x{H} RGB PNG)")
