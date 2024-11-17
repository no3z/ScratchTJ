#include <CapacitiveSensor.h>

#define encoderPinA 2
#define encoderPinB 3
#define faderPin A5

#define sendPin 10  // Capacitive sensor send pin
#define sensePin 12 // Capacitive sensor sense pin

CapacitiveSensor capSensor = CapacitiveSensor(sendPin, sensePin);

volatile long encoderValue = 0; // Use long to handle large counts
volatile byte lastEncoded = 0;

const int CPR = 2400; // Counts Per Revolution from datasheet

void setup() {
  pinMode(encoderPinA, INPUT_PULLUP); // Use internal pull-up resistors
  pinMode(encoderPinB, INPUT_PULLUP);

  attachInterrupt(digitalPinToInterrupt(encoderPinA), updateEncoder, CHANGE);
  attachInterrupt(digitalPinToInterrupt(encoderPinB), updateEncoder, CHANGE);

  Serial.begin(115200);

  // Optional: Adjust capacitive sensor auto-calibration
  capSensor.set_CS_AutocaL_Millis(0xFFFFFFFF); // Disable auto-calibration
}

unsigned long previousMillis = 0;
const unsigned long interval = 10; // Adjust the interval as needed

void loop() {
  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;

    int faderValue = analogRead(faderPin);

    // Ensure atomic access to encoderValue
    long currentEncoderValue;
    noInterrupts();
    currentEncoderValue = encoderValue;
    interrupts();

    // Map encoderValue to 0 - 4095 range
    int mapped_angle = (currentEncoderValue % CPR) * 4096 / CPR;
    if (mapped_angle < 0) mapped_angle += 4096; // Ensure positive value

    // Read capacitive sensor value
    long capacitiveValue = capSensor.capacitiveSensor(30); // Adjust sample size (30)

    // Send the values over serial
    Serial.print(faderValue);
    Serial.print(" ");
    Serial.print(mapped_angle);
    Serial.print(" ");
    Serial.println(capacitiveValue); // Add capacitive value to output
  }
}

void updateEncoder() {
  bool MSB = digitalRead(encoderPinA);
  bool LSB = digitalRead(encoderPinB);

  byte encoded = (MSB << 1) | LSB;
  byte sum = (lastEncoded << 2) | encoded;

  if (sum == 0b1101 || sum == 0b0100 || sum == 0b0010 || sum == 0b1011)
    encoderValue++;
  if (sum == 0b1110 || sum == 0b0111 || sum == 0b0001 || sum == 0b1000)
    encoderValue--;

  lastEncoded = encoded;
}
