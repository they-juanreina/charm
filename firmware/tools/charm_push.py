#!/usr/bin/env python3
"""Sincroniza una carpeta de poemas con el charm por WiFi (solo biblioteca estándar).

Uso:
  python3 tools/charm_push.py [--host 192.168.4.1] [--dir poems] [--prune] [--reshuffle] [--exit]

Convención de archivos en la carpeta:
  012-titulo.txt   -> carta #12 (PUT). Sin prefijo numérico -> POST y el archivo se renombra con el id devuelto.
  007.bmp1         -> imagen de 3904 bytes (250x122, 1 bpp, MSB-first, bit 1 = negro).
  foto.png         -> con --png y Pillow instalado se convierte a bmp1 (tramado Floyd-Steinberg) y se sube.
  dedicatoria.txt  -> la dedicatoria de quien regala: la carta de as, primera de cada baraja. Mismo formato
                      que un poema ("# Para…" y versos); la firma, en la última línea empezando por "— ".
"""
import argparse
import json
import os
import re
import sys
import urllib.error
import urllib.request

DEDICATION = "dedicatoria.txt"
NAME_RE = re.compile(r"^(\d{1,3})(?:[-_ ].*)?\.(txt|bmp1)$")


def call(host, method, path, data=None, ctype=None):
    req = urllib.request.Request(f"http://{host}{path}", data=data, method=method)
    if ctype:
        req.add_header("Content-Type", ctype)
    try:
        with urllib.request.urlopen(req, timeout=15) as r:
            return r.status, r.read()
    except urllib.error.HTTPError as e:
        return e.code, e.read()


def png_to_bmp1(path):
    try:
        from PIL import Image
    except ImportError:
        print(f"  {os.path.basename(path)}: hace falta Pillow (pip install pillow) o usa tools/eink-preview.html")
        return None
    im = Image.open(path).convert("L")
    im.thumbnail((250, 122))
    canvas = Image.new("L", (250, 122), 255)
    canvas.paste(im, ((250 - im.width) // 2, (122 - im.height) // 2))
    # tramado Floyd-Steinberg, el mismo que usa la página del charm por defecto
    g = [float(v) for v in canvas.getdata()]
    out = bytearray(32 * 122)
    for y in range(122):
        for x in range(250):
            i = y * 250 + x
            old = g[i]
            nv = 0 if old < 128 else 255
            g[i] = nv
            e = old - nv
            if x + 1 < 250:
                g[i + 1] += e * 7 / 16
            if y + 1 < 122:
                if x:
                    g[i + 249] += e * 3 / 16
                g[i + 250] += e * 5 / 16
                if x + 1 < 250:
                    g[i + 251] += e / 16
            if nv == 0:
                out[y * 32 + (x >> 3)] |= 0x80 >> (x & 7)
    return bytes(out)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--host", default="192.168.4.1")
    ap.add_argument("--dir", default="poems")
    ap.add_argument("--prune", action="store_true", help="borra en el charm las cartas que no están en la carpeta")
    ap.add_argument("--reshuffle", action="store_true")
    ap.add_argument("--exit", action="store_true", help="apaga el WiFi del charm al terminar")
    ap.add_argument("--png", action="store_true", help="convierte PNG/JPG de la carpeta con Pillow")
    ap.add_argument("--dedicatoria", metavar="TXT", help="sube este archivo como dedicatoria "
                    "(por defecto, dedicatoria.txt de la carpeta si existe)")
    ap.add_argument("--sin-dedicatoria", action="store_true", help="quita la dedicatoria del charm")
    a = ap.parse_args()

    code, body = call(a.host, "GET", "/api/cards")
    if code != 200:
        sys.exit(f"no se pudo hablar con el charm en {a.host} (HTTP {code}). ¿Estás en su red WiFi?")
    remote = {c["id"]: c for c in json.loads(body)}
    print(f"charm: {len(remote)} cartas")

    seen = set()
    for name in sorted(os.listdir(a.dir)):
        path = os.path.join(a.dir, name)
        if not os.path.isfile(path):
            continue
        low = name.lower()
        if low == DEDICATION:
            continue
        data, ext, cid = None, None, None
        m = NAME_RE.match(low)
        if m:
            cid, ext = int(m.group(1)), m.group(2)
            with open(path, "rb") as f:
                data = f.read()
        elif low.endswith(".txt"):
            ext = "txt"
            with open(path, "rb") as f:
                data = f.read()
        elif a.png and low.endswith((".png", ".jpg", ".jpeg")):
            ext = "bmp1"
            data = png_to_bmp1(path)
            if data is None:
                continue
        else:
            continue

        if ext == "bmp1" and len(data) != 3904:
            print(f"  {name}: tamaño {len(data)}, debe ser 3904 bytes. Saltado.")
            continue

        if cid is not None and cid in remote:
            _, cur = call(a.host, "GET", f"/api/cards/{cid}")
            if cur == data:
                print(f"  #{cid:03d} {name}: sin cambios")
                seen.add(cid)
                continue

        if ext == "txt":
            if cid is None:
                code, resp = call(a.host, "POST", "/api/cards", data, "text/plain; charset=utf-8")
            else:
                code, resp = call(a.host, "PUT", f"/api/cards/{cid}", data, "text/plain; charset=utf-8")
        else:
            q = f"?id={cid}" if cid is not None else ""
            code, resp = call(a.host, "POST", f"/api/images{q}", data, "application/octet-stream")

        if code not in (200, 201):
            print(f"  {name}: error HTTP {code} {resp[:80]!r}")
            continue
        new_id = json.loads(resp)["id"]
        seen.add(new_id)
        print(f"  #{new_id:03d} {name}: subido")
        if cid is None:
            stem, e = os.path.splitext(name)
            new_name = f"{new_id:03d}-{stem}{'.bmp1' if ext == 'bmp1' else e}"
            if ext == "bmp1":
                with open(os.path.join(a.dir, new_name), "wb") as f:
                    f.write(data)
            else:
                os.rename(path, os.path.join(a.dir, new_name))
            print(f"    renombrado a {new_name}")

    ded = a.dedicatoria or os.path.join(a.dir, DEDICATION)
    if a.sin_dedicatoria:
        call(a.host, "DELETE", "/api/dedication")
        print("dedicatoria quitada")
    elif os.path.isfile(ded):
        with open(ded, "rb") as f:
            data = f.read()
        code, cur = call(a.host, "GET", "/api/dedication")
        if code == 200 and cur == data:
            print("  dedicatoria: sin cambios")
        else:
            code, resp = call(a.host, "PUT", "/api/dedication", data, "text/plain; charset=utf-8")
            print("  dedicatoria: subida (sale en el próximo as)" if code == 200 else f"  dedicatoria: error HTTP {code} {resp[:80]!r}")

    if a.prune:
        for cid in sorted(set(remote) - seen):
            code, _ = call(a.host, "DELETE", f"/api/cards/{cid}")
            print(f"  #{cid:03d}: borrado del charm" if code == 200 else f"  #{cid:03d}: no se pudo borrar ({code})")
    if a.reshuffle:
        call(a.host, "POST", "/api/reshuffle")
        print("baraja nueva")
    if a.exit:
        call(a.host, "POST", "/api/exit")
        print("WiFi del charm apagado")


if __name__ == "__main__":
    main()
