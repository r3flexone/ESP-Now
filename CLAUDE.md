# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Projekt

ESP32-Projekt zur drahtlosen Kommunikation via **ESP-NOW** – ein direktes Peer-to-Peer Funkprotokoll ohne WLAN-Router (Reichweite ~200 m).

## Toolchain

- **IDE:** VS Code mit [ESP-IDF Extension](https://marketplace.visualstudio.com/items?itemName=espressif.esp-idf-vscode-extension)
- **Framework:** ESP-IDF (natives Espressif Framework, kein Arduino)
- **Board:** ESP32
- **Serieller Monitor:** 115200 Baud

## Befehle

Alle Befehle im Root-Verzeichnis des Repos ausführen (dort liegt `CMakeLists.txt`):

```bash
idf.py build                    # Projekt bauen
idf.py flash                    # Auf ESP32 flashen
idf.py monitor                  # Seriellen Monitor öffnen
idf.py flash monitor            # Flashen und Monitor in einem Schritt
idf.py -p COM3 flash monitor    # Mit explizitem Port (Windows)
idf.py -p /dev/ttyUSB0 flash monitor  # Mit explizitem Port (Linux)
```

VS Code Shortcuts (ESP-IDF Extension):
- `Ctrl+E B` – Build
- `Ctrl+E F` – Flash
- `Ctrl+E M` – Monitor

## Architektur

```
CMakeLists.txt            →  ESP-IDF Projekt-Root (VS Code öffnet diesen Ordner)
main/main.c               →  Hauptcode: Button + LED auf jedem Gerät (N Geräte, gleicher Code)
main/CMakeLists.txt       →  Komponenten-Definition
main/mac_config.h         →  NICHT IM GIT – MAC-Liste aller Geräte, von Hand gepflegt
main/mac_config.h.example →  Vorlage/Format-Beispiel für mac_config.h
Sender/Sender.ino         →  Simplex: Button-Druck → sendet (Arduino, nur Sender-Rolle)
Empfaenger/Empfaenger.ino →  Simplex: empfängt → LED leuchtet (Arduino, nur Empfänger-Rolle)
```

**N-Geräte-Ansatz:** Die MAC-Adressen aller Geräte stehen als Array `mac_liste[]` in `main/mac_config.h` (von Hand gepflegt, siehe `mac_config.h.example`). Jedes Gerät liest beim Start seine eigene MAC (`esp_wifi_get_mac()`), findet seinen eigenen Index in `mac_liste[]` und registriert alle ANDEREN Geräte als ESP-NOW-Peers. Button-Druck sendet per Unicast an jedes andere Gerät einzeln. So läuft ein einziger Code auf allen Geräten (2, 3, ... N).

**Setup neuer/zusätzlicher Geräte:** Gerät flashen, im seriellen Monitor die eigene MAC-Adresse ablesen ("Meine MAC: ..."), dann in `main/mac_config.h` zur Liste `mac_liste[]` hinzufügen und `MAC_ANZAHL` anpassen. Danach alle Geräte neu flashen.

**Gemeinsame Datenstruktur** (`typedef struct nachricht_t`): Alle Programme, die miteinander kommunizieren, müssen dieselbe Struct verwenden. Änderungen müssen in allen beteiligten Dateien identisch übernommen werden.

**Callback-Muster:** ESP-NOW arbeitet ereignisgesteuert:
- `esp_now_register_send_cb()` → wird nach dem Senden aufgerufen
- `esp_now_register_recv_cb()` → wird beim Empfang aufgerufen

## Wichtige Konventionen

- **GPIO-Pins:** Button = GPIO 0 (BOOT-Button, Pull-up → LOW wenn gedrückt), LED = GPIO 48 (onboard NeoPixel/WS2812, ESP32-S3)
- **MAC-Adressen:** Von Hand in `main/mac_config.h` eintragen (nicht im Git, siehe `.gitignore`) – Vorlage: `main/mac_config.h.example`
- **Verzögerungen:** Kein `delay()` – stattdessen `vTaskDelay(pdMS_TO_TICKS(ms))` (FreeRTOS)
- **Logging:** `ESP_LOGI` / `ESP_LOGW` / `ESP_LOGE` statt `Serial.println`
- **Kommentarsprache:** Deutsch – Kommentare und Log-Ausgaben bleiben auf Deutsch
