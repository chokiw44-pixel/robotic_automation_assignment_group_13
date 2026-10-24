# Automated Industrial Loading-Bay Access Gate (Arduino Prototype)

A small-scale, three-board Arduino Uno prototype of an automated loading-bay gate. It was built for the ICTE4005/5005 industrial automation and robotics report *"Distributed Sensing and Control for an Automated Industrial Loading-Bay Access Gate"* (Team 13, Curtin University).

The gate opens **only** when a truck is waiting, the loading bay is free, the swing path is clear, and both sensing boards have returned valid data. In every other case, the gate stays closed (fail-safe).

> **Prototype only.** This is a teaching demonstrator using hobby-grade parts (HC-SR04 ultrasonic sensors, a micro servo, a toy train as the "truck"). It is **not** safety-rated and must not be used to control a real gate. See [Known limitations](#known-limitations).

---

## How it works

Three Arduino Uno boards communicate over a single I²C bus. Arduino 3 is the master and polls the other two.

| Board | Role | I²C address | Sketch |
|---|---|---|---|
| Arduino 1 | Truck arrival detection (slave) | `0x08` | `try_arduino1.ino` |
| Arduino 2 | Bay occupancy + path-blocked button (slave) | `0x09` | `arduino2_bay_sensor.ino` |
| Arduino 3 | Gate controller / state machine (master) | – | `arduino3_master.ino` |

```
 [Arduino 1]  HC-SR04  ──┐
   0x08, truck           │
                         ├── I2C (SDA/SCL) ──► [Arduino 3 master]
 [Arduino 2]  HC-SR04  ──┤                        ├─ servo barrier
   0x09, bay + button    │                        ├─ green / red LEDs
                         ┘                        └─ buzzer
```

### Arduino 1 – truck detection
- Reads an HC-SR04 and reports **1** (truck present) if the distance is greater than 0 and below `detectionThresholdCm` (10 cm), otherwise **0**.
- Replies with a single byte when the master requests data.

### Arduino 2 – bay sensing and path check
- Reads an HC-SR04 to decide whether the bay is occupied (distance below `bayThresholdCm`, 10 cm).
- A push button (pin 8 to GND, internal pull-up) simulates "gate swing path blocked".
- Replies with **two bytes**: a status byte (`bit0` = bay occupied, `bit1` = path blocked) and its bitwise inverse, which the master uses as an integrity check.
- `NO_ECHO_MEANS_OCCUPIED` controls what a missing echo means. It defaults to `false`, i.e. no echo is treated as a free bay (see limitations).

### Arduino 3 – gate controller
- Polls both slaves and validates every reply: correct length, value range, inverse-byte check and no unexpected bits.
- Runs a simple state machine:

| State | Condition | Gate | Indication |
|---|---|---|---|
| `S_IDLE` | No truck (or a single bad read) | Closed | Red LED |
| `S_OPEN` | Truck waiting, bay free, path clear, data valid | Open | Green LED + short beep |
| `S_HOLD_BAY` | Truck waiting, bay occupied | Closed | Red LED + beep |
| `S_HOLD_PATH` | Path blocked | Closed | Red LED + beep |
| `S_FAULT` | `FAULT_LIMIT` (2) consecutive bad reads from a node | Closed | Red LED + repeating beep every 1 s |

- Any invalid or missing reply closes the gate. The `FAULT` state latches only after two consecutive failures, which filters out single glitches.

---

## Hardware

- 3 × Arduino Uno
- 2 × HC-SR04 ultrasonic sensor
- 1 × micro servo (barrier arm)
- 1 × green LED, 1 × red LED (with series resistors)
- 1 × buzzer
- 1 × push button (path-blocked simulation)
- Breadboards and jumper wires

### Pin map

| Board | Pin | Connected to |
|---|---|---|
| Arduino 1 | 9 | HC-SR04 TRIG |
| Arduino 1 | 10 | HC-SR04 ECHO |
| Arduino 2 | 6 | HC-SR04 TRIG |
| Arduino 2 | 7 | HC-SR04 ECHO |
| Arduino 2 | 8 | Push button to GND (internal pull-up) |
| Arduino 3 | 9 | Servo signal |
| Arduino 3 | 5 | Green LED |
| Arduino 3 | 6 | Red LED |
| Arduino 3 | 8 | Buzzer |

### I²C wiring
Connect **A4 (SDA)** to **A4**, **A5 (SCL)** to **A5**, and **GND** to **GND** across all three boards. Keep the wires short (the prototype used about 0.5 m). Power the servo from a suitable supply if it stalls or browns out the master.

---

## Getting started

1. Install the [Arduino IDE](https://www.arduino.cc/en/software). The sketches use only the built-in `Wire` and `Servo` libraries.
2. The Arduino IDE expects each sketch to sit in a folder with the same name, so arrange the files like this:
   ```
   arduino1_truck_sensor/arduino1_truck_sensor.ino   (from try_arduino1.ino)
   arduino2_bay_sensor/arduino2_bay_sensor.ino
   arduino3_master/arduino3_master.ino
   ```
3. Upload each sketch to its own Uno (select the correct board and port each time).
4. Wire the boards as above and power them up.
5. Open the Serial Monitor at **9600 baud** for each board to watch the readings and state changes.

### Quick test sequence
1. **No truck:** gate closed, red LED.
2. **Object within 10 cm of the Arduino 1 sensor:** truck detected, gate opens with a green LED and beep (if the bay is free and the path is clear).
3. **Object within 10 cm of the Arduino 2 sensor:** bay occupied, gate held closed.
4. **Press the button:** path blocked, gate closes or stays closed.
5. **Unplug an SDA wire:** two `WARN` messages, then `FAULT`, with the gate held closed and a repeating beep.

---

## Configuration

| Constant | File | Default | Meaning |
|---|---|---|---|
| `detectionThresholdCm` | Arduino 1 | `10` | Distance below which a truck is detected |
| `bayThresholdCm` | Arduino 2 | `10` | Distance below which the bay is occupied |
| `USE_PATH_BUTTON` | Arduino 2 | `true` | Enable the path-blocked button |
| `NO_ECHO_MEANS_OCCUPIED` | Arduino 2 | `false` | Treat a missing echo as occupied (fail-safe) |
| `GATE_OPEN` / `GATE_CLOSED` | Arduino 3 | `0` / `90` | Servo angles in degrees |
| `FAULT_LIMIT` | Arduino 3 | `2` | Consecutive bad reads before `FAULT` |
| `FAULT_BEEP_INTERVAL_MS` | Arduino 3 | `1000` | Fault beep interval |

---

## Known limitations

These are discussed in the accompanying report.

- **Missing ultrasonic echo can look like a free bay.** With the default `NO_ECHO_MEANS_OCCUPIED = false`, a disconnected or failed bay sensor can allow the gate to open. Setting it to `true` is safer, but needs a backdrop within range so an empty bay still returns an echo.
- **Slow response.** The master loop ends with `delay(1500)`, so the controller reacts only about every 1.5 s. The slave loops use `delay(150)`.
- **No physical position feedback.** The servo has no feedback, so the controller assumes the gate reached the commanded angle even if it is jammed.
- **Single-distance detection.** The system detects "something is there", not an authorised truck, and cannot tell a person or trolley from a vehicle.
- **I²C is short-range.** It is sensitive to noise and wiring faults and is unsuitable for a real industrial site without a more robust bus (the report suggests RS-485).
- **No independent safety circuit.** Safety logic runs on the same controller as the gate. An industrial version would need safety-rated sensors and a hard-wired, independent interlock (ISO 13849-1).

---

## Repository contents

| File | Description |
|---|---|
| `try_arduino1.ino` | Arduino 1 – truck detection (I²C slave `0x08`) |
| `arduino2_bay_sensor.ino` | Arduino 2 – bay occupancy and path button (I²C slave `0x09`) |
| `arduino3_master.ino` | Arduino 3 – master controller, state machine, servo/LED/buzzer |

## Authors

Team 13: Choki Wangmo, Mohammad Musharaf Shaikh, Devara Shashank, Curtin University, Perth, Australia.

## License

Add a license before publishing (for example MIT) if you want others to reuse the code. Without one, all rights are reserved by default.
