# SmartSan Hardware Integration Test Plan

Use this checklist when the ordered hardware and 3D printed part are available. Test each subsystem before assembling the full enclosure.

## Phase 1: ESP32 and Blynk

| Test | Method | Expected Result | Evidence |
| --- | --- | --- | --- |
| Wi-Fi connection | Upload the firmware with real Wi-Fi and Blynk credentials | Device appears online in Blynk | Screenshot of online widget |
| Virtual pin update | Run mock mode and send `d` in Serial Monitor | `usageCount`, `liquidWeightGrams`, `remainingPercent`, and `deviceState` update | Screenshot of Blynk dashboard |
| Control inputs | Press manual dispense, reset alert, and system enable switch | ESP32 state changes correctly | Short video or screenshots |

## Phase 2: Distance Sensor

| Test | Method | Expected Result | Evidence |
| --- | --- | --- | --- |
| Basic detection | Print corrected VL53L1X distance reading in Serial Monitor | Hand presence is clearly detectable between 70 mm and 150 mm | Serial output screenshot |
| False trigger check | Leave the sensor idle for 2 minutes | No unexpected dispense event | Serial output or observation |
| Lockout check | Hold hand near sensor | Only one dispense event occurs until the hand leaves and re-enters the detection range | Serial output or video |

## Phase 3: Servo Mechanism

| Test | Method | Expected Result | Evidence |
| --- | --- | --- | --- |
| Servo angle sweep | Test press and return angles without sanitizer bottle | Servo moves smoothly and returns | Short video |
| Bottle press | Mount bottle and trigger one press | One usable sanitizer output is produced | Short video |
| Power stability | Trigger repeated presses | ESP32 does not reset or disconnect | Observation notes |

## Phase 4: Weight and Refill Logic

| Test | Method | Expected Result | Evidence |
| --- | --- | --- | --- |
| Weight update | Trigger repeated dispense cycles in real hardware mode | `liquidWeightGrams` decreases after accepted cycles | Serial CSV output |
| Lockout safety | Hold trigger input active | Counter increases once per lockout window | Serial output or video |
| Refill threshold | Reduce measured liquid until `remainingPercent <= 10` | `refillAlert` becomes true and state enters `REFILL_REQUIRED` | Dashboard screenshot |
| Alert reset | Refill bottle and trigger `resetAlert` from Blynk | Weight is reread and alert clears when above threshold | Dashboard screenshot |

## Phase 5: Full System

| Test | Method | Expected Result | Evidence |
| --- | --- | --- | --- |
| End-to-end dispense | Hand enters distance trigger range | Servo presses, usage count increments, liquid weight updates, dashboard refreshes | Video of complete flow |
| Low sanitizer flow | Reduce measured liquid until `remainingPercent <= 10` | Dashboard shows refill warning | Screenshot |
| Reset after refill | Refill and press reset alert | Status returns to normal | Screenshot |
| 3D printed enclosure test | Install all components in printed structure | Distance sensor alignment, servo motion, weight readings, and sync remain reliable | Video and notes |

## Recommended Integration Order

1. Keep the firmware in mock mode until Blynk widgets and virtual pins are confirmed.
2. Switch to real hardware mode and verify VL53L1X distance readings without triggering the servo repeatedly.
3. Verify HX711 weight readings with empty bottle, known 600 ml reference, and repeated stable samples.
4. Verify `performDispense()` with real servo control and confirm one cycle per trigger.
5. Validate usage count, liquid weight, remaining percentage, and alert logic with repeated trigger tests and reset operations.
6. Assemble the 3D printed structure only after the electronics work on the bench.
7. Recalibrate servo angles and load cell readings after final assembly.

## Demo Acceptance Criteria

The final demo is acceptable when one hand-triggered event can reliably produce this complete chain:

```text
distance detection -> servo press -> usage count +1 -> weight update -> refill decision -> Blynk/dashboard update
```

Collect at least one screenshot or short video for each major subsystem. These become useful evidence for the final report and presentation.
