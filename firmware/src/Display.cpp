#include "Display.h"
#include "GxEPD2Multi.h"
#include "Power.h"
#include <SPI.h>
#include <algorithm>
#include <epd/GxEPD2_213_E0213A367.h>
#include <epd/GxEPD2_213_FC1.h>
#include <qrcode.h>

#include "fonts/DepartureMono11.h"
#include "fonts/DepartureMono22.h"
#include "fonts/NcenB12.h"
#include "fonts/NcenR08.h"
#include "fonts/NcenR10.h"

typedef GxEPD2_Multi<GxEPD2_213_FC1, GxEPD2_213_E0213A367> Epd;
#define EPD ((Epd *)_epd)

static const int16_t MARGIN_X = 6;
static const int16_t MARGIN_TOP = 4;
static const int16_t FOOTER_H = 10;
static const int16_t TITLE_GAP = 4;
static const uint8_t INDENT_CHARS = 2;
static const uint16_t INK = 1;   // bit a 1 en el lienzo = negro
static const uint16_t VOID = 0;  // bit a 0 = blanco

// Campo donde caben los corazones, entre los dos índices de esquina (coordenadas de Paper)
static const int16_t FIELD_X = 46, FIELD_W = 158, FIELD_Y = 12, FIELD_H = 98;
static const int16_t COL_TEXT = 108; // columna de texto; a 7 px por carácter caben 19 hasta el marco

// Constelación fija, copiada del archivo de Paper. Fija y no aleatoria para que una misma carta
// se vea siempre igual y para que coincida con el diseño.
static const int16_t SPARKS[][2] = {{44, 14},  {96, 10},  {148, 18}, {196, 12}, {90, 56},
                                    {156, 60}, {44, 100}, {196, 96}, {222, 48}, {26, 62}};
static const int16_t DOTS[][2] = {{70, 16},  {130, 22}, {180, 14}, {60, 58},  {122, 64}, {200, 58},
                                  {86, 104}, {150, 100}, {216, 20}, {34, 90}, {108, 46}, {172, 108}};

// Rango -> (columnas, filas) de la rejilla de corazones. Por encima de 24 se dibuja el número grande.
static const uint8_t GRID[25][2] = {{0, 0}, {1, 1}, {2, 1}, {3, 1}, {2, 2}, {3, 2}, {3, 2}, {4, 2}, {4, 2},
                                    {3, 3}, {5, 2}, {4, 3}, {4, 3}, {5, 3}, {5, 3}, {5, 3}, {6, 3}, {6, 3},
                                    {6, 3}, {5, 4}, {5, 4}, {6, 4}, {6, 4}, {6, 4}, {6, 4}};

bool Display::begin()
{
    _model = detectEink();
    pinMode(PIN_EINK_CS, OUTPUT); // GxEPD2 escribe en CS/DC antes de configurarlos y el HAL se queja
    pinMode(PIN_EINK_DC, OUTPUT);
    static SPIClass hspi(HSPI);
    hspi.begin(PIN_EINK_SCLK, -1, PIN_EINK_MOSI, PIN_EINK_CS);
    _epd = new Epd((uint8_t)_model, PIN_EINK_CS, PIN_EINK_DC, PIN_EINK_RES, PIN_EINK_BUSY, hspi);
    EPD->init(0);
    EPD->setRotation(3);

    _fBody = TextLayout::measure(&NcenR10);
    _fTitle = TextLayout::measure(&NcenB12);
    _fSmall = TextLayout::measure(&NcenR08);
    _fPix = TextLayout::measure(&DepartureMono11);
    _fPixBig = TextLayout::measure(&DepartureMono22);
    _canvas.setTextWrap(false);
    log_i("panel %s, serif %u/%u/%u px, pixel %u/%u px", panelName(), _fSmall.lineH, _fBody.lineH, _fTitle.lineH,
          _fPix.lineH, _fPixBig.lineH);
    return true;
}

