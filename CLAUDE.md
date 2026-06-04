# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Projekt

Arduino-Sketches für ESP32 zur drahtlosen Kommunikation via **ESP-NOW** – ein direktes Peer-to-Peer Funkprotokoll ohne WLAN-Router (Reichweite ~200 m).

## Toolchain

- **IDE:** Arduino IDE oder PlatformIO
- **Board:** ESP32 (Arduino ESP32 Core v2.x oder v3.x)
- **Bibliotheken:** `esp_now.h` und `WiFi.h` – beide im ESP32 Arduino Core enthalten, keine externe Installation nötig
- **Serieller Monitor:** 115200 Baud

## Architektur

Zwei eigenständige Sketches, die zusammenarbeiten:

```
Sender/Sender.ino         →  Button-Druck → sendet Nachricht per ESP-NOW
Empfaenger/Empfaenger.ino →  empfängt Nachricht → LED leuchtet auf
```

**Gemeinsame Datenstruktur** (`typedef struct Nachricht`): Beide Dateien definieren dieselbe Struct. Wenn die Struct geändert wird, muss sie in **beiden** Dateien identisch angepasst werden – sonst werden die Bytes falsch interpretiert.

**Callback-Muster:** ESP-NOW arbeitet ereignisgesteuert. Statt Polling in `loop()` registriert man Callbacks, die das Framework automatisch aufruft:
- Sender: `esp_now_register_send_cb()` → wird nach dem Senden aufgerufen
- Empfänger: `esp_now_register_recv_cb()` → wird beim Empfang aufgerufen

## Wichtige Konventionen

- **GPIO-Pins:** Button = GPIO 0 (BOOT-Button, `INPUT_PULLUP` → LOW wenn gedrückt), LED = GPIO 2 (eingebaute LED)
- **MAC-Adresse:** Muss einmalig manuell aus dem Seriellen Monitor des Empfängers in `Sender.ino` eingetragen werden (`empfaengerAdresse[]`)
- **API-Kompatibilität:** Der Receive-Callback hat in ESP32 Core v3.x eine andere Signatur (`esp_now_recv_info_t *`) als in v2.x (`const uint8_t *mac`). Aktuell wird die v3.x-Signatur verwendet.
- **Kommentarsprache:** Deutsch – Kommentare und Serial-Ausgaben bleiben auf Deutsch
