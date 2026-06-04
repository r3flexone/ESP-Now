/*
 * ============================================================
 *  ESP-NOW  –  SENDER (Gerät 1)
 * ============================================================
 *
 *  Was passiert hier?
 *  ------------------
 *  Wenn du den Button drückst, schickt dieses Gerät per ESP-NOW
 *  eine winzige Nachricht (1 Byte!) an den Empfänger.
 *  Der Empfänger lässt dann seine LED aufleuchten.
 *
 *  ESP-NOW ist wie ein Walkie-Talkie direkt zwischen zwei ESPs –
 *  kein WLAN-Router nötig, Reichweite ~200 m.
 *
 *  Verkabelung:
 *  ------------
 *   Button  →  GPIO 0  (auf den meisten ESP32-Boards = BOOT-Button)
 *   LED     →  GPIO 2  (eingebaute LED auf vielen ESP32-Boards)
 *
 *  Reihenfolge beim Flashen:
 *  -------------------------
 *   1. Zuerst den EMPFÄNGER flashen und Seriellen Monitor öffnen.
 *      Dort steht die MAC-Adresse des Empfängers.
 *   2. Diese MAC-Adresse unten bei "empfaengerAdresse" eintragen.
 *   3. Dann diesen Sender-Code flashen.
 * ============================================================
 */

#include <esp_now.h>
#include <WiFi.h>

// ── ANPASSEN: MAC-Adresse des Empfängers eintragen ──────────
// Beispiel: {0x24, 0x6F, 0x28, 0xAB, 0xCD, 0xEF}
// Die richtige Adresse steht im Seriellen Monitor des Empfängers.
uint8_t empfaengerAdresse[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
// ────────────────────────────────────────────────────────────

#define BUTTON_PIN 0   // BOOT-Button (LOW wenn gedrückt, wegen INPUT_PULLUP)
#define LED_PIN    2   // Eingebaute LED – blinkt kurz als Sendebestätigung

// Die Nachricht die wir senden – bewusst so klein wie möglich gehalten.
// Beide Geräte müssen dieselbe Struktur verwenden!
typedef struct {
  bool ledAn;  // true = "mach deine LED an"
} Nachricht;

Nachricht zuSendenderWert;


// ── Callback: Wird aufgerufen NACHDEM gesendet wurde ────────
// Hier erfahren wir ob der Empfänger die Nachricht bekommen hat.
void sendungAbgeschlossen(const uint8_t *mac, esp_now_send_status_t status) {
  if (status == ESP_NOW_SEND_SUCCESS) {
    Serial.println("✓ Empfaenger hat die Nachricht bekommen.");
  } else {
    // Das passiert wenn der Empfänger ausgeschaltet ist oder zu weit weg.
    Serial.println("✗ Senden fehlgeschlagen – Empfaenger nicht erreichbar?");
  }
}


void setup() {
  Serial.begin(115200);
  delay(500); // Kurz warten bis Serial bereit ist

  pinMode(BUTTON_PIN, INPUT_PULLUP);
  // INPUT_PULLUP bedeutet: Pin ist normalerweise HIGH.
  // Wenn der Button gedrückt wird, verbindet er den Pin mit GND → LOW.

  pinMode(LED_PIN, OUTPUT);

  // ESP-NOW braucht WiFi im Station-Modus, aber wir verbinden uns mit NICHTS.
  // Wir nutzen WiFi nur als "Funklayer" für ESP-NOW.
  WiFi.mode(WIFI_STA);

  // ESP-NOW starten
  if (esp_now_init() != ESP_OK) {
    // Das passiert fast nie – nur wenn die Hardware defekt ist.
    Serial.println("FEHLER: ESP-NOW konnte nicht gestartet werden.");
    return; // Ohne ESP-NOW läuft nichts, also aufhören.
  }

  esp_now_register_send_cb(sendungAbgeschlossen);

  // Empfänger als "Peer" (Kommunikationspartner) registrieren
  esp_now_peer_info_t peer = {};             // Alle Felder auf 0 setzen
  memcpy(peer.peer_addr, empfaengerAdresse, 6);
  peer.channel  = 0;      // 0 = automatisch den aktuellen Kanal nehmen
  peer.ifidx    = WIFI_IF_STA;
  peer.encrypt  = false;  // Keine Verschlüsselung – hält den Code einfach

  if (esp_now_add_peer(&peer) != ESP_OK) {
    Serial.println("FEHLER: Empfaenger konnte nicht registriert werden.");
    Serial.println("       Stimmt die MAC-Adresse oben im Code?");
    return;
  }

  Serial.println("Sender bereit! Druecke den Button (GPIO 0).");
  Serial.print("Sende an MAC: ");
  Serial.println(WiFi.macAddress()); // Zur Info – das ist die Adresse DIESES Geräts
}


void loop() {
  // INPUT_PULLUP: Button gedrückt = LOW
  if (digitalRead(BUTTON_PIN) == LOW) {

    zuSendenderWert.ledAn = true;
    esp_now_send(empfaengerAdresse, (uint8_t *)&zuSendenderWert, sizeof(zuSendenderWert));
    Serial.println("Button gedrückt – Nachricht gesendet!");

    // Eigene LED kurz blinken als sofortige visuelle Rückmeldung
    digitalWrite(LED_PIN, HIGH);
    delay(100);
    digitalWrite(LED_PIN, LOW);

    // Warten bis der Button losgelassen wird – sonst wird die Nachricht
    // viele Male pro Sekunde gesendet solange der Button gehalten wird.
    while (digitalRead(BUTTON_PIN) == LOW) {
      delay(10);
    }

    delay(50); // Kurze Pause gegen "Prellen" (mechanisches Zittern des Buttons)
  }
}
