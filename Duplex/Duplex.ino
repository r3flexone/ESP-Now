/*
 * ============================================================
 *  ESP-NOW  –  DUPLEX  (gleicher Code für BEIDE Geräte)
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
 *   1. Diesen Code auf GERÄT 1 flashen, Seriellen Monitor öffnen
 *      → MAC-Adresse notieren (z.B. 24:6F:28:AA:BB:01)
 *   2. Diesen Code auf GERÄT 2 flashen, Seriellen Monitor öffnen
 *      → MAC-Adresse notieren (z.B. 24:6F:28:AA:BB:02)
 *   3. Beide Adressen unten eintragen.
 *   4. Code nochmal auf BEIDE Geräte flashen – fertig!
 *
 *  Wie erkennt ein Gerät wen es anschreiben soll?
 *  -----------------------------------------------
 *  In setup() liest jedes Gerät seine eigene MAC-Adresse und
 *  vergleicht sie mit macGeraet1. Stimmt sie überein → sende an
 *  Gerät 2. Andernfalls → sende an Gerät 1.
 *  Beide Geräte laufen also mit identischem Code.
 * ============================================================
 */

#include <esp_now.h>
#include <WiFi.h>

// ── ANPASSEN: MAC-Adressen beider Geräte eintragen ──────────
// Beispiel: {0x24, 0x6F, 0x28, 0xAA, 0xBB, 0x01}
// Die Adressen stehen im Seriellen Monitor wenn du den Code
// das erste Mal flasht (vor Schritt 3 oben).
uint8_t macGeraet1[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x01};  // Gerät 1 eintragen
uint8_t macGeraet2[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x02};  // Gerät 2 eintragen
// ────────────────────────────────────────────────────────────

#define BUTTON_PIN 0
#define LED_PIN    2

typedef struct {
  bool ledAn;
} Nachricht;

Nachricht zuSendenderWert;
uint8_t *zielAdresse;  // Zeigt auf macGeraet1 oder macGeraet2 – wird in setup() gesetzt


// ── Hilfsfunktion: Vergleicht zwei 6-Byte MAC-Adressen ──────
bool macIstGleich(uint8_t *a, uint8_t *b) {
  return memcmp(a, b, 6) == 0;
}


// ── Callback: Aufgerufen nachdem eine Nachricht gesendet wurde
void sendungAbgeschlossen(const uint8_t *mac, esp_now_send_status_t status) {
  if (status == ESP_NOW_SEND_SUCCESS) {
    Serial.println("✓ Nachricht angekommen.");
  } else {
    Serial.println("✗ Senden fehlgeschlagen – anderes Gerät erreichbar?");
  }
}


// ── Callback: Aufgerufen wenn eine Nachricht empfangen wird ──
void nachrichtEmpfangen(const esp_now_recv_info_t *info, const uint8_t *daten, int laenge) {

  if (laenge != sizeof(Nachricht)) {
    return;  // Fremde oder kaputte Nachricht – ignorieren
  }

  Nachricht *empfangen = (Nachricht *)daten;

  if (empfangen->ledAn) {
    Serial.println("Nachricht empfangen → LED an!");
    digitalWrite(LED_PIN, HIGH);
    delay(500);
    digitalWrite(LED_PIN, LOW);
  }
}


void setup() {
  Serial.begin(115200);
  delay(500);

  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(LED_PIN, OUTPUT);

  WiFi.mode(WIFI_STA);

  // Eigene MAC-Adresse lesen
  uint8_t eigeneMac[6];
  WiFi.macAddress(eigeneMac);

  Serial.print("Meine MAC: ");
  Serial.println(WiFi.macAddress());

  // Anhand der eigenen MAC herausfinden, wer das "andere Gerät" ist
  if (macIstGleich(eigeneMac, macGeraet1)) {
    zielAdresse = macGeraet2;
    Serial.println("Ich bin Geraet 1 → sende an Geraet 2");
  } else if (macIstGleich(eigeneMac, macGeraet2)) {
    zielAdresse = macGeraet1;
    Serial.println("Ich bin Geraet 2 → sende an Geraet 1");
  } else {
    // Keiner der eingetragenen MACs passt – meistens vergessen die Adressen
    // nach Schritt 1+2 oben einzutragen und nochmal zu flashen.
    Serial.println("FEHLER: Meine MAC stimmt mit keiner der eingetragenen Adressen ueberein.");
    Serial.println("        Bitte macGeraet1 und macGeraet2 oben im Code anpassen!");
    return;
  }

  if (esp_now_init() != ESP_OK) {
    Serial.println("FEHLER: ESP-NOW konnte nicht gestartet werden.");
    return;
  }

  esp_now_register_send_cb(sendungAbgeschlossen);
  esp_now_register_recv_cb(nachrichtEmpfangen);

  // Das andere Gerät als Kommunikationspartner registrieren
  esp_now_peer_info_t peer = {};
  memcpy(peer.peer_addr, zielAdresse, 6);
  peer.channel = 0;
  peer.ifidx   = WIFI_IF_STA;
  peer.encrypt = false;

  if (esp_now_add_peer(&peer) != ESP_OK) {
    Serial.println("FEHLER: Partner konnte nicht registriert werden.");
    return;
  }

  Serial.println("Bereit! Button druecken zum Senden.");
}


void loop() {
  if (digitalRead(BUTTON_PIN) == LOW) {

    zuSendenderWert.ledAn = true;
    esp_now_send(zielAdresse, (uint8_t *)&zuSendenderWert, sizeof(zuSendenderWert));
    Serial.println("Gesendet!");

    // Eigene LED kurz blinken als Bestätigung dass gesendet wurde
    digitalWrite(LED_PIN, HIGH);
    delay(100);
    digitalWrite(LED_PIN, LOW);

    // Warten bis Button losgelassen wird
    while (digitalRead(BUTTON_PIN) == LOW) {
      delay(10);
    }
    delay(50);  // Gegen Button-Prellen
  }
}
