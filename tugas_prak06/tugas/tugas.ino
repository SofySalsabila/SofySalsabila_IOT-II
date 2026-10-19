#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <DHT.h>

// Kredensial Jaringan Wi-Fi
const char* ssid     = "sofy";
const char* password = "12345678";

// Konfigurasi Broker MQTT Publik
const char* mqttServer = "broker.hivemq.com";
const int mqttPort     = 1883;
const char* clientId   = "ESP8266_LabIoT_25782084_ClosedLoop";

// Pembagian Topik Mandiri (NPM: 25782084)
const char* topicPublish   = "kampus/labiot/25782084/telemetri";
const char* topicSubscribe = "kampus/labiot/25782084/kendali";
const char* topicAlert     = "kampus/labiot/25782084/alert";      // Topik Peringatan Bahaya
const char* topicStatus    = "kampus/labiot/25782084/status";     // Topik Status LWT (Online/Offline)

// Alokasi Pin Perangkat Keras
const byte dhtPin = 2; // Pin D4 (GPIO 2)
const byte ledPin = 5; // Pin D1 (GPIO 5)
#define DHTTYPE DHT22

DHT dht(dhtPin, DHTTYPE);
WiFiClient espClient;
PubSubClient client(espClient);

unsigned long lastTelemetryTime = 0;
const unsigned long telemetryInterval = 4000; // Siklus kirim telemetri 4 detik

// Flag penanda kondisi bahaya (State Latch Protection agar tidak spam broker)
bool isOverheatAlerted = false; 

void callback(char* topic, byte* payload, unsigned int length) {
  char incomingBuffer[length + 1];
  memcpy(incomingBuffer, payload, length);
  incomingBuffer[length] = '\0';

  Serial.println("----------------------------------------");
  Serial.printf("[KONTROL DITERIMA] Topik: %s | Payload: %s\n", topic, incomingBuffer);

  StaticJsonDocument<256> doc;
  DeserializationError err = deserializeJson(doc, incomingBuffer);
  if (err) {
    Serial.println("Gagal parsing JSON perintah!");
    return;
  }

  // Cek Suhu Saat Ini Sebelum Memproses Manual Override
  float currentTemp = dht.readTemperature();
  if (!isnan(currentTemp) && currentTemp > 31.0) {
    Serial.println("--> [DITOLAK] Sistem dalam kondisi OVERHEAT! Kendali manual dikunci.");
    Serial.println("----------------------------------------");
    return;
  }

  // Kendali Manual Override saat suhu aman (<= 31.0°C)
  if (doc.containsKey("led")) {
    int val = doc["led"];
    digitalWrite(ledPin, val == 1 ? HIGH : LOW);
    Serial.printf("--> [INDIKATOR] Status LED diubah manual: %s\n", val == 1 ? "MENYALA" : "PADAM");
  }
  Serial.println("----------------------------------------");
}

void reconnect() {
  while (!client.connected()) {
    Serial.print("Menghubungkan kembali ke Broker...");
    
    // Config Last Will and Testament (LWT) Payload
    const char* lwtPayload = "{\"status\":\"OFFLINE\",\"keterangan\":\"Perangkat Terputus Tak Terduga\"}";
    
    // Hubungkan Klien dengan LWT (Topic, QoS=1, Retain=true, Payload)
    if (client.connect(clientId, topicStatus, 1, true, lwtPayload)) {
      Serial.println(" Sukses Terhubung!");
      
      // Publikasikan Pesan ONLINE (Retain=true)
      const char* onlinePayload = "{\"status\":\"ONLINE\",\"keterangan\":\"Perangkat Beroperasi Normal\"}";
      client.publish(topicStatus, onlinePayload, true);
      
      // Subskripsi Topik Kendali
      client.subscribe(topicSubscribe);
      Serial.printf("Subskripsi Topik: %s\n", topicSubscribe);
    } else {
      Serial.printf("Gagal (rc=%d). Coba lagi dalam 4 detik...\n", client.state());
      delay(4000);
    }
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(ledPin, OUTPUT);
  digitalWrite(ledPin, LOW);

  dht.begin();

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.print("Menghubungkan Wi-Fi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWi-Fi Terkoneksi!");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());

  client.setServer(mqttServer, mqttPort);
  client.setCallback(callback);
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop(); // Memelihara sesi TCP dan memproses pesan masuk

  unsigned long now = millis();
  if (now - lastTelemetryTime >= telemetryInterval) {
    lastTelemetryTime = now;

    float t = dht.readTemperature();
    float h = dht.readHumidity();

    if (!isnan(t) && !isnan(h)) {
      // 1. LOGIKA KESELAMATAN & RULE ENGINE (Closed-Loop)
      String kondisiSuhu = (t > 31.0) ? "PANAS" : "NORMAL";

      if (t > 31.0) {
        // Otomatis nyalakan LED indikator bahaya
        digitalWrite(ledPin, HIGH);

        // Jika belum pernah kirim alert overheat (Flag Latch Protection)
        if (!isOverheatAlerted) {
          StaticJsonDocument<256> alertDoc;
          alertDoc["status"] = "BAHAYA";
          alertDoc["parameter"] = "OVERHEAT";
          alertDoc["nilai_suhu"] = serialized(String(t, 2));

          char alertBuffer[256];
          serializeJson(alertDoc, alertBuffer);
          
          client.publish(topicAlert, alertBuffer);
          Serial.printf("!!! [ALERT OVERHEAT TERKIRIM] %s !!!\n", alertBuffer);
          
          isOverheatAlerted = true; // Aktifkan flag penanda
        }
      } else {
        // Jika suhu kembali normal, reset flag penanda
        if (isOverheatAlerted) {
          Serial.println("--> [INFO] Suhu kembali normal (<= 31.0 C). Alert reset.");
          isOverheatAlerted = false;
        }
      }

      // 2. PUBLIKASI TELEMETRI RUTIN
      StaticJsonDocument<256> docOut;
      docOut["node_id"] = clientId;
      docOut["suhu"] = serialized(String(t, 2));
      docOut["kelembapan"] = serialized(String(h, 2));
      docOut["status_led"] = digitalRead(ledPin);
      docOut["kondisi_suhu"] = kondisiSuhu;

      char buffer[256];
      serializeJson(docOut, buffer);
      
      bool success = client.publish(topicPublish, buffer);
      if (success) {
        Serial.printf("[TELEMETRI TERKIRIM] %s\n", buffer);
      } else {
        Serial.println("[ERROR] Gagal memublikasikan telemetri!");
      }
    }
  }
}