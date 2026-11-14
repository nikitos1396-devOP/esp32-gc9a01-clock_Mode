import struct
from PIL import Image, ImageDraw, ImageFont

SIZE = 18
FONT_PATH = "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf"

font = ImageFont.truetype(FONT_PATH, SIZE)
ascent, descent = font.getmetrics()

codes = set(range(0x20, 0x7F))           # ASCII
codes |= set(range(0x410, 0x450))        # А-я
codes |= {0x401, 0x451, 0xB0}            # Ё, ё, °
codes = sorted(codes)                    # TFT_eSPI ищет двоичным поиском

meta, bitmaps = [], []
for cp in codes:
    ch = chr(cp)
    adv = int(round(font.getlength(ch)))
    l, t, r, b = font.getbbox(ch, anchor="ls")
    w, h = r - l, b - t
    if w > 0 and h > 0:
        img = Image.new("L", (w, h), 0)
        ImageDraw.Draw(img).text((-l, -t), ch, font=font, fill=255, anchor="ls")
        data = img.tobytes()
    else:
        w = h = 0
        data = b""
    meta.append((cp, h, w, adv, -t if h else 0, l if h else 0, 0))
    bitmaps.append(data)

out = struct.pack(">6i", len(codes), 11, SIZE, 0, ascent, descent)
for m in meta:
    out += struct.pack(">7i", *m)
out += b"".join(bitmaps)

with open("src/font_ru.h", "w") as f:
    f.write("#pragma once\n#include <Arduino.h>\n")
    f.write("static const uint8_t FontRu[] PROGMEM = {\n")
    for i in range(0, len(out), 16):
        f.write("  " + ", ".join("0x%02X" % x for x in out[i:i+16]) + ",\n")
    f.write("};\n")
print("font bytes:", len(out))