void Display::flush(Refresh r)
{
    bool full = r == Refresh::Full || fastCount >= FAST_LIMIT || firstAfterSleep;
    if (full) {
        EPD->setFullWindow();
        fastCount = 0;
    } else {
        EPD->setPartialWindow(0, 0, SCREEN_W, SCREEN_H);
        fastCount++;
    }
    firstAfterSleep = false;
    uint32_t t = millis();
    (void)t;
    EPD->firstPage();
    EPD->gfx()->drawBitmap(0, 0, _canvas.getBuffer(), SCREEN_W, SCREEN_H, GxEPD_BLACK);
    EPD->nextPage();
    EPD->hibernate();
    log_d("refresco %s en %lu ms", full ? "FULL" : "FAST", (unsigned long)(millis() - t));
}

// ---------------------------------------------------------------- primitivas

void Display::text(const TextLayout::FontMetrics &f, int16_t x, int16_t baseline, const uint8_t *s, size_t n,
                   uint16_t color)
{
    _canvas.setFont(f.font);
    _canvas.setTextColor(color); // el color es pegajoso en el lienzo: se fija en cada llamada
    _canvas.setCursor(x, baseline);
    _canvas.write(s, n);
}

void Display::text(const TextLayout::FontMetrics &f, int16_t x, int16_t baseline, const std::string &s, uint16_t color)
{
    text(f, x, baseline, (const uint8_t *)s.data(), s.size(), color);
}

void Display::centered(const TextLayout::FontMetrics &f, int16_t baseline, const std::string &s, uint16_t color)
{
    text(f, (SCREEN_W - TextLayout::textWidth(f, s)) / 2, baseline, s, color);
}

void Display::sprite(int16_t x, int16_t y, const Sprite &s, uint16_t color)
{
    _canvas.drawBitmap(x, y, s.bits, s.w, s.h, color);
}

// Marco de carta con las esquinas escalonadas en dos pasos de 2 px
void Display::frame(uint16_t color)
{
    _canvas.fillRect(7, 3, 236, 2, color);
    _canvas.fillRect(7, 117, 236, 2, color);
    _canvas.fillRect(3, 7, 2, 108, color);
    _canvas.fillRect(245, 7, 2, 108, color);
    _canvas.fillRect(5, 5, 2, 2, color);
    _canvas.fillRect(243, 5, 2, 2, color);
    _canvas.fillRect(5, 115, 2, 2, color);
    _canvas.fillRect(243, 115, 2, 2, color);
}

// Destellos de la constelación, saltando los que caerían encima de un corazón.
// `avoid` son cuartetos x,y,w,h.
void Display::sparkles(uint16_t color, const int16_t *avoid, uint8_t avoidCount)
{
    auto clear = [&](int16_t x, int16_t y, int16_t w, int16_t h) {
        for (uint8_t i = 0; i < avoidCount; i++) {
            const int16_t *r = avoid + i * 4;
            if (x < r[0] + r[2] && x + w > r[0] && y < r[1] + r[3] && y + h > r[1])
                return false;
        }
        return true;
    };
    for (auto &s : SPARKS)
        if (clear(s[0], s[1], spark.w, spark.h))
            sprite(s[0], s[1], spark, color);
    for (auto &d : DOTS)
        if (clear(d[0], d[1], 2, 2))
            _canvas.fillRect(d[0], d[1], 2, 2, color);
}

// Zonas de los índices de esquina (con el "de N" debajo del de arriba), para que los destellos no se peguen
static uint8_t indexBoxes(int16_t *a, int16_t w)
{
    const int16_t b[] = {7, 6, (int16_t)(w + 10), 52, (int16_t)(SCREEN_W - 11 - w - 6), 80, (int16_t)(w + 10), 36};
    for (int i = 0; i < 8; i++)
        a[i] = b[i];
    return 2;
}

// Índices de esquina: número arriba a la izquierda con su corazón debajo, y el espejo abajo a la derecha
void Display::cornerIndex(const std::string &rank, uint16_t color)
{
    int16_t w = TextLayout::textWidth(_fPixBig, rank);
    text(_fPixBig, 11, 26, rank, color);
    sprite(11 + (w - heartSmall.w) / 2, 30, heartSmall, color);
    int16_t rx = SCREEN_W - 11 - w;
    sprite(rx + (w - heartSmall.w) / 2, 84, heartSmall, color);
    text(_fPixBig, rx, 112, rank, color);
}

