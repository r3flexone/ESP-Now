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

Alle Befehle im `Duplex/` Verzeichnis ausführen (dort liegt das ESP-IDF Projekt):

```bash
idf.py build                    # Projekt bauen
idf.py flash                    # Auf ESP32 flashen
idf.py monitor                  # Seriellen Monitor öffnen
idf.py flash monitor            # Flashen und Monitor in einem Schritt
idf.py -p /dev/ttyUSB0 flash monitor  # Mit explizitem Port
```

VS Code Shortcuts (ESP-IDF Extension):
- `Ctrl+E B` – Build
- `Ctrl+E F` – Flash
- `Ctrl+E M` – Monitor

## Architektur

```
Sender/Sender.ino         →  Simplex: Button-Druck → sendet (Arduino, nur Sender-Rolle)
Empfaenger/Empfaenger.ino →  Simplex: empfängt → LED leuchtet (Arduino, nur Empfänger-Rolle)
Duplex/                   →  ESP-IDF Projekt: gleicher Code für BEIDE Geräte
  main/main.c             →  Hauptcode: Button + LED auf jedem Gerät
  main/CMakeLists.txt     →  Komponenten-Definition (Abhängigkeiten: esp_now, esp_wifi, nvs_flash, driver)
  CMakeLists.txt          →  Projekt-Root
```

**Duplex-Ansatz:** Beide MAC-Adressen sind im Code definiert. Jedes Gerät liest beim Start seine eigene MAC (`esp_wifi_get_mac()`), vergleicht sie mit den eingetragenen Adressen und registriert automatisch das jeweils andere Gerät als Peer. So läuft ein einziger Code auf beiden Geräten.

**Gemeinsame Datenstruktur** (`typedef struct nachricht_t`): Alle Programme, die miteinander kommunizieren, müssen dieselbe Struct verwenden. Änderungen müssen in allen beteiligten Dateien identisch übernommen werden.

**Callback-Muster:** ESP-NOW arbeitet ereignisgesteuert:
- `esp_now_register_send_cb()` → wird nach dem Senden aufgerufen
- `esp_now_register_recv_cb()` → wird beim Empfang aufgerufen

## Wichtige Konventionen

- **GPIO-Pins:** Button = GPIO 0 (BOOT-Button, Pull-up → LOW wenn gedrückt), LED = GPIO 2 (eingebaute LED)
- **MAC-Adresse:** Muss einmalig manuell aus dem Seriellen Monitor in `main.c` eingetragen werden (`mac_geraet1[]` / `mac_geraet2[]`)
- **Verzögerungen:** Kein `delay()` – stattdessen `vTaskDelay(pdMS_TO_TICKS(ms))` (FreeRTOS)
- **Logging:** `ESP_LOGI` / `ESP_LOGW` / `ESP_LOGE` statt `Serial.println`
- **Kommentarsprache:** Deutsch – Kommentare und Log-Ausgaben bleiben auf Deutsch
