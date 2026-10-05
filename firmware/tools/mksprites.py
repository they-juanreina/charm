#!/usr/bin/env python3
"""Genera src/Sprites.h a partir de las piezas de pixel art diseñadas en Paper.

Cada sprite se define como una lista de rectángulos en "píxeles de arte" y una escala. La escala
convierte la rejilla de arte a píxeles reales del panel: escala 2 = cada pixel de arte ocupa 2x2 px.
Es la misma geometría que los SVG del archivo de Paper "Noble nebula" (transform="scale(N)").

  python3 tools/mksprites.py            # escribe src/Sprites.h
  python3 tools/mksprites.py --show     # además dibuja cada sprite en ASCII para comprobarlo
"""
import os
import sys

# nombre: (ancho_arte, alto_arte, escala, [(x, y, w, h), ...])
SPRITES = {
    # Corazón grande del campo de las cartas (9x8 arte -> 18x16 px)
    "heartBig": (9, 8, 2, [(1, 0, 2, 1), (6, 0, 2, 1), (0, 1, 4, 1), (5, 1, 4, 1), (0, 2, 9, 2),
                           (1, 4, 7, 1), (2, 5, 5, 1), (3, 6, 3, 1), (4, 7, 1, 1)]),
    # Corazón chico de los índices de esquina y del pie (5x4 arte -> 10x8 px)
    "heartSmall": (5, 4, 2, [(1, 0, 1, 1), (3, 0, 1, 1), (0, 1, 5, 1), (1, 2, 3, 1), (2, 3, 1, 1)]),
    # Destello en cruz de la constelación (3x3 arte -> 6x6 px)
    "spark": (3, 3, 2, [(1, 0, 1, 3), (0, 1, 3, 1)]),
    # Marca de verificación de la pantalla Conectado (11x8 arte -> 44x32 px)
    "check": (11, 8, 4, [(9, 0, 2, 1), (8, 1, 2, 1), (7, 2, 2, 1), (6, 3, 2, 1), (0, 4, 2, 1),
                         (5, 4, 2, 1), (1, 5, 2, 1), (4, 5, 2, 1), (2, 6, 4, 1), (3, 7, 2, 1)]),
    # Contorno de batería con su borne (19x9 arte -> 76x36 px)
    "battery": (19, 9, 4, [(0, 0, 17, 1), (0, 8, 17, 1), (0, 1, 1, 7), (16, 1, 1, 7), (17, 3, 2, 3)]),
    # Rayo que va dentro de la batería (5x7 arte -> 20x28 px)
    "bolt": (5, 7, 4, [(3, 0, 2, 1), (2, 1, 2, 1), (1, 2, 2, 1), (0, 3, 4, 1), (2, 4, 2, 1),
                       (1, 5, 2, 1), (0, 6, 2, 1)]),
    # Carta vacía con admiración, para los mensajes (15x20 arte -> 45x60 px)
    "alert": (15, 20, 3, [(0, 0, 15, 1), (0, 19, 15, 1), (0, 1, 1, 18), (14, 1, 1, 18),
                          (6, 5, 3, 8), (6, 15, 3, 2)]),
}


def render(w, h, scale, rects):
    """Devuelve (ancho_px, alto_px, filas de bits)."""
    W, H = w * scale, h * scale
    grid = [[0] * W for _ in range(H)]
    for rx, ry, rw, rh in rects:
        for y in range(ry * scale, (ry + rh) * scale):
            for x in range(rx * scale, (rx + rw) * scale):
                grid[y][x] = 1
    return W, H, grid


def pack(W, grid):
    """1 bpp MSB primero, filas alineadas a byte: el formato que espera drawBitmap."""
    stride = (W + 7) // 8
    out = bytearray(stride * len(grid))
    for y, row in enumerate(grid):
        for x, v in enumerate(row):
            if v:
                out[y * stride + (x >> 3)] |= 0x80 >> (x & 7)
    return bytes(out)


def main():
    show = "--show" in sys.argv
    out = os.path.join(os.path.dirname(__file__), "..", "src", "Sprites.h")
    lines = ["// GENERADO por tools/mksprites.py. No editar a mano: cambia las piezas en Paper y regenera.",
             "#pragma once", "#include <pgmspace.h>", "#include <stdint.h>", "",
             "struct Sprite {", "    uint8_t w, h;", "    const uint8_t *bits;", "};", ""]
    table = []
    for name, (w, h, scale, rects) in SPRITES.items():
        W, H, grid = render(w, h, scale, rects)
        data = pack(W, grid)
        if show:
            print(f"--- {name} {W}x{H}")
            for row in grid:
                print("    " + "".join("#" if v else "." for v in row))
        lines.append(f"// {name}: {w}x{h} de arte a escala {scale} -> {W}x{H} px")
        lines.append(f"const uint8_t {name}Bits[] PROGMEM = {{")
        for i in range(0, len(data), 16):
            lines.append("  " + ", ".join(f"0x{b:02X}" for b in data[i:i + 16]) + ",")
        lines.append("};")
        table.append(f"const Sprite {name} = {{ {W}, {H}, {name}Bits }};")
        lines.append("")
    lines += table + [""]
    with open(out, "w") as f:
        f.write("\n".join(lines))
    print(f"{os.path.normpath(out)}: {len(SPRITES)} sprites")


if __name__ == "__main__":
    main()
