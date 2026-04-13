// Тест OTA: мигаем встроенным LED + OTA остаётся для следующей прошивки

#include <Arduino.h>
#include <WiFi.h>
#include <ArduinoOTA.h>

const char* WIFI_SSID = "Admin";
const char* WIFI_PASS = "92211667";

#define LED 2  // встроенный LED на ESP32

void setup() {
  Serial.begin(9600);
  pinMode(LED, OUTPUT);

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED) delay(500);

  ArduinoOTA.setHostname("qoima-matrix");
  ArduinoOTA.begin();

  Serial.print("OTA ready at ");
  Serial.println(WiFi.localIP());
}

void loop() {
  ArduinoOTA.handle();
  digitalWrite(LED, HIGH);
  delay(300);
  digitalWrite(LED, LOW);
  delay(300);
}
