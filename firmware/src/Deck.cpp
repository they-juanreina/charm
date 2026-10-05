#include "Deck.h"
#include <LittleFS.h>
#include <algorithm>
#include <esp_random.h>

String Deck::pathFor(uint16_t id, bool image)
{
    if (id == DEDICATION_ID && !image)
        return DEDICATION_PATH;
    char buf[24];
    snprintf(buf, sizeof(buf), CARDS_DIR "/%03u.%s", id, image ? "bmp1" : "txt");
    return String(buf);
}

bool Deck::begin()
{
    if (!LittleFS.begin(true)) {
        log_e("LittleFS no monta");
        return false;
    }
    if (!LittleFS.exists(CARDS_DIR))
        LittleFS.mkdir(CARDS_DIR);
    _prefs.begin("charm", false);
    _seed = _prefs.getUInt("seed", 0);
    _pos = _prefs.getUShort("pos", 0);
    _sig = _prefs.getUInt("sig", 0);
    _last = _prefs.getUShort("last", 0);
    _dedShown = _prefs.getBool("dedShown", false);
    rescan();
    return true;
}

void Deck::scan()
{
    _hasDed = LittleFS.exists(DEDICATION_PATH);
    _cards.clear();
    File dir = LittleFS.open(CARDS_DIR);
    if (!dir)
        return;
    File f;
    while ((f = dir.openNextFile())) {
        String name = f.name(); // "001.txt"
        f.close();
        int slash = name.lastIndexOf('/');
        if (slash >= 0)
            name = name.substring(slash + 1);
        int dot = name.indexOf('.');
        if (dot < 1)
            continue;
        String ext = name.substring(dot + 1);
        bool image;
        if (ext == "txt")
            image = false;
        else if (ext == "bmp1")
            image = true;
        else
            continue;
        int id = name.substring(0, dot).toInt();
        if (id < 1 || id > MAX_CARD_ID)
            continue;
        _cards.push_back({(uint16_t)id, image});
    }
    std::sort(_cards.begin(), _cards.end(), [](const CardRef &a, const CardRef &b) { return a.id < b.id; });
}

uint32_t Deck::signature() const
{
    uint32_t h = 2166136261u;
    for (auto &c : _cards) {
        h = (h ^ c.id) * 16777619u;
        h = (h ^ (c.image ? 1 : 0)) * 16777619u;
    }
    return h ? h : 1;
}

static uint32_t xorshift32(uint32_t &s)
{
    s ^= s << 13;
    s ^= s >> 17;
    s ^= s << 5;
    return s;
}

void Deck::buildPerm()
{
    _perm.resize(_cards.size());
    for (size_t i = 0; i < _cards.size(); i++)
        _perm[i] = _cards[i].id;
    uint32_t s = _seed ? _seed : 1;
    for (size_t i = _perm.size(); i > 1; i--) {
        size_t j = xorshift32(s) % i;
        std::swap(_perm[i - 1], _perm[j]);
    }
}

void Deck::rescan()
{
    scan();
    uint32_t sig = signature();
    if (sig != _sig || _seed == 0) {
        _sig = sig;
        reshuffle();
    } else {
        buildPerm();
    }
    log_i("deck: %u cartas, pos %u", (unsigned)_cards.size(), _pos);
}

void Deck::reshuffle()
{
    _seed = esp_random() | 1;
    _pos = 0;
    _dedShown = false; // cada baraja nueva empieza por la dedicatoria
    buildPerm();
    // evitar que la nueva baraja empiece por la última carta mostrada
    if (_perm.size() > 1 && _perm[0] == _last)
        std::swap(_perm[0], _perm[_perm.size() - 1]);
    saveState();
}

void Deck::saveState()
{
    _prefs.putUInt("seed", _seed);
    _prefs.putUShort("pos", _pos);
    _prefs.putUInt("sig", _sig);
    _prefs.putUShort("last", _last);
    _prefs.putBool("dedShown", _dedShown);
}

uint16_t Deck::rank() const
{
    if (!_hasDed)
        return _pos;
    return dedicationDue() || _cards.empty() ? 0 : _pos + 1;
}

bool Deck::draw(CardRef &out)
{
    if (_hasDed && (_cards.empty() || dedicationDue())) {
        if (_pos >= _perm.size() && !_cards.empty())
            reshuffle();
        _dedShown = true;
        saveState();
        out = {DEDICATION_ID, false};
        return true;
    }
    if (_cards.empty())
        return false;
    if (_pos >= _perm.size())
        reshuffle();
    uint16_t id = _perm[_pos++];
    _last = id;
    saveState();
    return find(id, out);
}

