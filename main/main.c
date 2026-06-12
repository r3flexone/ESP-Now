/*
 * ============================================================
 *  ESP-NOW  –  N GERÄTE  (gleicher Code für ALLE Geräte)
 *  ESP-IDF Projekt – VS Code + ESP-IDF Extension
 * ============================================================
 *
 *  Jedes Gerät hat einen Button UND eine LED.
 *  Button drücken → sendet an ALLE anderen Geräte → dort schaltet
 *  die LED um (an/aus). Funktioniert mit 2, 3 oder mehr Geräten -
 *  derselbe Code läuft auf jedem davon.
 *
 *  Verkabelung (alle Geräte identisch):
 *  --------------------------------------
 *   Button  →  GPIO 0   (BOOT-Button, LOW wenn gedrückt)
 *   LED     →  GPIO 48  (onboard NeoPixel/WS2812, ESP32-S3-DevKitC-1)
 *
 *  Setup-Reihenfolge:
 *  ------------------
 *   1. Jedes Gerät einmal flashen und im seriellen Monitor die Zeile
 *      "Meine MAC: AA:BB:CC:DD:EE:FF" ablesen.
 *   2. MAC-Adressen aller Geräte von Hand in main/mac_config.h
 *      eintragen (Vorlage: main/mac_config.h.example).
 *   3. Alle Geräte neu flashen. Fertig!
 *
 *  VS Code Befehle (ESP-IDF Extension):
 *  -------------------------------------
 *   Bauen:           Ctrl+E  B
 *   Flashen:         Ctrl+E  F
 *   Monitor öffnen:  Ctrl+E  M
 *   Alles auf einmal: idf.py flash monitor  (im Terminal)
 * ============================================================
 */

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "esp_wifi.h"
#include "esp_now.h"
#include "esp_log.h"
#include "driver/gpio.h"   // Komponente esp_driver_gpio (in IDF v6 umbenannt)
#include "led_strip.h"     // NeoPixel (WS2812) ueber RMT-Treiber
#include "mac_config.h"    // von Hand gepflegt (siehe mac_config.h.example)

#define BUTTON_PIN  GPIO_NUM_0
#define LED_PIN     GPIO_NUM_48   // onboard NeoPixel (WS2812)

// Tag für Lognachrichten – erscheint im Monitor als "[ESP-NOW] ..."
static const char *TAG = "ESP-NOW";

// Nachrichtenstruktur – muss auf allen Geräten identisch sein
typedef struct {
    bool led_an;
} nachricht_t;

// MAC-Adressen aller Geräte (aus mac_config.h) als Byte-Arrays
#define MAC_ARRAY_GROESSE (MAC_ANZAHL > 0 ? MAC_ANZAHL : 1)
static uint8_t mac_adressen[MAC_ARRAY_GROESSE][6];

// Index dieses Geräts in mac_adressen – -1 = noch nicht bekannt
static int eigener_index = -1;

// Aktueller LED-Zustand – wird bei jedem Empfang umgeschaltet
static bool led_status = false;

// Handle fuer den NeoPixel
static led_strip_handle_t led_strip;


// Schaltet den NeoPixel an (gedimmtes Weiss) oder aus
static void led_set(bool an)
{
    if (an) {
        led_strip_set_pixel(led_strip, 0, 16, 16, 16);
        led_strip_refresh(led_strip);
    } else {
        led_strip_clear(led_strip);
    }
}


// Wandelt "AA:BB:CC:DD:EE:FF" in ein 6-Byte-Array um
static void parse_mac(const char *str, uint8_t *out)
{
    unsigned int bytes[6];
    sscanf(str, "%x:%x:%x:%x:%x:%x",
           &bytes[0], &bytes[1], &bytes[2], &bytes[3], &bytes[4], &bytes[5]);
    for (int i = 0; i < 6; i++) {
        out[i] = (uint8_t)bytes[i];
    }
}


// ── Callback: aufgerufen NACHDEM wir gesendet haben ─────────
static void send_callback(const esp_now_send_info_t *tx_info, esp_now_send_status_t status)
{
    if (status == ESP_NOW_SEND_SUCCESS) {
        ESP_LOGI(TAG, "Nachricht angekommen");
    } else {
        ESP_LOGW(TAG, "Senden fehlgeschlagen – anderes Geraet erreichbar?");
    }
}


// ── Callback: aufgerufen WENN eine Nachricht ankommt ────────
static void recv_callback(const esp_now_recv_info_t *info,
                          const uint8_t *daten, int laenge)
{
    if (laenge != sizeof(nachricht_t)) {
        return;  // Fremde oder kaputte Nachricht – ignorieren
    }

    nachricht_t *msg = (nachricht_t *)daten;

    if (msg->led_an) {
        led_status = !led_status;
        led_set(led_status);
        if (led_status) {
            ESP_LOGI(TAG, "Nachricht empfangen -> LED an");
        } else {
            ESP_LOGI(TAG, "Nachricht empfangen -> LED aus");
        }
    }
}


