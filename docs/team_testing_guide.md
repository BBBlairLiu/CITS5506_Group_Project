# SmartSan Team Testing Guide

Use this guide to test the current SmartSan ESP32 and Blynk integration before the final hardware assembly.

## Current Test Scope

The main firmware is:

```text
firmware/SmartSanESP32/SmartSanESP32.ino
```

Do not use `sketch_may7a` for Blynk testing. That sketch only toggles GPIO 2 and does not contain the SmartSan state machine.

The current firmware starts in mock hardware mode:

```cpp
#define USE_MOCK_HARDWARE 1
```

In this mode, the real IR sensor and servo are not used. A Serial Monitor command simulates a hand-detection event.

## One-Time Local Setup

1. Open `firmware/SmartSanESP32/SmartSanESP32.ino` in Arduino IDE.
2. Copy `firmware/SmartSanESP32/secrets.example.h` to `firmware/SmartSanESP32/secrets.h`.
3. Fill `secrets.h` with the Blynk template ID, Blynk auth token, Wi-Fi SSID, and Wi-Fi password.
4. Keep `secrets.h` local. It is ignored by git and should not be pushed.
5. In Arduino IDE, select the ESP32 board and the correct serial port.
6. Set Serial Monitor baud rate to `115200`.

## Blynk Datastream Contract

| Pin | Name | Direction | Expected Use |
| --- | --- | --- | --- |
| `V0` | `usageCount` | ESP32 to Blynk | Total accepted dispense count |
| `V2` | `remainingPercent` | ESP32 to Blynk | Estimated remaining percentage |
| `V3` | `refillAlert` | ESP32 to Blynk | `1` when refill is required |
| `V4` | `deviceState` | ESP32 to Blynk | State string such as `IDLE` or `DISPENSING` |
| `V5` | `lastDispenseAt` | ESP32 to Blynk | Runtime label for last dispense |
| `V6` | `deviceOnline` | ESP32 to Blynk | `1` when Blynk is connected |
| `V7` | `remainingPumps` | ESP32 to Blynk | Remaining mock pump count |
| `V10` | `manualDispense` | Blynk to ESP32 | Button input, send `1` to trigger one cycle |
| `V11` | `resetAlert` | Blynk to ESP32 | Button input, send `1` to reset refill count |
| `V12` | `systemEnabled` | Blynk to ESP32 | Switch input, `1` enabled and `0` disabled |

## Mock Firmware Test

1. Upload the firmware with `USE_MOCK_HARDWARE` set to `1`.
2. Open Serial Monitor at `115200`.
3. Send `d` or `D`.
4. Expected Serial Monitor output includes:

```text
[MOCK] Trigger dispense
[MOCK] Dispense start
[MOCK] Dispense end
[COUNT] Total: 1 | Session: 1 | Remaining: 24
```

5. Expected Blynk changes:
   - `usageCount` increases by 1.
   - `remainingPumps` decreases by 1.
   - `remainingPercent` updates.
   - `deviceState` moves through `HAND_DETECTED`, `DISPENSING`, `WAIT_STABILISE`, then back to `IDLE`.

## Blynk Control Test

Run these tests after the mock firmware test works.

| Test | Action | Expected Result |
| --- | --- | --- |
| Manual dispense | Press the `manualDispense` widget mapped to `V10` | One dispense cycle runs |
| Disable system | Set `systemEnabled` on `V12` to `0` | Device state becomes `DISABLED`; dispense is blocked |
| Re-enable system | Set `systemEnabled` on `V12` to `1` | Device returns to `IDLE` or `REFILL_REQUIRED` |
| Reset refill alert | Press `resetAlert` on `V11` | `remainingPumps` resets to `25`; alert clears |

## If `d` or `D` Has No Response

Check these items in order:

1. Confirm the uploaded sketch is `firmware/SmartSanESP32/SmartSanESP32.ino`.
2. Confirm Serial Monitor is set to `115200`.
3. Confirm `USE_MOCK_HARDWARE` is `1`.
4. Confirm the ESP32 is connected to Wi-Fi and Blynk. `Blynk.begin(...)` can block the sketch if credentials or token are incorrect.
5. Confirm `systemEnabled` is `1` in Blynk.
6. Wait at least 2 seconds between repeated sends because the firmware has a dispense lockout.

## Real Hardware Test

Only move to this phase after the mock and Blynk control tests pass.

1. Install the `ESP32Servo` library in Arduino IDE.
2. Change the firmware to:

```cpp
#define USE_MOCK_HARDWARE 0
```

3. Wire the hardware according to the current firmware constants:

| Hardware | ESP32 GPIO | Firmware Constant |
| --- | --- | --- |
| IR sensor digital output | GPIO 2 | `PIN_IR_SENSOR` |
| Servo signal | GPIO 5 | `PIN_SERVO` |

4. Upload the firmware.
5. Place a hand near the IR sensor.
6. Expected result:
   - Serial Monitor prints `[IR] Hand detected`.
   - Servo performs one press-and-return cycle.
   - Blynk metrics update after the accepted cycle.

## Evidence To Capture

Each tester should capture:

- One screenshot of Blynk after a successful mock `d` test.
- One screenshot or short video of the `manualDispense` Blynk control working.
- One screenshot showing `systemEnabled = 0` blocks dispensing.
- If hardware is connected, one short video of IR detection triggering the servo and Blynk updates.

Record any failure with the exact step number, Serial Monitor output, Blynk widget values, and whether mock mode or real hardware mode was used.