void Display::batteryIcon(int16_t x, int16_t y, uint8_t pct, uint16_t color)
{
    _canvas.drawRect(x, y, 14, 7, color);
    _canvas.fillRect(x + 14, y + 2, 2, 3, color);
    int16_t fill = (12 * pct) / 100;
    if (fill > 0)
        _canvas.fillRect(x + 1, y + 1, fill, 5, color);
}

int16_t Display::drawQr(int16_t x, int16_t y, uint8_t scale, uint8_t version, const char *txt)
{
    QRCode qr;
    uint8_t data[qrcode_getBufferSize(4)];
    if (qrcode_initText(&qr, data, version, ECC_MEDIUM, txt) != 0)
        return 0;
    for (uint8_t yy = 0; yy < qr.size; yy++)
        for (uint8_t xx = 0; xx < qr.size; xx++)
            if (qrcode_getModule(&qr, xx, yy))
                _canvas.fillRect(x + xx * scale, y + yy * scale, scale, scale, INK);
    return qr.size * scale;
}

// Columna de texto común a WiFi, Conectado, Cargando y los mensajes
void Display::panel(const char *titleCp1252, const char *l1, const char *l2, const char *l3, int16_t col)
{
    std::string title(titleCp1252);
    if (TextLayout::textWidth(_fPixBig, title) <= 244 - col)
        text(_fPixBig, col, 30, title, VOID);
    else // no cabe a 22 px: mejor pequeño que cortado por el marco
        text(_fPix, col, 28, title, VOID);
    int16_t y = 50;
    for (const char *l : {l1, l2, l3}) {
        if (l && *l) {
            text(_fPix, col, y + _fPix.ascent, std::string(l), VOID);
            y += _fPix.lineH;
        }
    }
}

// ---------------------------------------------------------------- poemas (sin cambios)

void Display::layoutCard(const std::string &utf8, TextLayout::Layout &out)
{
    bool hasTitle = !TextLayout::extractTitle(utf8).empty();
    uint8_t titleRows = hasTitle ? (uint8_t)((_fTitle.lineH + TITLE_GAP + _fBody.lineH - 1) / _fBody.lineH) : 0;
    auto run = [&](bool footer) {
        uint8_t linesN = (uint8_t)((SCREEN_H - MARGIN_TOP - (footer ? FOOTER_H : 0)) / _fBody.lineH);
        uint8_t lines0 = linesN > titleRows ? linesN - titleRows : 1;
        TextLayout::PaginateOpts o{(uint16_t)(SCREEN_W - 2 * MARGIN_X), lines0, linesN, INDENT_CHARS, MAX_PAGES};
        TextLayout::paginate(utf8, _fBody, o, out);
    };
    run(false);
    if (out.pageCount() > 1)
        run(true);
}

void Display::showPage(const TextLayout::Layout &L, uint8_t page)
{
    _canvas.fillScreen(VOID);
    if (page >= L.pageCount())
        return;
    int16_t top = MARGIN_TOP;
    if (page == 0 && !L.title.empty()) {
        text(_fTitle, MARGIN_X, top + _fTitle.ascent, L.title, INK);
        top += _fTitle.lineH + TITLE_GAP;
    }
    const TextLayout::Page &P = L.pages[page];
    for (uint16_t i = P.start; i < P.end; i++) {
        const TextLayout::VisualLine &vl = L.lines[i];
        if (vl.len)
            text(_fBody, MARGIN_X + vl.indent * _fBody.spaceW, top + _fBody.ascent,
                 (const uint8_t *)L.body.data() + vl.off, vl.len, INK);
        top += _fBody.lineH;
    }
    if (L.pageCount() > 1) {
        char buf[12];
        snprintf(buf, sizeof(buf), "%u/%u", page + 1, L.pageCount());
        std::string s(buf);
        int16_t w = TextLayout::textWidth(_fSmall, s);
        text(_fSmall, SCREEN_W - MARGIN_X - w, SCREEN_H - 2, s, INK);
    }
}

void Display::showImage(const uint8_t *bmp)
{
    _canvas.fillScreen(VOID);
    _canvas.drawBitmap(0, 0, bmp, SCREEN_W, SCREEN_H, INK);
}

// ---------------------------------------------------------------- reverso: la carta del rango

