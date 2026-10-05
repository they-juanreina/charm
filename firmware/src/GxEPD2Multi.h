#pragma once
// Wrapper para elegir entre dos drivers GxEPD2_BW en runtime (los objetos no comparten clase base).
// Copiado y recortado de meshtastic/firmware src/graphics/GxEPD2Multi.h.
#include <GxEPD2_BW.h>

template <typename Driver0, typename Driver1> class GxEPD2_Multi
{
  public:
    GxEPD2_Multi(uint8_t whichDriver, int16_t cs, int16_t dc, int16_t rst, int16_t busy, SPIClass &spi) : which(whichDriver)
    {
        if (which == 0)
            driver0 = new GxEPD2_BW<Driver0, Driver0::HEIGHT>(Driver0(cs, dc, rst, busy, spi));
        else
            driver1 = new GxEPD2_BW<Driver1, Driver1::HEIGHT>(Driver1(cs, dc, rst, busy, spi));
    }

    Adafruit_GFX *gfx() { return which == 0 ? (Adafruit_GFX *)driver0 : (Adafruit_GFX *)driver1; }

    void init(uint32_t serial_diag_bitrate = 0)
    {
        if (which == 0)
            driver0->init(serial_diag_bitrate);
        else
            driver1->init(serial_diag_bitrate);
    }
    void setRotation(uint8_t r)
    {
        if (which == 0)
            driver0->setRotation(r);
        else
            driver1->setRotation(r);
    }
    void setFullWindow()
    {
        if (which == 0)
            driver0->setFullWindow();
        else
            driver1->setFullWindow();
    }
    void setPartialWindow(uint16_t x, uint16_t y, uint16_t w, uint16_t h)
    {
        if (which == 0)
            driver0->setPartialWindow(x, y, w, h);
        else
            driver1->setPartialWindow(x, y, w, h);
    }
    void firstPage()
    {
        if (which == 0)
            driver0->firstPage();
        else
            driver1->firstPage();
    }
    bool nextPage()
    {
        if (which == 0)
            return driver0->nextPage();
        else
            return driver1->nextPage();
    }
    void hibernate()
    {
        if (which == 0)
            driver0->hibernate();
        else
            driver1->hibernate();
    }
    void powerOff()
    {
        if (which == 0)
            driver0->powerOff();
        else
            driver1->powerOff();
    }

  private:
    uint8_t which;
    GxEPD2_BW<Driver0, Driver0::HEIGHT> *driver0 = nullptr;
    GxEPD2_BW<Driver1, Driver1::HEIGHT> *driver1 = nullptr;
};
