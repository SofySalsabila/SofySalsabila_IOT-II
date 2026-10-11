#include "painlessMesh.h"
#include <ArduinoJson.h>

#define MESH_PREFIX     "SmartMeshNet"
#define MESH_PASSWORD   "passwordrahasia"
#define MESH_PORT       5555

#define LED_PIN         12     // D6 (GPIO 12) pada NodeMCU

Scheduler userScheduler;
painlessMesh mesh;

// Variabel penyimpan data sensor terakhir untuk Rule Engine
float latestTemp = 0.0;
int latestAdc = 1023; // Default nilai awal terang

void evaluateRuleEngine() {
  // Rule Engine: JIKA suhu > 31.0 C ATAU nilai ADC cahaya < 300 (gelap), nyalakan LED
  if (latestTemp > 31.0 || latestAdc < 300) {
    digitalWrite(LED_PIN, HIGH);
    Serial.println("[RULE ENGINE] LED NYALA (Kondisi kritis / gelap terpenuhi)");
  } else {
    digitalWrite(LED_PIN, LOW);
    Serial.println("[RULE ENGINE] LED MATI (Kondisi lingkungan normal)");
  }
}

void receivedCallback(uint32_t from, String &msg) {
  Serial.printf("Pesan diterima dari node %u: %s\n", from, msg.c_str());

  // Bedah string JSON menggunakan ArduinoJson
  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, msg);

  if (error) {
    Serial.print("Gagal parsing JSON: ");
    Serial.println(error.c_str());
    return;
  }

  const char* tipe = doc["tipe"];
  if (strcmp(tipe, "suhu_node") == 0) {
    latestTemp = doc["suhu"];
    Serial.printf("-> Suhu Diperbarui: %.2f C\n", latestTemp);
  } else if (strcmp(tipe, "cahaya_node") == 0) {
    latestAdc = doc["adc"];
    Serial.printf("-> ADC Cahaya Diperbarui: %d\n", latestAdc);
  }

  // Evaluasi kondisi otomasi aktuator
  evaluateRuleEngine();
}

void newConnectionCallback(uint32_t nodeId) {
  Serial.printf("Koneksi baru terdeteksi, nodeId = %u\n", nodeId);
}

void changedConnectionCallback() {
  Serial.println("Topologi mesh berubah (Self-Healing / Multi-Hop terdeteksi).");
}

void nodeTimeAdjustedCallback(int32_t offset) {
  // Opsional
}

void setup() {
  Serial.begin(115200);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  mesh.setDebugMsgTypes(ERROR | STARTUP);  
  mesh.init(MESH_PREFIX, MESH_PASSWORD, &userScheduler, MESH_PORT);
  mesh.onReceive(&receivedCallback);
  mesh.onNewConnection(&newConnectionCallback);
  mesh.onChangedConnections(&changedConnectionCallback);
  mesh.onNodeTimeAdjusted(&nodeTimeAdjustedCallback);
}

void loop() {
  mesh.update();
}