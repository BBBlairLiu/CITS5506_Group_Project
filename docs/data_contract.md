# SmartSan Data Contract

This document defines the software interface between the ESP32 firmware, the Blynk dashboard, and the local mock dashboard.

## Status Payload

| Field | Type | Unit | Source | Update Frequency | Description |
| --- | --- | --- | --- | --- | --- |
| `usageCount` | Integer | events | ESP32 state | After dispense and every 5 seconds | Total valid dispense events since reset |
| `liquidWeightGrams` | Float | g | HX711 load cell | Every 5 seconds and after dispense | Estimated remaining liquid weight after empty-bottle tare |
| `remainingPercent` | Integer | % | ESP32 calculation | After measurement and after dispense | Estimated sanitizer remaining percentage from liquid weight |
| `distanceMm` | Integer | mm | VL53L1X distance sensor | Every 5 seconds and during detection | Corrected hand-distance reading |
| `lastDispenseGrams` | Float | g | ESP32 calculation | After dispense stabilisation | Estimated liquid used by the latest accepted dispense |
| `refillAlert` | Boolean | none | ESP32 threshold logic | On change and every 5 seconds | `true` when refill is required |
| `deviceState` | String | none | ESP32 state machine | On state change | Current software state |
| `deviceOnline` | Boolean | none | ESP32/Blynk connection | Every 5 seconds | Whether the device is connected |
| `systemEnabled` | Boolean | none | Blynk/local control | On change | Allows or blocks dispensing |
| `handDetected` | Boolean | none | VL53L1X distance range | During detection and state update | Whether a valid hand event is active |
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
| `fullLiquidGrams` | `600 g` | Weight used as the full-liquid calibration reference for the demo |
| `refillAlertPercent` | `10%` | Trigger refill alert when remaining percentage is at or below this value |
| `minHandDistanceMm` | `70 mm` | Lower bound of distance-trigger detection range |
| `maxHandDistanceMm` | `150 mm` | Upper bound of distance-trigger detection range |
| `dispenseLockoutMs` | `2000 ms` | Prevent repeated dispensing from one hand gesture |
| `stabiliseDelayMs` | `1200 ms` | Wait after servo movement before calculating post-dispense weight |
| `statusIntervalMs` | `5000 ms` | Regular idle dashboard update interval |

## Alert Logic

The remaining percentage is:

```text
remainingPercent = clamp((liquidWeightGrams / fullLiquidGrams) * 100, 0, 100)
```

The refill condition is:

```text
refillAlert = remainingPercent <= refillAlertPercent
```

The latest dispense amount is:

```text
lastDispenseGrams = max(previousLiquidWeightGrams - currentLiquidWeightGrams, 0)
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
- `readWeightMeasurement()`
- `sampleMeasurements()`

The state machine, Blynk virtual pin mapping, and dashboard fields should remain unchanged.
