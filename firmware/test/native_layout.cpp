// Prueba nativa de TextLayout (sin hardware):
//   g++ -std=c++17 -I".pio/libdeps/charm-dev/Adafruit GFX Library" -Isrc test/native_layout.cpp src/TextLayout.cpp -o /tmp/layout && /tmp/layout
#include "TextLayout.h"
#include <cassert>
#include <cstdio>
#include <cstring>

// Fuente monoespaciada sintética: 5 px por glifo, 11 px de línea, rango 0x01-0xFF
static GFXglyph glyphs[255];
static GFXfont font = {nullptr, glyphs, 0x01, 0xFF, 11};

static void dump(const TextLayout::Layout &L)
{
    printf("title='%s' lines=%zu pages=%zu\n", L.title.c_str(), L.lines.size(), L.pages.size());
    for (size_t p = 0; p < L.pages.size(); p++) {
        printf("--- pagina %zu/%zu\n", p + 1, L.pages.size());
        for (uint16_t i = L.pages[p].start; i < L.pages[p].end; i++) {
            const auto &l = L.lines[i];
            printf("%*s|%.*s|\n", l.indent, "", l.len, L.body.c_str() + l.off);
        }
    }
}

int main()
{
    for (int i = 0; i < 255; i++)
        glyphs[i] = {0, 5, 8, 5, 0, -8};
    glyphs[' ' - 1] = {0, 0, 0, 5, 0, 0};
    TextLayout::FontMetrics m = TextLayout::measure(&font);
    assert(m.lineH == 11 && m.ascent == 8 && m.spaceW == 5);

    std::string c = TextLayout::utf8ToCp1252("¿Qué? ¡Sí! ñÑ “—” …€");
    assert((uint8_t)c[0] == 0xBF && (uint8_t)c[3] == 0xE9 && (uint8_t)c.back() == 0x80);
    assert(TextLayout::utf8ToCp1252("\xE2\x82") == "?");

    TextLayout::PaginateOpts o{238, 7, 9, 2, 40}; // 47 chars por línea
    TextLayout::Layout L;
    std::string poem = "# Rima XXI\r\n\r\n¿Qué es poesía?, dices mientras clavas\nen mi pupila tu pupila azul.\n"
                       "\n\n¿Qué es poesía? ¿Y tú me lo preguntas?\nPoesía... eres tú.\n"
                       "Este es un verso deliberadamente largo que no cabe en una sola línea de cuarenta y siete "
                       "caracteres y debe partirse con sangría.\n"
                       "Supercalifragilisticoespialidosoextraordinariamentelargapalabra corta\n\n\n";
    TextLayout::paginate(poem, m, o, L);
    dump(L);
    assert(L.title == "Rima XXI");
    assert(L.pages.size() == 2);
    assert(L.lines[0].len > 0);                           // sin blanco inicial
    assert(L.lines[L.pages[0].start].len > 0);            // ninguna página empieza en blanco
    assert(L.lines[L.pages[1].start].len > 0);
    for (auto &l : L.lines)
        assert(TextLayout::textWidth(m, (const uint8_t *)L.body.data() + l.off, l.len) + l.indent * 5 <= 238);

    TextLayout::paginate("solo una línea", m, o, L);
    assert(L.pages.size() == 1 && L.title.empty() && L.lines.size() == 1);
    TextLayout::paginate("# Solo título\n", m, o, L);
    assert(L.pages.size() == 1 && L.pages[0].start == L.pages[0].end);
    TextLayout::paginate("", m, o, L);
    assert(L.pages.size() == 1);
    assert(TextLayout::extractTitle("#   Hola  \nx") == "Hola");
    printf("OK\n");
    return 0;
}
