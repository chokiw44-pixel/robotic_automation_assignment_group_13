// =====================================================
// ARDUINO 3 - Gate Controller (I2C MASTER)
//   Polls Arduino 1 (0x08, truck) and Arduino 2 (0x09, bay + path).
//   Gate opens ONLY when: truck waiting AND bay free AND path clear
//   AND both nodes replied with valid data.
//   Anything else -> gate closed (fail-safe).
//
// Pins: servo 9, green LED 5, red LED 6, buzzer 8
// =====================================================
#include <Wire.h> //setup wire is I2C library
#include <Servo.h>

#define ADDR_TRUCK 0x08 //I2C address it shows which board it is talking to 
#define ADDR_BAY   0x09

//pins
const int servoPin    = 9;
const int greenLedPin = 5;
const int redLedPin   = 6;
const int buzzerPin   = 8;

const int GATE_CLOSED = 90; //servo angle, setting the gate position
const int GATE_OPEN   = 0;

// Gate always closes on a bad read; the FAULT indication only latches
// after this many consecutive bad reads from the same node (filters glitches).
const byte FAULT_LIMIT = 2; //how many it takes bad reading to give alarm and raises fault alarm
const unsigned long FAULT_BEEP_INTERVAL_MS = 1000; //gap between alarm beeps 1000ms

//create servo object name
Servo gateServo;

//state machine enum is enumeration its a user-defined data type to assign human-readable maes to a set of realatable integer constants
enum State { S_START, S_IDLE, S_OPEN, S_HOLD_BAY, S_HOLD_PATH, S_FAULT };
State currentState = S_START; //used at reboot
// byte as a tiny storage box that holds 8 individual on/off switches (bits). Each switch can be either a 0 or a 1.
byte truckFaults = 0; //counts consecutive failed readings from Arduino 1
byte bayFaults   = 0; //counts consecutive failed readings from Arduino 2
unsigned long lastFaultBeep = 0; //stores the time when the last fault beep was started

// ---------- helpers ---------- clears any unread bytes, so it doesnot interfare in later reading
void flushWire() {
  while (Wire.available()) Wire.read();
}

// Arduino 1 reply: 1 byte, must be 0 or 1
bool readTruck(bool &truck) {
  byte n = Wire.requestFrom((uint8_t)ADDR_TRUCK, (uint8_t)1); //Arduino 3 requests one byte from address 0x08
  if (n != 1 || Wire.available() != 1) { flushWire(); return false; } //response should be exactly one byte
  byte v = Wire.read(); //received byte is stored in v
  if (v > 1) return false; //reject the reading
  truck = (v == 1); //if v - 1 truck is true and if v is 0 truck is false
  return true;
}

// Arduino 2 reply: 2 bytes (status, ~status)
bool readBay(bool &bayOccupied, bool &pathBlocked) {
  byte n = Wire.requestFrom((uint8_t)ADDR_BAY, (uint8_t)2); //Arduino 3 asks address 0x09 for two bytes.
  if (n != 2 || Wire.available() != 2) { flushWire(); return false; } //expected two bytes are not received, the reading is rejected
  byte s   = Wire.read();
  byte inv = Wire.read(); //need second byte to check if the first byte is recieved consistently
  if ((byte)(s ^ inv) != 0xFF) return false;  // integrity check failed ----error-detection check
  if (s & 0xFC) return false;                 // unexpected bits set
  bayOccupied = (s & 0x01);
  pathBlocked = (s & 0x02);
  return true;
}
//Controlling the servo and LEDs
void setOutputs(int gatePos, bool green, bool red) { //setOutputs(GATE_OPEN, true, false)
  gateServo.write(gatePos); //command servo to move
  digitalWrite(greenLedPin, green ? HIGH : LOW); //If green is true, the LED receives HIGH and turns on. Otherwise, it receives LOW and turns off.
  digitalWrite(redLedPin,   red   ? HIGH : LOW);
}

