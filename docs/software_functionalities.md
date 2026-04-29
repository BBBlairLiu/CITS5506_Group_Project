# SmartSan Software Functionalities

This document summarises the software functionalities for the current SmartSan design.

## Current Design Context

The current SmartSan prototype is based on:
- ESP32 as the main controller
- IR sensor for hand detection
- Servo motor for bottle pressing
- pump-count estimation for sanitizer level monitoring (HX711 can be added later)
- Blynk dashboard as the primary mobile interface
- Local browser interface as a mock/demo fallback

The current direction is to:
- use pump-count as the current level-monitoring method
- keep weight sensor integration as a future enhancement
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

### 4. Remaining Pump Tracking
The system tracks how many dispense actions are likely left in the current refill cycle.

**Purpose**
- provide a practical remaining-level estimate before full weight-sensor integration

**Input**
- successful dispense count
- configured `MAX_PUMPS` per refill

**Output**
- `remainingPumps`
- `remainingPercent`

---

### 5. Dispense Lockout and Stabilisation Window
The system applies lockout and delay logic to avoid repeated dispensing from one gesture.

**Purpose**
- prevent accidental rapid re-triggering
- keep event counting reliable during servo motion

**Logic**
- lockout period after dispense
- state transition delay before refill check

**Output**
- one counted event per accepted dispense cycle

---

### 6. Sanitizer Level Monitoring
The remaining pump count is compared with a refill threshold to determine whether the sanitizer level is normal or low.

**Purpose**
- monitor remaining sanitizer level
- support condition-based refill alerts

**Input**
- `remainingPumps`
- `refillThresholdPumps`

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
- `CHECK_REFILL`
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

### 10. Web Monitoring Display and Sync Status
The browser page shows key monitoring information plus sync health for testing without hardware.

**Current Display Fields**
- usage count
- remaining pumps
- remaining percentage
- sanitizer status
- device state
- refill threshold
- system message
- sync status and event timeline

---

## Suggested MVP Workflow

The current minimum viable software flow is:

`IR detects hand -> servo presses bottle -> usage count updates -> remaining count is updated -> refill status is checked -> status is sent by Wi-Fi -> browser page displays result`

---

## Current Priority

The current software priority is:
1. hand detection
2. dispensing control
3. usage event recording
4. remaining pump tracking
5. lockout and stabilisation window
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
- pump-count based level monitoring
- refill warning
- browser-based monitoring

The main goal for this stage is to build a simple, reliable, and testable monitoring workflow before full hardware integration.