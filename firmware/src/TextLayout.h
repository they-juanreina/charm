#pragma once
// Lógica pura de texto: UTF-8 -> Windows-1252, métricas de fuente GFX, wrap por verso y paginación.
#include <stdint.h>
#include <gfxfont.h>
#include <string>
#include <vector>

namespace TextLayout
{

// Re-codifica UTF-8 a cp1252 (los glifos de las fuentes DepartureMono_Win1252). Lo que no existe -> '?'.
std::string utf8ToCp1252(const std::string &utf8);

struct FontMetrics {
    const GFXfont *font = nullptr;
    uint8_t lineH = 0;   // avance vertical entre líneas
    uint8_t ascent = 0;  // píxeles por encima de la línea base
    uint8_t descent = 0; // píxeles por debajo
    uint8_t spaceW = 0;  // avance del espacio
};

FontMetrics measure(const GFXfont *font);
uint16_t textWidth(const FontMetrics &m, const uint8_t *s, size_t n);
inline uint16_t textWidth(const FontMetrics &m, const std::string &s)
{
    return textWidth(m, (const uint8_t *)s.data(), s.size());
}

struct VisualLine {
    uint16_t off;   // offset en Layout::body (cp1252)
    uint16_t len;   // 0 = línea en blanco
    uint8_t indent; // sangría en caracteres (continuación de un verso largo)
};

struct Page {
    uint16_t start, end; // rango [start, end) en Layout::lines
};

struct Layout {
    std::string title; // cp1252, vacío si no hay
    std::string body;  // cp1252
    std::vector<VisualLine> lines;
    std::vector<Page> pages;
    uint8_t pageCount() const { return (uint8_t)pages.size(); }
};

struct PaginateOpts {
    uint16_t widthPx;     // ancho útil de una línea
    uint8_t linesPage0;   // líneas que caben en la primera página (menos si hay título)
    uint8_t linesPageN;   // líneas en las siguientes
    uint8_t indentChars;  // sangría francesa de las continuaciones
    uint8_t maxPages;
};

// Separa el título (primera línea que empieza por '#'), envuelve cada verso y reparte en páginas.
void paginate(const std::string &utf8, const FontMetrics &font, const PaginateOpts &opts, Layout &out);

// Solo separa el título (para listados). Devuelve el título en UTF-8 sin el '#'.
std::string extractTitle(const std::string &utf8);

} // namespace TextLayout
