// =====================================================
// ARDUINO 2 - Bay Sensing (I2C SLAVE, 0x09)
//   One HC-SR04 ultrasonic sensor: is the loading bay occupied?
//   Optional push button (pin 8): simulates "gate swing path blocked".
//
// Reply to master = 2 bytes (unchanged, so Arduino 3 needs no edits):
//   byte 0: status  (bit0 = bay occupied, bit1 = path blocked)
//   byte 1: ~status (integrity check)
// =====================================================
#include <Wire.h>

#define MY_ADDRESS 0x09

const int trigPin   = 6;
const int echoPin   = 7;
const int buttonPin = 8;   // button between pin 8 and GND (uses internal pull-up)

const int bayThresholdCm = 10;   // closer than this = bay occupied

// Set true only if you wire a button on pin 8 for the swing-path check.
const bool USE_PATH_BUTTON = true;

// false: no echo = nothing close = bay free (works with open space behind the bay)
// true : no echo = treated as sensor fault -> reports bay OCCUPIED (fail-safe),
//        but needs a wall/backdrop within range so an empty bay still gives an echo.
const bool NO_ECHO_MEANS_OCCUPIED = false;

volatile byte statusByte = 0;

// Returns distance in cm, or -1 if no echo
long readDistanceCm() {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  long duration = pulseIn(echoPin, HIGH, 30000UL);
  if (duration == 0) return -1;
  return (long)(duration * 0.0343 / 2);
}

void requestEvent() {
  byte s = statusByte;
  byte msg[2] = { s, (byte)~s };
  Wire.write(msg, 2);
}

void setup() {
  Serial.begin(9600);
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);
  pinMode(buttonPin, INPUT_PULLUP);

  Wire.begin(MY_ADDRESS);
  Wire.onRequest(requestEvent);

  Serial.println("Arduino 2 - Bay Sensing (ultrasonic)");
  Serial.println("I2C Address: 0x09");
}

void loop() {
  long d = readDistanceCm();

  bool bayOccupied;
  if (d == -1) bayOccupied = NO_ECHO_MEANS_OCCUPIED;
  else         bayOccupied = (d < bayThresholdCm);

  bool pathBlocked = USE_PATH_BUTTON && (digitalRead(buttonPin) == LOW);
 
  statusByte = (bayOccupied ? 0x01 : 0x00) | (pathBlocked ? 0x02 : 0x00);

  Serial.print("Distance: ");
  if (d == -1) Serial.print("No echo");
  else { Serial.print(d); Serial.print(" cm"); }
  Serial.print(" | Bay occupied: ");
  Serial.print(bayOccupied ? "YES" : "NO");
  Serial.print(" | Path blocked: ");
  Serial.println(pathBlocked ? "YES" : "NO");

  delay(150);
}
