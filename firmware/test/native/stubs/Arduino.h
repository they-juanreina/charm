// Simulación mínima de Arduino para compilar las pantallas del charm en la Mac.
// Solo lo que usan Adafruit_GFX, Display.cpp y EinkDetect.h. Nada de esto llega a la placa.
#pragma once
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#define ARDUINO 10800
#define HIGH 1
#define LOW 0
#define INPUT 0
#define OUTPUT 1
#define PROGMEM
#define PGM_P const char *
#ifndef pgm_read_byte
#define pgm_read_byte(addr) (*(const unsigned char *)(addr))
#endif
#ifndef pgm_read_word
#define pgm_read_word(addr) (*(const unsigned short *)(addr))
#endif
#ifndef pgm_read_dword
#define pgm_read_dword(addr) (*(const unsigned long *)(addr))
#endif
typedef bool boolean;
#define radians(deg) ((deg) * 0.017453292519943295)

inline void pinMode(int, int) {}
inline void digitalWrite(int, int) {}
inline int digitalRead(int) { return HIGH; } // BUSY en alto = panel E0213A367, el que lleva el charm
inline void delay(unsigned long) {}
inline unsigned long millis() { return 0; }

#define log_i(...) ((void)0)
#define log_d(...) ((void)0)
#define log_e(...) ((void)0)
#define log_w(...) ((void)0)

class __FlashStringHelper;
class String
{
  public:
    String(const char *s = "") : _s(s) {}
    unsigned int length() const { return _s.size(); }
    const char *c_str() const { return _s.c_str(); }

  private:
    std::string _s;
};

#include "Print.h"
