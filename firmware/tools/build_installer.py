#!/usr/bin/env python3
"""Prepara el instalador web del charm: compila, junta los binarios y escribe los manifiestos.

  python3 firmware/tools/build_installer.py [--serve]

Deja en installer/firmware/ los cuatro trozos que van a la flash (gestor de arranque, tabla de
particiones, selector de app y firmware) más la imagen LittleFS con las cartas de ejemplo de
firmware/data-instalador/ —nunca las cartas personales de firmware/data/—, y dos manifiestos para
ESP Web Tools: completo (con cartas) y solo firmware.

Con --serve levanta http://localhost:8000 para probarlo: WebSerial funciona en localhost aunque no
haya HTTPS.
"""
import argparse
import json
import os
import re
import shutil
import subprocess
import sys

FW = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ROOT = os.path.dirname(FW)
OUT = os.path.join(ROOT, "installer")
BIN = os.path.join(OUT, "firmware")
PIO = os.path.expanduser("~/.platformio/penv/bin/pio")
BUILD = os.path.join(FW, ".pio", "build", "charm")
BOOT_APP0 = os.path.expanduser("~/.platformio/packages/framework-arduinoespressif32/tools/partitions/boot_app0.bin")

# Direcciones de la flash, las mismas que usa `pio run -t upload` (ver tabla de particiones de 8 MB)
PARTS = [
    ("bootloader.bin", 0x0000, os.path.join(BUILD, "bootloader.bin")),
    ("partitions.bin", 0x8000, os.path.join(BUILD, "partitions.bin")),
    ("boot_app0.bin", 0xE000, BOOT_APP0),
    ("charm.bin", 0x10000, os.path.join(BUILD, "firmware.bin")),
]
FS = ("littlefs.bin", 0x670000, os.path.join(BUILD, "littlefs.bin"))


def pio(*args):
    subprocess.run([PIO, "run", "-d", FW, "-e", "charm", *args], check=True)


def version():
    m = re.search(r'CHARM_FW_VERSION=\\"([^"\\]+)', open(os.path.join(FW, "platformio.ini")).read())
    return m.group(1) if m else "0.0.0"


def manifest(name, parts, ver):
    return {
        "name": name,
        "version": ver,
        "new_install_prompt_erase": True,
        "new_install_improv_wait_time": 0,  # el charm no habla Improv: no esperes a que conteste
        "builds": [{"chipFamily": "ESP32-S3",
                    "parts": [{"path": f"firmware/{f}", "offset": off} for f, off, _ in parts]}],
    }


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--serve", action="store_true", help="sirve installer/ en http://localhost:8000")
    a = ap.parse_args()

    if not os.path.exists(BOOT_APP0):
        sys.exit(f"falta {BOOT_APP0}: compila una vez con pio para que baje el framework")
    pio()
    # La imagen de contenido se hace con las cartas de ejemplo, no con las personales
    data = os.path.join(FW, "data")
    tmp = data + ".guardado"
    os.rename(data, tmp)
    shutil.copytree(os.path.join(FW, "data-instalador"), data)
    try:
        pio("-t", "buildfs")
    finally:
        shutil.rmtree(data)
        os.rename(tmp, data)

    os.makedirs(BIN, exist_ok=True)
    ver = version()
    for name, _, src in PARTS + [FS]:
        shutil.copyfile(src, os.path.join(BIN, name))
        print(f"  {name:16s} {os.path.getsize(src) / 1024:7.0f} KB")
    json.dump(manifest("charm (con poemas de ejemplo)", PARTS + [FS], ver),
              open(os.path.join(OUT, "manifest.json"), "w"), indent=2)
    json.dump(manifest("charm (solo firmware)", PARTS, ver),
              open(os.path.join(OUT, "manifest-solo-firmware.json"), "w"), indent=2)
    print(f"installer/ listo, versión {ver}")

    if a.serve:
        os.chdir(OUT)
        print("http://localhost:8000  (Ctrl-C para parar)")
        subprocess.run([sys.executable, "-m", "http.server", "8000"])


if __name__ == "__main__":
    main()