void Display::showBack(const uint8_t *custom, const std::string &nameUtf8, uint16_t rank, uint16_t total, uint16_t mv)
{
    _canvas.fillScreen(INK); // negro: el arte se "recorta" en blanco
    if (custom) {
        _canvas.fillScreen(VOID);
        _canvas.drawBitmap(0, 0, custom, SCREEN_W, SCREEN_H, INK);
        return;
    }
    frame(VOID);

    if (rank == 0) { // baraja nueva: la carta de as con el nombre
        cornerIndex("A", VOID);
        std::string name = TextLayout::utf8ToCp1252(nameUtf8);
        const TextLayout::FontMetrics *f = &_fPixBig;
        if (TextLayout::textWidth(*f, name) > SCREEN_W - 60)
            f = &_fPix;
        int16_t nameW = TextLayout::textWidth(*f, name);
        int16_t nameTop = 61 - (f->lineH + 8 + heartSmall.h) / 2;
        centered(*f, nameTop + f->ascent, name, VOID);
        int16_t hx = (SCREEN_W - (3 * heartSmall.w + 2 * 16)) / 2;
        for (int i = 0; i < 3; i++)
            sprite(hx + i * (heartSmall.w + 16), nameTop + f->lineH + 8, heartSmall, VOID);
        int16_t avoid[3 * 4] = {(int16_t)((SCREEN_W - nameW) / 2 - 6), (int16_t)(nameTop - 2), (int16_t)(nameW + 12),
                                (int16_t)(f->lineH + 8 + heartSmall.h + 4)};
        uint8_t n = 1 + indexBoxes(avoid + 4, TextLayout::textWidth(_fPixBig, "A"));
        sparkles(VOID, avoid, n);
        return;
    }

    char buf[8];
    snprintf(buf, sizeof(buf), "%u", rank);
    cornerIndex(buf, VOID);

    if (rank > 24) { // ya no caben corazones: el número manda
        text(_fPixBig, (SCREEN_W - TextLayout::textWidth(_fPixBig, buf)) / 2, 61 + _fPixBig.ascent / 2, std::string(buf),
             VOID);
        sprite(SCREEN_W / 2 - heartSmall.w / 2, 24, heartSmall, VOID);
        int16_t avoid[3 * 4];
        uint8_t n = indexBoxes(avoid, TextLayout::textWidth(_fPixBig, buf));
        int16_t numW = TextLayout::textWidth(_fPixBig, buf);
        int16_t mid[] = {(int16_t)((SCREEN_W - numW) / 2 - 8), 20, (int16_t)(numW + 16), 70};
        for (int i = 0; i < 4; i++)
            avoid[n * 4 + i] = mid[i];
        sparkles(VOID, avoid, n + 1);
    } else {
        uint8_t cols = GRID[rank][0], rows = GRID[rank][1];
        // reparto por filas: el resto va a las filas de fuera, como en el diseño del 14 (5-4-5)
        uint8_t cnt[4] = {0, 0, 0, 0}, order[4], n = 0;
        for (int a = 0, b = rows - 1; a <= b; a++, b--) {
            order[n++] = a;
            if (a != b)
                order[n++] = b;
        }
        for (uint8_t r = 0; r < rows; r++)
            cnt[r] = rank / rows;
        for (uint8_t i = 0; i < rank % rows; i++)
            cnt[order[i]]++;

        int16_t avoid[(24 + 2) * 4];
        uint8_t placed = indexBoxes(avoid, TextLayout::textWidth(_fPixBig, buf));
        for (uint8_t r = 0; r < rows; r++) {
            int16_t top = FIELD_Y + (FIELD_H * (2 * r + 1)) / (2 * rows) - heartBig.h / 2;
            top = (top + 1) & ~1;
            bool use[6] = {true, true, true, true, true, true};
            for (uint8_t d = cols - cnt[r]; d > 0; d--) { // quitar huecos del centro hacia fuera
                int best = -1, bestd = 99;
                for (uint8_t i = 0; i < cols; i++) {
                    int dist = abs(2 * i - (cols - 1));
                    if (use[i] && dist < bestd) {
                        bestd = dist;
                        best = i;
                    }
                }
                use[best] = false;
            }
            for (uint8_t i = 0; i < cols; i++) {
                if (!use[i])
                    continue;
                int16_t left = FIELD_X + (FIELD_W * (2 * i + 1)) / (2 * cols) - heartBig.w / 2;
                left = (left + 1) & ~1;
                sprite(left, top, heartBig, VOID);
                avoid[placed * 4 + 0] = left - 2;
                avoid[placed * 4 + 1] = top - 2;
                avoid[placed * 4 + 2] = heartBig.w + 4;
                avoid[placed * 4 + 3] = heartBig.h + 4;
                placed++;
            }
        }
        sparkles(VOID, avoid, placed);
    }

    if (total && rank) { // progreso discreto bajo el índice de la esquina
        snprintf(buf, sizeof(buf), "de %u", total);
        text(_fPix, 11, 54, std::string(buf), VOID);
    }
    if (mv && mv < BATTERY_LOW_MV) // arriba a la derecha: abajo se pisaría con el índice de esquina
        batteryIcon(SCREEN_W - 30, 10, Power::batteryPct(mv), VOID);
}

