// Charm · poemas al azar en un e-ink de bolsillo.
// GPIO0 largo = revelar carta, corto = página siguiente (tras la última se bloquea).
// GPIO21 corto = cerrar carta, largo = modo WiFi para actualizar el contenido.
#include "Buttons.h"
#include "Deck.h"
#include "Display.h"
#include "Power.h"
#include "Settings.h"
#include "TextLayout.h"
#include "WifiPortal.h"
#include "config.h"
#include <Arduino.h>
#include <esp_system.h>

enum class State : uint8_t { Locked, Revealed, Wifi };

struct RtcState {
    uint32_t magic;
    State state;
    uint16_t cardId;
    uint8_t page;
    uint8_t fastCount;
    uint8_t chargeBucket; // 0 = reverso normal; n = pantalla de carga con nivel (pct/5 + 1)
};
static const uint32_t RTC_MAGIC = 0xC4A2A001;
RTC_DATA_ATTR static RtcState rtc;

static Buttons buttons;
static Deck deck;
static Display display;
static WifiPortal portal(deck, display);

static State state = State::Locked;
static CardRef card;
static uint8_t page = 0;
static TextLayout::Layout layout;
static uint8_t imageBuf[BITMAP_BYTES];
static uint8_t backBuf[BITMAP_BYTES];
static bool hasCustomBack = false;
static uint32_t lastActivity = 0;
static uint32_t wifiStarted = 0;
static bool wifiConnectedShown = false;
static uint8_t chargeBucket = 0;
static uint32_t lastChargeCheck = 0;

// Pantalla de reposo: el reverso de la carta o, si está enchufado, el estado de carga
static void showBack()
{
    uint16_t mv = Power::batteryMv();
    if (Power::isCharging(mv)) {
        uint8_t bucket = mv >= BATTERY_CHARGING_MV - 30 ? 2 : 1; // 1 = cargando, 2 = completa
        display.showCharging(Settings::name(), mv, Power::usbConnected());
        display.flush(chargeBucket ? Display::Refresh::Fast : Display::Refresh::Full);
        chargeBucket = bucket;
    } else {
        display.showBack(hasCustomBack ? backBuf : nullptr, Settings::name(), deck.rank(), deck.count(), mv);
        display.flush(Display::Refresh::Full);
        chargeBucket = 0;
    }
    state = State::Locked;
    lastChargeCheck = millis();
}

// En reposo: redibujar solo si cambió el estado de carga o el nivel (evita refrescos inútiles)
static void maybeRefreshIdle()
{
    uint16_t mv = Power::batteryMv();
    uint8_t bucket = Power::isCharging(mv) ? (mv >= BATTERY_CHARGING_MV - 30 ? 2 : 1) : 0;
    lastChargeCheck = millis();
    if (bucket != chargeBucket)
        showBack();
}

// Carga la carta actual (texto o imagen) y dibuja la página pedida. false si la carta ya no existe.
static bool showCard(uint8_t p, Display::Refresh r)
{
    if (card.image) {
        if (!deck.loadImage(card.id, imageBuf))
            return false;
        display.showImage(imageBuf);
        page = 0;
    } else {
        std::string text;
        if (!deck.loadText(card.id, text))
            return false;
        display.layoutCard(text, layout);
        if (p >= layout.pageCount())
            p = layout.pageCount() - 1;
        page = p;
        display.showPage(layout, page);
    }
    display.flush(r);
    state = State::Revealed;
    return true;
}

static void reveal()
{
    if (!deck.draw(card)) {
        display.showNotice("Sin cartas", "Mantén el botón 2", "para abrir el WiFi", "y subir poemas.");
        display.flush(Display::Refresh::Full);
        state = State::Locked;
        return;
    }
    log_i("carta %u (%s)", card.id, card.image ? "imagen" : "texto");
    if (!showCard(0, Display::Refresh::Full))
        showBack();
}

static void nextPage()
{
    uint8_t total = card.image ? 1 : layout.pageCount();
    if (page + 1 >= total) {
        showBack();
        return;
    }
    page++;
    display.showPage(layout, page);
    display.flush(Display::Refresh::Fast);
}

static void enterWifi()
{
    uint16_t mv = Power::batteryMv();
    if (mv && mv < BATTERY_WIFI_MIN_MV) {
        char volt[32];
        snprintf(volt, sizeof(volt), "Medida: %u.%02u V.", mv / 1000, (mv % 1000) / 10);
        display.showNotice("Poca carga", volt, "Carga el charm antes", "de usar el WiFi.");
        display.flush(Display::Refresh::Full);
        state = State::Locked;
        return;
    }
    // Primero la pantalla (el refresco bloquea 2-3 s) y luego el AP: así el portal responde desde el primer segundo
    display.showWifi(WifiPortal::makeSsid().c_str(), AP_PASSWORD, portal.url());
    display.flush(Display::Refresh::Full);
    if (!portal.begin(display.panelName())) {
        showBack();
        return;
    }
    state = State::Wifi;
    wifiStarted = millis();
    wifiConnectedShown = false;
}

static void leaveWifi()
{
    portal.stop();
    deck.rescan();
    showBack();
}

static void handle(BtnEvent ev)
{
    if (ev == BtnEvent::None)
        return;
    log_i("boton evento %d en estado %d", (int)ev, (int)state);
    lastActivity = millis();
    switch (state) {
    case State::Locked:
        if (ev == BtnEvent::NavLong)
            reveal();
        else if (ev == BtnEvent::AuxLong)
            enterWifi();
        break;
    case State::Revealed:
        if (ev == BtnEvent::NavShort)
            nextPage();
        else if (ev == BtnEvent::NavLong)
            reveal();
        else if (ev == BtnEvent::AuxShort)
            showBack();
        else if (ev == BtnEvent::AuxLong)
            enterWifi();
        break;
    case State::Wifi:
        if (ev != BtnEvent::NavLong)
            leaveWifi();
        break;
    }
}

