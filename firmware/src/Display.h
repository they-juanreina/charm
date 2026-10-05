#pragma once
// Panel e-ink: detección, GxEPD2, lienzo 1 bpp de 250x122, política de refresco y pantallas del charm.
// Los poemas van en negro sobre blanco con la serif; las cartas y la interfaz, en blanco sobre negro
// con pixel art (sprites de src/Sprites.h y Departure Mono). Ver el archivo de Paper "Noble nebula".
#include "EinkDetect.h"
#include "Sprites.h"
#include "TextLayout.h"
#include "config.h"
#include <Adafruit_GFX.h>
#include <string>

class Display
{
  public:
    enum class Refresh { Full, Fast };

    bool begin();
    const char *panelName() const { return einkModelName(_model); }

    uint8_t fastCount = 0;       // refrescos rápidos seguidos (se guarda en RTC RAM)
    bool firstAfterSleep = true; // el controlador perdió su RAM: el próximo refresco debe ser completo

    void flush(Refresh r);

    // El lienzo es byte a byte idéntico a un .bmp1: lo sirve /api/screen para comparar con el diseño
    const uint8_t *buffer() const { return _canvas.getBuffer(); }

    // --- Poemas: fondo blanco, tipografía serif. Sin cambios respecto al diseño anterior ---
    void layoutCard(const std::string &utf8, TextLayout::Layout &out);
    void showPage(const TextLayout::Layout &L, uint8_t page);
    void showImage(const uint8_t *bmp);

    // --- Cartas e interfaz: blanco sobre negro ---
    // rank 0 = baraja recién barajada, se dibuja la carta de as con el nombre
    void showBack(const uint8_t *custom, const std::string &nameUtf8, uint16_t rank, uint16_t total, uint16_t mv);
    void showWifi(const char *ssid, const char *pass, const char *url);
    void showWifiConnected(const std::string &nameUtf8, size_t cards);
    void showCharging(const std::string &nameUtf8, uint16_t mv, bool usb);
    void showNotice(const char *titleUtf8, const char *l1, const char *l2, const char *l3);

  private:
    GFXcanvas1 _canvas{SCREEN_W, SCREEN_H};
    EinkModel _model = EinkModel::LCMEN213EFC1;
    void *_epd = nullptr; // GxEPD2_Multi<...>, oculto para no arrastrar GxEPD2 a todos los includes
    TextLayout::FontMetrics _fBody, _fTitle, _fSmall; // serif, solo para poemas
    TextLayout::FontMetrics _fPix, _fPixBig;          // Departure Mono 11 y 22

    void text(const TextLayout::FontMetrics &f, int16_t x, int16_t baseline, const uint8_t *s, size_t n, uint16_t color);
    void text(const TextLayout::FontMetrics &f, int16_t x, int16_t baseline, const std::string &cp1252, uint16_t color);
    void centered(const TextLayout::FontMetrics &f, int16_t baseline, const std::string &cp1252, uint16_t color);
    void sprite(int16_t x, int16_t y, const Sprite &s, uint16_t color);
    void frame(uint16_t color);
    void sparkles(uint16_t color, const int16_t *avoid, uint8_t avoidCount);
    void cornerIndex(const std::string &rank, uint16_t color);
    void batteryIcon(int16_t x, int16_t y, uint8_t pct, uint16_t color);
    int16_t drawQr(int16_t x, int16_t y, uint8_t scale, uint8_t version, const char *text);
    // Pantallas de interfaz: marco + icono a la izquierda + columna de texto a la derecha
    void panel(const char *titleCp1252, const char *l1, const char *l2, const char *l3, int16_t col);
};
