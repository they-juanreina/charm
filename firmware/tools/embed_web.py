"""PlatformIO pre-script: comprime web/index.html con gzip y lo emite como src/web_page.h.

Se ejecuta antes de cada build. Solo regenera si el HTML es mas nuevo que el header.
"""
import gzip
import os

Import("env")  # noqa: F821  (inyectado por PlatformIO)

PROJECT = env["PROJECT_DIR"]  # noqa: F821
SRC = os.path.join(PROJECT, "web", "index.html")
DST = os.path.join(PROJECT, "src", "web_page.h")


def build():
    if not os.path.exists(SRC):
        print("[embed_web] falta web/index.html, no se genera web_page.h")
        return
    font = os.path.join(PROJECT, "web", "font.b64")
    newest = max([os.path.getmtime(SRC)] + ([os.path.getmtime(font)] if os.path.exists(font) else []))
    if os.path.exists(DST) and os.path.getmtime(DST) >= newest:
        return
    with open(SRC, "rb") as f:
        raw = f.read()
    # la fuente del portal viaja dentro de la página: no hay internet en el punto de acceso
    font = os.path.join(PROJECT, "web", "font.b64")
    if b"__FONT_B64__" in raw:
        if not os.path.exists(font):
            print("[embed_web] falta web/font.b64: ejecuta tools/mkfont.py")
            return
        with open(font) as f:
            raw = raw.replace(b"__FONT_B64__", f.read().strip().encode())
    gz = gzip.compress(raw, compresslevel=9, mtime=0)
    lines = ["// GENERADO por tools/embed_web.py a partir de web/index.html. No editar.",
             "#pragma once", "#include <pgmspace.h>",
             f"const size_t WEB_INDEX_GZ_LEN = {len(gz)};",
             "const uint8_t WEB_INDEX_GZ[] PROGMEM = {"]
    for i in range(0, len(gz), 16):
        lines.append("  " + ", ".join(f"0x{b:02x}" for b in gz[i:i + 16]) + ",")
    lines.append("};")
    with open(DST, "w") as f:
        f.write("\n".join(lines) + "\n")
    print(f"[embed_web] web_page.h: {len(raw)} B -> {len(gz)} B gzip")


build()
