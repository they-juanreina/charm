#include "WifiPortal.h"
#include "Power.h"
#include "Display.h"
#include "Settings.h"
#include "config.h"
#include "web_page.h"
#include <LittleFS.h>
#include <WiFi.h>
#include <uri/UriBraces.h>

static const char *TMP_PATH = CARDS_DIR "/upload.tmp";

String WifiPortal::makeSsid()
{
    return String(AP_NAME);
}

bool WifiPortal::begin(const char *panelName)
{
    _panel = panelName;
    _exit = false;
    _pageServed = false;
    _ssid = makeSsid();
    const char *ssid = _ssid.c_str();
    WiFi.persistent(false);
    static bool eventsHooked = false;
    if (!eventsHooked) {
        eventsHooked = true;
        WiFi.onEvent([](WiFiEvent_t e, WiFiEventInfo_t) { log_i("wifi evento %d", (int)e); });
    }
    WiFi.mode(WIFI_AP);
#ifdef CHARM_AP_OPEN
    bool up = WiFi.softAP(ssid);
#else
    bool up = WiFi.softAP(ssid, AP_PASSWORD);
#endif
    // la IP por defecto del AP es 192.168.4.1; no se llama a softAPConfig antes de softAP (deja el DHCP sin arrancar)
    if (!up) {
        log_e("softAP falló");
        return false;
    }
    WiFi.setTxPower(WIFI_POWER_11dBm); // el teléfono está al lado: menos pico de corriente, menos riesgo de brownout
    _dns.setTTL(30);
    _dns.start(53, "*", IPAddress(192, 168, 4, 1));
    if (!_routed) {
        routes();
        _routed = true;
    }
    _server.begin();
    _lastReq = millis();
    log_i("AP %s en %s", ssid, url());
    return true;
}

uint8_t WifiPortal::clients() const
{
    return WiFi.softAPgetStationNum();
}

void WifiPortal::loop()
{
    _dns.processNextRequest();
    _server.handleClient();
}

void WifiPortal::stop()
{
    _server.stop();
    _dns.stop();
    WiFi.softAPdisconnect(true); // apaga también el WiFi
    if (_tmp)
        _tmp.close();
    LittleFS.remove(TMP_PATH);
}

void WifiPortal::routes()
{
    _server.on("/", HTTP_GET, [this] { handleIndex(); });
    _server.on("/api/status", HTTP_GET, [this] { handleStatus(); });
    _server.on("/api/screen", HTTP_GET, [this] { handleScreen(); });
    _server.on("/api/cards", HTTP_GET, [this] { handleList(); });
    _server.on("/api/cards", HTTP_POST, [this] { handlePut(true); });
    _server.on(UriBraces("/api/cards/{}"), HTTP_GET, [this] { handleGet(); });
    _server.on(UriBraces("/api/cards/{}"), HTTP_PUT, [this] { handlePut(false); });
    _server.on(UriBraces("/api/cards/{}"), HTTP_DELETE, [this] { handleDelete(); });
    _server.on("/api/images", HTTP_POST, [this] { handleImageDone(); }, [this] { handleImageBody(); });
    _server.on("/api/name", HTTP_PUT, [this] {
        std::string body = _server.arg("plain").c_str();
        if (!Settings::setName(body))
            return error(400, "nombre vacío o demasiado largo (máx. 32 bytes)");
        json(200, String("{\"name\":\"") + escape(Settings::name()) + "\"}");
    });
    _server.on("/api/dedication", HTTP_GET, [this] {
        if (!_deck.hasDedication())
            return error(404, "sin dedicatoria");
        _lastReq = millis();
        File f = LittleFS.open(DEDICATION_PATH, "r");
        _server.sendHeader("Cache-Control", "no-store");
        _server.sendHeader("Connection", "close");
        _server.streamFile(f, "text/plain; charset=utf-8");
        f.close();
    });
    _server.on("/api/dedication", HTTP_PUT, [this] {
        String body = _server.arg("plain");
        if (body.isEmpty())
            return error(400, "cuerpo vacío");
        if (body.length() > CARD_MAX_BYTES)
            return error(413, "dedicatoria demasiado larga");
        if (!_deck.saveDedication((const uint8_t *)body.c_str(), body.length()))
            return error(500, "no se pudo guardar");
        json(200, "{\"ok\":true}");
    });
    _server.on("/api/dedication", HTTP_DELETE, [this] {
        _deck.removeDedication();
        json(200, "{\"ok\":true}");
    });
    _server.on("/api/reshuffle", HTTP_POST, [this] {
        _deck.reshuffle();
        json(200, "{\"ok\":true}");
    });
    _server.on("/api/exit", HTTP_POST, [this] {
        log_i("api/exit pedido por %s", _server.client().remoteIP().toString().c_str());
        json(200, "{\"ok\":true}");
        _exit = true;
    });
    _server.onNotFound([this] { handleNotFound(); });
}

