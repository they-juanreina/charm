#include "Buttons.h"

void Buttons::begin()
{
    _b[0] = {PIN_BTN_NAV, false, false, false, false, 0, 0};
    _b[1] = {PIN_BTN_AUX, false, false, false, false, 0, 0};
    for (auto &b : _b) {
        pinMode(b.pin, INPUT_PULLUP);
        b.rawLast = digitalRead(b.pin) == LOW;
        b.pressed = b.rawLast;
        b.consumed = b.pressed; // si arranca pulsado sin wake, no generar evento espurio
        b.lastEdge = millis();
    }
}

BtnEvent Buttons::make(int idx, bool isLong)
{
    if (idx == 0)
        return isLong ? BtnEvent::NavLong : BtnEvent::NavShort;
    return isLong ? BtnEvent::AuxLong : BtnEvent::AuxShort;
}

BtnEvent Buttons::poll()
{
    uint32_t now = millis();
    for (int i = 0; i < 2; i++) {
        State &b = _b[i];
        bool raw = digitalRead(b.pin) == LOW;
        if (raw != b.rawLast) {
            b.rawLast = raw;
            b.lastEdge = now;
        }
        if (raw != b.pressed && now - b.lastEdge >= BTN_DEBOUNCE_MS) {
            b.pressed = raw;
            if (raw) {
                b.downAt = now;
                b.longFired = false;
            } else {
                bool wasConsumed = b.consumed;
                bool wasLong = b.longFired;
                b.consumed = false;
                if (!wasConsumed && !wasLong)
                    return make(i, false);
            }
        }
        if (b.pressed && !b.longFired && !b.consumed && now - b.downAt >= BTN_LONG_MS) {
            b.longFired = true;
            return make(i, true);
        }
    }
    return BtnEvent::None;
}

BtnEvent Buttons::classifyWakePress(int pin)
{
    int idx = pin == PIN_BTN_AUX ? 1 : 0;
    State &b = _b[idx];
    uint32_t t0 = millis();
    if (digitalRead(b.pin) == HIGH) {
        b.rawLast = b.pressed = false;
        b.consumed = false;
        return make(idx, false); // soltado durante el arranque: pulsación corta
    }
    while (digitalRead(b.pin) == LOW) {
        if (millis() - t0 >= WAKE_LONG_MS) {
            b.rawLast = b.pressed = true;
            b.longFired = true;
            b.consumed = true; // el poll ignorará la suelta
            b.downAt = t0;
            b.lastEdge = t0;
            return make(idx, true);
        }
        delay(5);
    }
    b.rawLast = b.pressed = false;
    b.consumed = false;
    b.lastEdge = millis();
    return make(idx, false);
}
