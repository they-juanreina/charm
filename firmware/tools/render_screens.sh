#!/bin/sh
# Compila las pantallas reales del charm (src/Display.cpp) para la Mac, las dibuja y las guarda como PNG.
# Las capturas son el mismo lienzo de 3904 bytes que va a la tinta: lo que se ve aquí es lo que ve el charm.
#
#   tools/render_screens.sh [carpeta_salida]      (por defecto ../manual/img/pantallas)
set -e
cd "$(dirname "$0")/.."
OUT="${1:-../manual/img/pantallas}"
TMP="$(mktemp -d)"
G=".pio/libdeps/charm-dev/Adafruit GFX Library"
Q=".pio/libdeps/charm-dev/QRCode/src"
[ -d "$G" ] || { echo "falta $G: compila el firmware una vez con pio para descargar las librerías"; exit 1; }

clang -c -O1 -w "$Q/qrcode.c" -o "$TMP/qrcode.o"
clang++ -std=c++17 -O1 -w -DARDUINO=10800 -Itest/native/stubs -Isrc -I"$G" -I"$Q" \
    test/native/render_screens.cpp src/Display.cpp src/TextLayout.cpp "$G/Adafruit_GFX.cpp" "$TMP/qrcode.o" \
    -o "$TMP/render_screens"

python3 tools/bmp1png.py --sample "$TMP/muestra.bmp1"
mkdir -p "$TMP/raw" "$OUT"
POEMA=data/cards/001.txt
[ -f "$POEMA" ] || POEMA=data-instalador/cards/001.txt   # el repositorio público no lleva las cartas personales
"$TMP/render_screens" "$TMP/raw" "$POEMA" "$TMP/muestra.bmp1" test/native/dedicatoria-ejemplo.txt >/dev/null
python3 tools/bmp1png.py "$TMP/raw" "$OUT"
rm -rf "$TMP"
