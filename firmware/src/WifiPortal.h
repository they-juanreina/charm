#pragma once
// Punto de acceso + portal cautivo + API REST para editar las cartas desde el teléfono o la Mac.
#include "Deck.h"
#include <DNSServer.h>
#include <FS.h>
#include <WebServer.h>

class Display;

class WifiPortal
{
  public:
    WifiPortal(Deck &deck, Display &display) : _deck(deck), _display(display) {}

    static String makeSsid(); // charm-XXXX a partir de la MAC, sin arrancar el WiFi
    bool begin(const char *panelName);
    void loop();
    void stop();

    const char *ssid() const { return _ssid.c_str(); }
    const char *url() const { return "http://192.168.4.1"; }
    bool exitRequested() const { return _exit; }
    bool pageServed() const { return _pageServed; } // el teléfono ya cargó la página
    uint8_t clients() const;
    void clearPageServed() { _pageServed = false; }
    uint32_t lastRequestMs() const { return _lastReq; }

  private:
    Deck &_deck;
    Display &_display;
    WebServer _server{80};
    DNSServer _dns;
    String _ssid;
    const char *_panel = "";
    bool _exit = false;
    bool _pageServed = false;
    bool _routed = false;
    uint32_t _lastReq = 0;

    File _tmp;
    size_t _tmpTotal = 0;

    void routes();
    void json(int code, const String &body);
    void ok(uint16_t id);
    void error(int code, const char *msg);
    static String escape(const std::string &s);
    int idArg(bool fromPath); // id de la ruta ({}) o del parámetro ?id=; -1 si no hay o no es válido
    void handleIndex();
    void handleStatus();
    void handleScreen();
    void handleList();
    void handleGet();
    void handlePut(bool create);
    void handleDelete();
    void handleImageBody();
    void handleImageDone();
    void handleNotFound();
};
