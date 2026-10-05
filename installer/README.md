# Instalador web del charm

Una página que flashea el firmware del charm en una Heltec Vision Master E213 **desde el navegador**,
con [ESP Web Tools](https://esphome.github.io/esp-web-tools/) y WebSerial. Quien la use no instala
nada: abre la página en Chrome o Edge, conecta la placa por USB-C y pulsa un botón.

```
installer/
  index.html                    la página
  manifest.json                 instalación completa: firmware + cartas de ejemplo
  manifest-solo-firmware.json   solo el programa, respeta las cartas que ya haya
  firmware/*.bin                los cinco trozos que van a la flash (2.7 MB en total)
  font.woff2, pantalla.png      la letra y la captura de la cabecera
```

## Qué lleva dentro

| Archivo | Dirección | Qué es |
|---|---|---|
| `bootloader.bin` | `0x0` | gestor de arranque del ESP32-S3 |
| `partitions.bin` | `0x8000` | tabla de particiones (8 MB: dos apps y 1.5 MB de contenido) |
| `boot_app0.bin` | `0xe000` | selector de app |
| `charm.bin` | `0x10000` | el firmware, con el portal web dentro |
| `littlefs.bin` | `0x670000` | las cartas: tres poemas de ejemplo y una dedicatoria |

**La imagen de contenido se hace con `firmware/data-instalador/`, nunca con `firmware/data/`.** Los
poemas personales no salen de tu charm: los de ejemplo son de dominio público (Sor Juana Inés de la
Cruz y Rosalía de Castro) más una carta de bienvenida.

## Regenerarlo

```bash
python3 firmware/tools/build_installer.py
```

Compila el entorno de producción, construye la imagen LittleFS con las cartas de ejemplo, copia los
binarios y escribe los dos manifiestos con la versión de `CHARM_FW_VERSION` (en `platformio.ini`).
Súbele la versión ahí antes de publicar una tanda nueva.

## Probarlo en local

```bash
python3 firmware/tools/build_installer.py --serve
```

Abre <http://localhost:8000> **en Chrome o Edge** y pulsa *instalar el charm*. WebSerial funciona en
`localhost` aunque no haya HTTPS; desde un archivo `file://` o por `http://` en otra máquina, no.

## Publicarlo

Hace falta **HTTPS**. Tres caminos, de menos a más trabajo:

- **GitHub Pages**: sube la carpeta `installer/` a un repositorio y actívalo en *Settings → Pages*.
  Queda en `https://usuario.github.io/charm/`. Gratis y sirve los binarios sin problema.
- **Netlify Drop** (<https://app.netlify.com/drop>): arrastra la carpeta y da una URL al momento, sin
  cuenta ni repositorio.
- **Vercel**: `vercel deploy installer --prod` con la CLI, o conectando el repositorio.

Da igual cuál: son archivos estáticos, no hace falta servidor.

## Limitaciones

- **Solo Chrome y Edge de escritorio.** Safari y Firefox no tienen WebSerial, y en móvil tampoco
  funciona. La página lo avisa.
- El cable debe ser **de datos**. Muchos cables USB-C solo llevan corriente y la placa no aparece.
- Si la placa está en deep sleep, su puerto desaparece: **mantén BOOT y toca RST** para entrar en modo
  de carga.
- La instalación completa **borra las cartas que hubiera**. Para actualizar sin perderlas, usa *solo
  firmware*.

## Sin navegador

Con [esptool](https://docs.espressif.com/projects/esptool/) instalado, desde esta carpeta:

```bash
esptool --chip esp32s3 write-flash -z 0x0 firmware/bootloader.bin 0x8000 firmware/partitions.bin 0xe000 firmware/boot_app0.bin 0x10000 firmware/charm.bin 0x670000 firmware/littlefs.bin
```

Y con el proyecto completo y PlatformIO, lo de siempre:

```bash
~/.platformio/penv/bin/pio run -d firmware -e charm -t upload
```
