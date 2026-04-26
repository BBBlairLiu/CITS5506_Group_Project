# SmartSan Software Functionalities

This document summarises the software functionalities for the current SmartSan design.

## Current Design Context

The current SmartSan prototype is based on:
- ESP32 as the main controller
- IR sensor for hand detection
- Servo motor for bottle pressing
- Load cell with HX711 for sanitizer level monitoring
- Blynk dashboard as the primary mobile interface
- Local browser interface as a mock/demo fallback

The current direction is to:
- use the weight sensor as the primary level-monitoring method
- keep ToF only as a backup option
- focus first on a reliable monitoring workflow instead of advanced analytics
- use simulated sensor values before hardware arrives so the software flow can be tested early

---

## Main Software Functions

### 1. Hand Detection
The system continuously reads the IR sensor and decides whether a valid hand-detection event has occurred.

**Purpose**
- detect a user hand in the valid sensing range
- avoid false triggers from small fluctuations

**Input**
- IR sensor reading

**Output**
- `handDetected = true/false`

---

### 2. Dispensing Control
Once a valid hand is detected, the ESP32 triggers the servo motor to press the sanitizer bottle once and return to its initial position.

**Purpose**
- perform one valid dispense action
- prevent repeated dispensing from one gesture

**Input**
- `handDetected`

**Output**
- one dispense event
- servo press-and-return cycle completed

---

### 3. Usage Event Recording
Each successful dispense is recorded as one valid usage event.

**Purpose**
- track how many times the dispenser has been used
- connect physical action to monitoring data

**Input**
- dispense event

**Output**
- `usageCount++`
- latest usage event updated

---

### 4. Weight Reading
The system reads bottle weight through the HX711 module and load cell.

**Purpose**
- collect raw weight data for sanitizer level monitoring

**Input**
- HX711 raw readings

**Output**
- `rawWeight`
- `stableWeight`

---

### 5. Reading Stabilisation
The system applies basic stabilisation logic to reduce noise caused by bottle vibration during dispensing.

**Purpose**
- improve reliability of weight readings
- avoid false level updates during servo motion

**Logic**
- gated sampling after dispensing
- averaging multiple HX711 readings
- ignoring very small fluctuations as noise

**Output**
- stable processed weight value

---

### 6. Sanitizer Level Monitoring
The stable weight value is compared with a refill threshold to determine whether the sanitizer level is normal or low.

**Purpose**
- monitor remaining sanitizer level
- support condition-based refill alerts

**Input**
- `stableWeight`
- tare weight
- refill threshold

**Output**
- `sanitizerLow = true/false`

---

### 7. Refill Alert Logic
When the sanitizer level falls below the refill threshold, the system updates the warning state.

**Purpose**
- provide a clear low-sanitizer alert

**Input**
- `sanitizerLow`

**Output**
- `warningStatus = LOW/NORMAL`

---

### 8. Device State Management
The software manages the sequence of system states so the workflow happens in the correct order.

**Purpose**
- keep the workflow organised
- make debugging easier

**Example States**
- `IDLE`
- `HAND_DETECTED`
- `DISPENSING`
- `WAIT_STABILISE`
- `READ_WEIGHT`
- `UPDATE_STATUS`

---

### 9. Wi-Fi / Blynk Status Transmission
The ESP32 sends the latest system variables to Blynk and can also support the local browser mock interface during development.

**Purpose**
- make monitoring information visible through the web page

**Input**
- usage count
- sanitizer status
- device state

**Output**
- updated values available to the dashboard

---

### 10. Web Monitoring Display
The browser page shows the most important monitoring information for the current prototype.

**Current Display Fields**
- usage count
- current weight
- sanitizer status
- device state
- refill threshold
- system message

---

## Suggested MVP Workflow

The current minimum viable software flow is:

`IR detects hand -> servo presses bottle -> usage count updates -> weight is read and stabilised -> refill status is checked -> status is sent by Wi-Fi -> browser page displays result`

---

## Current Priority

The current software priority is:
1. hand detection
2. dispensing control
3. usage event recording
4. weight reading
5. reading stabilisation
6. sanitizer level monitoring
7. refill alert logic
8. Wi-Fi status transmission
9. web monitoring display

The implementation details are split across:
- `blynk_dashboard.md` for Blynk virtual pins and widgets
- `data_contract.md` for shared ESP32/dashboard fields
- `integration_test_plan.md` for hardware arrival and final demo testing

---

## Not the Current Priority

At this stage, the software should not focus too much on:
- advanced analytics
- prediction models
- too many admin-side functions
- full ToF integration

These can be considered later after the basic monitoring workflow is stable.

---

## Summary

The software for the current SmartSan design is intended to support:
- automatic hand detection
- controlled dispensing
- usage recording
- weight-based level monitoring
- refill warning
- browser-based monitoring

The main goal for this stage is to build a simple, reliable, and testable monitoring workflow before full hardware integration.