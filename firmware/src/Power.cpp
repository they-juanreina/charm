#include "Power.h"
#include "config.h"
#include <Arduino.h>
#include <SPI.h>
#include <WiFi.h>
#include <driver/gpio.h>
#include <driver/rtc_io.h>
#include <esp_sleep.h>
#if ARDUINO_USB_MODE
#include <HWCDC.h>
#endif

static uint32_t vextOnAt = 0;

static const gpio_num_t heldPins[] = {(gpio_num_t)PIN_LORA_CS, (gpio_num_t)PIN_LORA_RST, (gpio_num_t)PIN_VEXT};

void Power::earlyInit()
{
    gpio_deep_sleep_hold_dis();
    for (gpio_num_t p : heldPins)
        gpio_hold_dis(p);
    pinMode(PIN_VEXT, OUTPUT);
    digitalWrite(PIN_VEXT, VEXT_ON);
    vextOnAt = millis();
}

void Power::waitWarmup()
{
    while (millis() - vextOnAt < VEXT_WARMUP_MS)
        delay(5);
}

void Power::radioSleep()
{
    SPIClass spi(FSPI);
    spi.begin(PIN_LORA_SCK, PIN_LORA_MISO, PIN_LORA_MOSI, PIN_LORA_CS);
    pinMode(PIN_LORA_CS, OUTPUT);
    digitalWrite(PIN_LORA_CS, HIGH);
    pinMode(PIN_LORA_BUSY, INPUT);
    pinMode(PIN_LORA_RST, OUTPUT);
    digitalWrite(PIN_LORA_RST, LOW);
    delay(2);
    digitalWrite(PIN_LORA_RST, HIGH);
    uint32_t t = millis();
    while (digitalRead(PIN_LORA_BUSY) == HIGH && millis() - t < 100)
        delay(1);
    spi.beginTransaction(SPISettings(2000000, MSBFIRST, SPI_MODE0));
    digitalWrite(PIN_LORA_CS, LOW);
    spi.transfer(0x84); // SetSleep
    spi.transfer(0x00); // arranque en frío, sin RTC
    digitalWrite(PIN_LORA_CS, HIGH);
    spi.endTransaction();
    spi.end();
    // Mantener CS y RST altos también en deep sleep para que la radio no se despierte ni se resetee
    pinMode(PIN_LORA_CS, OUTPUT);
    digitalWrite(PIN_LORA_CS, HIGH);
    pinMode(PIN_LORA_RST, OUTPUT);
    digitalWrite(PIN_LORA_RST, HIGH);
    gpio_hold_en((gpio_num_t)PIN_LORA_CS);
    gpio_hold_en((gpio_num_t)PIN_LORA_RST);
}

uint16_t Power::batteryMv()
{
    // La atenuación global debe fijarse antes de la primera lectura: el HAL crea la calibración con ella
    static bool adcReady = false;
    if (!adcReady) {
        analogSetAttenuation(ADC_2_5db); // 4.2 V / 4.9 = 0.86 V en el pin, dentro del rango de 2.5 dB
        adcReady = true;
    }
    pinMode(PIN_ADC_CTRL, OUTPUT);
    digitalWrite(PIN_ADC_CTRL, HIGH);
    delay(60); // el nodo del divisor tiene un condensador: con 10 ms la lectura salía baja
    analogReadMilliVolts(PIN_BATTERY); // primera lectura: inicializa el canal
    uint32_t v[9];
    for (int i = 0; i < 9; i++) {
        v[i] = analogReadMilliVolts(PIN_BATTERY);
        delay(2);
    }
    digitalWrite(PIN_ADC_CTRL, LOW);
    for (int i = 1; i < 9; i++) // mediana: ignora picos de consumo
        for (int j = i; j > 0 && v[j - 1] > v[j]; j--) {
            uint32_t t = v[j];
            v[j] = v[j - 1];
            v[j - 1] = t;
        }
    uint32_t mv = (uint32_t)(v[4] * BATTERY_MULT);
    log_i("bateria: %lu mV en el pin -> %lu mV", (unsigned long)v[4], (unsigned long)mv);
    if (mv < 2500 || mv > 4600)
        return 0; // sin batería o lectura inválida: se trata como desconocido
    return (uint16_t)mv;
}


bool Power::usbConnected()
{
#if ARDUINO_USB_MODE
    return HWCDC::isPlugged();
#else
    return false;
#endif
}

bool Power::isCharging(uint16_t mv)
{
    return usbConnected() || (mv >= BATTERY_CHARGING_MV);
}

bool Power::wokeByTimer()
{
    return esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_TIMER;
}

bool Power::wokeFromSleep()
{
    return esp_sleep_get_wakeup_cause() != ESP_SLEEP_WAKEUP_UNDEFINED;
}

int Power::wakePin()
{
    if (esp_sleep_get_wakeup_cause() != ESP_SLEEP_WAKEUP_EXT1)
        return -1;
    uint64_t mask = esp_sleep_get_ext1_wakeup_status();
    if (mask & (1ULL << PIN_BTN_AUX))
        return PIN_BTN_AUX;
    if (mask & (1ULL << PIN_BTN_NAV))
        return PIN_BTN_NAV;
    return -1;
}

void Power::deepSleep(uint32_t timerSeconds)
{
    log_i("deep sleep (timer %lu s)", (unsigned long)timerSeconds);
    if (timerSeconds)
        esp_sleep_enable_timer_wakeup((uint64_t)timerSeconds * 1000000ULL);
    WiFi.mode(WIFI_OFF);
    Serial.flush();

    // Panel sin alimentación y sin retroalimentarlo por las líneas de datos
    digitalWrite(PIN_VEXT, !VEXT_ON);
    const uint8_t einkPins[] = {PIN_EINK_CS, PIN_EINK_BUSY, PIN_EINK_DC, PIN_EINK_RES, PIN_EINK_SCLK, PIN_EINK_MOSI};
    for (uint8_t p : einkPins)
        pinMode(p, INPUT);
    gpio_hold_en((gpio_num_t)PIN_VEXT);

    // Botones: pull-up RTC y despertar con cualquiera en LOW
    for (gpio_num_t p : {(gpio_num_t)PIN_BTN_NAV, (gpio_num_t)PIN_BTN_AUX}) {
        rtc_gpio_init(p);
        rtc_gpio_set_direction(p, RTC_GPIO_MODE_INPUT_ONLY);
        rtc_gpio_pulldown_dis(p);
        rtc_gpio_pullup_en(p);
    }
    esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_ON); // mantiene los pull-up durante el sueño
    esp_sleep_enable_ext1_wakeup((1ULL << PIN_BTN_NAV) | (1ULL << PIN_BTN_AUX), ESP_EXT1_WAKEUP_ANY_LOW);

    gpio_deep_sleep_hold_en();
    esp_deep_sleep_start();
}
