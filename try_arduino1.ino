#include <Wire.h>

// =========================
// I2C SETTINGS
// =========================
#define MY_ADDRESS 0x08

// =========================
// HC-SR04 PINS
// =========================
const int trigPin = 9;
const int echoPin = 10;

// Truck detection distance
const int detectionThresholdCm = 10;

// Stores truck detection status
volatile byte truckPresent = 0;


// =========================
// SETUP
// =========================
void setup() {

  Serial.begin(9600);

  // HC-SR04 pins
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);

  // Start Arduino 1 as I2C slave
  Wire.begin(MY_ADDRESS);

  // When Arduino 3 asks for data
  Wire.onRequest(requestEvent);

  Serial.println("Arduino 1 - Truck Detection");
  Serial.println("I2C Address: 0x08");
}


// =========================
// READ DISTANCE
// =========================
long readDistanceCm() {

  // Send trigger pulse
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);

  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);

  digitalWrite(trigPin, LOW);

  // Measure echo time
  long duration = pulseIn(echoPin, HIGH, 30000UL);

  // No echo received
  if (duration == 0) {
    return -1;
  }

  // Convert time to distance in cm
  long distance = duration * 0.0343 / 2;

  return distance;
}


// =========================
// SEND DATA TO ARDUINO 3
// =========================
void requestEvent() {

  // Send:
  // 1 = truck detected
  // 0 = no truck

  Wire.write(truckPresent);
}


// =========================
// MAIN LOOP
// =========================
void loop() {

  long distance = readDistanceCm();

  // Check whether truck is within 20 cm
  bool present = (distance > 0 &&
                  distance < detectionThresholdCm);

  if (present) {
    truckPresent = 1;
  } 
  else {
    truckPresent = 0;
  }


  // Display information in Serial Monitor
  Serial.print("Distance: ");

  if (distance == -1) {
    Serial.print("No reading");
  } 
  else {
    Serial.print(distance);
    Serial.print(" cm");
  }

  Serial.print(" | Truck: ");

  if (truckPresent == 1) {
    Serial.println("DETECTED");
  } 
  else {
    Serial.println("NOT DETECTED");
  }


  // Small delay before next reading
  delay(150);
}