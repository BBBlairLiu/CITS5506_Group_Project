# SmartSan Software Test Checklist

This checklist focuses on software functionality that can be validated before full hardware integration.

## Dashboard and State Flow

| Test ID | Scenario | Steps | Expected Result |
| --- | --- | --- | --- |
| SW-01 | Sensor-triggered dispense | Click `Simulate Sensor Dispense` | State transitions through `HAND_DETECTED` -> `DISPENSING` -> `WAIT_STABILISE` -> `IDLE/REFILL_REQUIRED` |
| SW-02 | Manual dispense command | Click `Manual Dispense` | One dispense cycle is executed and `usageCount` increments by 1 |
| SW-03 | Single-cycle safety | Click dispense repeatedly during one running cycle | Only one active cycle runs, buttons remain disabled until cycle ends |
| SW-04 | Refill alert trigger | Trigger cycles until `remainingPumps` reaches threshold | `Sanitizer Status` changes to `LOW` and state moves to `REFILL_REQUIRED` |
| SW-05 | Alert reset path | Click `Reset Alert` | `remainingPumps` resets to max and warning returns to `NORMAL` |

## Sync and Failure Handling

| Test ID | Scenario | Steps | Expected Result |
| --- | --- | --- | --- |
| SW-06 | Healthy sync loop | Keep dashboard open for 15+ seconds | `Sync Status` remains `HEALTHY` and `Last Sync` keeps updating |
| SW-07 | Simulated sync failure | Enable `Simulate sync failure` toggle | `Sync Status` changes to `DEGRADED` and event timeline logs failure |
| SW-08 | Recovery after failure | Disable `Simulate sync failure` toggle | Sync returns to `HEALTHY`, device returns online, event timeline records recovery |

## Control Safety and Operator Flow

| Test ID | Scenario | Steps | Expected Result |
| --- | --- | --- | --- |
| SW-09 | Disable system lock | Turn off `System enabled` | State changes to `DISABLED`, manual/sensor dispense actions are blocked |
| SW-10 | Re-enable system | Turn on `System enabled` | State returns to `IDLE` or `REFILL_REQUIRED` based on alert status |
| SW-11 | Dashboard reset | Click `Reset Dashboard` | Counters, status, sync, and timeline reset to defaults |

## Evidence to Capture

- 1 screenshot for normal dispense flow (`SW-01`).
- 1 screenshot for refill alert (`SW-04`).
- 1 screenshot for degraded sync and recovery (`SW-07`, `SW-08`).
- 1 short video showing disabled/enabled safety lock behavior (`SW-09`, `SW-10`).
