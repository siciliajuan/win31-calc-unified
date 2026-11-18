"""Generates the fallback calc.ico: 32x32, 16 colors (Windows 3.x compatible)."""
import struct, sys

# Standard VGA palette (index -> RGB)
PAL = [(0,0,0),(128,0,0),(0,128,0),(128,128,0),(0,0,128),(128,0,128),(0,128,128),(192,192,192),
       (128,128,128),(255,0,0),(0,255,0),(255,255,0),(0,0,255),(255,0,255),(0,255,255),(255,255,255)]
BLACK, NAVY, LGRAY, DGRAY, RED, BLUE, WHITE = 0, 4, 7, 8, 9, 12, 15
T = None  # transparent

px = [[T]*32 for _ in range(32)]

def rect(x0, y0, x1, y1, c):
    for y in range(y0, y1 + 1):
        for x in range(x0, x1 + 1):
            px[y][x] = c

# Raised body: black outline, highlight top/left, shadow bottom/right
rect(5, 1, 26, 30, BLACK)
rect(6, 2, 25, 29, LGRAY)
rect(6, 2, 24, 2, WHITE); rect(6, 2, 6, 28, WHITE)
rect(7, 29, 25, 29, DGRAY); rect(25, 3, 25, 29, DGRAY)
# Sunken display
rect(8, 4, 23, 9, DGRAY)
rect(9, 5, 23, 9, WHITE)
rect(9, 5, 22, 5, BLACK); rect(9, 5, 9, 8, BLACK)
rect(19, 7, 21, 7, BLACK)  # "digits"
# 4x4 keys
for row in range(4):
    for col in range(4):
        x, y = 8 + col * 4, 12 + row * 4
        color = RED if col == 3 else BLUE
        rect(x, y, x + 2, y + 2, WHITE)
        rect(x + 1, y + 1, x + 2, y + 2, DGRAY)
        rect(x + 1, y + 1, x + 1, y + 1, color)
# Wide '=' key
rect(8, 28, 23, 28, LGRAY)

# XOR (4 bpp, bottom-up rows) and AND (1 bpp)
xor = bytearray()
andm = bytearray()
for y in range(31, -1, -1):
    row = px[y]
    for x in range(0, 32, 2):
        a = row[x] if row[x] is not None else 0
        b = row[x + 1] if row[x + 1] is not None else 0
        xor.append((a << 4) | b)
    bits = 0
    rowb = bytearray()
    for x in range(32):
        bits = (bits << 1) | (1 if row[x] is None else 0)
        if x % 8 == 7:
            rowb.append(bits); bits = 0
    andm += rowb  # 4 bytes per row, already 32-bit aligned

pal = b"".join(struct.pack("<BBBB", b, g, r, 0) for (r, g, b) in PAL)
bih = struct.pack("<IiiHHIIiiII", 40, 32, 64, 1, 4, 0, len(xor) + len(andm), 0, 0, 16, 0)
img = bih + pal + bytes(xor) + bytes(andm)
header = struct.pack("<HHH", 0, 1, 1)
entry = struct.pack("<BBBBHHII", 32, 32, 16, 0, 1, 4, len(img), 6 + 16)
open(sys.argv[1], "wb").write(header + entry + img)
