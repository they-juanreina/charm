# charm

Un charm de bolsillo que da **un poema al azar**: una carta que se destapa como en el tarot, en una
pantalla de tinta electrónica que conserva la imagen sin gastar batería. Firmware propio para la placa
[Heltec Vision Master E213](https://heltec.org/project/vision-master-e213/) (ESP32-S3, e-ink de
2.13" y 250 × 122 píxeles, LoRa SX1262, batería LiPo).

- **Cartas al azar sin repetir.** El reverso es la carta de la baraja por la que vas: un 6 con seis
  corazones. El as puede llevar la dedicatoria de quien regala el charm.
- **Poemas e imágenes**, con paginación, tramado y recorte.
- **Se actualiza desde el teléfono.** El charm levanta su propio WiFi y sirve una página donde se
  escriben los poemas, se suben imágenes y se cambia la dedicatoria. Sin app y sin internet.
- **Duerme entre pulsaciones** (deep sleep) y despierta con cualquiera de los dos botones.

## Instalarlo en una placa

Abre **[el instalador](installer/)** en Chrome o Edge, conecta la placa por USB-C y pulsa un botón.
No hace falta instalar nada: ver [installer/README.md](installer/README.md).

Con PlatformIO, desde el código:

```bash
~/.platformio/penv/bin/pio run -d firmware -e charm -t upload
```

Todo lo demás —botones, pantallas, API del portal, batería, tipografías— está en
[firmware/README.md](firmware/README.md).

## Licencia

GPL-3.0. El firmware usa [GxEPD2](https://github.com/ZinggJM/GxEPD2) (GPL-3.0) para la tinta
electrónica, además de Adafruit GFX (BSD) y QRCode (MIT). Las tipografías empotradas son Departure
Mono (OFL) y New Century Schoolbook de las fuentes X11 de dominio público.

Los poemas de ejemplo de `firmware/data-instalador/` son de dominio público (Sor Juana Inés de la
Cruz, Rosalía de Castro).
