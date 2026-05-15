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

In this mode, the real VL53L1X distance sensor, HX711 load cell, and servo are not used. A Serial Monitor command simulates a hand-detection event.

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
| `V7` | `liquidWeightGrams` | ESP32 to Blynk | Estimated liquid weight in grams |
| `V8` | `distanceMm` | ESP32 to Blynk | Corrected distance sensor reading |
| `V9` | `lastDispenseGrams` | ESP32 to Blynk | Estimated grams used by the latest dispense |
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
[COUNT] Total: 1 | Session: 1 | Remaining %: 96 | Liquid g: 576.0
```

5. Expected Blynk changes:
   - `usageCount` increases by 1.
   - `liquidWeightGrams` decreases.
   - `remainingPercent` updates.
   - `deviceState` moves through `HAND_DETECTED`, `DISPENSING`, `WAIT_STABILISE`, then back to `IDLE`.

## Blynk Control Test

Run these tests after the mock firmware test works.

| Test | Action | Expected Result |
| --- | --- | --- |
| Manual dispense | Press the `manualDispense` widget mapped to `V10` | One dispense cycle runs |
| Disable system | Set `systemEnabled` on `V12` to `0` | Device state becomes `DISABLED`; dispense is blocked |
| Re-enable system | Set `systemEnabled` on `V12` to `1` | Device returns to `IDLE` or `REFILL_REQUIRED` |
| Reset refill alert | Press `resetAlert` on `V11` | Mock mode resets liquid weight to `600 g`; real mode rereads the load cell and clears only if weight is above the refill threshold |

## If `d` or `D` Has No Response

Check these items in order:

1. Confirm the uploaded sketch is `firmware/SmartSanESP32/SmartSanESP32.ino`.
2. Confirm Serial Monitor is set to `115200`.
3. Confirm `USE_MOCK_HARDWARE` is `1`.
4. Confirm the ESP32 has uploaded successfully. The current firmware uses non-blocking Blynk connection logic, so Serial Monitor samples should still print even if Wi-Fi/Blynk is not connected yet.
5. Confirm `systemEnabled` is `1` in Blynk.
6. Wait at least 2 seconds between repeated sends because the firmware has a dispense lockout.

## Real Hardware Test

Only move to this phase after the mock and Blynk control tests pass.

1. Install the `VL53L1X`, `HX711`, and `ESP32Servo` libraries in Arduino IDE.
2. Change the firmware to:

```cpp
#define USE_MOCK_HARDWARE 0
```

3. Select the board package that supports the connected XIAO ESP32 `D1`-style pin labels.
4. Wire the hardware according to the current firmware constants:

| Hardware | Board Pin | Firmware Constant |
| --- | --- | --- |
| Servo signal | `D1` | `PIN_SERVO` |
| HX711 DT / DOUT | `D2` | `PIN_LOADCELL_DOUT` |
| HX711 SCK / CLK | `D3` | `PIN_LOADCELL_SCK` |
| VL53L1X SDA | `D4` | `PIN_DISTANCE_SDA` |
| VL53L1X SCL | `D5` | `PIN_DISTANCE_SCL` |

5. Upload the firmware.
6. Place a hand between `70 mm` and `150 mm` from the distance sensor.
7. Expected result:
   - Serial Monitor prints `[DISTANCE] Hand detected at ... mm`.
   - Servo performs one press-and-return cycle.
   - Serial Monitor prints CSV samples with `time_ms,distance_mm,load_raw,liquid_g,remaining_percent,last_dispense_g,state,event`.
   - Blynk metrics update after the accepted cycle.

## Evidence To Capture

Each tester should capture:

- One screenshot of Blynk after a successful mock `d` test.
- One screenshot or short video of the `manualDispense` Blynk control working.
- One screenshot showing `systemEnabled = 0` blocks dispensing.
- If hardware is connected, one short video of distance detection triggering the servo and Blynk updates.
- A copied CSV sample from Serial Monitor covering at least 10 dispense attempts.

Record any failure with the exact step number, Serial Monitor output, Blynk widget values, and whether mock mode or real hardware mode was used.
