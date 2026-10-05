#pragma once
#include "../SPI.h"
#include <stdint.h>
class GxEPD2_213_FC1
{
  public:
    static const uint16_t WIDTH = 128;
    static const uint16_t HEIGHT = 250;
    GxEPD2_213_FC1(int16_t, int16_t, int16_t, int16_t, SPIClass &) {}
};
