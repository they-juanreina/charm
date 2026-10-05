#!/usr/bin/env python3
"""Convierte capturas .bmp1 del charm (250x122, 1 bpp, bit 1 = negro) en PNG para documentación.

  python3 tools/bmp1png.py <carpeta_bmp1> <carpeta_png>   convierte todas, a 3x con marco de pantalla
  python3 tools/bmp1png.py --sample <salida.bmp1>          genera una imagen de ejemplo tramada

Las PNG imitan la tinta: negro cálido sobre papel gris claro, ampliadas sin suavizar para que se vea
cada píxel, dentro de un marco oscuro que las separa del fondo de la página en tema claro u oscuro.
"""
import math
import os
import sys

from PIL import Image, ImageDraw

W, H, STRIDE = 250, 122, 32
INK, PAPER, BEZEL = (38, 36, 33), (236, 233, 225), (58, 56, 52)
SCALE, PAD, RADIUS = 3, 14, 14


def load(path):
    data = open(path, "rb").read()
    assert len(data) == STRIDE * H, f"{path}: {len(data)} bytes"
    im = Image.new("RGB", (W, H), PAPER)
    px = im.load()
    for y in range(H):
        for x in range(W):
            if data[y * STRIDE + (x >> 3)] & (0x80 >> (x & 7)):
                px[x, y] = INK
    return im


def framed(im):
    big = im.resize((W * SCALE, H * SCALE), Image.NEAREST)
    out = Image.new("RGBA", (big.width + 2 * PAD, big.height + 2 * PAD), (0, 0, 0, 0))
    ImageDraw.Draw(out).rounded_rectangle([0, 0, out.width - 1, out.height - 1], RADIUS, fill=BEZEL)
    out.paste(big, (PAD, PAD))
    return out


def sample(path):
    """Luna saliendo sobre el mar, en grises, tramada con Floyd-Steinberg como hace el portal."""
    g = [[0.0] * W for _ in range(H)]
    horizon = 78
    for y in range(H):
        for x in range(W):
            if y < horizon:
                v = 40 + 150 * (y / horizon) ** 1.6              # cielo: oscuro arriba, claro en el horizonte
                d = math.hypot(x - 168, y - 46)
                if d < 22:
                    v = 250 - 25 * (d / 22) ** 2                  # la luna
                elif d < 34:
                    v += 50 * (1 - (d - 22) / 12)                 # halo
            else:
                v = 70 + 30 * math.sin(y * 1.7) + 20 * math.sin(x * 0.31 + y)    # oleaje
                if abs(x - 168) < 16 - (y - horizon) * 0.25 and (y % 3):
                    v = 225                                       # reflejo de la luna
            g[y][x] = max(0.0, min(255.0, v))
    out = bytearray(STRIDE * H)
    for y in range(H):
        for x in range(W):
            old = g[y][x]
            new = 0 if old < 128 else 255
            e = old - new
            if x + 1 < W:
                g[y][x + 1] += e * 7 / 16
            if y + 1 < H:
                if x:
                    g[y + 1][x - 1] += e * 3 / 16
                g[y + 1][x] += e * 5 / 16
                if x + 1 < W:
                    g[y + 1][x + 1] += e / 16
            if new == 0:
                out[y * STRIDE + (x >> 3)] |= 0x80 >> (x & 7)
    open(path, "wb").write(out)


def main():
    if sys.argv[1] == "--sample":
        sample(sys.argv[2])
        return
    src, dst = sys.argv[1], sys.argv[2]
    os.makedirs(dst, exist_ok=True)
    for name in sorted(os.listdir(src)):
        if name.endswith(".bmp1"):
            png = os.path.join(dst, name[:-5] + ".png")
            framed(load(os.path.join(src, name))).save(png)
            print(png)


if __name__ == "__main__":
    main()
