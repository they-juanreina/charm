#pragma once
// Alimentación: VEXT del panel, radio LoRa dormida, batería, deep sleep y análisis del despertar.
#include <stdint.h>

namespace Power
{
void earlyInit();       // deshace holds de GPIO y enciende VEXT; llamar lo primero en setup()
void waitWarmup();      // espera a que el panel lleve VEXT_WARMUP_MS encendido
void radioSleep();      // SX1262 a SLEEP por SPI y CS/RST retenidos en HIGH
uint16_t batteryMv();
// Curva LiPo aproximada por tramos. Inline porque es cálculo puro: así las pantallas se pueden
// compilar fuera de la placa (test/native/render_screens.cpp) sin arrastrar el hardware.
inline uint8_t batteryPct(uint16_t mv)
{
    if (mv >= 4150)
        return 100;
    if (mv <= 3300)
        return 0;
    if (mv >= 3900)
        return 80 + (mv - 3900) * 20 / 250;
    if (mv >= 3700)
        return 30 + (mv - 3700) * 50 / 200;
    return (mv - 3300) * 30 / 400;
}
bool usbConnected();                // hay un host USB enumerado (no detecta cargadores sin datos)
bool isCharging(uint16_t mv);       // USB presente o tensión de celda en el cargador
bool wokeFromSleep();   // true si el arranque viene de deep sleep
int wakePin();          // 0, 21 o -1 si no despertó un botón
bool wokeByTimer();
void deepSleep(uint32_t timerSeconds = 0); // no vuelve; con timerSeconds > 0 también despierta por tiempo
} // namespace Power