// ---------------------------------------------------------------- interfaz

void Display::showWifi(const char *ssid, const char *pass, const char *url)
{
    _canvas.fillScreen(INK);
    frame(VOID);
    // El QR va en negro sobre un panel blanco: invertido muchas cámaras no lo leen
    _canvas.fillRect(8, 13, 95, 96, VOID);
    char wifiQr[80];
    snprintf(wifiQr, sizeof(wifiQr), "WIFI:T:WPA;S:%s;P:%s;;", ssid, pass);
    if (!drawQr(12, 17, 3, 3, wifiQr))
        drawQr(12, 17, 3, 4, wifiQr);

    std::string red = TextLayout::utf8ToCp1252(std::string("Red: ") + ssid);
    std::string clave = TextLayout::utf8ToCp1252(std::string("Clave: ") + pass);
    panel("WiFi", red.c_str(), clave.c_str(), url, COL_TEXT);
    text(_fPix, COL_TEXT, 107, std::string("Bot\xf3n corto: salir"), VOID);
}

void Display::showWifiConnected(const std::string &nameUtf8, size_t cards)
{
    _canvas.fillScreen(INK);
    frame(VOID);
    sprite(34, 45, check, VOID);
    sprite(24, 26, spark, VOID);
    sprite(88, 84, spark, VOID);
    char cartas[32];
    snprintf(cartas, sizeof(cartas), "%u cartas", (unsigned)cards);
    std::string name = TextLayout::utf8ToCp1252(nameUtf8);
    panel("Conectado", name.c_str(), cartas, nullptr, COL_TEXT);
    text(_fPix, COL_TEXT, 100, std::string("Bot\xf3n corto: salir"), VOID);
}

void Display::showCharging(const std::string &nameUtf8, uint16_t mv, bool usb)
{
    _canvas.fillScreen(INK);
    frame(VOID);
    sprite(18, 43, battery, VOID);
    sprite(42, 47, bolt, VOID);
    sprite(24, 22, spark, VOID);
    sprite(88, 92, spark, VOID);
    bool full = mv >= BATTERY_CHARGING_MV - 30;
    char volt[16];
    snprintf(volt, sizeof(volt), "%u.%02u V", mv / 1000, (mv % 1000) / 10);
    std::string name = TextLayout::utf8ToCp1252(nameUtf8);
    panel(usb && !full ? "Cargando" : "Completa", volt, name.c_str(), nullptr, COL_TEXT);
    text(_fPix, COL_TEXT, 100, std::string("Bot\xf3n 1: carta"), VOID);
}

void Display::showNotice(const char *titleUtf8, const char *l1, const char *l2, const char *l3)
{
    _canvas.fillScreen(INK);
    frame(VOID);
    sprite(24, 31, alert, VOID);
    sprite(24, 18, spark, VOID);
    sprite(88, 92, spark, VOID);
    std::string title = TextLayout::utf8ToCp1252(titleUtf8);
    std::string a = TextLayout::utf8ToCp1252(l1 ? l1 : "");
    std::string b = TextLayout::utf8ToCp1252(l2 ? l2 : "");
    std::string c = TextLayout::utf8ToCp1252(l3 ? l3 : "");
    panel(title.c_str(), a.c_str(), b.c_str(), c.c_str(), 96);
}
