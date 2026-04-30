# SmartSan Data Contract

This document defines the software interface between the ESP32 firmware, the Blynk dashboard, and the local mock dashboard.

## Status Payload

| Field | Type | Unit | Source | Update Frequency | Description |
| --- | --- | --- | --- | --- | --- |
| `usageCount` | Integer | events | ESP32 state | After dispense and every 5 seconds | Total valid dispense events since reset |
| `remainingPumps` | Integer | pumps | ESP32 pump counter | After dispense and every 5 seconds | Estimated remaining dispense actions in current refill cycle |
| `remainingPercent` | Integer | % | ESP32 calculation | After dispense and every 5 seconds | Estimated sanitizer remaining percentage |
| `refillAlert` | Boolean | none | ESP32 threshold logic | On change and every 5 seconds | `true` when refill is required |
| `deviceState` | String | none | ESP32 state machine | On state change | Current software state |
| `deviceOnline` | Boolean | none | ESP32/Blynk connection | Every 5 seconds | Whether the device is connected |
| `systemEnabled` | Boolean | none | Blynk/local control | On change | Allows or blocks dispensing |
| `handDetected` | Boolean | none | IR sensor | During detection and state update | Whether a valid hand event is active |
| `lastDispenseAt` | String | time | ESP32 clock/runtime | After dispense | Last successful dispense timestamp or runtime label |
| `message` | String | none | ESP32 state machine | On state change | Human-readable dashboard message |

## State Values

Use these values consistently in firmware and dashboard displays:

- `IDLE`
- `HAND_DETECTED`
- `DISPENSING`
- `WAIT_STABILISE`
- `CHECK_REFILL`
- `UPDATE_STATUS`
- `REFILL_REQUIRED`
- `DISABLED`
- `ERROR`

## Thresholds and Timing

| Name | Default | Purpose |
| --- | --- | --- |
| `maxPumps` | `25` | Number of dispense actions expected from a full refill |
| `refillThresholdPumps` | `0` | Trigger refill alert when remaining pump count is at or below this value |
| `dispenseLockoutMs` | `2000 ms` | Prevent repeated dispensing from one hand gesture |
| `stabiliseDelayMs` | `1200 ms` | Wait after servo movement before reading weight |
| `statusIntervalMs` | `5000 ms` | Regular idle dashboard update interval |

## Alert Logic

The basic refill condition is:

```text
refillAlert = remainingPumps <= refillThresholdPumps
```

The remaining percentage is:

```text
remainingPercent = clamp((remainingPumps / maxPumps) * 100, 0, 100)
```

## Control Inputs

| Control | Source | Expected Value | Behaviour |
| --- | --- | --- | --- |
| `manualDispense` | Blynk button or mock dashboard | `1` when pressed | Runs one dispense cycle if `systemEnabled = true` |
| `resetAlert` | Blynk button or mock dashboard | `1` when pressed | Clears the visible alert after refill/testing |
| `systemEnabled` | Blynk switch or mock dashboard | `0` or `1` | Disables or enables automatic and manual dispensing |

## Hardware-Abstraction Rule

Before hardware arrives, firmware functions may return simulated values. After hardware arrives, the hardware adapter functions should remain isolated:

- `readHandDetected()`
- `performDispense()`
- `readRemainingPumps()` (or future `readStableWeight()` when HX711 is reintroduced)

The state machine, Blynk virtual pin mapping, and dashboard fields should remain unchanged.
