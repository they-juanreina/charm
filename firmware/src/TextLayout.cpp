#include "TextLayout.h"
#include <algorithm>

namespace TextLayout
{

static uint8_t cp1252(uint32_t cp)
{
    if (cp < 0x80)
        return (uint8_t)cp;
    if (cp == 0xA0)
        return ' ';
    if (cp >= 0xA0 && cp <= 0xFF)
        return (uint8_t)cp; // Latin-1 coincide con cp1252 en este rango
    switch (cp) {
    case 0x20AC: return 0x80; // €
    case 0x201A: return 0x82;
    case 0x0192: return 0x83;
    case 0x201E: return 0x84;
    case 0x2026: return 0x85; // …
    case 0x2020: return 0x86;
    case 0x2021: return 0x87;
    case 0x02C6: return 0x88;
    case 0x2030: return 0x89;
    case 0x0160: return 0x8A;
    case 0x2039: return 0x8B;
    case 0x0152: return 0x8C;
    case 0x017D: return 0x8E;
    case 0x2018: return 0x91; // ‘
    case 0x2019: return 0x92; // ’
    case 0x201C: return 0x93; // “
    case 0x201D: return 0x94; // ”
    case 0x2022: return 0x95; // •
    case 0x2013: return 0x96; // –
    case 0x2014: return 0x97; // —
    case 0x02DC: return 0x98;
    case 0x2122: return 0x99;
    case 0x0161: return 0x9A;
    case 0x203A: return 0x9B;
    case 0x0153: return 0x9C;
    case 0x017E: return 0x9E;
    case 0x0178: return 0x9F;
    default: return '?';
    }
}

std::string utf8ToCp1252(const std::string &in)
{
    std::string out;
    out.reserve(in.size());
    size_t i = 0, n = in.size();
    while (i < n) {
        uint8_t b = (uint8_t)in[i];
        uint32_t cp;
        size_t len;
        if (b < 0x80) {
            cp = b;
            len = 1;
        } else if ((b & 0xE0) == 0xC0) {
            cp = b & 0x1F;
            len = 2;
        } else if ((b & 0xF0) == 0xE0) {
            cp = b & 0x0F;
            len = 3;
        } else if ((b & 0xF8) == 0xF0) {
            cp = b & 0x07;
            len = 4;
        } else {
            out.push_back('?');
            i++;
            continue;
        }
        if (i + len > n) {
            out.push_back('?');
            break;
        }
        bool ok = true;
        for (size_t k = 1; k < len; k++) {
            uint8_t c = (uint8_t)in[i + k];
            if ((c & 0xC0) != 0x80) {
                ok = false;
                break;
            }
            cp = (cp << 6) | (c & 0x3F);
        }
        if (!ok) {
            out.push_back('?');
            i++;
            continue;
        }
        if (cp == '\n' || cp == '\t' || cp >= 0x20)
            out.push_back((char)cp1252(cp));
        i += len;
    }
    return out;
}

FontMetrics measure(const GFXfont *font)
{
    FontMetrics m;
    m.font = font;
    m.lineH = font->yAdvance;
    int asc = 0, desc = 0;
    for (uint16_t c = font->first; c <= font->last; c++) {
        const GFXglyph &g = font->glyph[c - font->first];
        if (g.height == 0)
            continue;
        asc = std::max(asc, (int)-g.yOffset);
        desc = std::max(desc, (int)g.height + (int)g.yOffset);
    }
    m.ascent = (uint8_t)std::max(asc, 0);
    m.descent = (uint8_t)std::max(desc, 0);
    m.spaceW = (' ' >= font->first && ' ' <= font->last) ? font->glyph[' ' - font->first].xAdvance : m.lineH / 2;
    return m;
}

uint16_t textWidth(const FontMetrics &m, const uint8_t *s, size_t n)
{
    uint32_t w = 0;
    for (size_t i = 0; i < n; i++) {
        uint8_t c = s[i];
        if (c < m.font->first || c > m.font->last)
            c = '?';
        w += m.font->glyph[c - m.font->first].xAdvance;
    }
    return (uint16_t)std::min<uint32_t>(w, 0xFFFF);
}

std::string extractTitle(const std::string &utf8)
{
    if (utf8.empty() || utf8[0] != '#')
        return "";
    size_t nl = utf8.find('\n');
    std::string t = utf8.substr(1, nl == std::string::npos ? std::string::npos : nl - 1);
    size_t a = t.find_first_not_of(" \t\r");
    size_t b = t.find_last_not_of(" \t\r");
    return a == std::string::npos ? "" : t.substr(a, b - a + 1);
}

// Envuelve un verso [off, off+len) del cuerpo y añade las líneas visuales resultantes.
static void wrapLine(const Layout &L, const FontMetrics &font, const PaginateOpts &opts, uint16_t off, uint16_t len,
                     std::vector<VisualLine> &lines)
{
    if (len == 0) {
        lines.push_back({off, 0, 0});
        return;
    }
    const uint8_t *s = (const uint8_t *)L.body.data() + off;
    const uint16_t indentPx = opts.indentChars * font.spaceW;
    uint16_t curS = 0, curE = 0; // línea en construcción, relativa a off
    uint8_t indent = 0;
    auto emit = [&]() {
        lines.push_back({(uint16_t)(off + curS), (uint16_t)(curE - curS), indent});
        indent = opts.indentChars;
    };
    uint16_t i = 0;
    while (i < len) {
        while (i < len && s[i] == ' ')
            i++;
        if (i >= len)
            break;
        uint16_t ws = i;
        while (i < len && s[i] != ' ')
            i++;
        uint16_t we = i;
        while (true) {
            uint16_t avail = opts.widthPx > (indent ? indentPx : 0) ? opts.widthPx - (indent ? indentPx : 0) : 1;
            if (curE == curS) {
                if (textWidth(font, s + ws, we - ws) <= avail) {
                    curS = ws;
                    curE = we;
                    break;
                }
                // palabra más ancha que la línea: cortar en el último glifo que cabe
                uint16_t k = ws + 1;
                while (k < we && textWidth(font, s + ws, k + 1 - ws) <= avail)
                    k++;
                curS = ws;
                curE = k;
                emit();
                curS = curE = k;
                ws = k;
                if (ws >= we)
                    break;
                continue;
            }
            if (textWidth(font, s + curS, we - curS) <= avail) {
                curE = we;
                break;
            }
            emit();
            curS = curE = 0; // vacía; la palabra se reintenta en línea nueva
        }
    }
    if (curE > curS)
        emit();
}

void paginate(const std::string &utf8, const FontMetrics &font, const PaginateOpts &opts, Layout &out)
{
    out = Layout();
    std::string text = utf8;
    text.erase(std::remove(text.begin(), text.end(), '\r'), text.end());

    std::string title = extractTitle(text);
    if (!title.empty() || (!text.empty() && text[0] == '#')) {
        size_t nl = text.find('\n');
        text = nl == std::string::npos ? "" : text.substr(nl + 1);
    }
    out.title = utf8ToCp1252(title);
    out.body = utf8ToCp1252(text);
    for (char &c : out.body)
        if (c == '\t')
            c = ' ';

    // versos lógicos: quitar espacios finales, colapsar blancos repetidos, sin blancos al inicio/fin
    std::vector<std::pair<uint16_t, uint16_t>> logical;
    size_t pos = 0, n = out.body.size();
    bool lastBlank = true;
    while (pos <= n && n > 0) {
        size_t nl = out.body.find('\n', pos);
        size_t end = nl == std::string::npos ? n : nl;
        size_t e = end;
        while (e > pos && out.body[e - 1] == ' ')
            e--;
        size_t b = pos;
        while (b < e && out.body[b] == ' ')
            b++;
        bool blank = (b == e);
        if (!(blank && lastBlank))
            logical.push_back({(uint16_t)b, (uint16_t)(e - b)});
        lastBlank = blank;
        if (nl == std::string::npos)
            break;
        pos = nl + 1;
    }
    while (!logical.empty() && logical.back().second == 0)
        logical.pop_back();

    for (auto &lg : logical)
        wrapLine(out, font, opts, lg.first, lg.second, out.lines);

    size_t i = 0, total = out.lines.size();
    while (i < total && out.pages.size() < opts.maxPages) {
        while (i < total && out.lines[i].len == 0)
            i++;
        if (i >= total)
            break;
        uint8_t cap = out.pages.empty() ? opts.linesPage0 : opts.linesPageN;
        if (cap == 0)
            cap = 1;
        size_t end = std::min(total, i + cap);
        size_t e = end;
        while (e > i + 1 && out.lines[e - 1].len == 0)
            e--;
        out.pages.push_back({(uint16_t)i, (uint16_t)e});
        i = end;
    }
    if (out.pages.empty())
        out.pages.push_back({0, 0}); // solo título, o carta vacía
}

} // namespace TextLayout
