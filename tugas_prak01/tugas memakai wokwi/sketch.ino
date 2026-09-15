const int buttonPin = 4;
const int ledPin = 5;

int buttonState = LOW;
int lastButtonState = LOW;
bool ledState = false;

void setup() {
  Serial.begin(115200);
  pinMode(buttonPin, INPUT);
  pinMode(ledPin, OUTPUT);
  digitalWrite(ledPin, LOW);
  Serial.println("Sistem Toggle Sakelar Siap!");
}

void loop() {
  buttonState = digitalRead(buttonPin);

  if (buttonState == HIGH && lastButtonState == LOW) {
    ledState = !ledState;
    digitalWrite(ledPin, ledState ? HIGH : LOW);

    if (ledState) {
      Serial.println("Tombol ditekan -> LED ON");
    } else {
      Serial.println("Tombol ditekan -> LED OFF");
    }

    delay(200);
  }

  lastButtonState = buttonState;
}