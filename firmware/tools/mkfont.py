#!/usr/bin/env python3
"""Recorta Departure Mono a Latin-1 y la deja en web/font.b64 para empotrarla en el portal.

El portal se sirve desde el punto de acceso del charm, sin internet, así que la fuente tiene que
viajar dentro de la página. Recortada pesa ~3.5 KB comprimidos. Solo hay que volver a ejecutarlo
si se cambia la fuente; embed_web.py se encarga de inyectarla en cada compilación.
"""
import base64
import io
import os

from fontTools import subset
from fontTools.ttLib import TTFont

HERE = os.path.dirname(__file__)
SRC = os.path.join(HERE, "..", "..", "tools", "fonts", "DepartureMono.woff2")
DST = os.path.join(HERE, "..", "web", "font.b64")

f = TTFont(SRC)
opts = subset.Options()
opts.flavor = "woff2"
opts.desubroutinize = True
opts.layout_features = []
opts.notdef_outline = False
sub = subset.Subsetter(options=opts)
# Latin-1 completo (acentos, ñ, ¿, ¡) más comillas tipográficas, rayas y puntos suspensivos
sub.populate(unicodes=set(range(0x20, 0x100)) | {0x2018, 0x2019, 0x201C, 0x201D, 0x2013, 0x2014, 0x2026, 0x00B7})
sub.subset(f)
buf = io.BytesIO()
f.flavor = "woff2"
f.save(buf)
b64 = base64.b64encode(buf.getvalue()).decode()
with open(DST, "w") as out:
    out.write(b64)
print(f"{os.path.normpath(DST)}: {len(buf.getvalue())} B de fuente -> {len(b64)} B en base64")
