#include <DHT.h>

const byte ldrPin   = A0;   // Sensor Cahaya LDR
const byte dhtPin   = 13;   // Sensor DHT (D7)
const byte relayPin = 12;   // Modul Relay 5V (D6)
const byte ledPin   = 5;    // Indikator LED (D1)

#define DHTTYPE DHT11

DHT dht(dhtPin, DHTTYPE);

void setup() {
  Serial.begin(115200);
  dht.begin();

  pinMode(relayPin, OUTPUT);
  pinMode(ledPin, OUTPUT);
 
  // Inisialisasi kondisi awal: Relay Mati (Active-Low), LED Mati
  digitalWrite(relayPin, HIGH);
  digitalWrite(ledPin, LOW);

  Serial.println("--------------------------------");
  Serial.println("        SMART WAREHOUSE         ");
  Serial.println("--------------------------------");
}

void loop() {
  delay(2500); // Jeda pembacaan sensor

  int ldrValue = analogRead(ldrPin);
  float temperature = dht.readTemperature();
  float humidity = dht.readHumidity();

  // Validasi pembacaan sensor DHT
  if (isnan(temperature) || isnan(humidity)) {
    Serial.println("Gagal membaca data DHT!");
    return;
  }

  // Menampilkan Data Sensor ke Serial Monitor
  Serial.println("--------------------------------");
  Serial.print("Suhu       : ");
  Serial.print(temperature);
  Serial.println(" C");

  Serial.print("Kelembapan : ");
  Serial.print(humidity);
  Serial.println(" %");

  Serial.print("LDR ADC    : ");
  Serial.println(ldrValue);


  if (temperature > 34 || ldrValue < 300) {
    digitalWrite(relayPin, LOW);  // Aktifkan Relay (Active-Low)
    digitalWrite(ledPin, HIGH);   // Nyalakan LED
    Serial.println("Peringatan: Aktuator Aktif!");
  } else {
    digitalWrite(relayPin, HIGH); // Matikan Relay
    digitalWrite(ledPin, LOW);    // Matikan LED
    Serial.println("Kondisi Aman");
  }
}