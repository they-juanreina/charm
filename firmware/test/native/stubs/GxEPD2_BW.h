// Sustituto del driver GxEPD2: el panel no existe en la Mac; el lienzo del charm es lo que se captura.
#pragma once
#include "SPI.h"
#include <Adafruit_GFX.h>
#define GxEPD_BLACK 0x0000
template <typename Driver, uint16_t H> class GxEPD2_BW : public Adafruit_GFX
{
  public:
    GxEPD2_BW(Driver) : Adafruit_GFX(Driver::WIDTH, H) {}
    void drawPixel(int16_t, int16_t, uint16_t) override {}
    void init(uint32_t = 0) {}
    void setRotation(uint8_t r) { Adafruit_GFX::setRotation(r); }
    void setFullWindow() {}
    void setPartialWindow(uint16_t, uint16_t, uint16_t, uint16_t) {}
    void firstPage() {}
    bool nextPage() { return false; }
    void hibernate() {}
    void powerOff() {}
};
