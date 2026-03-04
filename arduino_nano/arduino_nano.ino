#include <CapacitiveSensor.h>

#define encoderPinA 2
#define encoderPinB 3
#define faderPin A5

#define sendPin 10  // Capacitive sensor send pin
#define sensePin 12 // Capacitive sensor sense pin

CapacitiveSensor capSensor = CapacitiveSensor(sendPin, sensePin);

volatile long encoderValue = 0; // Use long to handle large counts
volatile byte lastEncoded = 0;

const int CPR = 2400; // Counts Per Revolution (600 PPR encoder with 4x quadrature decoding)

// Binary protocol: [0xAA] [faderHi] [faderLo] [angleHi] [angleLo] [capHi] [capLo] [checksum]
// Total: 8 bytes per packet. Checksum = XOR of bytes 1-6.
#define SYNC_BYTE 0xAA
#define HANDSHAKE_MAGIC 0x53 // 'S' for ScratchTJ

bool handshakeCompleted = false;

void setup() {
  pinMode(encoderPinA, INPUT_PULLUP); // Use internal pull-up resistors
  pinMode(encoderPinB, INPUT_PULLUP);

  attachInterrupt(digitalPinToInterrupt(encoderPinA), updateEncoder, CHANGE);
  attachInterrupt(digitalPinToInterrupt(encoderPinB), updateEncoder, CHANGE);

  Serial.begin(500000); // 500kbaud for minimal serial latency

  // Optional: Adjust capacitive sensor auto-calibration
  capSensor.set_CS_AutocaL_Millis(0xFFFFFFFF); // Disable auto-calibration
}

unsigned long previousMillis = 0;
const unsigned long interval = 5; // 5ms = 200Hz update rate

void loop() {
  // Handle handshake: Pi sends 'S', we reply 'T' to confirm link
  if (Serial.available() > 0) {
    byte incoming = Serial.read();
    if (incoming == HANDSHAKE_MAGIC) {
      Serial.write('T'); // Reply with 'T' for ScratchTJ
      handshakeCompleted = true;
    }
  }

  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;

    int faderValue = analogRead(faderPin);

    // Ensure atomic access to encoderValue
    long currentEncoderValue;
    noInterrupts();
    currentEncoderValue = encoderValue;
    interrupts();

    // Raw encoder position in 0 to CPR-1 range (0-2399)
    int angle = (int)(((currentEncoderValue % CPR) + CPR) % CPR);

    // Read capacitive sensor value (clamped to uint16 range)
    long rawCap = capSensor.capacitiveSensor(30);
    unsigned int capacitiveValue = (rawCap > 65535) ? 65535 : (rawCap < 0 ? 0 : (unsigned int)rawCap);

    // Send binary packet: [SYNC] [fHi] [fLo] [aHi] [aLo] [cHi] [cLo] [XOR checksum]
    byte packet[8];
    packet[0] = SYNC_BYTE;
    packet[1] = (faderValue >> 8) & 0xFF;
    packet[2] = faderValue & 0xFF;
    packet[3] = (angle >> 8) & 0xFF;
    packet[4] = angle & 0xFF;
    packet[5] = (capacitiveValue >> 8) & 0xFF;
    packet[6] = capacitiveValue & 0xFF;
    packet[7] = packet[1] ^ packet[2] ^ packet[3] ^ packet[4] ^ packet[5] ^ packet[6];

    Serial.write(packet, 8);
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