bool Deck::find(uint16_t id, CardRef &out) const
{
    if (id == DEDICATION_ID) {
        out = {DEDICATION_ID, false};
        return _hasDed;
    }
    for (auto &c : _cards)
        if (c.id == id) {
            out = c;
            return true;
        }
    return false;
}

bool Deck::loadText(uint16_t id, std::string &out) const
{
    File f = LittleFS.open(pathFor(id, false), "r");
    if (!f)
        return false;
    size_t n = std::min((size_t)f.size(), (size_t)CARD_MAX_BYTES);
    out.resize(n);
    size_t got = f.read((uint8_t *)&out[0], n);
    f.close();
    out.resize(got);
    return true;
}

bool Deck::loadImage(uint16_t id, uint8_t *buf) const
{
    File f = LittleFS.open(pathFor(id, true), "r");
    if (!f)
        return false;
    bool ok = f.size() == BITMAP_BYTES && f.read(buf, BITMAP_BYTES) == BITMAP_BYTES;
    f.close();
    return ok;
}

bool Deck::loadBack(uint8_t *buf) const
{
    File f = LittleFS.open(BACK_PATH, "r");
    if (!f)
        return false;
    bool ok = f.size() == BITMAP_BYTES && f.read(buf, BITMAP_BYTES) == BITMAP_BYTES;
    f.close();
    return ok;
}

std::string Deck::readTitle(uint16_t id) const
{
    File f = LittleFS.open(pathFor(id, false), "r");
    if (!f)
        return "";
    String line = f.readStringUntil('\n');
    f.close();
    line.trim();
    if (!line.startsWith("#"))
        return "";
    line = line.substring(1);
    line.trim();
    return std::string(line.c_str());
}

std::string Deck::firstLine(uint16_t id) const
{
    File f = LittleFS.open(pathFor(id, false), "r");
    if (!f)
        return "";
    String line;
    for (int i = 0; i < 6 && f.available(); i++) {
        line = f.readStringUntil('\n');
        line.trim();
        if (!line.isEmpty() && !line.startsWith("#"))
            break;
        line = "";
    }
    f.close();
    return std::string(line.c_str());
}

size_t Deck::fileSize(uint16_t id, bool image) const
{
    File f = LittleFS.open(pathFor(id, image), "r");
    if (!f)
        return 0;
    size_t s = f.size();
    f.close();
    return s;
}

bool Deck::saveText(uint16_t id, const uint8_t *data, size_t n)
{
    if (id < 1 || id > MAX_CARD_ID || n == 0 || n > CARD_MAX_BYTES)
        return false;
    File f = LittleFS.open(pathFor(id, false), "w");
    if (!f)
        return false;
    bool ok = f.write(data, n) == n;
    f.close();
    if (ok)
        LittleFS.remove(pathFor(id, true));
    return ok;
}

bool Deck::saveImage(uint16_t id, const char *tmpPath)
{
    if (id < 1 || id > MAX_CARD_ID)
        return false;
    String dst = pathFor(id, true);
    LittleFS.remove(dst);
    if (!LittleFS.rename(tmpPath, dst))
        return false;
    LittleFS.remove(pathFor(id, false));
    return true;
}

bool Deck::remove(uint16_t id)
{
    bool a = LittleFS.remove(pathFor(id, false));
    bool b = LittleFS.remove(pathFor(id, true));
    return a || b;
}

int Deck::freeId() const
{
    uint16_t next = 1;
    for (auto &c : _cards) { // _cards está ordenado
        if (c.id > next)
            break;
        if (c.id == next)
            next++;
    }
    return next <= MAX_CARD_ID ? next : -1;
}

bool Deck::saveDedication(const uint8_t *data, size_t n)
{
    if (n == 0 || n > CARD_MAX_BYTES)
        return false;
    File f = LittleFS.open(DEDICATION_PATH, "w");
    if (!f)
        return false;
    bool ok = f.write(data, n) == n;
    f.close();
    _hasDed = ok;
    _dedShown = false; // que quien la escribió la vea en el próximo as
    saveState();
    return ok;
}

bool Deck::removeDedication()
{
    bool ok = LittleFS.remove(DEDICATION_PATH);
    _hasDed = false;
    return ok;
}
