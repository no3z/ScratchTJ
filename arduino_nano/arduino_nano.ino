#include <CapacitiveSensor.h>

#define faderPin A5
#define sendPin 10
#define sensePin 12

#define NUM_BUTTONS 4
const int buttonPins[NUM_BUTTONS] = {A0, A1, A2, A3};

CapacitiveSensor capSensor = CapacitiveSensor(sendPin, sensePin);

// Packet: [0xAA] [fHi] [fLo] [cHi] [cLo] [buttons] [checksum]
#define SYNC_BYTE 0xAA
#define HANDSHAKE_MAGIC 0x53

bool handshakeCompleted = false;

// Cap sensor is read every CAP_SKIP loops, fader+buttons every loop
#define CAP_SKIP 5
unsigned int capValue = 0;  // Cached cap value, updated every CAP_SKIP loops
int loopCount = 0;

void setup() {
  for (int i = 0; i < NUM_BUTTONS; i++)
    pinMode(buttonPins[i], INPUT_PULLUP);

  Serial.begin(500000);
  capSensor.set_CS_AutocaL_Millis(0xFFFFFFFF);
}

void loop() {
  if (Serial.available() > 0) {
    byte incoming = Serial.read();
    if (incoming == HANDSHAKE_MAGIC) {
      Serial.write('T');
      handshakeCompleted = true;
    }
  }

  // Read fader every loop (~100us)
  int faderValue = analogRead(faderPin);

  // Read buttons every loop (~20us)
  byte buttons = 0;
  for (int i = 0; i < NUM_BUTTONS; i++) {
    if (digitalRead(buttonPins[i]) == LOW)
      buttons |= (1 << i);
  }

  // Read cap sensor only every CAP_SKIP loops (~3ms every 5th loop)
  if (++loopCount >= CAP_SKIP) {
    loopCount = 0;
    long rawCap = capSensor.capacitiveSensor(30);
    capValue = (rawCap > 65535) ? 65535 : (rawCap < 0 ? 0 : (unsigned int)rawCap);
  }

  // Send packet every loop -- fader is always fresh, cap may be cached
  byte packet[7];
  packet[0] = SYNC_BYTE;
  packet[1] = (faderValue >> 8) & 0xFF;
  packet[2] = faderValue & 0xFF;
  packet[3] = (capValue >> 8) & 0xFF;
  packet[4] = capValue & 0xFF;
  packet[5] = buttons;
  packet[6] = packet[1] ^ packet[2] ^ packet[3] ^ packet[4] ^ packet[5];

  Serial.write(packet, 7);
}
