#pragma once
#include <stdint.h>

// ---- Pines (Heltec Vision Master E213, ESP32-S3) ----
#define PIN_EINK_CS 5
#define PIN_EINK_BUSY 1
#define PIN_EINK_DC 2
#define PIN_EINK_RES 3
#define PIN_EINK_SCLK 4
#define PIN_EINK_MOSI 6

#define PIN_VEXT 18 // HIGH = alimenta el panel e-ink
#define VEXT_ON HIGH
#define VEXT_WARMUP_MS 150

#define PIN_BTN_NAV 0  // BOOT, junto al USB: largo = revelar, corto = página
#define PIN_BTN_AUX 21 // corto = cerrar carta, largo = WiFi

#define PIN_ADC_CTRL 46 // HIGH habilita el divisor de la batería
#define PIN_BATTERY 7
#define BATTERY_MULT (4.9f * 1.03f)

#define PIN_LORA_CS 8
#define PIN_LORA_SCK 9
#define PIN_LORA_MOSI 10
#define PIN_LORA_MISO 11
#define PIN_LORA_RST 12
#define PIN_LORA_BUSY 13
#define PIN_LORA_DIO1 14

// ---- Pantalla ----
#define SCREEN_W 250
#define SCREEN_H 122
#define BITMAP_BYTES 3904 // 250x122, 1 bpp, 32 bytes por fila
#define FAST_LIMIT 8      // refrescos rápidos seguidos antes de forzar uno completo

// ---- Botones ----
#define BTN_DEBOUNCE_MS 30
#define BTN_LONG_MS 650
#define WAKE_LONG_MS 450 // pulsación larga medida tras despertar (el arranque ya consumió ~200 ms)

// ---- Tiempos ----
#define SLEEP_LOCKED_MS 3000
#define SLEEP_REVEALED_MS 45000
#define WIFI_IDLE_MS (5UL * 60 * 1000)
#define WIFI_MAX_MS (20UL * 60 * 1000)

// ---- WiFi ----
// Nombre fijo, sin sufijo de MAC: es un objeto personal, no una flota. La clave sale de un verso suyo
// ("vuelvo sin duda / al hogar de tus huesos"). WPA2 exige 8 caracteres como mínimo.
#define AP_NAME "amarilla-marin"
#ifndef AP_PASSWORD
#define AP_PASSWORD "sin-duda"
#endif

// ---- Contenido ----
#define CARDS_DIR "/cards"
#define BACK_PATH "/back.bmp1"
// Dedicatoria de quien regala el charm: es la carta de as, la primera de cada baraja (id 0)
#define DEDICATION_PATH "/dedicatoria.txt"
#define DEDICATION_ID 0
#define CARD_MAX_BYTES 8192
#define MAX_CARD_ID 999
#define MAX_PAGES 40

// ---- Batería ----
#define BATTERY_WIFI_MIN_MV 3200 // celda casi vacía: el pico del WiFi la tumbaría
#define BATTERY_LOW_MV 3550
#define BATTERY_CHARGING_MV 4200 // por encima, la celda está en el cargador (o recién llena)
#define CHARGE_REFRESH_S 120     // cada cuánto despierta para actualizar la pantalla de carga

// ---- Nombre en el reverso (configurable desde la web) ----
#define DEFAULT_NAME "@stayyellow"
#define NAME_MAX_BYTES 32

#ifndef CHARM_FW_VERSION
#define CHARM_FW_VERSION "dev"
#endif
