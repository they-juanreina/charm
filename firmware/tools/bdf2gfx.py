#!/usr/bin/env python3
"""Convierte una fuente BDF (bitmap dibujada a mano, p. ej. las X11 de U8g2) a un header GFXfont cp1252 0x20-0xFF.

  python3 tools/bdf2gfx.py fuente.bdf Nombre src/fonts/Nombre.h [--preview out.png --title fuente_titulo.bdf]

Los glifos 0x80-0x9F (comillas tipográficas, guiones largos, puntos suspensivos) no existen en las fuentes
ISO-8859-1: se sintetizan con equivalentes ASCII para que el re-codificador del firmware no muestre '?'.
"""
import argparse
import os

from PIL import Image, ImageDraw


def parse_bdf(path):
    glyphs = {}
    meta = {}
    with open(path, "r", encoding="latin-1") as f:
        cur = None
        in_bitmap = False
        rows = []
        for line in f:
            t = line.split()
            if not t:
                continue
            k = t[0]
            if k == "FONT_ASCENT":
                meta["ascent"] = int(t[1])
            elif k == "FONT_DESCENT":
                meta["descent"] = int(t[1])
            elif k == "STARTCHAR":
                cur = {}
                rows = []
            elif k == "ENCODING":
                cur["code"] = int(t[1])
            elif k == "DWIDTH":
                cur["adv"] = int(t[1])
            elif k == "BBX":
                cur["w"], cur["h"], cur["xo"], cur["yo"] = map(int, t[1:5])
            elif k == "BITMAP":
                in_bitmap = True
            elif k == "ENDCHAR":
                in_bitmap = False
                w = cur.get("w", 0)
                bits = []
                for r in rows:
                    v = int(r, 16)
                    nbits = len(r) * 4
                    bits.append([(v >> (nbits - 1 - i)) & 1 for i in range(w)])
                cur["rows"] = bits
                if cur.get("code", -1) >= 0:
                    glyphs[cur["code"]] = cur
            elif in_bitmap:
                rows.append(k)
    return glyphs, meta


def trim(g):
    """Recorta filas/columnas vacías y devuelve (rows, w, h, xoff, yoff_baseline)."""
    rows = [r[:] for r in g["rows"]]
    w, h, xo, yo = g["w"], g["h"], g["xo"], g["yo"]
    while rows and not any(rows[0]):
        rows.pop(0); h -= 1
    while rows and not any(rows[-1]):
        rows.pop(); h -= 1; yo += 1
    if not rows:
        return [], 0, 0, 0, 0
    left = min(next(i for i, v in enumerate(r) if v) if any(r) else w for r in rows)
    right = max(max(i for i, v in enumerate(r) if v) if any(r) else -1 for r in rows)
    rows = [r[left:right + 1] for r in rows]
    w = right + 1 - left
    # BDF: yo = distancia del borde inferior del bitmap a la línea base (positivo hacia arriba)
    ytop = -(yo + h)  # offset del borde superior respecto a la línea base, en coordenadas de pantalla
    return rows, w, h, xo + left, ytop


def synth(glyphs, code):
    """Glifos cp1252 0x80-0x9F que no existen en Latin-1: equivalentes sencillos."""
    table = {0x91: 0x27, 0x92: 0x27, 0x93: 0x22, 0x94: 0x22, 0x96: 0x2D, 0x97: 0x2D, 0x82: 0x2C, 0x84: 0x22,
             0x8B: 0x3C, 0x9B: 0x3E, 0x95: 0xB7, 0x80: 0x45, 0x8A: 0x53, 0x9A: 0x73, 0x8E: 0x5A, 0x9E: 0x7A,
             0x9F: 0x59, 0x8C: 0x4F, 0x9C: 0x6F}
    if code in glyphs:
        return glyphs[code]
    if code == 0x85 and 0x2E in glyphs:  # … = tres puntos pegados
        dot = glyphs[0x2E]
        r, w, h, xo, yt = trim(dot)
        step = max(dot["adv"] - 1, w + 1)
        rows = [[0] * (step * 2 + w) for _ in range(h)]
        for k in range(3):
            for y in range(h):
                for x in range(w):
                    rows[y][k * step + x] = r[y][x]
        return {"rows": rows, "w": step * 2 + w, "h": h, "xo": xo, "yo": dot["yo"] + (dot["h"] - trim(dot)[2]) - 0, "adv": step * 3 + 1}
    if code in table and table[code] in glyphs:
        return glyphs[table[code]]
    return None


