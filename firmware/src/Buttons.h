#pragma once
// Dos botones con debounce y eventos corto/largo. El largo se dispara mientras se mantiene pulsado.
#include "config.h"
#include <Arduino.h>

enum class BtnEvent : uint8_t { None, NavShort, NavLong, AuxShort, AuxLong };

class Buttons
{
  public:
    void begin();
    BtnEvent poll();
    bool anyPressed() const { return _b[0].pressed || _b[1].pressed; }

    // Tras despertar por EXT1: clasifica la pulsación que despertó al chip (bloquea hasta WAKE_LONG_MS como máximo).
    BtnEvent classifyWakePress(int pin);

  private:
    struct State {
        uint8_t pin;
        bool rawLast;      // última lectura cruda (true = pulsado)
        bool pressed;      // estado estable
        bool longFired;
        bool consumed;     // la pulsación ya generó evento (wake): ignorar hasta soltar
        uint32_t lastEdge;
        uint32_t downAt;
    } _b[2];

    static BtnEvent make(int idx, bool isLong);
};
