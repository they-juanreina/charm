#pragma once
// Almacén de cartas en LittleFS (/cards/NNN.txt | NNN.bmp1) y baraja sin repetición persistida en NVS.
#include "config.h"
#include <Arduino.h>
#include <Preferences.h>
#include <string>
#include <vector>

struct CardRef {
    uint16_t id;
    bool image;
};

class Deck
{
  public:
    bool begin();   // monta LittleFS, carga el estado de la baraja y escanea /cards
    void rescan();  // reconstruye el índice; si cambió el contenido, rebaraja

    size_t count() const { return _cards.size(); }
    uint16_t position() const { return _pos; }
    // Número de la carta que sale a continuación, el que dibuja el reverso. 0 = as: la dedicatoria si
    // la hay (primera de cada baraja); sin dedicatoria, el as es la baraja recién empezada.
    uint16_t rank() const;
    bool hasDedication() const { return _hasDed; }
    bool saveDedication(const uint8_t *data, size_t n); // la vuelve a mostrar en el próximo as
    bool removeDedication();
    const std::vector<CardRef> &cards() const { return _cards; }
    bool find(uint16_t id, CardRef &out) const;

    bool draw(CardRef &out); // la dedicatoria si toca, si no la siguiente de la permutación; persiste
    void reshuffle();        // semilla nueva, posición 0

    bool loadText(uint16_t id, std::string &out) const;
    bool loadImage(uint16_t id, uint8_t *buf) const; // BITMAP_BYTES
    bool loadBack(uint8_t *buf) const;               // /back.bmp1 opcional
    std::string readTitle(uint16_t id) const;        // UTF-8, vacío si no hay '#'
    std::string firstLine(uint16_t id) const;        // primer verso no vacío (sin el título), UTF-8
    size_t fileSize(uint16_t id, bool image) const;

    bool saveText(uint16_t id, const uint8_t *data, size_t n);
    bool saveImage(uint16_t id, const char *tmpPath); // renombra un .tmp ya escrito
    bool remove(uint16_t id);
    int freeId() const; // menor id libre 1..MAX_CARD_ID, -1 si no hay

    static String pathFor(uint16_t id, bool image);

  private:
    std::vector<CardRef> _cards;
    std::vector<uint16_t> _perm;
    Preferences _prefs;
    uint32_t _seed = 0, _sig = 0;
    uint16_t _pos = 0, _last = 0;
    bool _hasDed = false, _dedShown = false;
    bool dedicationDue() const { return _hasDed && (!_dedShown || _pos >= _perm.size()); }

    void scan();
    uint32_t signature() const;
    void buildPerm();
    void saveState();
};
