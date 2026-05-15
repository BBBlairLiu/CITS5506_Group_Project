# SmartSan Blynk Dashboard Setup

This project uses Blynk as the primary dashboard platform because it gives the team a mobile-friendly demo quickly while the hardware and 3D printed enclosure are still being prepared.

## Blynk Template

Create one Blynk template for the prototype:

- Template name: `SmartSan Prototype`
- Hardware: `ESP32`
- Connection type: `WiFi`
- Device name: `SmartSan Demo Unit`

Keep Adafruit IO as a backup for data logging only. The main demo should use Blynk first so the flow is easy to show on a phone.

## Virtual Pin Map

| Virtual Pin | Field | Type | Unit | Direction | Suggested Widget |
| --- | --- | --- | --- | --- | --- |
| `V0` | `usageCount` | Integer | events | ESP32 to Blynk | Value Display |
| `V2` | `remainingPercent` | Integer | % | ESP32 to Blynk | Gauge |
| `V3` | `refillAlert` | Integer | 0/1 | ESP32 to Blynk | LED or Label |
| `V4` | `deviceState` | String | none | ESP32 to Blynk | Label |
| `V5` | `lastDispenseAt` | String | time | ESP32 to Blynk | Label |
| `V6` | `deviceOnline` | Integer | 0/1 | ESP32 to Blynk | LED |
| `V7` | `liquidWeightGrams` | Double | g | ESP32 to Blynk | Value Display or Chart |
| `V8` | `distanceMm` | Integer | mm | ESP32 to Blynk | Value Display |
| `V9` | `lastDispenseGrams` | Double | g | ESP32 to Blynk | Value Display |
| `V10` | `manualDispense` | Integer | 0/1 | Blynk to ESP32 | Button |
| `V11` | `resetAlert` | Integer | 0/1 | Blynk to ESP32 | Button |
| `V12` | `systemEnabled` | Integer | 0/1 | Blynk to ESP32 | Switch |

## Blynk Console Changes for Weight-Based Firmware

Update the existing template datastreams before uploading the hardware-enabled firmware:

1. Edit `V7`:
   - Name: `liquidWeightGrams`
   - Data type: `Double`
   - Units: `g`
   - Min: `0`
   - Max: `700`
   - Decimals: `1`
2. Add `V8`:
   - Name: `distanceMm`
   - Data type: `Integer`
   - Units: `mm`
   - Min: `0`
   - Max: `2000`
3. Add `V9`:
   - Name: `lastDispenseGrams`
   - Data type: `Double`
   - Units: `g`
   - Min: `0`
   - Max: `100`
   - Decimals: `1`
4. Keep `V2` as `remainingPercent` with Min `0`, Max `100`, and units `%`.
5. Keep `V3` as `refillAlert` with Min `0`, Max `1`.
6. Click **Save And Apply** after editing the datastreams.

## Dashboard Layout

Use this layout for the first demo:

- Top row: device online, system state, refill alert.
- Main metrics: usage count, liquid weight, remaining percentage.
- Diagnostics: distance reading and last dispense grams.
- Activity: last dispense time.
- Controls: manual dispense, reset alert, system enable switch.

## Update Rules

- Send status updates every 5 seconds while idle.
- Send immediate updates after a valid dispense event.
- Send immediate updates when refill alert changes.
- Treat `refillAlert = 1` when `remainingPercent <= 10`.
- Use `systemEnabled = 0` to block manual and sensor-triggered dispensing during testing.

## Demo Notes

The Blynk dashboard should be ready before the hardware arrives. During early testing, the ESP32 sketch can send simulated values to these virtual pins so the team can verify the dashboard workflow without sensors connected.
