# charm · firmware

Firmware propio (PlatformIO + Arduino) para el charm de poemas sobre la Heltec Vision Master E213
(ESP32-S3, e-ink 2.13" de 250×122, SX1262 LoRa, LiPo 250 mAh).

El charm entrega cartas al azar: poemas o imágenes que se revelan como cartas de tarot y se vuelven a
ocultar al terminar. Entre pulsaciones duerme en deep sleep; la tinta conserva la imagen sin consumo.

## Botones

| Botón | Pulsación | Acción |
|---|---|---|
| **1** (BOOT, GPIO0, junto al USB) | larga | revelar una carta nueva |
| **1** | corta | página siguiente; tras la última página la carta se oculta |
| **2** (GPIO21) | corta | ocultar la carta |
| **2** | larga | abrir el WiFi para actualizar poemas |
| cualquiera | corta, en modo WiFi | cerrar el WiFi |

Con el USB conectado, en reposo se muestra la pantalla de carga (batería grande con el nivel, tensión y
porcentaje) y se actualiza cada dos minutos; al desenchufar vuelve el reverso. La placa no tiene señal del
cargador: un cargador de pared sin datos solo se detecta cuando la celda ya supera 4.2 V ("Batería llena").

**El reverso es una carta de verdad.** Si vas por la carta 6 de la baraja muestra un 6 con seis corazones,
con sus índices en las esquinas opuestas; al rebarajar aparece la carta de as con el nombre del charm
(por defecto `@stayyellow`, editable desde la web). Por encima de 24 ya no caben corazones y manda el
número. Es a la vez el salvapantallas y el indicador de por dónde vas.

## Compilar y flashear

PlatformIO no está en el PATH; usa la instalación de `~/.platformio`:

```bash
PIO=~/.platformio/penv/bin/pio
$PIO run -e charm-dev                 # desarrollo: sin deep sleep, logs verbosos
$PIO run -e charm-dev -t upload       # flashear (el charm debe estar despierto: en deep sleep el USB desaparece)
$PIO run -e charm-dev -t uploadfs     # cargar data/ (poemas semilla) en LittleFS. Borra todas las cartas del charm.
$PIO device monitor -b 115200
$PIO run -e charm -t upload           # producción: con deep sleep
```

Si el charm está dormido o no aparece el puerto: mantén BOOT (botón 1) y toca RST, o reconecta el USB con
BOOT pulsado. Entra en modo descarga y `upload` funciona.

## Instalar en otra placa

`../installer/` es una página web que flashea una Heltec E213 desde Chrome o Edge con WebSerial, sin
PlatformIO ni esptool. Se regenera con:

```bash
python3 tools/build_installer.py [--serve]
```

Compila el entorno `charm`, construye la imagen LittleFS a partir de `data-instalador/` (cartas de
ejemplo de dominio público, **no** las de `data/`), copia los binarios a `installer/firmware/` y
escribe los manifiestos. Detalles y cómo publicarla: [../installer/README.md](../installer/README.md).

## Contenido

Las cartas viven en LittleFS, en `/cards`:

- `NNN.txt`: poema en UTF-8. Si la primera línea empieza por `#`, es el título. Cada línea es un verso;
  una línea en blanco separa estrofas. Los versos largos se parten con sangría. Máximo 8 KB.
- `NNN.bmp1`: imagen de exactamente 3904 bytes: 250×122, 1 bit por píxel, MSB primero, 32 bytes por fila,
  bit 1 = negro. Es el mismo formato que exporta `tools/eink-preview.html` (en la raíz del repo).
- `/back.bmp1` (opcional): reverso personalizado en el mismo formato.
- `/dedicatoria.txt` (opcional): la dedicatoria de quien regala, con el formato de un poema (la firma, en
  la última línea empezando por `— `). Es la **carta de as**: `Deck::draw()` la entrega primero en cada
  baraja nueva (id 0) y el reverso muestra el as hasta que se destapa. Después, el reverso numera los
  poemas desde 1. Al guardarla se marca como pendiente, para que quien la escribe la vea enseguida.

`NNN` va de 001 a 999. Un id es texto o imagen, no ambos.

La baraja se recorre sin repetir: una permutación derivada de una semilla guardada en NVS. Cuando cambia el
contenido o se agota la baraja, se rebaraja.

## Actualizar sin cable

1. Pulsación larga del botón 2. La pantalla muestra un QR grande para unirse a la red **amarilla-marin**
   (clave `sin-duda`) y la dirección `http://192.168.4.1`.
2. Escanea el QR grande con la cámara del teléfono y acepta unirte. Debería abrirse el portal solo; si el
   teléfono avisa de que "no hay internet", elige quedarte en la red y abre la dirección (o el QR pequeño) en
   el navegador. En iPhone, para subir imágenes conviene abrir Safari directamente en esa dirección.
3. Edita, crea o borra poemas, sube imágenes (se convierten a blanco y negro en el navegador), baraja.
4. "Apagar WiFi" o cualquier botón corto. Sin actividad, el WiFi se apaga solo a los 5 minutos.

Desde la Mac, con la carpeta `poems/` de este directorio:

```bash
python3 tools/charm_push.py --dir poems --prune --exit
```

`012-titulo.txt` actualiza la carta 12; un archivo sin número se crea y se renombra con el id asignado.
`--prune` borra del charm lo que no esté en la carpeta. Un `dedicatoria.txt` en la carpeta se sube como
dedicatoria y no como poema (`--dedicatoria otro.txt` para usar otro archivo, `--sin-dedicatoria` para quitarla). `--png` convierte PNG/JPG con Pillow.

### API

| Método y ruta | Cuerpo | Respuesta |
|---|---|---|
| GET `/api/status` | — | firmware, panel, cartas, batería, espacio |
| GET `/api/screen` | — | los 3904 bytes de lo que hay ahora en la pantalla |
| GET `/api/cards` | — | `[{id,type,title,size}]` |
| GET `/api/cards/{id}` | — | texto plano o binario |
| PUT `/api/cards/{id}` | `text/plain` | `{id}` |
| POST `/api/cards` | `text/plain` | 201 `{id}` (menor id libre) |
| POST `/api/images?id=N` | 3904 bytes `application/octet-stream` | `{id}` (sin `id`: menor libre) |
| DELETE `/api/cards/{id}` | — | `{ok}` |
| GET `/api/dedication` | — | texto plano; 404 si no hay |
| PUT `/api/dedication` | `text/plain` (máx. 8 KB) | `{ok}`; sale en el próximo as |
| DELETE `/api/dedication` | — | `{ok}` |
| PUT `/api/name` | `text/plain` (máx. 32 bytes) | `{name}` |
| POST `/api/reshuffle` | — | `{ok}` |
| POST `/api/exit` | — | apaga el WiFi |

## Estructura

```
src/main.cpp        máquina de estados (LOCKED / REVEALED / WIFI), tiempos, RTC RAM
src/config.h        pines, tiempos, clave del AP, límites
src/Display.*       detección del panel, GxEPD2, lienzo 1 bpp, refresco y todas las pantallas
src/Sprites.h       GENERADO: piezas de pixel art (tools/mksprites.py)
src/TextLayout.*    UTF-8 -> cp1252, métricas, wrap por verso, paginación (sin hardware; ver test/)
src/Deck.*          cartas en LittleFS y baraja persistida en NVS
src/Buttons.*       debounce, corto/largo, clasificación de la pulsación que despertó al chip
src/Power.*         VEXT, radio LoRa dormida, batería, deep sleep con wake por ambos botones
src/WifiPortal.*    punto de acceso, portal cautivo, API REST
web/index.html      página del portal, embebida en gzip por tools/embed_web.py
tools/charm_push.py sincroniza poems/ con el charm
data/cards/         contenido semilla para uploadfs
```

Prueba nativa de la paginación, sin placa:

```bash
g++ -std=c++17 -I".pio/libdeps/charm-dev/Adafruit GFX Library" -Isrc test/native_layout.cpp src/TextLayout.cpp -o /tmp/layout && /tmp/layout
```

## Diseño

El aspecto vive en Paper, en el archivo **Noble nebula**
(`https://app.paper.design/file/01M2VEWEXR3SCDCAG7188QQWVE`): los reversos, las pantallas de interfaz y el
portal, con tokens para el negro, el amarillo y los tamaños de tipografía. El amarillo es seguro porque al
convertir a 1 bit se vuelve blanco, así que el diseño se hace en el color de la marca y la tinta lo respeta.

Todo el arte va sobre una **rejilla de 2 px**: en tinta electrónica los detalles de 1 px se desvanecen con
el refresco parcial. Las piezas (corazones, destellos, marca de verificación, batería, rayo, carta de aviso)
se definen como rectángulos en `tools/mksprites.py` y se compilan a `src/Sprites.h`:

```bash
python3 tools/mksprites.py --show    # regenera los sprites y los dibuja en ASCII para comprobarlos
```

Las pantallas no son imágenes: el firmware las compone, porque todas llevan datos que cambian. Para
comprobar que coinciden con Paper sin desmontar nada, **`GET /api/screen` devuelve los 3904 bytes del
lienzo** y el portal los pinta arriba del todo.

### Tipografía

Las fuentes de píxel solo funcionan en su tamaño nativo o en múltiplos enteros; en cualquier otro salen
rotas. **Departure Mono** se usa a 11 px (línea 14, 34 caracteres por línea) y a 22 px para títulos y
numerales, con acentos y ñ completos. Los poemas conservan la serif New Century Schoolbook, que es más
cómoda para leer.

```bash
python3 tools/fontconv.py ../tools/fonts/DepartureMono.woff2 11 DepartureMono11 src/fonts/DepartureMono11.h
python3 tools/bdf2gfx.py ../tools/fonts/bdf/ncenR10.bdf NcenR10 src/fonts/NcenR10.h
python3 tools/mkfont.py      # recorta la fuente a Latin-1 para empotrarla en el portal (web/font.b64)
```

El portal no tiene internet, así que la fuente viaja dentro de la página: `tools/mkfont.py` la recorta a
3.5 KB y `embed_web.py` la inyecta en cada compilación donde el HTML dice `__FONT_B64__`.

## Refresco y batería

- Al revelar u ocultar una carta el refresco es completo; al pasar página es rápido (parcial). Cada 8
  rápidos se fuerza uno completo para limpiar el ghosting (`FAST_LIMIT` en `config.h`).
- Tras dormir, el primer refresco es completo porque el controlador del panel pierde su memoria.
- Deep sleep: VEXT apagado, líneas del panel en alta impedancia, SX1262 en sleep, pull-ups RTC en los
  botones y wake por cualquiera de los dos. Heltec especifica 20 µA; conviene medirlo en serie con la batería.
- No se abre el WiFi con menos de 3.2 V (celda casi vacía); el aviso muestra la tensión medida.
- Con USB conectado no se muestra porcentaje: el pin de batería ve la tensión del cargador, no la de la celda.

## LoRa

En esta versión la radio se duerme al arrancar. Opciones evaluadas para más adelante:

- **Buzón punto a punto** (RadioLib, pines CS 8 / SCK 9 / MOSI 10 / MISO 11 / RST 12 / BUSY 13 / DIO1 14):
  otro Heltec o un gateway en casa envía poemas. Exige RX continuo (mA) o RX por ciclos; encaja como modo
  "recibir ahora" activado por botón, no como escucha permanente.
- **Malla Meshtastic**: solo con el fork de `../firmware-src`; descartado por batería e interfaz.
- **LoRaWAN**: los downlinks son minúsculos; no sirve para bajar contenido.
