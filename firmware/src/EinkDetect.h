#pragma once
// Detección del panel en runtime, portada de Meshtastic (variants/.../einkDetect.h).
// Con RST activo el controlador reporta BUSY: Fitipower (LCMEN2R13EFC1) en LOW, Solomon (E0213A367) en HIGH.
#include "config.h"
#include <Arduino.h>

enum class EinkModel : uint8_t {
    LCMEN213EFC1 = 0, // E213 V1 (sin marca)
    E0213A367 = 1,    // E213 PCB V1.1 (mediados de 2025)
};

inline EinkModel detectEink()
{
    pinMode(PIN_EINK_RES, OUTPUT);
    digitalWrite(PIN_EINK_RES, LOW);
    delay(10);
    pinMode(PIN_EINK_BUSY, INPUT);
    bool busy = digitalRead(PIN_EINK_BUSY);
    pinMode(PIN_EINK_RES, INPUT);
    return busy == LOW ? EinkModel::LCMEN213EFC1 : EinkModel::E0213A367;
}

inline const char *einkModelName(EinkModel m)
{
    return m == EinkModel::LCMEN213EFC1 ? "FC1" : "A367";
}