static void saveRtc()
{
    rtc.magic = RTC_MAGIC;
    rtc.state = state;
    rtc.cardId = card.id;
    rtc.page = page;
    rtc.fastCount = display.fastCount;
    rtc.chargeBucket = chargeBucket;
}

static void goToSleep()
{
    saveRtc();
#ifdef CHARM_NO_SLEEP
    lastActivity = millis(); // en desarrollo no se duerme; solo se reinicia el temporizador
#else
    Power::deepSleep(chargeBucket ? CHARGE_REFRESH_S : 0); // cargando: despertar por tiempo para actualizar el nivel
#endif
}

void setup()
{
    Power::earlyInit();
    Serial.begin(115200);
    Serial.setTxTimeoutMs(0);
    buttons.begin();

    int wp = Power::wakePin();
    BtnEvent wakeEv = wp >= 0 ? buttons.classifyWakePress(wp) : BtnEvent::None;

    Power::radioSleep();
    Settings::begin();
    deck.begin();
    hasCustomBack = deck.loadBack(backBuf);

    Power::waitWarmup();
    display.begin();
    esp_reset_reason_t rr = esp_reset_reason();
    log_i("charm %s, reset %d, wake pin %d, evento %d", CHARM_FW_VERSION, (int)rr, wp, (int)wakeEv);

    // Lo que hay en la tinta corresponde al estado guardado en RTC RAM solo si venimos de deep sleep
    bool inkValid = Power::wokeFromSleep() && rtc.magic == RTC_MAGIC;
    bool restored = false;
    if (inkValid && rtc.state == State::Revealed && deck.find(rtc.cardId, card)) {
        std::string text;
        if (card.image) {
            restored = true;
        } else if (deck.loadText(card.id, text)) {
            display.layoutCard(text, layout);
            restored = rtc.page < layout.pageCount();
        }
    }
    display.firstAfterSleep = true;
    if (restored) {
        page = rtc.page;
        state = State::Revealed;
        display.fastCount = rtc.fastCount;
    } else if (inkValid && rtc.state == State::Locked) {
        state = State::Locked; // lo que hay en la tinta sigue siendo válido
        chargeBucket = rtc.chargeBucket;
        maybeRefreshIdle(); // salvo que haya cambiado el estado de carga (o despertó el temporizador para actualizarlo)
    } else {
        showBack();
    }
#ifdef CHARM_NO_SLEEP
    // Diagnóstico en desarrollo: si el chip se reinició por un fallo, decirlo en pantalla
    const char *why = nullptr;
    if (rr == ESP_RST_BROWNOUT)
        why = "La alimentación cayó";
    else if (rr == ESP_RST_PANIC)
        why = "El firmware se cayó";
    else if (rr == ESP_RST_TASK_WDT || rr == ESP_RST_INT_WDT || rr == ESP_RST_WDT)
        why = "Se quedó colgado";
    if (why) {
        display.showNotice("Se reinició", why, "Mira el monitor serie.", nullptr);
        display.flush(Display::Refresh::Full);
        state = State::Locked;
    }
#endif
    handle(wakeEv);
    lastActivity = millis();
}

void loop()
{
    handle(buttons.poll());
    uint32_t now = millis();

    // El USB nativo tarda en reconectarse tras un reset y el log de setup() se pierde: repetirlo pasados 3 s
    static bool bootInfoRepeated = false;
    if (!bootInfoRepeated && now > 3000) {
        bootInfoRepeated = true;
        log_i("charm %s, reset %d, estado %d, cartas %u, bateria %u mV", CHARM_FW_VERSION, (int)esp_reset_reason(), (int)state,
              (unsigned)deck.count(), Power::batteryMv());
    }

    if (state == State::Wifi) {
        portal.loop();
        // "Conectado" solo cuando el teléfono ya cargó la página: el refresco bloquea el servidor un par de segundos
        if (!wifiConnectedShown && portal.pageServed()) {
            wifiConnectedShown = true;
            display.showWifiConnected(Settings::name(), deck.count());
            display.flush(Display::Refresh::Fast);
        } else if (wifiConnectedShown && portal.clients() == 0) {
            wifiConnectedShown = false; // el teléfono se fue: volver a mostrar los datos de la red
            portal.clearPageServed();
            display.showWifi(portal.ssid(), AP_PASSWORD, portal.url());
            display.flush(Display::Refresh::Fast);
        }
        // la petición atendida puede haber actualizado lastRequestMs después de tomar `now`: comparar con signo
        now = millis();
        int32_t idle = (int32_t)(now - portal.lastRequestMs());
        int32_t age = (int32_t)(now - wifiStarted);
        if (portal.exitRequested() || idle > (int32_t)WIFI_IDLE_MS || age > (int32_t)WIFI_MAX_MS) {
            log_i("salir wifi: exit=%d idle=%ld s", (int)portal.exitRequested(), (long)(idle / 1000));
            leaveWifi();
            lastActivity = millis();
        }
        return;
    }

    if (state == State::Locked && now - lastChargeCheck > CHARGE_REFRESH_S * 1000UL)
        maybeRefreshIdle(); // despierto (modo desarrollo o USB): seguir el nivel de carga

    uint32_t limit = state == State::Revealed ? SLEEP_REVEALED_MS : SLEEP_LOCKED_MS;
    if (!buttons.anyPressed() && now - lastActivity > limit)
        goToSleep();
    delay(5);
}
