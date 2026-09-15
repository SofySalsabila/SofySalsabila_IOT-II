#include <DHT.h>

const int ldrPin = 34;
const int dhtPin = 13;
const int relayPin = 26;
const int ledPin = 5;

#define DHTTYPE DHT22

DHT dht(dhtPin, DHTTYPE);

void setup() {
  Serial.begin(115200);

  dht.begin();

  pinMode(relayPin, OUTPUT);
  pinMode(ledPin, OUTPUT);

  digitalWrite(relayPin, HIGH);
  digitalWrite(ledPin, LOW);

  Serial.println("--------------------------------");
  Serial.println(" SMART WAREHOUSE");
  Serial.println("--------------------------------");
}

void loop() {
  delay(2500);

  int ldrValue = analogRead(ldrPin);

  float temperature = dht.readTemperature();
  float humidity = dht.readHumidity();

 
  if (isnan(temperature) || isnan(humidity)) {
    Serial.println("Gagal membaca data DHT!");
    return;
  }
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

    digitalWrite(relayPin, LOW);
    digitalWrite(ledPin, HIGH);

    Serial.println("Peringatan: Aktuator Aktif!");

  } else {

    digitalWrite(relayPin, HIGH);
    digitalWrite(ledPin, LOW);

    Serial.println("Kondisi Aman");
  }
}