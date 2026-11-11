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

int faderValue = 0;

// Capacitive touch.
//
// The reported value is the sum of the last CAP_WINDOW single-sample
// measurements: 30 samples, the same scale as one capacitiveSensor(30)
// reading, so the touch threshold on the Pi is unchanged.
//
// One sample is taken every loop instead of thirty at once every fifth
// loop. Thirty at once blocks for several milliseconds (longer still
// while the platter is touched), and nothing else is sampled or sent
// meanwhile: the fader went out in bursts of four packets followed by a
// hole. One sample costs well under 0.1ms, so the loop - and with it
// the fader - runs several times faster and on an even clock, and the
// touch value refreshes every loop too.
//
// The speed matters for a second reason: the CH340 USB bridge hands
// data to the Pi in 32-byte blocks, so the more packets per second we
// send, the less time each one waits in the bridge for its block to
// fill.
#define CAP_WINDOW 30

unsigned int capSample[CAP_WINDOW];  // most recent single-sample readings
byte capIndex = 0;                   // slot the next reading goes in
unsigned long capSum = 0;            // running sum of capSample[]

void setup() {
  for (int i = 0; i < NUM_BUTTONS; i++)
    pinMode(buttonPins[i], INPUT_PULLUP);

  Serial.begin(500000);
  capSensor.set_CS_AutocaL_Millis(0xFFFFFFFF);

  faderValue = analogRead(faderPin);
}

void loop() {
  if (Serial.available() > 0) {
    byte incoming = Serial.read();
    if (incoming == HANDSHAKE_MAGIC) {
      Serial.write('T');
      handshakeCompleted = true;
    }
  }

  // Read buttons every loop (~20us)
  byte buttons = 0;
  for (int i = 0; i < NUM_BUTTONS; i++) {
    if (digitalRead(buttonPins[i]) == LOW)
      buttons |= (1 << i);
  }

  unsigned int capValue = (capSum > 65535UL) ? 65535U : (unsigned int)capSum;

  // faderValue was read during the previous packet's transmission, so
  // it is only a fraction of a millisecond old
  byte packet[7];
  packet[0] = SYNC_BYTE;
  packet[1] = (faderValue >> 8) & 0xFF;
  packet[2] = faderValue & 0xFF;
  packet[3] = (capValue >> 8) & 0xFF;
  packet[4] = capValue & 0xFF;
  packet[5] = buttons;
  packet[6] = packet[1] ^ packet[2] ^ packet[3] ^ packet[4] ^ packet[5];

  Serial.write(packet, 7);

  // Convert the fader for the next packet while this one goes out
  // (the conversion takes ~110us, the packet 140us on the wire)
  faderValue = analogRead(faderPin);

  // Let the packet finish before measuring touch, so the transmit
  // interrupt cannot fire in the middle of the measurement and disturb
  // the count. It also leaves a pause on the wire between packets,
  // which the CH340 bridge needs: fed several packets back to back at
  // 500k baud it dropped the tail of about 5% of them.
  Serial.flush();

  // One touch sample, replacing the oldest in the running sum
  long raw = capSensor.capacitiveSensor(1);
  unsigned int sample = (raw > 65535L) ? 65535U : (raw < 0 ? 0 : (unsigned int)raw);

  capSum -= capSample[capIndex];
  capSample[capIndex] = sample;
  capSum += sample;
  if (++capIndex >= CAP_WINDOW)
    capIndex = 0;
}