def convert(bdf, name, out, first=0x20, last=0xFF):
    glyphs, meta = parse_bdf(bdf)
    bitmaps = bytearray()
    table = []
    for code in range(first, last + 1):
        g = synth(glyphs, code)
        if g is None:
            g = glyphs.get(0x3F)  # '?'
        rows, w, h, xo, yt = trim(g)
        off = len(bitmaps)
        bits = [b for r in rows for b in r]
        for i in range(0, len(bits), 8):
            chunk = bits[i:i + 8] + [0] * (8 - len(bits[i:i + 8]))
            bitmaps.append(sum(v << (7 - k) for k, v in enumerate(chunk)))
        table.append((off, w, h, g["adv"], xo, yt, code))
    lh = meta["ascent"] + meta["descent"]
    lines = [f"// Generado por tools/bdf2gfx.py desde {os.path.basename(bdf)} (cp1252 0x{first:02X}-0x{last:02X}). No editar.",
             "#pragma once", "#include <gfxfont.h>", f"const uint8_t {name}Bitmaps[] PROGMEM = {{"]
    for i in range(0, len(bitmaps), 16):
        lines.append("  " + ", ".join(f"0x{b:02X}" for b in bitmaps[i:i + 16]) + ",")
    lines.append("};")
    lines.append(f"const GFXglyph {name}Glyphs[] PROGMEM = {{")
    for off, w, h, adv, xo, yt, code in table:
        ch = chr(code) if 0x20 < code < 0x7F and chr(code) not in "*/\\" else ""
        lines.append(f"  {{ {off:5d}, {w:3d}, {h:3d}, {adv:3d}, {xo:3d}, {yt:4d} }}, // 0x{code:02X} {ch}")
    lines.append("};")
    lines.append(f"const GFXfont {name} PROGMEM = {{ (uint8_t *){name}Bitmaps, (GFXglyph *){name}Glyphs, 0x{first:02X}, 0x{last:02X}, {lh} }};")
    with open(out, "w") as f:
        f.write("\n".join(lines) + "\n")
    print(f"{out}: {len(bitmaps)} B, línea {lh} px, ascent {meta['ascent']}")


def draw_text(im, glyphs, meta, x, baseline, text):
    px = im.load()
    for ch in text:
        code = ord(ch)
        try:
            code = ch.encode("cp1252")[0]
        except UnicodeEncodeError:
            code = 0x3F
        g = synth(glyphs, code) or glyphs.get(0x3F)
        rows, w, h, xo, yt = trim(g)
        for yy, r in enumerate(rows):
            for xx, v in enumerate(r):
                if v:
                    X, Y = x + xo + xx, baseline + yt + yy
                    if 0 <= X < im.width and 0 <= Y < im.height:
                        px[X, Y] = 0
        x += g["adv"]
    return x


def width(glyphs, text):
    return sum((synth(glyphs, ch.encode("cp1252", "replace")[0]) or glyphs[0x3F])["adv"] for ch in text)


SAMPLE = ("Hombres necios que acusáis\na la mujer sin razón,\nsin ver que sois la ocasión\nde lo mismo que culpáis.\n\n"
          "Si con ansia sin igual\nsolicitáis su desdén,\n¿por qué queréis que obren bien\nsi las incitáis al mal? ¡Ñandú! “—”…")


def preview(bdf, out, title_bdf=None, scale=3, label=None):
    glyphs, meta = parse_bdf(bdf)
    lh = meta["ascent"] + meta["descent"]
    im = Image.new("1", (250, 122), 1)
    y = 4
    if title_bdf:
        tg, tm = parse_bdf(title_bdf)
        draw_text(im, tg, tm, 6, y + tm["ascent"], "Hombres necios")
        y += tm["ascent"] + tm["descent"] + 4
    for line in SAMPLE.split("\n"):
        indent = 0
        while y + lh <= 122 - 10:
            if width(glyphs, line) + indent <= 238:
                draw_text(im, glyphs, meta, 6 + indent, y + meta["ascent"], line)
                y += lh
                break
            cut = len(line)
            while cut > 0 and width(glyphs, line[:cut]) + indent > 238:
                cut = line.rfind(" ", 0, cut)
            if cut <= 0:
                cut = len(line) // 2
            draw_text(im, glyphs, meta, 6 + indent, y + meta["ascent"], line[:cut])
            y += lh
            line = line[cut:].lstrip()
            indent = 2 * glyphs[0x20]["adv"]
    draw_text(im, glyphs, meta, 250 - 6 - width(glyphs, "1/2"), 122 - 3, "1/2")
    big = im.resize((250 * scale, 122 * scale), Image.NEAREST).convert("L")
    if label:
        from PIL import ImageFont
        canvas = Image.new("L", (big.width, big.height + 22), 255)
        canvas.paste(big, (0, 22))
        ImageDraw.Draw(canvas).text((6, 4), label, fill=0)
        big = canvas
    big.save(out)
    return lh


if __name__ == "__main__":
    ap = argparse.ArgumentParser()
    ap.add_argument("bdf")
    ap.add_argument("name", nargs="?")
    ap.add_argument("out", nargs="?")
    ap.add_argument("--preview")
    ap.add_argument("--title")
    ap.add_argument("--label")
    a = ap.parse_args()
    if a.preview:
        lh = preview(a.bdf, a.preview, a.title, label=a.label)
        print(f"{a.preview}: línea {lh} px")
    else:
        convert(a.bdf, a.name, a.out)
