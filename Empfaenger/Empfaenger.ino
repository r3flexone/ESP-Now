/*
 * ============================================================
 *  ESP-NOW  –  EMPFÄNGER (Gerät 2)
 * ============================================================
 *
 *  Was passiert hier?
 *  ------------------
 *  Dieses Gerät wartet auf Nachrichten vom Sender.
 *  Wenn eine Nachricht ankommt, leuchtet die LED für 500 ms auf.
 *
 *  Verkabelung:
 *  ------------
 *   LED  →  GPIO 2  (eingebaute LED auf vielen ESP32-Boards)
 *
 *  Reihenfolge beim Flashen:
 *  -------------------------
 *   1. ZUERST diesen Code flashen.
 *   2. Seriellen Monitor öffnen (115200 Baud).
 *   3. Die angezeigte MAC-Adresse in den Sender-Code kopieren.
 *   4. Dann den Sender flashen.
 * ============================================================
 */

#include <esp_now.h>
#include <WiFi.h>

#define LED_PIN 2  // Eingebaute LED

// Muss IDENTISCH sein mit der Struktur im Sender-Code!
typedef struct {
  bool ledAn;
} Nachricht;


// ── Callback: Wird aufgerufen WENN eine Nachricht ankommt ───
// Diese Funktion unterbricht automatisch den normalen Programmfluss
// wenn ein Signal eingeht – man nennt das einen "Interrupt-ähnlichen Callback".
void nachrichtEmpfangen(const esp_now_recv_info_t *info, const uint8_t *daten, int laenge) {

  // Prüfen ob die Nachricht die richtige Größe hat
  // (Schutz gegen zufälligen Datenmüll von anderen ESP-NOW Geräten)
  if (laenge != sizeof(Nachricht)) {
    Serial.println("Unbekannte Nachricht empfangen – ignoriert.");
    return;
  }

  // Rohe Bytes in unsere Struktur umwandeln
  Nachricht *empfangen = (Nachricht *)daten;

  if (empfangen->ledAn) {
    Serial.println("Nachricht empfangen → LED an!");

    // LED kurz aufleuchten lassen
    // Hinweis: delay() im Callback ist eigentlich unschön (blockiert den Prozessor),
    // hier aber okay weil wir sowieso nichts anderes tun.
    digitalWrite(LED_PIN, HIGH);
    delay(500);
    digitalWrite(LED_PIN, LOW);
  }
}


void setup() {
  Serial.begin(115200);
  delay(500); // Kurz warten bis Serial bereit ist

  pinMode(LED_PIN, OUTPUT);

  WiFi.mode(WIFI_STA);

  // WICHTIG: Diese MAC-Adresse in den Sender-Code kopieren!
  Serial.println("===========================================");
  Serial.print("MAC-Adresse dieses Empfaengers: ");
  Serial.println(WiFi.macAddress());
  Serial.println("Diese Adresse in Sender.ino eintragen!");
  Serial.println("===========================================");

  if (esp_now_init() != ESP_OK) {
    Serial.println("FEHLER: ESP-NOW konnte nicht gestartet werden.");
    return;
  }

  // Callback registrieren: ESP-NOW ruft diese Funktion auf wenn was ankommt.
  // Der Empfänger muss keine MAC-Adresse vom Sender kennen –
  // er akzeptiert Nachrichten von allen ESP-NOW Geräten.
  esp_now_register_recv_cb(nachrichtEmpfangen);

  Serial.println("Empfaenger bereit – warte auf Nachrichten...");
}


void loop() {
  // Hier ist nichts zu tun!
  // Wenn eine Nachricht ankommt, springt ESP-NOW automatisch
  // in die Funktion "nachrichtEmpfangen" oben – völlig unabhängig von loop().
}
