#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <DHT.h>

const char* ssid = "sofy";
const char* password = "12345678";

// Konfigurasi Pin Hardware
const byte dhtPin = 2;        // D4 (GPIO 2) - DHT22
const byte buttonPin = 4;     // D2 (GPIO 4) - Push Button
const byte ledPin = 12;       // D6 (GPIO 12) - LED Digital ON/OFF
const byte pwmLedPin = 5;     // D1 (GPIO 5) - [TUGAS 4.7] LED Dimmer PWM Baru

DHT dht(dhtPin, DHT22);

// Variabel Pelacak Status (State & Cache)
bool ledState = false;
int pwmValue = 0;             // [TUGAS 4.7] Menampung nilai PWM (0 - 1023)
String currentTemp = "--";
String currentHum = "--";     // [LATIHAN 4] Data kelembapan

int buttonState;
int lastButtonState = LOW;
unsigned long lastDebounceTime = 0;
const unsigned long debounceDelay = 50;
unsigned long lastTime = 0;

// Inisialisasi Async Web Server (port 80) & WebSocket (rute /ws)
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

// ---------------- HTML & JAVASCRIPT (FRONT-END) ----------------
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>Smart Room IoT Dashboard</title>
  <style>
    body { font-family: Arial, sans-serif; text-align: center; background-color: #f4f4f9; margin: 0; padding: 20px; }
    h1 { color: #333; }
    .card { background: white; margin: 15px auto; padding: 20px; max-width: 320px; border-radius: 12px; box-shadow: 0 4px 8px rgba(0,0,0,0.1); }
    button { padding: 12px 28px; font-size: 18px; border: none; border-radius: 6px; cursor: pointer; color: white; font-weight: bold; }  
    .btn-on { background-color: #4CAF50; }  
    .btn-off { background-color: #f44336; }  
    .slider { width: 90%; margin: 15px 0; }
  </style>
</head>
<body>
  <h1>Smart Room Control</h1>
  
  <div class="card">
    <h2>Suhu: <span id="tempValue">--</span> Celcius</h2>
  </div>

  <!-- [LATIHAN 4] Kartu Kelembapan -->
  <div class="card">
    <h2>Kelembapan: <span id="humValue">--</span> %</h2>
  </div>

  <div class="card">
    <h2>LED Sakelar: <span id="ledStatus">OFF</span></h2>
    <button id="toggleBtn" class="btn-off" onclick="toggleLed()">Turn ON</button>
  </div>

  <!-- [TUGAS 4.7] Kartu Slider PWM Dimmer -->
  <div class="card">
    <h2>LED Dimmer (PWM)</h2>
    <p>Intensitas: <span id="pwmText">0</span> / 1023</p>
    <input type="range" min="0" max="1023" value="0" id="pwmSlider" class="slider" oninput="sendPWM(this.value)">
  </div>

  <script>
    var gateway = `ws://${window.location.hostname}/ws`;
    var websocket;

    window.addEventListener('load', onLoad);
    function onLoad(event) { initWebSocket(); }

    function initWebSocket() {
      websocket = new WebSocket(gateway);
      websocket.onopen    = onOpen;
      websocket.onclose   = onClose;
      websocket.onmessage = onMessage;
    }

    function onOpen(event) { console.log('WebSocket Terkoneksi'); }
    function onClose(event) { setTimeout(initWebSocket, 2000); }

    function toggleLed() {  
      websocket.send('toggle');
    }

    // [TUGAS 4.7] Mengirim string "pwm,NILAI" ke NodeMCU saat slider digeser
    function sendPWM(val) {
      document.getElementById('pwmText').innerHTML = val;
      websocket.send('pwm,' + val);
    }

    function onMessage(event) {
      var dataObj = JSON.parse(event.data);  
        
      if(dataObj.suhu !== undefined) {  
         document.getElementById('tempValue').innerHTML = dataObj.suhu;  
      }  
        
      if(dataObj.hum !== undefined) {  
         document.getElementById('humValue').innerHTML = dataObj.hum;  
      }  
        
      if(dataObj.led !== undefined) {  
         var btn = document.getElementById('toggleBtn');  
         var status = document.getElementById('ledStatus');  
         if(dataObj.led == "1"){  
           status.innerHTML = "ON";  
           btn.innerHTML = "Turn OFF";  
           btn.className = "btn-on";  
         } else {  
           status.innerHTML = "OFF";  
           btn.innerHTML = "Turn ON";  
           btn.className = "btn-off";  
         }  
      }

      if(dataObj.pwm !== undefined) {
         document.getElementById('pwmSlider').value = dataObj.pwm;
         document.getElementById('pwmText').innerHTML = dataObj.pwm;
      }
    }  
  </script>  
</body>  
</html>  
)rawliteral";

// ---------------- BACK-END & WEBSOCKET LOGIC ----------------

void notifyClients() {
  String jsonString = "{\"led\":\"" + String(ledState ? 1 : 0) + "\", ";
  jsonString += "\"suhu\":\"" + currentTemp + "\", ";
  jsonString += "\"hum\":\"" + currentHum + "\", ";
  jsonString += "\"pwm\":\"" + String(pwmValue) + "\"}";
  ws.textAll(jsonString);
}

// [TUGAS 4.7] Parsing pesan WebSocket yang masuk dari web
void handleWebSocketMessage(void *arg, uint8_t *data, size_t len) {
  AwsFrameInfo *info = (AwsFrameInfo*)arg;
  if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
    data[len] = 0;
    String message = String((char*)data);

    // Sakelar LED Digital
    if (message == "toggle") {
      ledState = !ledState;
      notifyClients();
    }
    // Slider Dimmer PWM (Format: "pwm,ANGKA")
    else if (message.startsWith("pwm,")) {
      String valueStr = message.substring(4); // Mengambil angka setelah "pwm,"
      pwmValue = valueStr.toInt();            // Konversi String ke Integer
      analogWrite(pwmLedPin, pwmValue);       // Mengatur redup-terang LED pin D1
      notifyClients();
    }
  }
}

void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type,
             void *arg, uint8_t *data, size_t len) {
  switch (type) {
    case WS_EVT_CONNECT:
      Serial.printf("Client WebSocket #%u terhubung\n", client->id());
      notifyClients();
      break;
    case WS_EVT_DISCONNECT:
      Serial.printf("Client WebSocket #%u terputus\n", client->id());
      break;
    case WS_EVT_DATA:
      handleWebSocketMessage(arg, data, len);
      break;
  }
}

void setup() {
  Serial.begin(115200);

  pinMode(buttonPin, INPUT);
  pinMode(ledPin, OUTPUT);
  pinMode(pwmLedPin, OUTPUT); // Pin LED Dimmer PWM

  digitalWrite(ledPin, LOW);
  analogWrite(pwmLedPin, 0);  // LED PWM mati di awal

  dht.begin();

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) { delay(500); Serial.print("."); }
  Serial.println("\nIP Address: " + WiFi.localIP().toString());

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/html", index_html);
  });

  ws.onEvent(onEvent);
  server.addHandler(&ws);
  server.begin();
}

void loop() {
  ws.cleanupClients();

  // 1. Eksekusi LED Sakelar (D6)
  digitalWrite(ledPin, ledState ? HIGH : LOW);

  // 2. Baca Tombol Fisik (Debounce)
  int reading = digitalRead(buttonPin);
  if (reading != lastButtonState) {
    lastDebounceTime = millis();
  }
  if ((millis() - lastDebounceTime) > debounceDelay) {
    if (reading != buttonState) {
      buttonState = reading;
      if (buttonState == HIGH) {
        ledState = !ledState;
        notifyClients();
      }
    }
  }
  lastButtonState = reading;

  // 3. Baca Sensor Suhu & Kelembapan (Non-blocking millis)
  if ((millis() - lastTime) > 3000) {
    float t = dht.readTemperature();
    float h = dht.readHumidity();
    
    if(!isnan(t) && !isnan(h)) {
      currentTemp = String(t);
      currentHum = String(h);
      notifyClients();
    }
    lastTime = millis();
  }
}