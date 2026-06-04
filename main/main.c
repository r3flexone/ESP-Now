/*
 * ============================================================
 *  ESP-NOW  –  DUPLEX  (gleicher Code für BEIDE Geräte)
 *  ESP-IDF Projekt – VS Code + ESP-IDF Extension
 * ============================================================
 *
 *  Jedes Gerät hat einen Button UND eine LED.
 *  Button drücken → sendet an das andere Gerät → dort leuchtet die LED.
 *  Funktioniert in beide Richtungen gleichzeitig.
 *
 *  Verkabelung (beide Geräte identisch):
 *  --------------------------------------
 *   Button  →  GPIO 0  (BOOT-Button, LOW wenn gedrückt)
 *   LED     →  GPIO 2  (eingebaute LED)
 *
 *  Setup-Reihenfolge:
 *  ------------------
 *   1. Code auf GERÄT 1 flashen  →  Seriellen Monitor öffnen
 *      →  MAC-Adresse notieren (z.B. 24:6F:28:AA:BB:01)
 *   2. Code auf GERÄT 2 flashen  →  MAC-Adresse notieren
 *   3. Beide Adressen unten bei mac_geraet1 / mac_geraet2 eintragen.
 *   4. Code nochmal auf BEIDE Geräte flashen – fertig!
 *
 *  VS Code Befehle (ESP-IDF Extension):
 *  -------------------------------------
 *   Bauen:           Ctrl+E  B
 *   Flashen:         Ctrl+E  F
 *   Monitor öffnen:  Ctrl+E  M
 *   Alles auf einmal: idf.py flash monitor  (im Terminal)
 * ============================================================
 */

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "esp_wifi.h"
#include "esp_now.h"
#include "esp_log.h"
#include "driver/gpio.h"   // ab ESP-IDF v6: Komponente esp_driver_gpio

// ── ANPASSEN: MAC-Adressen beider Geräte eintragen ──────────
// Beispiel: {0x24, 0x6F, 0x28, 0xAA, 0xBB, 0x01}
static uint8_t mac_geraet1[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x01};
static uint8_t mac_geraet2[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x02};
// ────────────────────────────────────────────────────────────

#define BUTTON_PIN  GPIO_NUM_0
#define LED_PIN     GPIO_NUM_2

// Tag für Lognachrichten – erscheint im Monitor als "[ESP-NOW] ..."
static const char *TAG = "ESP-NOW";

// Nachrichtenstruktur – muss auf beiden Geräten identisch sein
typedef struct {
    bool led_an;
} nachricht_t;

// Wird in app_main gesetzt nachdem bekannt ist welches Gerät wir sind
static uint8_t *ziel_adresse = NULL;


// ── Callback: aufgerufen NACHDEM wir gesendet haben ─────────
static void send_callback(const uint8_t *mac, esp_now_send_status_t status)
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
        ESP_LOGI(TAG, "Nachricht empfangen -> LED an!");
        gpio_set_level(LED_PIN, 1);
        vTaskDelay(pdMS_TO_TICKS(500));  // 500 ms leuchten
        gpio_set_level(LED_PIN, 0);
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

    // Eigene MAC-Adresse lesen und anzeigen
    uint8_t eigene_mac[6];
    esp_wifi_get_mac(WIFI_IF_STA, eigene_mac);
    ESP_LOGI(TAG, "Meine MAC: %02X:%02X:%02X:%02X:%02X:%02X",
             eigene_mac[0], eigene_mac[1], eigene_mac[2],
             eigene_mac[3], eigene_mac[4], eigene_mac[5]);

    // Anhand der eigenen MAC herausfinden wer das andere Gerät ist
    if (memcmp(eigene_mac, mac_geraet1, 6) == 0) {
        ziel_adresse = mac_geraet2;
        ESP_LOGI(TAG, "Ich bin Geraet 1 -> sende an Geraet 2");
    } else if (memcmp(eigene_mac, mac_geraet2, 6) == 0) {
        ziel_adresse = mac_geraet1;
        ESP_LOGI(TAG, "Ich bin Geraet 2 -> sende an Geraet 1");
    } else {
        // Keiner der eingetragenen MACs passt
        ESP_LOGE(TAG, "MAC stimmt mit keiner eingetragenen Adresse ueberein!");
        ESP_LOGE(TAG, "Bitte mac_geraet1 und mac_geraet2 in main.c anpassen.");
        return;
    }

    // ESP-NOW starten
    ESP_ERROR_CHECK(esp_now_init());
    esp_now_register_send_cb(send_callback);
    esp_now_register_recv_cb(recv_callback);

    // Das andere Gerät als Kommunikationspartner registrieren
    esp_now_peer_info_t peer = {};
    memcpy(peer.peer_addr, ziel_adresse, 6);
    peer.channel = 0;
    peer.ifidx   = ESP_IF_WIFI_STA;
    peer.encrypt = false;
    ESP_ERROR_CHECK(esp_now_add_peer(&peer));

    // GPIO konfigurieren – LED als Ausgang
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << LED_PIN),
        .mode         = GPIO_MODE_OUTPUT,
    };
    gpio_config(&io_conf);

    // GPIO konfigurieren – Button als Eingang mit internem Pull-up
    io_conf.pin_bit_mask = (1ULL << BUTTON_PIN);
    io_conf.mode         = GPIO_MODE_INPUT;
    io_conf.pull_up_en   = GPIO_PULLUP_ENABLE;
    gpio_config(&io_conf);

    ESP_LOGI(TAG, "Bereit! Button druecken zum Senden.");

    // Hauptschleife – läuft als FreeRTOS-Task (ersetzt Arduino's loop())
    while (1) {
        // Pull-up: 0 = gedrückt, 1 = nicht gedrückt
        if (gpio_get_level(BUTTON_PIN) == 0) {
            nachricht_t msg = { .led_an = true };
            esp_now_send(ziel_adresse, (uint8_t *)&msg, sizeof(msg));
            ESP_LOGI(TAG, "Gesendet!");

            // Eigene LED kurz blinken als Bestätigung
            gpio_set_level(LED_PIN, 1);
            vTaskDelay(pdMS_TO_TICKS(100));
            gpio_set_level(LED_PIN, 0);

            // Warten bis Button losgelassen wird
            while (gpio_get_level(BUTTON_PIN) == 0) {
                vTaskDelay(pdMS_TO_TICKS(10));
            }
            vTaskDelay(pdMS_TO_TICKS(50));  // Gegen Button-Prellen
        }

        vTaskDelay(pdMS_TO_TICKS(10));  // CPU nicht dauerhaft blockieren
    }
}
