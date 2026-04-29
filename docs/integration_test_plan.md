# SmartSan Hardware Integration Test Plan

Use this checklist when the ordered hardware and 3D printed part are available. Test each subsystem before assembling the full enclosure.

## Phase 1: ESP32 and Blynk

| Test | Method | Expected Result | Evidence |
| --- | --- | --- | --- |
| Wi-Fi connection | Upload the firmware with real Wi-Fi and Blynk credentials | Device appears online in Blynk | Screenshot of online widget |
| Virtual pin update | Run mock mode and send `d` in Serial Monitor | `usageCount`, `remainingPumps`, and `deviceState` update | Screenshot of Blynk dashboard |
| Control inputs | Press manual dispense, reset alert, and system enable switch | ESP32 state changes correctly | Short video or screenshots |

## Phase 2: IR Sensor

| Test | Method | Expected Result | Evidence |
| --- | --- | --- | --- |
| Basic detection | Print raw/digital IR reading in Serial Monitor | Hand presence is clearly detectable | Serial output screenshot |
| False trigger check | Leave the sensor idle for 2 minutes | No unexpected dispense event | Serial output or observation |
| Lockout check | Hold hand near sensor | Only one dispense event occurs per lockout period | Serial output or video |

## Phase 3: Servo Mechanism

| Test | Method | Expected Result | Evidence |
| --- | --- | --- | --- |
| Servo angle sweep | Test press and return angles without sanitizer bottle | Servo moves smoothly and returns | Short video |
| Bottle press | Mount bottle and trigger one press | One usable sanitizer output is produced | Short video |
| Power stability | Trigger repeated presses | ESP32 does not reset or disconnect | Observation notes |

## Phase 4: Pump Count and Refill Logic

| Test | Method | Expected Result | Evidence |
| --- | --- | --- | --- |
| Counter update | Trigger repeated dispense cycles in mock mode | `remainingPumps` decreases by one per accepted cycle | Serial output |
| Lockout safety | Hold trigger input active | Counter increases once per lockout window | Serial output or video |
| Refill threshold | Continue until `remainingPumps = 0` | `refillAlert` becomes true and state enters `REFILL_REQUIRED` | Dashboard screenshot |
| Alert reset | Trigger `resetAlert` from Blynk | Counter resets and alert clears | Dashboard screenshot |

## Phase 5: Full System

| Test | Method | Expected Result | Evidence |
| --- | --- | --- | --- |
| End-to-end dispense | Hand triggers IR sensor | Servo presses, usage count increments, remaining count updates, dashboard refreshes | Video of complete flow |
| Low sanitizer flow | Simulate depleted count (`remainingPumps = 0`) | Dashboard shows refill warning | Screenshot |
| Reset after refill | Refill and press reset alert | Status returns to normal | Screenshot |
| 3D printed enclosure test | Install all components in printed structure | IR alignment, servo motion, and counting/sync remain reliable | Video and notes |

## Recommended Integration Order

1. Keep the firmware in mock mode until Blynk widgets and virtual pins are confirmed.
2. Replace `readHandDetected()` with real IR logic and test without servo movement.
3. Replace `performDispense()` with real servo control and verify one cycle per trigger.
4. Validate count and alert logic with repeated trigger tests and reset operations.
5. Assemble the 3D printed structure only after the electronics work on the bench.
6. Recalibrate servo angles and load cell readings after final assembly.

## Demo Acceptance Criteria

The final demo is acceptable when one hand-triggered event can reliably produce this complete chain:

```text
IR detection -> servo press -> usage count +1 -> remaining count update -> refill decision -> Blynk/dashboard update
```

Collect at least one screenshot or short video for each major subsystem. These become useful evidence for the final report and presentation.