void WifiPortal::json(int code, const String &body)
{
    _lastReq = millis();
    _server.sendHeader("Cache-Control", "no-store");
    _server.sendHeader("Connection", "close");
    _server.send(code, "application/json; charset=utf-8", body);
}

void WifiPortal::ok(uint16_t id)
{
    json(200, String("{\"id\":") + id + "}");
}

void WifiPortal::error(int code, const char *msg)
{
    json(code, String("{\"error\":\"") + msg + "\"}");
}

String WifiPortal::escape(const std::string &s)
{
    String out;
    out.reserve(s.size() + 8);
    for (unsigned char c : s) {
        if (c == '"' || c == '\\') {
            out += '\\';
            out += (char)c;
        } else if (c < 0x20) {
            char b[8];
            snprintf(b, sizeof(b), "\\u%04x", c);
            out += b;
        } else {
            out += (char)c;
        }
    }
    return out;
}

int WifiPortal::idArg(bool fromPath)
{
    // pathArg() aborta (assert) si la ruta no tiene {}: solo se consulta en rutas con parámetro
    String s = fromPath ? _server.pathArg(0) : String();
    if (s.isEmpty() && _server.hasArg("id"))
        s = _server.arg("id");
    if (s.isEmpty())
        return -1;
    int id = s.toInt();
    return (id >= 1 && id <= MAX_CARD_ID) ? id : -1;
}

void WifiPortal::handleIndex()
{
    _lastReq = millis();
    _pageServed = true;
    _server.sendHeader("Content-Encoding", "gzip");
    _server.sendHeader("Cache-Control", "no-store");
    _server.sendHeader("Connection", "close");
    _server.send_P(200, "text/html; charset=utf-8", (const char *)WEB_INDEX_GZ, WEB_INDEX_GZ_LEN);
}

// Los 3904 bytes del lienzo, el mismo formato que un .bmp1: el portal lo pinta tal cual
void WifiPortal::handleScreen()
{
    _lastReq = millis();
    _server.sendHeader("Cache-Control", "no-store");
    _server.sendHeader("Connection", "close");
    _server.setContentLength(BITMAP_BYTES);
    _server.send(200, "application/octet-stream", "");
    _server.sendContent((const char *)_display.buffer(), BITMAP_BYTES);
}

void WifiPortal::handleStatus()
{
    uint16_t mv = Power::batteryMv();
    String s = "{";
    s += "\"fw\":\"" CHARM_FW_VERSION "\",";
    s += String("\"name\":\"") + escape(Settings::name()) + "\",";
    s += String("\"panel\":\"") + _panel + "\",";
    s += String("\"cards\":") + _deck.count() + ",";
    s += String("\"deckPos\":") + _deck.rank() + ",";
    s += String("\"dedication\":") + (_deck.hasDedication() ? "true" : "false") + ",";
    s += String("\"battery_mv\":") + mv + ",";
    s += String("\"battery_pct\":") + Power::batteryPct(mv) + ",";
    s += String("\"usb\":") + (Power::usbConnected() ? "true" : "false") + ",";
    s += String("\"fs_used\":") + LittleFS.usedBytes() + ",";
    s += String("\"fs_total\":") + LittleFS.totalBytes() + ",";
    s += String("\"uptime_s\":") + (millis() / 1000);
    s += "}";
    json(200, s);
}