// function defines what the system should do when it enters a particular state. Runs once each time the state changes
void enterState(State s) { 
  switch (s) {
    case S_OPEN:
      setOutputs(GATE_OPEN, true, false); //commands the gate to open, turns on the green LED, and turns off the red LED.
      tone(buzzerPin, 1200, 200); //buzzer produces a short 1,200 Hz tone for 200 milliseconds.
      Serial.println(">> GATE OPEN (truck waiting, bay free, path clear)");
      break;
    case S_IDLE:
      setOutputs(GATE_CLOSED, false, true);
      noTone(buzzerPin);
      Serial.println(">> GATE CLOSED (no truck)");
      break;
    case S_HOLD_BAY:
      setOutputs(GATE_CLOSED, false, true);
      tone(buzzerPin, 700, 150);
      Serial.println(">> HOLD: loading bay occupied");
      break;
    case S_HOLD_PATH:
      setOutputs(GATE_CLOSED, false, true);
      tone(buzzerPin, 700, 150);
      Serial.println(">> HOLD: gate swing path blocked");
      break;
    case S_FAULT:
      setOutputs(GATE_CLOSED, false, true);
      Serial.println(">> FAULT: sensor node not responding - gate held CLOSED");
      break;
    default:
      break;
  }
}

// ---------- setup / loop ----------
void setup() {
  Serial.begin(9600); //Start serial communication

  Wire.begin();  // initialises I2C communication. Arduino 3 uses it to request data from the two other boards.
#if defined(WIRE_HAS_TIMEOUT)
  Wire.setWireTimeout(3000, true);  // stop a stuck bus from freezing the gate
#endif
  //tell Arduino that pins 5, 6 and 8 will send signals to devices
  gateServo.attach(servoPin);
  pinMode(greenLedPin, OUTPUT);
  pinMode(redLedPin, OUTPUT);
  pinMode(buzzerPin, OUTPUT);

  setOutputs(GATE_CLOSED, false, true);  // start closed
  Serial.println("Arduino 3 ready - gate closed");
}

void loop() {
  // Create variables for the sensor results
  bool truck = false, bayOccupied = false, pathBlocked = false;
  //Request data from both Arduinos
  bool truckOk = readTruck(truck);
  bool bayOk   = readBay(bayOccupied, pathBlocked);

  truckFaults = truckOk ? 0 : min(truckFaults + 1, 255); //If truckOk is true, reset truckFaults to zero. Otherwise, increase truckFaults by one, up to a maximum of 255.
  bayFaults   = bayOk   ? 0 : min(bayFaults + 1, 255);

  if (!truckOk) Serial.println("WARN: bad/no reply from Arduino 1 (truck sensor)"); //! means not valid
  if (!bayOk)   Serial.println("WARN: bad/no reply from Arduino 2 (bay/path sensors)");

  State next; //Decide the next state
  if (!truckOk || !bayOk) { //Did either Arduino return invalid data? || is or
    // Never guess: closed on any bad read; FAULT shown once it persists.
    bool latched = (truckFaults >= FAULT_LIMIT) || (bayFaults >= FAULT_LIMIT); //If either failure counter reaches 2, latched becomes true.
    next = latched ? S_FAULT : S_IDLE; //Both states command the gate closed.
  } else if (pathBlocked) { //Is the gate's swing path blocked?
    next = S_HOLD_PATH;           // also closes an open gate if something enters the swing path
  } else if (truck) { //Is a truck waiting?
    next = bayOccupied ? S_HOLD_BAY : S_OPEN; //gate open
  } else {
    next = S_IDLE; //gate close
  }
  //Apply the state only when it changes
  if (next != currentState) {
    currentState = next;
    enterState(currentState);
  }

  // Repeat the fault beep and wait
  if (currentState == S_FAULT && millis() - lastFaultBeep >= FAULT_BEEP_INTERVAL_MS) {
    lastFaultBeep = millis();
    tone(buzzerPin, 500, 200);
  }

  delay(1500); //The program pauses for (1.5 seconds) before starting the next loop iteration.
}
