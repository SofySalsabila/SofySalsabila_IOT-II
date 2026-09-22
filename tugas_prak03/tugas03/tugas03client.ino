#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClient.h>

const char* ssid = "sofy";
const char* password = "12345678";

// IP NodeMCU SERVER
const char* serverName = "http://10.235.57.114/relay/on";

const byte ldrPin = A0;

void setup() {
  Serial.begin(115200);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nClient Terhubung ke Wi-Fi!");
  Serial.print("IP Client: ");
  Serial.println(WiFi.localIP());
}

void loop() {
  int ldrValue = analogRead(ldrPin);

  Serial.print("Nilai LDR: ");
  Serial.println(ldrValue);

  // Jika kondisi gelap, kirim GET ke Server
  if ((WiFi.status() == WL_CONNECTED) && (ldrValue < 300)) {
    WiFiClient client;
    HTTPClient http;

    http.begin(client, serverName);

    int httpResponseCode = http.GET();

    Serial.print("HTTP Response code: ");
    Serial.println(httpResponseCode);

    http.end();

    delay(10000);
  }

  delay(2000);
}