// ── WiFi starten (nur als Funk-Layer, keine Verbindung) ──────
static void wifi_init(void)
{
    // NVS (Non-Volatile Storage) wird von WiFi intern benutzt.
    // Beim ersten Start ist der Speicher leer → Reset nötig.
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());
}


void app_main(void)
{
    wifi_init();

    // MAC-Adressen aus mac_config.h in Byte-Arrays umwandeln
    for (int i = 0; i < MAC_ANZAHL; i++) {
        parse_mac(mac_liste[i], mac_adressen[i]);
    }

    // Eigene MAC-Adresse lesen und anzeigen
    uint8_t eigene_mac[6];
    esp_wifi_get_mac(WIFI_IF_STA, eigene_mac);
    ESP_LOGI(TAG, "Meine MAC: %02X:%02X:%02X:%02X:%02X:%02X",
             eigene_mac[0], eigene_mac[1], eigene_mac[2],
             eigene_mac[3], eigene_mac[4], eigene_mac[5]);

    // Eigenen Platz in der Geräteliste finden
    for (int i = 0; i < MAC_ANZAHL; i++) {
        if (memcmp(eigene_mac, mac_adressen[i], 6) == 0) {
            eigener_index = i;
            break;
        }
    }

    if (eigener_index < 0) {
        ESP_LOGE(TAG, "MAC stimmt mit keinem Eintrag in mac_config.h ueberein!");
        ESP_LOGE(TAG, "Bitte die eigene MAC-Adresse (siehe oben) in main/mac_config.h eintragen.");
        return;
    }
    ESP_LOGI(TAG, "Ich bin Geraet %d von %d", eigener_index + 1, MAC_ANZAHL);

    // ESP-NOW starten
    ESP_ERROR_CHECK(esp_now_init());
    esp_now_register_send_cb(send_callback);
    esp_now_register_recv_cb(recv_callback);

    // Alle anderen Geräte als Kommunikationspartner registrieren
    for (int i = 0; i < MAC_ANZAHL; i++) {
        if (i == eigener_index) {
            continue;
        }
        esp_now_peer_info_t peer = {};
        memcpy(peer.peer_addr, mac_adressen[i], 6);
        peer.channel = 0;
        peer.ifidx   = WIFI_IF_STA;
        peer.encrypt = false;
        ESP_ERROR_CHECK(esp_now_add_peer(&peer));
    }

    // NeoPixel (WS2812) ueber RMT-Treiber initialisieren
    led_strip_config_t strip_config = {
        .strip_gpio_num = LED_PIN,
        .max_leds       = 1,
        .led_model      = LED_MODEL_WS2812,
        .color_component_format = LED_STRIP_COLOR_COMPONENT_FMT_GRB,
        .flags = {
            .invert_out = false,
        },
    };
    led_strip_rmt_config_t rmt_config = {
        .clk_src       = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = 10 * 1000 * 1000,  // 10 MHz
        .flags = {
            .with_dma = false,
        },
    };
    ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strip));
    led_strip_clear(led_strip);

    // GPIO konfigurieren – Button als Eingang mit internem Pull-up
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << BUTTON_PIN),
        .mode         = GPIO_MODE_INPUT,
        .pull_up_en   = GPIO_PULLUP_ENABLE,
    };
    gpio_config(&io_conf);

    ESP_LOGI(TAG, "Bereit! Button druecken zum Senden.");

    // Hauptschleife – läuft als FreeRTOS-Task (ersetzt Arduino's loop())
    while (1) {
        // Pull-up: 0 = gedrückt, 1 = nicht gedrückt
        if (gpio_get_level(BUTTON_PIN) == 0) {
            nachricht_t msg = { .led_an = true };

            // An alle anderen Geräte senden
            for (int i = 0; i < MAC_ANZAHL; i++) {
                if (i == eigener_index) {
                    continue;
                }
                esp_now_send(mac_adressen[i], (uint8_t *)&msg, sizeof(msg));
            }
            ESP_LOGI(TAG, "Gesendet an %d Geraet(e)!", MAC_ANZAHL - 1);

            // Eigene LED kurz blinken als Bestätigung, danach zurück zum aktuellen Zustand
            led_set(true);
            vTaskDelay(pdMS_TO_TICKS(100));
            led_set(led_status);

            // Warten bis Button losgelassen wird
            while (gpio_get_level(BUTTON_PIN) == 0) {
                vTaskDelay(pdMS_TO_TICKS(10));
            }
            vTaskDelay(pdMS_TO_TICKS(50));  // Gegen Button-Prellen
        }

        vTaskDelay(pdMS_TO_TICKS(10));  // CPU nicht dauerhaft blockieren
    }
}