void WifiPortal::handleList()
{
    String s = "[";
    bool first = true;
    for (auto &c : _deck.cards()) {
        if (!first)
            s += ',';
        first = false;
        s += String("{\"id\":") + c.id + ",\"type\":\"" + (c.image ? "image" : "text") + "\",\"size\":" +
             _deck.fileSize(c.id, c.image);
        if (!c.image) {
            std::string title = _deck.readTitle(c.id);
            s += String(",\"title\":\"") + escape(title) + "\"";
            if (title.empty())
                s += String(",\"first\":\"") + escape(_deck.firstLine(c.id)) + "\"";
        }
        s += "}";
    }
    s += "]";
    json(200, s);
}

void WifiPortal::handleGet()
{
    int id = idArg(true);
    CardRef c;
    if (id < 0 || !_deck.find((uint16_t)id, c))
        return error(404, "no existe");
    _lastReq = millis();
    File f = LittleFS.open(Deck::pathFor(c.id, c.image), "r");
    if (!f)
        return error(500, "no se pudo abrir");
    _server.sendHeader("Cache-Control", "no-store");
    _server.sendHeader("Connection", "close");
    _server.streamFile(f, c.image ? "application/octet-stream" : "text/plain; charset=utf-8");
    f.close();
}

void WifiPortal::handlePut(bool create)
{
    int id = create ? _deck.freeId() : idArg(true);
    if (id < 0)
        return error(create ? 507 : 400, create ? "sin ids libres" : "id inválido");
    String body = _server.arg("plain");
    if (body.isEmpty())
        return error(400, "cuerpo vacío");
    if (body.length() > CARD_MAX_BYTES)
        return error(413, "poema demasiado largo");
    if (!_deck.saveText((uint16_t)id, (const uint8_t *)body.c_str(), body.length()))
        return error(500, "no se pudo guardar");
    _deck.rescan();
    _lastReq = millis();
    _server.sendHeader("Cache-Control", "no-store");
    _server.sendHeader("Connection", "close");
    _server.send(create ? 201 : 200, "application/json; charset=utf-8", String("{\"id\":") + id + "}");
}

void WifiPortal::handleDelete()
{
    int id = idArg(true);
    if (id < 0 || !_deck.remove((uint16_t)id))
        return error(404, "no existe");
    _deck.rescan();
    json(200, "{\"ok\":true}");
}

// Cuerpo binario en trozos (WebServer::raw). Se escribe a un .tmp y se valida al final.
void WifiPortal::handleImageBody()
{
    HTTPRaw &r = _server.raw();
    if (r.status == RAW_START) {
        _tmpTotal = 0;
        _tmp = LittleFS.open(TMP_PATH, "w");
    } else if (r.status == RAW_WRITE) {
        if (_tmp && _tmpTotal + r.currentSize <= BITMAP_BYTES)
            _tmp.write(r.buf, r.currentSize);
        _tmpTotal += r.currentSize;
    } else {
        if (_tmp)
            _tmp.close();
        if (r.status == RAW_ABORTED)
            LittleFS.remove(TMP_PATH);
    }
}

void WifiPortal::handleImageDone()
{
    if (_tmp)
        _tmp.close();
    if (_tmpTotal != BITMAP_BYTES) {
        LittleFS.remove(TMP_PATH);
        return error(400, "la imagen debe tener 3904 bytes (250x122, 1 bpp)");
    }
    int id = idArg(false); // /api/images?id=N
    if (id < 0)
        id = _deck.freeId();
    if (id < 0 || !_deck.saveImage((uint16_t)id, TMP_PATH)) {
        LittleFS.remove(TMP_PATH);
        return error(500, "no se pudo guardar");
    }
    _deck.rescan();
    ok((uint16_t)id);
}

void WifiPortal::handleNotFound()
{
    _lastReq = millis();
    String host = _server.hostHeader();
    if (host != "192.168.4.1") { // portal cautivo: cualquier otro host redirige a la página
        _server.sendHeader("Location", String(url()) + "/", true);
        _server.sendHeader("Connection", "close");
        _server.send(302, "text/plain", "");
        return;
    }
    _server.send(404, "text/plain", "404");
}
