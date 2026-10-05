#!/usr/bin/env python3
"""Convierte una fuente TTF/OTF/WOFF2 a un header GFXfont (Adafruit_GFX) con rango cp1252 0x20-0xFF.

Rasteriza en modo 1 bit al tamaño en píxeles indicado. Para fuentes de píxel hay que usar su tamaño nativo
(Departure Mono 11, Spleen 6x12 -> 12, PixelOperator8 -> 8, etc.) o los glifos salen rotos.

  python3 tools/fontconv.py tools/fonts/DepartureMono.woff2 11 DepartureMono11 src/fonts/DepartureMono11.h
  python3 tools/fontconv.py --preview out.png FUENTE PX   (solo vista previa de una página del charm)
"""
import argparse
import io
import os
import sys

from PIL import Image, ImageDraw, ImageFont


def load_font(path, px):
    if path.lower().endswith(".woff2"):
        from fontTools.ttLib import TTFont
        f = TTFont(path)
        f.flavor = None
        buf = io.BytesIO()
        f.save(buf)
        buf.seek(0)
        return ImageFont.truetype(buf, px)
    return ImageFont.truetype(path, px)


def raster(font, ch):
    """Devuelve (bitmap 2D de 0/1, ancho, alto, xoff, yoff_desde_baseline, avance)."""
    ascent, _ = font.getmetrics()
    adv = font.getlength(ch)
    adv = int(round(adv))
    bbox = font.getbbox(ch, mode="1")  # (x0, y0, x1, y1) relativo al origen del texto (y0 desde la parte superior)
    if bbox is None or bbox[2] <= bbox[0] or bbox[3] <= bbox[1]:
        return [], 0, 0, 0, 0, adv
    w, h = bbox[2] - bbox[0], bbox[3] - bbox[1]
    im = Image.new("1", (w + 2, h + 2), 0)
    d = ImageDraw.Draw(im)
    d.text((-bbox[0] + 1, -bbox[1] + 1), ch, font=font, fill=1)
    px = im.load()
    rows = [[px[x + 1, y + 1] for x in range(w)] for y in range(h)]
    # recortar filas/columnas vacías por si el bbox sobra
    while rows and not any(rows[0]):
        rows.pop(0); bbox = (bbox[0], bbox[1] + 1, bbox[2], bbox[3])
    while rows and not any(rows[-1]):
        rows.pop(); bbox = (bbox[0], bbox[1], bbox[2], bbox[3] - 1)
    if not rows:
        return [], 0, 0, 0, 0, adv
    w, h = len(rows[0]), len(rows)
    yoff = bbox[1] - ascent  # negativo = por encima de la línea base
    return rows, w, h, bbox[0], yoff, adv


def convert(path, px, name, out, first=0x20, last=0xFF):
    font = load_font(path, px)
    ascent, descent = font.getmetrics()
    bitmaps = bytearray()
    glyphs = []
    for code in range(first, last + 1):
        try:
            ch = bytes([code]).decode("cp1252")
        except UnicodeDecodeError:
            ch = "�"
        rows, w, h, xo, yo, adv = raster(font, ch)
        off = len(bitmaps)
        bits = [b for r in rows for b in r]
        for i in range(0, len(bits), 8):
            chunk = bits[i:i + 8] + [0] * (8 - len(bits[i:i + 8]))
            bitmaps.append(sum(v << (7 - k) for k, v in enumerate(chunk)))
        glyphs.append((off, w, h, adv, xo, yo, code, ch))
    lines = [f"// Generado por tools/fontconv.py desde {os.path.basename(path)} a {px} px, cp1252 0x{first:02X}-0x{last:02X}. No editar.",
             "#pragma once", "#include <gfxfont.h>", f"const uint8_t {name}Bitmaps[] PROGMEM = {{"]
    for i in range(0, len(bitmaps), 16):
        lines.append("  " + ", ".join(f"0x{b:02X}" for b in bitmaps[i:i + 16]) + ",")
    lines.append("};")
    lines.append(f"const GFXglyph {name}Glyphs[] PROGMEM = {{")
    for off, w, h, adv, xo, yo, code, ch in glyphs:
        label = ch if 0x20 < code < 0x7F and ch not in "*/\\" else ""
        lines.append(f"  {{ {off:5d}, {w:3d}, {h:3d}, {adv:3d}, {xo:3d}, {yo:4d} }}, // 0x{code:02X} {label}")
    lines.append("};")
    lines.append(f"const GFXfont {name} PROGMEM = {{ (uint8_t *){name}Bitmaps, (GFXglyph *){name}Glyphs, 0x{first:02X}, 0x{last:02X}, {ascent + descent} }};")
    with open(out, "w") as f:
        f.write("\n".join(lines) + "\n")
    print(f"{out}: {len(glyphs)} glifos, {len(bitmaps)} B, línea {ascent + descent} px, avance 'H' {glyphs[ord('H') - first][3]} px")


SAMPLE = ("Hombres necios que acusáis\na la mujer sin razón,\nsin ver que sois la ocasión\nde lo mismo que culpáis.\n\n"
          "Si con ansia sin igual\nsolicitáis su desdén,\n¿por qué queréis que obren bien\nsi las incitáis al mal? ¡Ñandú! “—”")


def preview(path, px, out, title_font=None, title_px=None, scale=3):
    font = load_font(path, px)
    asc, desc = font.getmetrics()
    lh = asc + desc
    im = Image.new("1", (250, 122), 1)
    d = ImageDraw.Draw(im)
    y = 4
    if title_font:
        tf = load_font(title_font, title_px)
        ta, td = tf.getmetrics()
        d.text((6, y), "Hombres necios", font=tf, fill=0)
        y += ta + td + 4
    adv = font.getlength("n")
    maxc = int(238 // adv)
    for line in SAMPLE.split("\n"):
        while True:
            if y + lh > 122 - 10:
                break
            if len(line) <= maxc:
                d.text((6, y), line, font=font, fill=0)
                y += lh
                break
            cut = line.rfind(" ", 0, maxc)
            cut = cut if cut > 0 else maxc
            d.text((6, y), line[:cut], font=font, fill=0)
            y += lh
            line = "  " + line[cut:].lstrip()
    d.text((250 - 6 - font.getlength("1/2"), 122 - lh - 1), "1/2", font=font, fill=0)
    big = im.resize((250 * scale, 122 * scale), Image.NEAREST)
    big.save(out)
    return lh, maxc


if __name__ == "__main__":
    ap = argparse.ArgumentParser()
    ap.add_argument("font")
    ap.add_argument("px", type=int)
    ap.add_argument("name", nargs="?")
    ap.add_argument("out", nargs="?")
    ap.add_argument("--preview")
    ap.add_argument("--title-font")
    ap.add_argument("--title-px", type=int)
    a = ap.parse_args()
    if a.preview:
        lh, maxc = preview(a.font, a.px, a.preview, a.title_font, a.title_px)
        print(f"{a.preview}: línea {lh} px, ~{maxc} caracteres por línea")
    else:
        if not (a.name and a.out):
            sys.exit("faltan nombre y archivo de salida")
        convert(a.font, a.px, a.name, a.out)
