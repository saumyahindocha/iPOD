#!/usr/bin/env python3
"""Turn any picture into a Pod home-screen wallpaper (wallpaper.uf2).

    python3 make_wallpaper.py photo.jpg                 # centre crop
    python3 make_wallpaper.py photo.jpg --focus 0.55    # 0 = keep the top, 1 = keep the bottom
    python3 make_wallpaper.py photo.jpg -o wallpaper.uf2 --preview preview.png

Flash it like firmware: hold BOOTSEL, plug in, drag wallpaper.uf2 onto RP2350.
It only writes the wallpaper area of flash (POD_WALLPAPER_OFFSET in common/pod_wallpaper.h),
so it survives firmware updates, and firmware updates don't erase it.
Needs Pillow:  pip install pillow
"""
import argparse
import struct
from PIL import Image

FLASH_BASE = 0x10000000
WALLPAPER_OFFSET = 0x300000          # keep in sync with POD_WALLPAPER_OFFSET
W, H = 240, 320
MAGIC = b"PODW"
FAMILY_ABSOLUTE = 0xE48BFF57         # RP2350 "absolute" UF2 family: written exactly where addressed


def to_rgb565_be(img):
    raw = img.tobytes()
    out = bytearray()
    for i in range(0, len(raw), 3):
        r, g, b = raw[i], raw[i + 1], raw[i + 2]
        v = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)
        out += struct.pack(">H", v)   # big-endian = the display's byte order, so the Pod can copy it straight
    return bytes(out)


def make_uf2(data, addr):
    blocks = [data[i:i + 256] for i in range(0, len(data), 256)]
    out = bytearray()
    for n, chunk in enumerate(blocks):
        chunk = chunk.ljust(256, b"\x00")
        hdr = struct.pack("<8I", 0x0A324655, 0x9E5D5157, 0x00002000, addr + n * 256, 256, n, len(blocks), FAMILY_ABSOLUTE)
        out += hdr + chunk + b"\x00" * (476 - 256) + struct.pack("<I", 0x0AB16F30)
    return bytes(out)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("image")
    ap.add_argument("-o", "--out", default="wallpaper.uf2")
    ap.add_argument("--focus", type=float, default=0.5, help="vertical crop position, 0 (top) .. 1 (bottom)")
    ap.add_argument("--hfocus", type=float, default=0.5, help="horizontal crop position, 0 (left) .. 1 (right)")
    ap.add_argument("--preview", help="also save the cropped 240x320 picture here")
    a = ap.parse_args()

    img = Image.open(a.image).convert("RGB")
    s = max(W / img.width, H / img.height)                      # cover the screen, then crop
    img = img.resize((max(W, round(img.width * s)), max(H, round(img.height * s))), Image.LANCZOS)
    x = round((img.width - W) * min(max(a.hfocus, 0), 1))
    y = round((img.height - H) * min(max(a.focus, 0), 1))
    img = img.crop((x, y, x + W, y + H))
    if a.preview:
        img.save(a.preview)

    pixels = to_rgb565_be(img)
    header = MAGIC + struct.pack("<HHII", W, H, len(pixels), sum(pixels) & 0xFFFFFFFF)
    blob = header.ljust(32, b"\x00") + pixels
    with open(a.out, "wb") as f:
        f.write(make_uf2(blob, FLASH_BASE + WALLPAPER_OFFSET))
    print(f"{a.out}: {W}x{H} wallpaper, {len(blob)} bytes at flash offset 0x{WALLPAPER_OFFSET:X}")


if __name__ == "__main__":
    main()
